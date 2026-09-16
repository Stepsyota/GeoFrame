#pragma once

#include <cstddef>

namespace geoframe::core {

/** Per-job-type counters grouped by queue status. */
struct JobTypeStats {
    std::size_t pending = 0;
    std::size_t processing = 0;
    std::size_t done = 0;
    std::size_t failed = 0;

    [[nodiscard]] std::size_t total() const {
        return pending + processing + done + failed;
    }

    [[nodiscard]] int percent_done() const {
        const auto count = total();
        if (count == 0) {
            return 100;
        }
        return static_cast<int>((done * 100) / count);
    }
};

/** Snapshot of the background job queue used for progress reporting. */
struct JobStats {
    JobTypeStats hash;
    JobTypeStats metadata;
    JobTypeStats video_metadata;
    JobTypeStats thumbnail;
    JobTypeStats preview;
};

}  // namespace geoframe::core
