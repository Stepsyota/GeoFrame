#pragma once

#include "core/job.hpp"

namespace geoframe::worker {

class IJobHandler {
public:
    virtual ~IJobHandler() = default;

    /**
     * @brief Выполняет задачу. Реализация должна быть thread-safe.
     */
    virtual void execute(const core::Job& job) = 0;
};

}  // namespace geoframe::worker
