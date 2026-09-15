#include "worker/video_job_handler.hpp"

#include "media/video_extractor.hpp"

#include <stdexcept>
#include <utility>

namespace geoframe::worker {

VideoJobHandler::VideoJobHandler(core::IAssetRepository& assets, const core::JobType type,
                                 std::filesystem::path cache_directory, const int max_px)
    : assets(assets),
      type(type),
      cache_directory(std::move(cache_directory)),
      max_px(max_px) {
    if (type != core::JobType::VideoMetadata && type != core::JobType::Thumbnail) {
        throw std::invalid_argument(
            "VideoJobHandler requires VideoMetadata or Thumbnail job type");
    }
    if (type == core::JobType::Thumbnail && !this->cache_directory.is_absolute()) {
        throw std::invalid_argument("Video thumbnail cache directory must be absolute");
    }
    if (type == core::JobType::Thumbnail && max_px <= 0) {
        throw std::invalid_argument("Video thumbnail max size must be positive");
    }
}

core::JobType VideoJobHandler::job_type() const noexcept {
    return type;
}

void VideoJobHandler::execute(const core::Job& job) {
    if (job.type != type) {
        throw std::invalid_argument("VideoJobHandler received unsupported job type");
    }

    const auto asset = assets.find_by_id(job.asset_id);
    if (!asset.has_value()) {
        throw std::out_of_range("Asset for video job was not found");
    }
    if (asset->media_type != core::MediaType::Video) {
        throw std::invalid_argument("VideoJobHandler received non-video asset");
    }

    if (type == core::JobType::VideoMetadata) {
        const auto metadata = media::extract_video_metadata(asset->source_path);
        assets.set_video_metadata(asset->id, metadata);
        return;
    }

    // Thumbnail job: generate poster frame
    const auto dest = cache_directory / (std::to_string(asset->id) + ".jpg");
    media::generate_video_poster(asset->source_path, dest, max_px);
    assets.set_thumbnail_path(asset->id, dest);
}

}  // namespace geoframe::worker
