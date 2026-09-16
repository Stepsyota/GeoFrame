#include "db/sqlite_job_repository.hpp"

#include <stdexcept>
#include <string>

namespace geoframe::db {

namespace {

std::string_view to_string(const core::JobType type) {
    switch (type) {
        case core::JobType::Hash:
            return "hash";
        case core::JobType::Metadata:
            return "metadata";
        case core::JobType::VideoMetadata:
            return "video_metadata";
        case core::JobType::Thumbnail:
            return "thumbnail";
        case core::JobType::Preview:
            return "preview";
        case core::JobType::Phash:
            return "phash";
        case core::JobType::SeriesDetect:
            return "series_detect";
    }
    throw std::invalid_argument("Unknown job type");
}

core::JobType type_from_string(const std::string_view value) {
    if (value == "hash") {
        return core::JobType::Hash;
    }
    if (value == "metadata") {
        return core::JobType::Metadata;
    }
    if (value == "video_metadata") {
        return core::JobType::VideoMetadata;
    }
    if (value == "thumbnail") {
        return core::JobType::Thumbnail;
    }
    if (value == "preview") {
        return core::JobType::Preview;
    }
    if (value == "phash") {
        return core::JobType::Phash;
    }
    if (value == "series_detect") {
        return core::JobType::SeriesDetect;
    }
    throw std::runtime_error("Unknown job type in database: " + std::string{value});
}

core::JobStatus status_from_string(const std::string_view value) {
    if (value == "pending") {
        return core::JobStatus::Pending;
    }
    if (value == "processing") {
        return core::JobStatus::Processing;
    }
    if (value == "done") {
        return core::JobStatus::Done;
    }
    if (value == "failed") {
        return core::JobStatus::Failed;
    }
    throw std::runtime_error("Unknown job status in database: " + std::string{value});
}

core::Job read_job(const Statement& statement) {
    core::Job job{
        .id = statement.column_int64(0),
        .asset_id = statement.column_int64(1),
        .type = type_from_string(statement.column_text(2)),
        .status = status_from_string(statement.column_text(3)),
        .attempts = static_cast<int>(statement.column_int64(4)),
        .error = std::nullopt,
    };
    if (!statement.column_is_null(5)) {
        job.error = statement.column_text(5);
    }
    return job;
}

}  // namespace

SqliteJobRepository::SqliteJobRepository(Database& database) : database(database) {}

void SqliteJobRepository::enqueue(const std::int64_t asset_id, const core::JobType type) {
    auto statement =
        database.prepare("INSERT OR IGNORE INTO jobs (asset_id, type) VALUES (?, ?)");
    statement.bind(1, asset_id);
    statement.bind(2, to_string(type));
    statement.step();
}

std::optional<core::Job> SqliteJobRepository::find_by_id(const std::int64_t id) {
    auto statement = database.prepare(
        "SELECT id, asset_id, type, status, attempts, error FROM jobs WHERE id = ?");
    statement.bind(1, id);
    if (!statement.step()) {
        return std::nullopt;
    }
    return read_job(statement);
}

std::optional<core::Job> SqliteJobRepository::claim_next() {
    auto statement = database.prepare(
        "UPDATE jobs "
        "SET status = 'processing', attempts = attempts + 1, "
        "started_at = CURRENT_TIMESTAMP, error = NULL "
        "WHERE id = (SELECT id FROM jobs WHERE status = 'pending' ORDER BY id LIMIT 1) "
        "RETURNING id, asset_id, type, status, attempts, error");

    if (!statement.step()) {
        return std::nullopt;
    }
    return read_job(statement);
}

void SqliteJobRepository::mark_done(const std::int64_t id) {
    auto statement = database.prepare(
        "UPDATE jobs SET status = 'done', completed_at = CURRENT_TIMESTAMP, error = NULL "
        "WHERE id = ? AND status = 'processing' RETURNING id");
    statement.bind(1, id);
    if (!statement.step()) {
        throw std::out_of_range("Job not found or has unexpected status");
    }
}

void SqliteJobRepository::mark_failed(const std::int64_t id, const std::string_view error) {
    auto statement = database.prepare(
        "UPDATE jobs SET status = 'failed', completed_at = CURRENT_TIMESTAMP, error = ? "
        "WHERE id = ? AND status = 'processing' RETURNING id");
    statement.bind(1, error);
    statement.bind(2, id);
    if (!statement.step()) {
        throw std::out_of_range("Job not found or has unexpected status");
    }
}

int SqliteJobRepository::recover_interrupted() {
    database.execute(
        "UPDATE jobs SET status = 'pending', started_at = NULL "
        "WHERE status = 'processing'");
    return database.changes();
}

int SqliteJobRepository::retry_failed() {
    database.execute(
        "UPDATE jobs SET status = 'pending', started_at = NULL, error = NULL "
        "WHERE status = 'failed'");
    return database.changes();
}

core::JobStats SqliteJobRepository::stats() {
    auto statement = database.prepare(
        "SELECT type, status, COUNT(*) FROM jobs GROUP BY type, status");

    core::JobStats result;
    while (statement.step()) {
        const auto type = type_from_string(statement.column_text(0));
        const auto status = status_from_string(statement.column_text(1));
        const auto count = static_cast<std::size_t>(statement.column_int64(2));

        core::JobTypeStats* bucket = nullptr;
        switch (type) {
        case core::JobType::Hash:
            bucket = &result.hash;
            break;
        case core::JobType::Metadata:
            bucket = &result.metadata;
            break;
        case core::JobType::VideoMetadata:
            bucket = &result.video_metadata;
            break;
        case core::JobType::Thumbnail:
            bucket = &result.thumbnail;
            break;
        case core::JobType::Preview:
            bucket = &result.preview;
            break;
        default:
            continue;
        }

        switch (status) {
        case core::JobStatus::Pending:
            bucket->pending += count;
            break;
        case core::JobStatus::Processing:
            bucket->processing += count;
            break;
        case core::JobStatus::Done:
            bucket->done += count;
            break;
        case core::JobStatus::Failed:
            bucket->failed += count;
            break;
        }
    }
    return result;
}

void SqliteJobRepository::requeue(const std::int64_t asset_id, const core::JobType type) {
    auto statement = database.prepare(
        "UPDATE jobs SET status = 'pending', started_at = NULL, completed_at = NULL, "
        "error = NULL, attempts = 0 "
        "WHERE asset_id = ? AND type = ?");
    statement.bind(1, asset_id);
    statement.bind(2, to_string(type));
    statement.step();
    if (database.changes() == 0) {
        enqueue(asset_id, type);
    }
}

}  // namespace geoframe::db
