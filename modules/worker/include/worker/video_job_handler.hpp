#pragma once

#include "core/asset_repository.hpp"
#include "core/job.hpp"
#include "worker/job_handler.hpp"

#include <filesystem>

namespace geoframe::worker {

/**
 * @brief Обрабатывает VideoMetadata-задачи: запускает ffprobe, сохраняет в DB.
 *
 * Для Thumbnail-задач видео генерирует постер (первый кадр) через ffmpeg.
 */
class VideoJobHandler : public IJobHandler {
public:
    VideoJobHandler(core::IAssetRepository& assets, core::JobType type,
                    std::filesystem::path cache_directory, int max_px);

    [[nodiscard]] core::JobType job_type() const noexcept override;
    void execute(const core::Job& job) override;

private:
    core::IAssetRepository& assets;
    core::JobType type;
    std::filesystem::path cache_directory;
    int max_px;
};

}  // namespace geoframe::worker
