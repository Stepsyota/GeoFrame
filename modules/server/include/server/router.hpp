#pragma once

#include "core/asset_repository.hpp"
#include "core/config.hpp"
#include "core/job_repository.hpp"
#include "core/progress_tracker.hpp"
#include "server/map_tile_service.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Router uses our own HttpRequest/HttpResponse structs — no Boost headers needed here.
namespace geoframe::server {

struct HttpRequest {
    std::string method;          // GET POST DELETE
    std::string target;          // /api/assets?limit=50
    std::string path;            // /api/assets (without query)
    std::string query;           // limit=50
    std::string body;
    bool keep_alive = true;
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
    std::string redirect_location;          // for 302
    std::filesystem::path file_path;        // for file streaming
    bool range_requests = false;            // enable HTTP Range for PMTiles
    bool keep_alive = true;
};

/**
 * @brief Сопоставляет HTTP-запрос с обработчиком и возвращает ответ.
 */
class Router {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&, const std::vector<std::string>&)>;

    Router(core::IAssetRepository& assets,
           core::IJobRepository& jobs,
           const core::Config& config,
           core::ProgressTracker& progress);

    HttpResponse dispatch(const HttpRequest& req) const;

private:
    struct Route {
        std::string method;
        std::vector<std::string> segments;   // split by '/', "{id}" is a capture
        Handler handler;
    };

    void register_routes();
    bool match(const Route& route, const std::string& method,
               const std::vector<std::string>& path_segments,
               std::vector<std::string>& captures) const;

    std::vector<Route> routes_;
    core::IAssetRepository& assets_;
    core::IJobRepository& jobs_;
    const core::Config& config_;
    core::ProgressTracker& progress_;
    mutable MapTileService map_tiles_;
    std::atomic<bool> scan_running_{false};
};

}  // namespace geoframe::server
