#include "core/live_photo_pairer.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace geoframe::core {

namespace {

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(),
                           [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string pairing_key(const std::filesystem::path& source_path) {
    const auto parent = source_path.parent_path().lexically_normal().string();
    const auto stem = lowercase(source_path.stem().string());
    return parent + "|" + stem;
}

int image_preference(const std::filesystem::path& path) {
    const auto extension = lowercase(path.extension().string());
    if (extension == ".heic") {
        return 0;
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return 1;
    }
    return 99;
}

bool is_paired(const std::int64_t id, const std::vector<LivePhotoPair>& existing_pairs) {
    return std::ranges::any_of(existing_pairs, [&](const LivePhotoPair& pair) {
        return pair.image_asset_id == id || pair.video_asset_id == id;
    });
}

}  // namespace

bool is_valid_live_photo_pair(const std::filesystem::path& image_path,
                              const std::optional<double>& video_duration) {
    const auto extension = lowercase(image_path.extension().string());

    if (extension == ".heic") {
        // iPhone Live Photo: HEIC still + MOV with the same stem.
        return !video_duration.has_value()
               || *video_duration <= kMaxLivePhotoDurationSeconds;
    }

    if (extension == ".jpg" || extension == ".jpeg") {
        // JPEG + MOV is ambiguous — only pair short companion clips.
        return video_duration.has_value()
               && *video_duration <= kMaxLivePhotoDurationSeconds;
    }

    return false;
}

std::vector<LivePhotoPair> detect_live_photo_pairs(
    const std::vector<PairingCandidate>& candidates,
    const std::vector<LivePhotoPair>& existing_pairs) {
    struct BucketEntry {
        std::int64_t image_id = 0;
        int image_rank = 99;
        std::filesystem::path image_path;
        std::int64_t video_id = 0;
        std::optional<double> video_duration;
    };

    std::unordered_map<std::string, BucketEntry> buckets;
    for (const auto& candidate : candidates) {
        if (is_paired(candidate.id, existing_pairs)) {
            continue;
        }

        auto& bucket = buckets[pairing_key(candidate.source_path)];
        if (candidate.media_type == MediaType::Image) {
            const auto rank = image_preference(candidate.source_path);
            if (bucket.image_id == 0 || rank < bucket.image_rank) {
                bucket.image_id = candidate.id;
                bucket.image_rank = rank;
                bucket.image_path = candidate.source_path;
            }
        } else if (candidate.media_type == MediaType::Video && bucket.video_id == 0) {
            bucket.video_id = candidate.id;
            bucket.video_duration = candidate.duration_seconds;
        }
    }

    std::vector<LivePhotoPair> pairs;
    for (const auto& [_, bucket] : buckets) {
        if (bucket.image_id > 0 && bucket.video_id > 0
            && is_valid_live_photo_pair(bucket.image_path, bucket.video_duration)) {
            pairs.push_back(LivePhotoPair{
                .image_asset_id = bucket.image_id,
                .video_asset_id = bucket.video_id,
            });
        }
    }

    std::sort(pairs.begin(), pairs.end(),
              [](const LivePhotoPair& left, const LivePhotoPair& right) {
                  return left.image_asset_id < right.image_asset_id;
              });
    return pairs;
}

}  // namespace geoframe::core
