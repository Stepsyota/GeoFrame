#pragma once

#include "core/asset_repository.hpp"
#include "worker/job_handler.hpp"

namespace geoframe::worker {

/**
 * @brief Вычисляет SHA-256 оригинала.
 */
class HashJobHandler : public IJobHandler {
public:
    explicit HashJobHandler(core::IAssetRepository& assets);

    void execute(const core::Job& job) override;

private:
    core::IAssetRepository& assets;
};

}  // namespace geoframe::worker
