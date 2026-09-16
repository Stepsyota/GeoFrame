#pragma once

#include <cstddef>
#include <mutex>
#include <string>

namespace geoframe::core {

/**
 * @brief Mutable scan/worker state shared between background tasks and HTTP layer.
 */
class ProgressTracker {
public:
    void set_scanning(bool value);
    void increment_scan_files();
    void set_current_file(std::string filename);
    void clear_current_file();

    struct Snapshot {
        bool scanning = false;
        std::size_t scan_files_seen = 0;
        std::string current_file;
    };

    [[nodiscard]] Snapshot snapshot() const;

private:
    mutable std::mutex mutex_;
    bool scanning_ = false;
    std::size_t scan_files_seen_ = 0;
    std::string current_file_;
};

}  // namespace geoframe::core
