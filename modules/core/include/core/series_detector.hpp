#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace geoframe::core {

struct TimedAsset {
    std::int64_t id;
    std::string captured_at;
};

struct PhotoSeries {
    std::vector<std::int64_t> asset_ids;
};

/**
 * @brief Detect burst/series groups from capture timestamps.
 *
 * Assets must be sorted by capture time. Consecutive photos within
 * @p max_gap_seconds are merged into one series (minimum 2 assets).
 */
std::vector<PhotoSeries> detect_photo_series(const std::vector<TimedAsset>& assets,
                                             int max_gap_seconds = 3);

/** Parse EXIF or ISO capture timestamps to epoch seconds. */
std::optional<std::int64_t> parse_captured_epoch(const std::string& captured_at);

}  // namespace geoframe::core
