#include "server/router.hpp"

#include <nlohmann/json.hpp>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

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

json asset_to_json(const core::Asset& a) {
    json obj;
    obj["id"] = a.id;
    obj["originalFilename"] = a.original_filename;
    obj["mediaType"] = (a.media_type == core::MediaType::Image) ? "image" : "video";
    obj["status"] = (a.status == core::AssetStatus::Active) ? "active" : "trashed";
    obj["sizeBytes"] = a.size_bytes;
    obj["capturedAt"] = a.captured_at.has_value() ? json{*a.captured_at} : json{nullptr};
    obj["width"] = a.width.has_value() ? json{*a.width} : json{nullptr};
    obj["height"] = a.height.has_value() ? json{*a.height} : json{nullptr};
    obj["camera"] = a.camera.has_value() ? json{*a.camera} : json{nullptr};
    obj["favorite"] = a.favorite;
    obj["sha256"] = a.sha256.has_value() ? json{*a.sha256} : json{nullptr};
    obj["durationSeconds"] =
        a.duration_seconds.has_value() ? json{*a.duration_seconds} : json{nullptr};
    obj["videoCodec"] = a.video_codec.has_value() ? json{*a.video_codec} : json{nullptr};

    if (a.location.has_value()) {
        obj["gps"] = {{"lat", a.location->latitude},
                      {"lon", a.location->longitude},
                      {"alt", a.location->altitude.has_value()
                                  ? json{*a.location->altitude}
                                  : json{nullptr}}};
    } else {
        obj["gps"] = nullptr;
    }

    obj["thumbnailUrl"] = a.thumbnail_path.has_value()
                              ? json{"/api/assets/" + std::to_string(a.id) + "/thumbnail"}
                              : json{nullptr};
    obj["previewUrl"] = a.preview_path.has_value()
                            ? json{"/api/assets/" + std::to_string(a.id) + "/preview"}
                            : json{nullptr};
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

    // Static file fallback (web/dist)
    const auto dist_dir = config_.data_dir.parent_path() / "web" / "dist";
    if (req.method == "GET") {
        auto file = dist_dir / req.path.substr(1);  // strip leading /
        if (std::filesystem::is_directory(file)) {
            file /= "index.html";
        }
        if (std::filesystem::is_regular_file(file)) {
            return {.status = 200, .content_type = "text/html", .file_path = file};
        }
        // SPA fallback
        const auto index = dist_dir / "index.html";
        if (std::filesystem::is_regular_file(index)) {
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

            const core::AssetStatus status =
                (status_str == "trashed") ? core::AssetStatus::Trashed
                                           : core::AssetStatus::Active;

            std::size_t limit = 50;
            std::size_t offset = 0;
            std::from_chars(limit_str.data(), limit_str.data() + limit_str.size(), limit);
            std::from_chars(offset_str.data(), offset_str.data() + offset_str.size(), offset);
            limit = std::min(limit, std::size_t{200});

            const auto items = assets_.list(limit, offset);
            const auto total = assets_.count(status);

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
