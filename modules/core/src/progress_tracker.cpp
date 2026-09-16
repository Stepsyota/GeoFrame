#include "core/progress_tracker.hpp"

namespace geoframe::core {

void ProgressTracker::set_scanning(const bool value) {
    std::lock_guard lock{mutex_};
    scanning_ = value;
    if (!value) {
        current_file_.clear();
    }
}

void ProgressTracker::increment_scan_files() {
    std::lock_guard lock{mutex_};
    ++scan_files_seen_;
}

void ProgressTracker::set_current_file(std::string filename) {
    std::lock_guard lock{mutex_};
    current_file_ = std::move(filename);
}

void ProgressTracker::clear_current_file() {
    std::lock_guard lock{mutex_};
    current_file_.clear();
}

ProgressTracker::Snapshot ProgressTracker::snapshot() const {
    std::lock_guard lock{mutex_};
    return Snapshot{
        .scanning = scanning_,
        .scan_files_seen = scan_files_seen_,
        .current_file = current_file_,
    };
}

}  // namespace geoframe::core
