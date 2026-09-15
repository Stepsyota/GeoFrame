#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace geoframe::core {

/**
 * @brief Конфигурация запуска GeoFrame (из CLI-флагов или config.toml в будущем).
 */
struct Config {
    std::filesystem::path source;                                         // source library folder
    std::filesystem::path data_dir;                                       // db + cache + certs
    std::string host = "0.0.0.0";                                        // bind address
    std::uint16_t port = 8443;                                           // HTTPS port
    int worker_threads = 0;                                               // 0 = hardware_concurrency - 1
    int thumbnail_max_px = 500;                                           // max dimension for thumbnail
    int preview_max_px = 2000;                                            // max dimension for preview
    bool skip_tls = false;                                                // HTTP (no TLS) for dev
};

}  // namespace geoframe::core
