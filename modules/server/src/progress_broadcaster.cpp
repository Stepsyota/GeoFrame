#include "server/progress_broadcaster.hpp"

#include <nlohmann/json.hpp>

namespace geoframe::server {

namespace {

using json = nlohmann::json;

int combined_percent(const core::JobTypeStats& left, const core::JobTypeStats& right) {
    const auto total = left.total() + right.total();
    if (total == 0) {
        return 100;
    }
    const auto done = left.done + right.done;
    return static_cast<int>((done * 100) / total);
}

}  // namespace

ProgressBroadcaster::ProgressBroadcaster(core::EventBus& events, core::ProgressTracker& progress,
                                         core::IAssetRepository& assets, core::IJobRepository& jobs,
                                         const std::chrono::milliseconds interval)
    : events_(events),
      progress_(progress),
      assets_(assets),
      jobs_(jobs),
      interval_(interval) {}

ProgressBroadcaster::~ProgressBroadcaster() {
    stop();
}

void ProgressBroadcaster::start() {
    if (thread_.joinable()) {
        return;
    }
    thread_ = std::jthread([this](const std::stop_token stop_token) { run(stop_token); });
}

void ProgressBroadcaster::stop() {
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_.join();
    }
}

std::string ProgressBroadcaster::snapshot_json() const {
    const auto progress = progress_.snapshot();
    const auto stats = jobs_.stats();
    const auto active_assets = assets_.count(core::AssetStatus::Active);

    json body;
    body["type"] = "scan_progress";
    body["scanning"] = progress.scanning;
    body["filesTotal"] = progress.scanning ? progress.scan_files_seen : active_assets;
    body["filesDone"] = progress.scanning ? progress.scan_files_seen : active_assets;
    body["metadataPercent"] = combined_percent(stats.metadata, stats.video_metadata);
    body["thumbnailsPercent"] = stats.thumbnail.percent_done();
    body["previewsPercent"] = stats.preview.percent_done();
    body["hashPercent"] = stats.hash.percent_done();
    body["pendingJobs"] = stats.hash.pending + stats.hash.processing + stats.metadata.pending
                        + stats.metadata.processing + stats.video_metadata.pending
                        + stats.video_metadata.processing + stats.thumbnail.pending
                        + stats.thumbnail.processing + stats.preview.pending
                        + stats.preview.processing;
    if (progress.current_file.empty()) {
        body["currentFile"] = nullptr;
    } else {
        body["currentFile"] = progress.current_file;
    }
    return body.dump();
}

void ProgressBroadcaster::run(const std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        const auto payload = snapshot_json();
        if (payload != last_payload_) {
            last_payload_ = payload;
            events_.publish(payload);
        }

        std::this_thread::sleep_for(interval_);
    }
}

}  // namespace geoframe::server
