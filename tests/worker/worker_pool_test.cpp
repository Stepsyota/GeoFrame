#include "core/asset.hpp"
#include "core/job.hpp"
#include "db/database.hpp"
#include "db/migration_runner.hpp"
#include "db/sqlite_asset_repository.hpp"
#include "db/sqlite_job_repository.hpp"
#include "worker/hash_job_handler.hpp"
#include "worker/worker_pool.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>

namespace geoframe::worker {

class WorkerPoolTest : public testing::Test {
protected:
    WorkerPoolTest() : database(":memory:"), assets(database), jobs(database), handler(assets) {
        db::MigrationRunner{database}.migrate();
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        source = std::filesystem::temp_directory_path() / ("geoframe-pool-" + std::to_string(suffix));
    }

    ~WorkerPoolTest() override {
        std::error_code error;
        std::filesystem::remove(source, error);
    }

    std::int64_t enqueue_hash_job(const bool create_file) {
        if (create_file) {
            std::ofstream{source, std::ios::binary} << "test";
        }

        const auto asset =
            assets.create(core::NewAsset{
                .source_path = source,
                .original_filename = source.filename().string(),
                .media_type = core::MediaType::Image,
                .size_bytes = 4,
                .sha256 = std::nullopt,
                .captured_at = std::nullopt,
                .width = std::nullopt,
                .height = std::nullopt,
                .location = std::nullopt,
                .camera = std::nullopt,
            });
        jobs.enqueue(asset.id, core::JobType::Hash);

        const auto claimed = jobs.claim_next();
        if (!claimed.has_value()) {
            throw std::runtime_error("Test job was not queued");
        }
        return claimed->id;
    }

    core::Job wait_for_status(const std::int64_t id, const core::JobStatus expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
        while (std::chrono::steady_clock::now() < deadline) {
            const auto job = jobs.find_by_id(id);
            if (job.has_value() && job->status == expected) {
                return *job;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        throw std::runtime_error("Timed out waiting for worker");
    }

    db::Database database;
    db::SqliteAssetRepository assets;
    db::SqliteJobRepository jobs;
    HashJobHandler handler;
    std::filesystem::path source;
};

TEST_F(WorkerPoolTest, CalculatesHashAndCompletesJob) {
    const auto job_id = enqueue_hash_job(true);
    WorkerPool pool{jobs, handler, 1};

    pool.start();
    const auto job = wait_for_status(job_id, core::JobStatus::Done);
    pool.stop();

    pool.rethrow_if_failed();
    EXPECT_EQ(job.attempts, 2);
    const auto asset = assets.find_by_source_path(source);
    ASSERT_TRUE(asset.has_value());
    EXPECT_EQ(asset->sha256,
              "9f86d081884c7d659a2feaa0c55ad015"
              "a3bf4f1b2b0b822cd15d6c15b0f00a08");
}

TEST_F(WorkerPoolTest, MarksJobFailedWhenFileIsMissing) {
    const auto job_id = enqueue_hash_job(false);
    WorkerPool pool{jobs, handler, 1};

    pool.start();
    const auto job = wait_for_status(job_id, core::JobStatus::Failed);
    pool.stop();

    pool.rethrow_if_failed();
    ASSERT_TRUE(job.error.has_value());
    EXPECT_NE(job.error->find("Cannot open file"), std::string::npos);
}

TEST_F(WorkerPoolTest, RejectsEmptyPool) {
    EXPECT_THROW(WorkerPool(jobs, handler, 0), std::invalid_argument);
}

}  // namespace geoframe::worker
