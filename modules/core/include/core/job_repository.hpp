#pragma once

#include "core/job.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace geoframe::core {

/**
 * @brief Персистентная очередь фоновой обработки.
 */
class IJobRepository {
public:
    virtual ~IJobRepository() = default;

    virtual void enqueue(std::int64_t asset_id, JobType type) = 0;
    virtual std::optional<Job> find_by_id(std::int64_t id) = 0;
    virtual std::optional<Job> claim_next() = 0;
    virtual void mark_done(std::int64_t id) = 0;
    virtual void mark_failed(std::int64_t id, std::string_view error) = 0;
    virtual int recover_interrupted() = 0;
    /** Reset all Failed jobs back to Pending so they are retried. */
    virtual int retry_failed() = 0;
};

}  // namespace geoframe::core
