#include "worker/hash_job_handler.hpp"

#include "media/sha256.hpp"

#include <stdexcept>

namespace geoframe::worker {

HashJobHandler::HashJobHandler(core::IAssetRepository& assets) : assets(assets) {}

void HashJobHandler::execute(const core::Job& job) {
    if (job.type != core::JobType::Hash) {
        throw std::invalid_argument("HashJobHandler received unsupported job type");
    }

    const auto asset = assets.find_by_id(job.asset_id);
    if (!asset.has_value()) {
        throw std::out_of_range("Asset for hash job was not found");
    }

    assets.set_sha256(asset->id, media::sha256_file(asset->source_path));
}

}  // namespace geoframe::worker
