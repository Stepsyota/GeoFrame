#pragma once

#include "core/asset_repository.hpp"
#include "worker/job_handler.hpp"

#include <filesystem>

namespace geoframe::worker {

/**
 * @brief Генерирует thumbnail или preview изображения в кэше GeoFrame.
 */
class PreviewJobHandler : public IJobHandler {
public:
    PreviewJobHandler(core::IAssetRepository& assets, core::JobType type,
                      std::filesystem::path cache_directory, int max_size);

    [[nodiscard]] core::JobType job_type() const noexcept override;
    void execute(const core::Job& job) override;

private:
    core::IAssetRepository& assets;
    core::JobType type;
    std::filesystem::path cache_directory;
    int max_size;
};

}  // namespace geoframe::worker
