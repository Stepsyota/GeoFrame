#pragma once

#include "core/asset_repository.hpp"
#include "core/event_bus.hpp"
#include "core/job_repository.hpp"
#include "core/progress_tracker.hpp"

#include <atomic>
#include <chrono>
#include <stop_token>
#include <string>
#include <thread>

namespace geoframe::server {

/**
 * @brief Periodically publishes library progress snapshots to the event bus.
 */
class ProgressBroadcaster {
public:
    ProgressBroadcaster(core::EventBus& events, core::ProgressTracker& progress,
                        core::IAssetRepository& assets, core::IJobRepository& jobs,
                        std::chrono::milliseconds interval = std::chrono::milliseconds{500});

    ~ProgressBroadcaster();

    ProgressBroadcaster(const ProgressBroadcaster&) = delete;
    ProgressBroadcaster& operator=(const ProgressBroadcaster&) = delete;

    void start();
    void stop();
    [[nodiscard]] std::string snapshot_json() const;

private:
    void run(std::stop_token stop_token);

    core::EventBus& events_;
    core::ProgressTracker& progress_;
    core::IAssetRepository& assets_;
    core::IJobRepository& jobs_;
    std::chrono::milliseconds interval_;
    std::jthread thread_;
    std::string last_payload_;
};

}  // namespace geoframe::server
