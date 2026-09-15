#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace geoframe::core {

enum class JobType {
    Hash,
    Metadata,         ///< EXIF extraction for images (Exiv2)
    VideoMetadata,    ///< metadata extraction for videos (ffprobe)
    Thumbnail,        ///< thumbnail generation (images: ffmpeg scale; videos: poster frame)
    Preview,          ///< larger preview, images only
    Phash,
    SeriesDetect,
};

enum class JobStatus {
    Pending,
    Processing,
    Done,
    Failed,
};

/**
 * @brief Фоновая задача обработки одного медиафайла.
 */
struct Job {
    std::int64_t id;
    std::int64_t asset_id;
    JobType type;
    JobStatus status;
    int attempts;
    std::optional<std::string> error;
};

}  // namespace geoframe::core
