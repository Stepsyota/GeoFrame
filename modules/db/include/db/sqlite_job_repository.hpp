#pragma once

#include "core/job_repository.hpp"
#include "db/database.hpp"

namespace geoframe::db {

/**
 * @brief SQLite-реализация персистентной очереди обработки.
 */
class SqliteJobRepository : public core::IJobRepository {
public:
    explicit SqliteJobRepository(Database& database);

    void enqueue(std::int64_t asset_id, core::JobType type) override;
    std::optional<core::Job> find_by_id(std::int64_t id) override;
    std::optional<core::Job> claim_next() override;
    void mark_done(std::int64_t id) override;
    void mark_failed(std::int64_t id, std::string_view error) override;
    int recover_interrupted() override;

private:
    Database& database;
};

}  // namespace geoframe::db
