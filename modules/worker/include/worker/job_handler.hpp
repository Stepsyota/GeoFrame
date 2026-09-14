#pragma once

#include "core/job.hpp"

namespace geoframe::worker {

class IJobExecutor {
public:
    virtual ~IJobExecutor() = default;

    /**
     * @brief Выполняет задачу. Реализация должна быть thread-safe.
     */
    virtual void execute(const core::Job& job) = 0;
};

class IJobHandler : public IJobExecutor {
public:
    [[nodiscard]] virtual core::JobType job_type() const noexcept = 0;
};

}  // namespace geoframe::worker
