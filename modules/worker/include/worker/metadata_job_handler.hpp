#pragma once

#include "core/asset_repository.hpp"
#include "worker/job_handler.hpp"

namespace geoframe::worker {

/**
 * @brief Извлекает и сохраняет нормализованные EXIF-метаданные изображения.
 */
class MetadataJobHandler : public IJobHandler {
public:
    explicit MetadataJobHandler(core::IAssetRepository& assets);

    [[nodiscard]] core::JobType job_type() const noexcept override;
    void execute(const core::Job& job) override;

private:
    core::IAssetRepository& assets;
};

}  // namespace geoframe::worker
