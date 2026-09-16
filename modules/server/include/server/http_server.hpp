#pragma once

#include "core/asset_repository.hpp"
#include "core/config.hpp"
#include "core/event_bus.hpp"
#include "core/job_repository.hpp"
#include "core/progress_tracker.hpp"

#include <memory>

namespace geoframe::server {

/**
 * @brief HTTPS-сервер с REST API и WebSocket-каналом событий.
 *
 * Принимает соединения в отдельном потоке, обслуживает каждую сессию
 * в собственном std::jthread. Изолирует Boost.Beast/Asio от остального кода.
 */
class HttpServer {
public:
    HttpServer(const core::Config& config,
               core::IAssetRepository& assets,
               core::IJobRepository& jobs,
               core::EventBus& events,
               core::ProgressTracker& progress);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /** Запускает acceptor-цикл (блокирует до stop()). */
    void run();

    /** Сигнализирует серверу о завершении работы. */
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace geoframe::server
