#pragma once

#include "core/asset.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace geoframe::core {

constexpr double kMaxLivePhotoDurationSeconds = 4.0;

struct PairingCandidate {
    std::int64_t id;
    std::filesystem::path source_path;
    MediaType media_type;
    std::optional<double> duration_seconds;
};

struct LivePhotoPair {
    std::int64_t image_asset_id;
    std::int64_t video_asset_id;
};

/** Apple Live Photo: HEIC+MOV (same stem), or JPEG+MOV when clip duration <= 4 s. */
[[nodiscard]] bool is_valid_live_photo_pair(const std::filesystem::path& image_path,
                                            const std::optional<double>& video_duration);

std::vector<LivePhotoPair> detect_live_photo_pairs(
    const std::vector<PairingCandidate>& candidates,
    const std::vector<LivePhotoPair>& existing_pairs);

}  // namespace geoframe::core
