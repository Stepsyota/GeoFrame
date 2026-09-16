#include "server/router.hpp"

#include "core/duplicate_grouper.hpp"
#include "core/map_clusterer.hpp"
#include "core/series_detector.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

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

json asset_to_json(const core::Asset& a) {
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
    return obj;
}

}  // namespace

// ── Router ─────────────────────────────────────────────────────────────────

Router::Router(core::IAssetRepository& assets,
               core::IJobRepository& jobs,
               const core::Config& config)
    : assets_(assets), jobs_(jobs), config_(config) {
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

            const auto items = assets_.list(limit, offset, status, favorite_filter);
            const auto total = assets_.count(status, favorite_filter);

            json body;
            body["items"] = json::array();
            for (const auto& asset : items) {
                body["items"].push_back(asset_to_json(asset));
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
            return json_ok(asset_to_json(*asset));
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
            assets_.set_status(id, core::AssetStatus::Trashed);
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
            assets_.set_status(id, core::AssetStatus::Active);
            return json_ok({{"id", id}, {"status", "active"}});
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

            json body;
            body["assets"]["active"] = active;
            body["assets"]["trashed"] = trashed;
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
            // Scan is enqueued; the WorkerPool picks up the jobs.
            // For now return accepted and let the worker pool handle it.
            // Full scan trigger will be implemented when we wire the scan service.
            return {.status = 202,
                    .content_type = "application/json",
                    .body = json{{"status", "accepted"},
                                 {"source", source.string()}}.dump()};
        },
    });
}

}  // namespace geoframe::server
