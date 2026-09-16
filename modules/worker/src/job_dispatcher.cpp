#include "worker/job_dispatcher.hpp"

#include <optional>
#include <stdexcept>

namespace geoframe::worker {

JobDispatcher::JobDispatcher(core::IAssetRepository& assets, core::IJobRepository& jobs,
                             std::initializer_list<IJobHandler*> handlers,
                             core::ProgressTracker* progress_tracker)
    : assets(assets), jobs(jobs), progress(progress_tracker), handlers(handlers) {
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

    if (progress != nullptr) {
        const auto asset = assets.find_by_id(job.asset_id);
        if (asset.has_value()) {
            progress->set_current_file(asset->original_filename);
        }
    }

    handler->execute(job);
    enqueue_next(job);

    if (progress != nullptr) {
        progress->clear_current_file();
    }
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
    std::optional<core::JobType> next_type;

    switch (completed_job.type) {
    case core::JobType::Hash: {
        const auto asset = assets.find_by_id(completed_job.asset_id);
        if (!asset.has_value()) {
            throw std::out_of_range("Asset disappeared after job processing");
        }
        if (asset->media_type == core::MediaType::Image) {
            next_type = core::JobType::Metadata;
        } else if (asset->media_type == core::MediaType::Video) {
            next_type = core::JobType::VideoMetadata;
        }
        break;
    }
    case core::JobType::Metadata:
        // Image: Metadata → Thumbnail
        next_type = core::JobType::Thumbnail;
        break;
    case core::JobType::VideoMetadata:
        // Video: VideoMetadata → Thumbnail (poster frame)
        next_type = core::JobType::Thumbnail;
        break;
    case core::JobType::Thumbnail:
        // Images only: Thumbnail → Preview; videos skip Preview
        {
            const auto asset = assets.find_by_id(completed_job.asset_id);
            if (asset.has_value() && asset->media_type == core::MediaType::Image) {
                next_type = core::JobType::Preview;
            }
        }
        break;
    default:
        return;
    }

    if (next_type.has_value() && find_handler(*next_type) != nullptr) {
        jobs.enqueue(completed_job.asset_id, *next_type);
    }
}

}  // namespace geoframe::worker
