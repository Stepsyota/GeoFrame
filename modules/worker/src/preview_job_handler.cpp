#include "worker/preview_job_handler.hpp"

#include "media/preview_generator.hpp"

#include <stdexcept>
#include <utility>

namespace geoframe::worker {

PreviewJobHandler::PreviewJobHandler(core::IAssetRepository& assets, const core::JobType type,
                                     std::filesystem::path cache_directory, const int max_size)
    : assets(assets),
      type(type),
      cache_directory(std::move(cache_directory)),
      max_size(max_size) {
    if (type != core::JobType::Thumbnail && type != core::JobType::Preview) {
        throw std::invalid_argument("Preview handler requires thumbnail or preview job type");
    }
    if (!this->cache_directory.is_absolute()) {
        throw std::invalid_argument("Preview cache directory must be absolute");
    }
    if (max_size <= 0) {
        throw std::invalid_argument("Preview max size must be positive");
    }
}

core::JobType PreviewJobHandler::job_type() const noexcept {
    return type;
}

void PreviewJobHandler::execute(const core::Job& job) {
    if (job.type != type) {
        throw std::invalid_argument("PreviewJobHandler received unsupported job type");
    }

    const auto asset = assets.find_by_id(job.asset_id);
    if (!asset.has_value()) {
        throw std::out_of_range("Asset for preview job was not found");
    }
    if (asset->media_type != core::MediaType::Image) {
        throw std::invalid_argument("Image preview handler received non-image asset");
    }

    const auto destination = cache_directory / (std::to_string(asset->id) + ".jpg");
    media::generate_image_preview(asset->source_path, destination, max_size);

    if (type == core::JobType::Thumbnail) {
        assets.set_thumbnail_path(asset->id, destination);
    } else {
        assets.set_preview_path(asset->id, destination);
    }
}

}  // namespace geoframe::worker
