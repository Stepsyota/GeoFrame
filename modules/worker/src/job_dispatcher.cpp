#include "worker/job_dispatcher.hpp"

#include <stdexcept>

namespace geoframe::worker {

JobDispatcher::JobDispatcher(core::IAssetRepository& assets, core::IJobRepository& jobs,
                             std::initializer_list<IJobHandler*> handlers)
    : assets(assets), jobs(jobs), handlers(handlers) {
    for (const auto* handler : this->handlers) {
        if (handler == nullptr) {
            throw std::invalid_argument("Job dispatcher cannot contain null handler");
        }
    }
}

void JobDispatcher::execute(const core::Job& job) {
    auto* handler = find_handler(job.type);
    if (handler == nullptr) {
        throw std::invalid_argument("No handler registered for job type");
    }

    handler->execute(job);
    enqueue_next(job);
}

IJobHandler* JobDispatcher::find_handler(const core::JobType type) const {
    for (auto* handler : handlers) {
        if (handler->job_type() == type) {
            return handler;
        }
    }
    return nullptr;
}

void JobDispatcher::enqueue_next(const core::Job& completed_job) {
    if (completed_job.type != core::JobType::Hash) {
        return;
    }

    const auto asset = assets.find_by_id(completed_job.asset_id);
    if (!asset.has_value()) {
        throw std::out_of_range("Asset disappeared after job processing");
    }
    if (asset->media_type == core::MediaType::Image) {
        jobs.enqueue(asset->id, core::JobType::Metadata);
    }
}

}  // namespace geoframe::worker
