#pragma once

#include "core/asset_repository.hpp"
#include "core/job_repository.hpp"
#include "core/progress_tracker.hpp"
#include "worker/job_handler.hpp"

#include <initializer_list>
#include <vector>

namespace geoframe::worker {

/**
 * @brief Направляет задачу обработчику и ставит следующий этап pipeline.
 */
class JobDispatcher : public IJobExecutor {
public:
    JobDispatcher(core::IAssetRepository& assets, core::IJobRepository& jobs,
                  std::initializer_list<IJobHandler*> handlers,
                  core::ProgressTracker* progress = nullptr);

    void execute(const core::Job& job) override;

private:
    IJobHandler* find_handler(core::JobType type) const;
    void enqueue_next(const core::Job& completed_job);

    core::IAssetRepository& assets;
    core::IJobRepository& jobs;
    core::ProgressTracker* progress;
    std::vector<IJobHandler*> handlers;
};

}  // namespace geoframe::worker
