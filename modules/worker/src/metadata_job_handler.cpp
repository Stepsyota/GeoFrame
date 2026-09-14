#include "worker/metadata_job_handler.hpp"

#include "media/metadata_extractor.hpp"

#include <stdexcept>

namespace geoframe::worker {

MetadataJobHandler::MetadataJobHandler(core::IAssetRepository& assets) : assets(assets) {}

core::JobType MetadataJobHandler::job_type() const noexcept {
    return core::JobType::Metadata;
}

void MetadataJobHandler::execute(const core::Job& job) {
    if (job.type != job_type()) {
        throw std::invalid_argument("MetadataJobHandler received unsupported job type");
    }

    const auto asset = assets.find_by_id(job.asset_id);
    if (!asset.has_value()) {
        throw std::out_of_range("Asset for metadata job was not found");
    }
    if (asset->media_type != core::MediaType::Image) {
        throw std::invalid_argument("Image metadata handler received non-image asset");
    }

    assets.set_metadata(asset->id, media::extract_image_metadata(asset->source_path));
}

}  // namespace geoframe::worker
