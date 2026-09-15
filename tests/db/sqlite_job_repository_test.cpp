#include "core/asset.hpp"
#include "core/job.hpp"
#include "db/database.hpp"
#include "db/migration_runner.hpp"
#include "db/sqlite_asset_repository.hpp"
#include "db/sqlite_job_repository.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace geoframe::db {

class SqliteJobRepositoryTest : public testing::Test {
protected:
    SqliteJobRepositoryTest() : database(":memory:"), assets(database), jobs(database) {
        MigrationRunner{database}.migrate();
        asset_id =
            assets
                .create(core::NewAsset{
                    .source_path = "/library/IMG_0001.HEIC",
                    .original_filename = "IMG_0001.HEIC",
                    .media_type = core::MediaType::Image,
                    .size_bytes = 1024,
                    .sha256 = std::nullopt,
                    .captured_at = std::nullopt,
                    .width = std::nullopt,
                    .height = std::nullopt,
                    .location = std::nullopt,
                    .camera = std::nullopt,
                })
                .id;
    }

    Database database;
    SqliteAssetRepository assets;
    SqliteJobRepository jobs;
    std::int64_t asset_id = 0;
};

TEST_F(SqliteJobRepositoryTest, ClaimsPendingJob) {
    jobs.enqueue(asset_id, core::JobType::Hash);

    const auto claimed = jobs.claim_next();

    ASSERT_TRUE(claimed.has_value());
    EXPECT_EQ(claimed->asset_id, asset_id);
    EXPECT_EQ(claimed->type, core::JobType::Hash);
    EXPECT_EQ(claimed->status, core::JobStatus::Processing);
    EXPECT_EQ(claimed->attempts, 1);
    EXPECT_FALSE(jobs.claim_next().has_value());
}

TEST_F(SqliteJobRepositoryTest, DuplicateEnqueueIsIgnored) {
    jobs.enqueue(asset_id, core::JobType::Hash);
    jobs.enqueue(asset_id, core::JobType::Hash);

    ASSERT_TRUE(jobs.claim_next().has_value());
    EXPECT_FALSE(jobs.claim_next().has_value());
}

TEST_F(SqliteJobRepositoryTest, MarksJobDone) {
    jobs.enqueue(asset_id, core::JobType::Hash);
    const auto claimed = jobs.claim_next();
    ASSERT_TRUE(claimed.has_value());

    EXPECT_NO_THROW(jobs.mark_done(claimed->id));
    EXPECT_FALSE(jobs.claim_next().has_value());
}

TEST_F(SqliteJobRepositoryTest, RecoversInterruptedJob) {
    jobs.enqueue(asset_id, core::JobType::Hash);
    ASSERT_TRUE(jobs.claim_next().has_value());

    EXPECT_EQ(jobs.recover_interrupted(), 1);

    const auto reclaimed = jobs.claim_next();
    ASSERT_TRUE(reclaimed.has_value());
    EXPECT_EQ(reclaimed->attempts, 2);
}

TEST_F(SqliteJobRepositoryTest, EnqueuesVideoMetadataJob) {
    jobs.enqueue(asset_id, core::JobType::VideoMetadata);

    const auto claimed = jobs.claim_next();
    ASSERT_TRUE(claimed.has_value());
    EXPECT_EQ(claimed->type, core::JobType::VideoMetadata);
}

TEST_F(SqliteJobRepositoryTest, RequeuesDoneJob) {
    jobs.enqueue(asset_id, core::JobType::Preview);
    const auto claimed = jobs.claim_next();
    ASSERT_TRUE(claimed.has_value());
    jobs.mark_done(claimed->id);

    jobs.requeue(asset_id, core::JobType::Preview);

    const auto reclaimed = jobs.claim_next();
    ASSERT_TRUE(reclaimed.has_value());
    EXPECT_EQ(reclaimed->type, core::JobType::Preview);
    EXPECT_EQ(reclaimed->attempts, 1);
}

TEST_F(SqliteJobRepositoryTest, RejectsUnknownJob) {
    EXPECT_THROW(jobs.mark_done(42), std::out_of_range);
}

}  // namespace geoframe::db
