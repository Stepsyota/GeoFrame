#include "server/router.hpp"

#include "server/map_tile_service.hpp"

#include "core/duplicate_grouper.hpp"
#include "core/map_clusterer.hpp"
#include "core/series_detector.hpp"
#include "media/metadata_extractor.hpp"
#include "storage/directory_scanner.hpp"
#include "worker/scan_service.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

namespace geoframe::server {

using json = nlohmann::json;

namespace {

// ── Helpers ────────────────────────────────────────────────────────────────

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

// Simple query string parser: key=value&key2=value2
std::unordered_map<std::string, std::string> parse_query(const std::string& query) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream ss(query);
    std::string token;
    while (std::getline(ss, token, '&')) {
        const auto eq = token.find('=');
        if (eq == std::string::npos) {
            params[token] = "";
        } else {
            params[token.substr(0, eq)] = token.substr(eq + 1);
        }
    }
    return params;
}

std::int64_t require_id(const std::string& s) {
    std::int64_t value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || value <= 0) {
        throw std::invalid_argument("Invalid asset id: " + s);
    }
    return value;
}

HttpResponse json_ok(const json& body) {
    return {.status = 200, .content_type = "application/json", .body = body.dump()};
}

HttpResponse json_error(const int status, const std::string& message) {
    return {.status = status, .content_type = "application/json",
            .body = json{{"error", message}}.dump()};
}

HttpResponse not_found(const std::string& what = "Not found") {
    return json_error(404, what);
}

// ── Asset serialisation ────────────────────────────────────────────────────

/**
 * @brief Normalises an EXIF date string to ISO 8601 for JSON consumption.
 *
 * Exiv2 stores dates as "YYYY:MM:DD HH:MM:SS".  JavaScript's Date() and
 * Intl.DateTimeFormat require "YYYY-MM-DDTHH:MM:SS".  Returns empty string
 * on unrecognised input so the caller can emit null.
 */
std::string exif_to_iso(const std::string& exif) {
    // Expected length: "YYYY:MM:DD HH:MM:SS" = 19 chars
    if (exif.size() < 19) {
        return {};
    }
    std::string iso = exif;
    iso[4]  = '-';
    iso[7]  = '-';
    iso[10] = 'T';
    return iso;
}

bool is_path_under(const std::filesystem::path& child, const std::filesystem::path& parent) {
    std::error_code ec;
    const auto rel = std::filesystem::relative(child, parent, ec);
    if (ec || rel.empty()) {
        return false;
    }
    for (const auto& part : rel) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

/** Append ?v=<size>:<mtime> so browsers fetch a new file after regeneration. */
std::string media_url_with_version(const std::int64_t id, const std::string_view kind,
                                   const std::filesystem::path& path) {
    std::string url = "/api/assets/" + std::to_string(id) + "/" + std::string{kind};
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        url += "?v=" + std::to_string(size) + ':'
             + std::to_string(mtime.time_since_epoch().count());
    }
    return url;
}

json asset_to_json(const core::Asset& a,
                   const std::optional<std::int64_t> live_video_id = std::nullopt) {
    json obj;
    obj["id"] = a.id;
    obj["originalFilename"] = a.original_filename;
    obj["mediaType"] = (a.media_type == core::MediaType::Image) ? "image" : "video";
    obj["status"] = (a.status == core::AssetStatus::Active) ? "active" : "trashed";
    obj["sizeBytes"] = a.size_bytes;

    // json{scalar} with braces creates a JSON array — assign scalars directly.
    // Never use (condition ? string : nullptr): common type becomes std::string
    // and nullptr branch throws "construction from null is not valid".
    if (a.captured_at.has_value()) {
        const auto iso = exif_to_iso(*a.captured_at);
        if (iso.empty()) {
            obj["capturedAt"] = nullptr;
        } else {
            obj["capturedAt"] = iso;
        }
    } else {
        obj["capturedAt"] = nullptr;
    }

    if (a.width.has_value())  obj["width"]  = *a.width;  else obj["width"]  = nullptr;
    if (a.height.has_value()) obj["height"] = *a.height; else obj["height"] = nullptr;
    if (a.camera.has_value()) obj["camera"] = *a.camera; else obj["camera"] = nullptr;
    obj["favorite"] = a.favorite;
    if (a.sha256.has_value()) obj["sha256"] = *a.sha256; else obj["sha256"] = nullptr;
    if (a.duration_seconds.has_value()) obj["durationSeconds"] = *a.duration_seconds;
    else obj["durationSeconds"] = nullptr;
    if (a.video_codec.has_value()) obj["videoCodec"] = *a.video_codec;
    else obj["videoCodec"] = nullptr;

    if (a.location.has_value()) {
        json gps;
        gps["lat"] = a.location->latitude;
        gps["lon"] = a.location->longitude;
        if (a.location->altitude.has_value()) gps["alt"] = *a.location->altitude;
        else gps["alt"] = nullptr;
        obj["gps"] = gps;
    } else {
        obj["gps"] = nullptr;
    }

    if (a.thumbnail_path.has_value()) {
        obj["thumbnailUrl"] = media_url_with_version(a.id, "thumbnail", *a.thumbnail_path);
    } else {
        obj["thumbnailUrl"] = nullptr;
    }
    if (a.preview_path.has_value()) {
        obj["previewUrl"] = media_url_with_version(a.id, "preview", *a.preview_path);
    } else {
        obj["previewUrl"] = nullptr;
    }
    if (live_video_id.has_value()) {
        obj["livePhoto"] = json{{"videoId", *live_video_id}};
    }
    return obj;
}

std::unordered_map<std::int64_t, std::int64_t> live_photo_video_map(
    core::IAssetRepository& assets) {
    std::unordered_map<std::int64_t, std::int64_t> map;
    for (const auto& link : assets.list_live_photo_pairs()) {
        map.emplace(link.image_asset_id, link.video_asset_id);
    }
    return map;
}

void remove_file_quietly(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void remove_asset_files(const core::Asset& asset) {
    remove_file_quietly(asset.source_path);
    if (asset.thumbnail_path.has_value()) {
        remove_file_quietly(*asset.thumbnail_path);
    }
    if (asset.preview_path.has_value()) {
        remove_file_quietly(*asset.preview_path);
    }
}

void trash_asset_with_companion(core::IAssetRepository& assets, const std::int64_t id) {
    if (const auto video_id = assets.live_photo_video_for_image(id); video_id.has_value()) {
        assets.set_status(*video_id, core::AssetStatus::Trashed);
    } else if (const auto image_id = assets.live_photo_image_for_video(id); image_id.has_value()) {
        assets.set_status(*image_id, core::AssetStatus::Trashed);
    }
    assets.set_status(id, core::AssetStatus::Trashed);
}

void restore_asset_with_companion(core::IAssetRepository& assets, const std::int64_t id) {
    if (const auto video_id = assets.live_photo_video_for_image(id); video_id.has_value()) {
        assets.set_status(*video_id, core::AssetStatus::Active);
    } else if (const auto image_id = assets.live_photo_image_for_video(id); image_id.has_value()) {
        assets.set_status(*image_id, core::AssetStatus::Active);
    }
    assets.set_status(id, core::AssetStatus::Active);
}

void erase_asset_with_companion(core::IAssetRepository& assets, const core::Asset& asset) {
    std::optional<std::int64_t> companion_id = assets.live_photo_video_for_image(asset.id);
    if (!companion_id.has_value()) {
        companion_id = assets.live_photo_image_for_video(asset.id);
    }

    if (companion_id.has_value()) {
        const auto companion = assets.find_by_id(*companion_id);
        if (companion.has_value()) {
            remove_asset_files(*companion);
            assets.erase(*companion_id);
        }
    }

    remove_asset_files(asset);
    assets.erase(asset.id);
}

std::uint64_t directory_size_bytes(const std::filesystem::path& root) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return 0;
    }

    std::uint64_t total = 0;
    for (std::filesystem::recursive_directory_iterator it{root, ec}, end;
         !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            const auto size = std::filesystem::file_size(it->path(), ec);
            if (!ec) {
                total += size;
            }
        }
    }
    return total;
}

bool confirms_action(const HttpRequest& req, const std::string& expected) {
    if (req.body.empty()) {
        return false;
    }
    const auto parsed = json::parse(req.body, nullptr, false);
    return parsed.is_object() && parsed.contains("confirm") && parsed["confirm"].is_string()
           && parsed["confirm"].get<std::string>() == expected;
}

}  // namespace

// ── Router ─────────────────────────────────────────────────────────────────

Router::Router(core::IAssetRepository& assets,
               core::IJobRepository& jobs,
               const core::Config& config,
               core::ProgressTracker& progress)
    : assets_(assets),
      jobs_(jobs),
      config_(config),
      progress_(progress),
      map_tiles_(config.data_dir) {
    register_routes();
}

bool Router::match(const Route& route,
                   const std::string& method,
                   const std::vector<std::string>& path_segments,
                   std::vector<std::string>& captures) const {
    if (route.method != method) {
        return false;
    }
    if (route.segments.size() != path_segments.size()) {
        return false;
    }
    captures.clear();
    for (std::size_t i = 0; i < route.segments.size(); ++i) {
        if (route.segments[i].size() >= 2 && route.segments[i].front() == '{' &&
            route.segments[i].back() == '}') {
            captures.push_back(path_segments[i]);
        } else if (route.segments[i] != path_segments[i]) {
            return false;
        }
    }
    return true;
}

HttpResponse Router::dispatch(const HttpRequest& req) const {
    const auto path_segs = split_path(req.path);

    std::vector<std::string> captures;
    for (const auto& route : routes_) {
        if (match(route, req.method, path_segs, captures)) {
            try {
                return route.handler(req, captures);
            } catch (const std::invalid_argument& ex) {
                return json_error(400, ex.what());
            } catch (const std::out_of_range& ex) {
                return not_found(ex.what());
            } catch (const std::exception& ex) {
                return json_error(500, ex.what());
            }
        }
    }

    // Static file fallback (React production build)
    const auto& dist_dir = config_.web_dir;
    if (req.method == "GET" && !dist_dir.empty()) {
        const auto rel = req.path.empty() || req.path == "/" ? "index.html" : req.path.substr(1);
        auto file = dist_dir / rel;
        std::error_code ec;
        if (std::filesystem::is_directory(file, ec)) {
            file /= "index.html";
        }
        const auto resolved = std::filesystem::weakly_canonical(file, ec);
        const auto dist_root = std::filesystem::weakly_canonical(dist_dir, ec);
        if (!ec && is_path_under(resolved, dist_root) &&
            std::filesystem::is_regular_file(resolved, ec)) {
            return {.status = 200, .content_type = "text/html", .file_path = resolved};
        }
        // SPA fallback — unknown routes serve index.html
        const auto index = dist_root / "index.html";
        if (std::filesystem::is_regular_file(index, ec)) {
            return {.status = 200, .content_type = "text/html", .file_path = index};
        }
    }

    return not_found();
}

void Router::register_routes() {
    // ── GET /api/assets?limit=50&offset=0&status=active ──────────────────
    routes_.push_back({
        "GET",
        {"api", "assets"},
        [this](const HttpRequest& req, const std::vector<std::string>&) -> HttpResponse {
            const auto params = parse_query(req.query);
            const auto status_str =
                params.count("status") ? params.at("status") : std::string{"active"};
            const auto limit_str = params.count("limit") ? params.at("limit") : std::string{"50"};
            const auto offset_str =
                params.count("offset") ? params.at("offset") : std::string{"0"};
            const auto favorite_str =
                params.count("favorite") ? params.at("favorite") : std::string{};
            const auto search_query = params.count("q") ? params.at("q") : std::string{};

            const core::AssetStatus status =
                (status_str == "trashed") ? core::AssetStatus::Trashed
                                           : core::AssetStatus::Active;

            std::optional<bool> favorite_filter;
            if (favorite_str == "1" || favorite_str == "true") {
                favorite_filter = true;
            } else if (favorite_str == "0" || favorite_str == "false") {
                favorite_filter = false;
            }

            std::size_t limit = 50;
            std::size_t offset = 0;
            std::from_chars(limit_str.data(), limit_str.data() + limit_str.size(), limit);
            std::from_chars(offset_str.data(), offset_str.data() + offset_str.size(), offset);
            limit = std::min(limit, std::size_t{200});

            std::optional<std::string> search_filter;
            if (!search_query.empty()) {
                search_filter = search_query;
            }

            const auto items = assets_.list(limit, offset, status, favorite_filter, search_filter);
            const auto total = assets_.count(status, favorite_filter, search_filter);
            const auto live_photos = live_photo_video_map(assets_);

            json body;
            body["items"] = json::array();
            for (const auto& asset : items) {
                const auto it = live_photos.find(asset.id);
                const auto live_video_id =
                    it != live_photos.end() ? std::optional<std::int64_t>{it->second}
                                            : std::nullopt;
                body["items"].push_back(asset_to_json(asset, live_video_id));
            }
            body["total"] = total;
            body["limit"] = limit;
            body["offset"] = offset;
            return json_ok(body);
        },
    });

    // ── GET /api/assets/{id} ──────────────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "assets", "{id}"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }
            const auto live_video_id = assets_.live_photo_video_for_image(id);
            return json_ok(asset_to_json(*asset, live_video_id));
        },
    });

    // ── GET /api/assets/{id}/thumbnail ────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "assets", "{id}", "thumbnail"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value() || !asset->thumbnail_path.has_value()) {
                return not_found("Thumbnail not yet generated");
            }
            return {.status = 200,
                    .content_type = "image/jpeg",
                    .file_path = *asset->thumbnail_path};
        },
    });

    // ── GET /api/assets/{id}/preview ─────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "assets", "{id}", "preview"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }
            // Fall back to thumbnail if preview not yet generated
            const auto& path =
                asset->preview_path.has_value() ? asset->preview_path : asset->thumbnail_path;
            if (!path.has_value()) {
                return not_found("Preview not yet generated");
            }
            return {.status = 200, .content_type = "image/jpeg", .file_path = *path};
        },
    });

    // ── GET /api/assets/{id}/exif ─────────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "assets", "{id}", "exif"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }

            json body;
            body["tags"] = json::array();
            if (asset->media_type != core::MediaType::Image) {
                body["mediaType"] = "video";
                return json_ok(body);
            }

            try {
                const auto tags = media::extract_image_exif(asset->source_path);
                for (const auto& tag : tags) {
                    body["tags"].push_back({{"key", tag.key}, {"value", tag.value}});
                }
            } catch (const std::exception& error) {
                return json_error(500, error.what());
            }

            body["mediaType"] = "image";
            return json_ok(body);
        },
    });

    // ── GET /api/assets/{id}/original ─────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "assets", "{id}", "original"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }
            return {.status = 200,
                    .content_type = "application/octet-stream",
                    .file_path = asset->source_path};
        },
    });

    // ── POST /api/assets/{id}/favorite ────────────────────────────────────
    routes_.push_back({
        "POST",
        {"api", "assets", "{id}", "favorite"},
        [this](const HttpRequest& req, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }
            // Toggle if no body; body {"favorite": bool} to set explicitly
            bool new_value = !asset->favorite;
            if (!req.body.empty()) {
                const auto parsed = json::parse(req.body, nullptr, false);
                if (parsed.is_object() && parsed.contains("favorite")) {
                    new_value = parsed["favorite"].get<bool>();
                }
            }
            assets_.set_favorite(id, new_value);
            return json_ok({{"id", id}, {"favorite", new_value}});
        },
    });

    // ── POST /api/assets/{id}/trash ───────────────────────────────────────
    routes_.push_back({
        "POST",
        {"api", "assets", "{id}", "trash"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }
            trash_asset_with_companion(assets_, id);
            return json_ok({{"id", id}, {"status", "trashed"}});
        },
    });

    // ── POST /api/assets/{id}/restore ─────────────────────────────────────
    routes_.push_back({
        "POST",
        {"api", "assets", "{id}", "restore"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }
            restore_asset_with_companion(assets_, id);
            return json_ok({{"id", id}, {"status", "active"}});
        },
    });

    // ── DELETE /api/assets/{id} ───────────────────────────────────────────
    routes_.push_back({
        "DELETE",
        {"api", "assets", "{id}"},
        [this](const HttpRequest& req, const std::vector<std::string>& caps) -> HttpResponse {
            if (!confirms_action(req, "DELETE")) {
                return json_error(400, R"(Confirmation required: {"confirm":"DELETE"})");
            }

            const auto id = require_id(caps[0]);
            const auto asset = assets_.find_by_id(id);
            if (!asset.has_value()) {
                return not_found("Asset not found");
            }
            if (asset->status != core::AssetStatus::Trashed) {
                return json_error(400, "Only trashed assets can be permanently deleted");
            }

            erase_asset_with_companion(assets_, *asset);
            return json_ok({{"id", id}, {"deleted", true}});
        },
    });

    // ── POST /api/trash/empty ─────────────────────────────────────────────
    routes_.push_back({
        "POST",
        {"api", "trash", "empty"},
        [this](const HttpRequest& req, const std::vector<std::string>&) -> HttpResponse {
            if (!confirms_action(req, "CLEAR TRASH")) {
                return json_error(400, R"(Confirmation required: {"confirm":"CLEAR TRASH"})");
            }

            std::size_t deleted = 0;
            while (true) {
                const auto batch =
                    assets_.list(200, 0, core::AssetStatus::Trashed, std::nullopt);
                if (batch.empty()) {
                    break;
                }
                for (const auto& asset : batch) {
                    erase_asset_with_companion(assets_, asset);
                    ++deleted;
                }
            }

            return json_ok({{"deleted", deleted}});
        },
    });

    // ── GET /api/map/region.pmtiles ──────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "map", "region.pmtiles"},
        [this](const HttpRequest&, const std::vector<std::string>&) -> HttpResponse {
            if (!map_tiles_.has_region()) {
                return json_error(404, "Map region file not installed");
            }
            return {.status = 200,
                    .content_type = "application/vnd.pmtiles",
                    .file_path = map_tiles_.region_path(),
                    .range_requests = true};
        },
    });

    // ── GET /api/map/tiles/{z}/{x}/{y}.mvt ───────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "map", "tiles", "{z}", "{x}", "{y}"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const int z = std::stoi(caps[0]);
            const int x = std::stoi(caps[1]);
            const auto dot = caps[2].find('.');
            const int y = std::stoi(dot == std::string::npos ? caps[2] : caps[2].substr(0, dot));

            const auto path = map_tiles_.tile(z, x, y);
            if (!path.has_value()) {
                return json_error(404, "Map tile not found");
            }
            return {.status = 200,
                    .content_type = "application/vnd.mapbox-vector-tile",
                    .file_path = *path};
        },
    });

    // ── GET /api/map/fonts/{fontstack}/{range}.pbf ────────────────────────
    routes_.push_back({
        "GET",
        {"api", "map", "fonts", "{fontstack}", "{range}"},
        [this](const HttpRequest&, const std::vector<std::string>& caps) -> HttpResponse {
            const auto dot = caps[1].find('.');
            const auto range =
                dot == std::string::npos ? caps[1] : caps[1].substr(0, dot);
            const auto path = map_tiles_.font(caps[0], range);
            if (!path.has_value()) {
                return json_error(404, "Font glyph range not found");
            }
            return {.status = 200, .content_type = "application/x-protobuf", .file_path = *path};
        },
    });

    const auto serve_sprite = [this](const std::string& name) -> HttpResponse {
        const auto path = map_tiles_.sprite(name);
        if (!path.has_value()) {
            return json_error(404, "Sprite asset not found");
        }
        const auto content_type = name.ends_with(".png") ? "image/png" : "application/json";
        return {.status = 200, .content_type = content_type, .file_path = *path};
    };

    routes_.push_back({
        "GET",
        {"api", "map", "sprite.json"},
        [serve_sprite](const HttpRequest&, const std::vector<std::string>&) -> HttpResponse {
            return serve_sprite("sprite.json");
        },
    });
    routes_.push_back({
        "GET",
        {"api", "map", "sprite.png"},
        [serve_sprite](const HttpRequest&, const std::vector<std::string>&) -> HttpResponse {
            return serve_sprite("sprite.png");
        },
    });

    // ── GET /api/map/points ───────────────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "map", "points"},
        [this](const HttpRequest& req, const std::vector<std::string>&) -> HttpResponse {
            const auto params = parse_query(req.query);
            const auto status_str =
                params.count("status") ? params.at("status") : std::string{"active"};
            const core::AssetStatus status =
                (status_str == "trashed") ? core::AssetStatus::Trashed
                                          : core::AssetStatus::Active;

            const auto points = assets_.list_geo_points(status);

            json body;
            body["type"] = "FeatureCollection";
            body["features"] = json::array();

            for (const auto& point : points) {
                const auto asset = assets_.find_by_id(point.id);
                if (!asset.has_value()) {
                    continue;
                }

                json properties;
                properties["cluster"] = false;
                properties["pointCount"] = 1;
                properties["assetIds"] = json::array({point.id});
                properties["assetId"] = point.id;
                properties["mediaType"] =
                    (point.media_type == core::MediaType::Image) ? "image" : "video";
                properties["favorite"] = point.favorite;
                if (asset->captured_at.has_value()) {
                    const auto iso = exif_to_iso(*asset->captured_at);
                    properties["capturedAt"] = iso.empty() ? nullptr : json(iso);
                } else {
                    properties["capturedAt"] = nullptr;
                }
                properties["thumbnailUrl"] = nullptr;
                properties["previewUrl"] = nullptr;
                if (asset->thumbnail_path.has_value()) {
                    properties["thumbnailUrl"] =
                        media_url_with_version(asset->id, "thumbnail", *asset->thumbnail_path);
                }
                if (asset->preview_path.has_value()) {
                    properties["previewUrl"] =
                        media_url_with_version(asset->id, "preview", *asset->preview_path);
                }

                json feature;
                feature["type"] = "Feature";
                feature["geometry"] = {
                    {"type", "Point"},
                    {"coordinates", json::array({point.longitude, point.latitude})},
                };
                feature["properties"] = properties;
                body["features"].push_back(feature);
            }

            return json_ok(body);
        },
    });

    // ── GET /api/map/clusters?zoom=10 ─────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "map", "clusters"},
        [this](const HttpRequest& req, const std::vector<std::string>&) -> HttpResponse {
            const auto params = parse_query(req.query);
            const auto zoom_str = params.count("zoom") ? params.at("zoom") : std::string{"10"};
            int zoom = 10;
            std::from_chars(zoom_str.data(), zoom_str.data() + zoom_str.size(), zoom);
            zoom = std::clamp(zoom, 0, 22);

            const auto status_str =
                params.count("status") ? params.at("status") : std::string{"active"};
            const core::AssetStatus status =
                (status_str == "trashed") ? core::AssetStatus::Trashed
                                          : core::AssetStatus::Active;

            const auto points = assets_.list_geo_points(status);
            const auto clusters = core::cluster_geo_assets(points, zoom);

            json body;
            body["type"] = "FeatureCollection";
            body["features"] = json::array();

            for (const auto& cluster : clusters) {
                json feature;
                feature["type"] = "Feature";
                feature["geometry"] = {
                    {"type", "Point"},
                    {"coordinates", json::array({cluster.longitude, cluster.latitude})},
                };

                json properties;
                properties["cluster"] = cluster.is_cluster;
                properties["pointCount"] = cluster.asset_ids.size();
                properties["assetIds"] = cluster.asset_ids;
                properties["thumbnailUrl"] = nullptr;
                properties["previewUrl"] = nullptr;
                properties["assetId"] = nullptr;
                properties["mediaType"] = nullptr;
                properties["favorite"] = false;

                bool has_representative = false;
                for (const auto asset_id : cluster.asset_ids) {
                    const auto asset = assets_.find_by_id(asset_id);
                    if (!asset.has_value()) {
                        continue;
                    }
                    if (!has_representative) {
                        properties["assetId"] = asset->id;
                        properties["mediaType"] =
                            (asset->media_type == core::MediaType::Image) ? "image" : "video";
                        properties["favorite"] = asset->favorite;
                        has_representative = true;
                    }
                    if (properties["thumbnailUrl"].is_null() &&
                        asset->thumbnail_path.has_value()) {
                        properties["thumbnailUrl"] = media_url_with_version(
                            asset->id, "thumbnail", *asset->thumbnail_path);
                    }
                    if (properties["previewUrl"].is_null() && asset->preview_path.has_value()) {
                        properties["previewUrl"] =
                            media_url_with_version(asset->id, "preview", *asset->preview_path);
                    }
                    if (!properties["thumbnailUrl"].is_null() &&
                        !properties["previewUrl"].is_null()) {
                        break;
                    }
                }

                feature["properties"] = properties;
                body["features"].push_back(feature);
            }

            return json_ok(body);
        },
    });

    // ── GET /api/duplicates?status=active ───────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "duplicates"},
        [this](const HttpRequest& req, const std::vector<std::string>&) -> HttpResponse {
            const auto params = parse_query(req.query);
            const auto status_str =
                params.count("status") ? params.at("status") : std::string{"active"};
            const core::AssetStatus status =
                (status_str == "trashed") ? core::AssetStatus::Trashed
                                          : core::AssetStatus::Active;

            const auto groups = core::group_duplicate_hashes(assets_.list_hashed_assets(status));

            json body;
            body["groups"] = json::array();
            for (const auto& group : groups) {
                json item;
                item["sha256"] = group.sha256;
                item["assetIds"] = group.asset_ids;
                item["assets"] = json::array();
                for (const auto asset_id : group.asset_ids) {
                    const auto asset = assets_.find_by_id(asset_id);
                    if (asset.has_value()) {
                        item["assets"].push_back(asset_to_json(*asset));
                    }
                }
                body["groups"].push_back(item);
            }
            body["total"] = groups.size();
            return json_ok(body);
        },
    });

    // ── GET /api/series?status=active&gap=3 ─────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "series"},
        [this](const HttpRequest& req, const std::vector<std::string>&) -> HttpResponse {
            const auto params = parse_query(req.query);
            const auto status_str =
                params.count("status") ? params.at("status") : std::string{"active"};
            const auto gap_str = params.count("gap") ? params.at("gap") : std::string{"3"};
            const core::AssetStatus status =
                (status_str == "trashed") ? core::AssetStatus::Trashed
                                          : core::AssetStatus::Active;

            int gap_seconds = 3;
            std::from_chars(gap_str.data(), gap_str.data() + gap_str.size(), gap_seconds);
            gap_seconds = std::clamp(gap_seconds, 1, 60);

            const auto detected =
                core::detect_photo_series(assets_.list_timed_assets(status), gap_seconds);

            json body;
            body["series"] = json::array();
            std::int64_t series_id = 1;
            for (const auto& entry : detected) {
                json item;
                item["id"] = series_id++;
                item["assetIds"] = entry.asset_ids;
                item["assets"] = json::array();
                for (const auto asset_id : entry.asset_ids) {
                    const auto asset = assets_.find_by_id(asset_id);
                    if (asset.has_value()) {
                        item["assets"].push_back(asset_to_json(*asset));
                    }
                }
                body["series"].push_back(item);
            }
            body["total"] = detected.size();
            body["gapSeconds"] = gap_seconds;
            return json_ok(body);
        },
    });

    // ── GET /api/status ───────────────────────────────────────────────────
    routes_.push_back({
        "GET",
        {"api", "status"},
        [this](const HttpRequest&, const std::vector<std::string>&) -> HttpResponse {
            const auto active = assets_.count(core::AssetStatus::Active);
            const auto trashed = assets_.count(core::AssetStatus::Trashed);

            std::error_code ec;
            const auto space = std::filesystem::space(config_.data_dir, ec);

            const auto progress = progress_.snapshot();

            json body;
            body["assets"]["active"] = active;
            body["assets"]["trashed"] = trashed;
            body["scanning"] = progress.scanning;
            if (config_.source.empty()) {
                body["source"] = nullptr;
            } else {
                body["source"] = config_.source.string();
            }
            body["dataDir"] = config_.data_dir.string();

            const auto thumb_dir = config_.data_dir / "cache" / "thumbnails";
            const auto preview_dir = config_.data_dir / "cache" / "previews";
            const auto thumb_bytes = directory_size_bytes(thumb_dir);
            const auto preview_bytes = directory_size_bytes(preview_dir);
            body["cache"]["thumbnailsDir"] = thumb_dir.string();
            body["cache"]["previewsDir"] = preview_dir.string();
            body["cache"]["thumbnailsMB"] =
                static_cast<std::int64_t>(thumb_bytes / (1024 * 1024));
            body["cache"]["previewsMB"] =
                static_cast<std::int64_t>(preview_bytes / (1024 * 1024));
            body["cache"]["totalMB"] =
                static_cast<std::int64_t>((thumb_bytes + preview_bytes) / (1024 * 1024));

            const bool has_region = map_tiles_.has_region();
            const int offline_zoom = map_tiles_.offline_max_zoom();
            body["map"]["regionPmtiles"] = has_region;
            body["map"]["regionMB"] =
                static_cast<std::int64_t>(map_tiles_.region_bytes() / (1024 * 1024));
            body["map"]["localMaxZoom"] = offline_zoom;
            body["map"]["remoteMaxZoom"] = MapTileService::kUpstreamMaxZoom;
            if (!has_region) {
                body["map"]["basemap"] = "carto";
            } else if (offline_zoom >= 0 && offline_zoom < MapTileService::kUpstreamMaxZoom) {
                body["map"]["basemap"] = "hybrid";
            } else {
                body["map"]["basemap"] = "pmtiles";
            }

            if (!ec) {
                body["disk"]["availableMB"] =
                    static_cast<std::int64_t>(space.available / (1024 * 1024));
                body["disk"]["totalMB"] =
                    static_cast<std::int64_t>(space.capacity / (1024 * 1024));
            }
            return json_ok(body);
        },
    });

    // ── POST /api/scan ────────────────────────────────────────────────────
    routes_.push_back({
        "POST",
        {"api", "scan"},
        [this](const HttpRequest& req, const std::vector<std::string>&) -> HttpResponse {
            // source path from body {"source": "/path"} or from config
            std::filesystem::path source = config_.source;
            if (!req.body.empty()) {
                const auto parsed = json::parse(req.body, nullptr, false);
                if (parsed.is_object() && parsed.contains("source")) {
                    source = parsed["source"].get<std::string>();
                }
            }
            if (source.empty()) {
                return json_error(400, "No source path configured");
            }

            std::error_code ec;
            if (!std::filesystem::is_directory(source, ec)) {
                return json_error(400, "Source path is not a directory");
            }

            if (progress_.snapshot().scanning) {
                return json_error(409, "Scan already in progress");
            }
            if (scan_running_.exchange(true)) {
                return json_error(409, "Scan already in progress");
            }

            const auto scan_path = std::filesystem::absolute(source).lexically_normal();
            std::thread([this, scan_path]() {
                try {
                    storage::DirectoryScanner scanner;
                    worker::ScanService scan_svc{scanner, assets_, jobs_, &progress_};
                    const auto report = scan_svc.scan(scan_path);
                    spdlog::info("API scan finished: {} new, {} existing, {} unsupported, {} errors",
                                 report.assets_created, report.already_indexed,
                                 report.unsupported, report.filesystem_errors);
                } catch (const std::exception& ex) {
                    spdlog::error("API scan failed: {}", ex.what());
                }
                scan_running_ = false;
            }).detach();

            return {.status = 202,
                    .content_type = "application/json",
                    .body = json{{"status", "accepted"},
                                 {"source", scan_path.string()}}.dump()};
        },
    });
}

}  // namespace geoframe::server
