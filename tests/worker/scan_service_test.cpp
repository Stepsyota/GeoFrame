#include "db/database.hpp"
#include "db/migration_runner.hpp"
#include "db/sqlite_asset_repository.hpp"
#include "db/sqlite_job_repository.hpp"
#include "storage/directory_scanner.hpp"
#include "worker/scan_service.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace geoframe::worker {

class ScanServiceTest : public testing::Test {
protected:
    ScanServiceTest()
        : source(create_temp_directory()),
          database(":memory:"),
          assets(database),
          jobs(database),
          service(scanner, assets, jobs) {
        db::MigrationRunner{database}.migrate();
    }

    ~ScanServiceTest() override {
        std::error_code error;
        std::filesystem::remove_all(source, error);
    }

    static std::filesystem::path create_temp_directory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        auto path =
            std::filesystem::temp_directory_path() / ("geoframe-worker-" + std::to_string(suffix));
        std::filesystem::create_directories(path);
        return path;
    }

    void create_file(const std::filesystem::path& relative_path) {
        const auto path = source / relative_path;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream{path} << "test";
    }

    std::filesystem::path source;
    db::Database database;
    db::SqliteAssetRepository assets;
    db::SqliteJobRepository jobs;
    storage::DirectoryScanner scanner;
    ScanService service;
};

TEST_F(ScanServiceTest, IndexesSupportedFilesAndQueuesHashJobs) {
    create_file("IMG_0001.HEIC");
    create_file("nested/IMG_0002.MOV");
    create_file("notes.txt");

    const auto report = service.scan(source);

    EXPECT_EQ(report.files_seen, 3);
    EXPECT_EQ(report.assets_created, 2);
    EXPECT_EQ(report.already_indexed, 0);
    EXPECT_EQ(report.unsupported, 1);
    EXPECT_EQ(report.filesystem_errors, 0);
    EXPECT_EQ(assets.list(10, 0).size(), 2);

    const auto first_job = jobs.claim_next();
    const auto second_job = jobs.claim_next();
    ASSERT_TRUE(first_job.has_value());
    ASSERT_TRUE(second_job.has_value());
    EXPECT_EQ(first_job->type, core::JobType::Hash);
    EXPECT_EQ(second_job->type, core::JobType::Hash);
    EXPECT_FALSE(jobs.claim_next().has_value());
}

TEST_F(ScanServiceTest, DoesNotIndexKnownFileTwice) {
    create_file("IMG_0001.HEIC");
    service.scan(source);

    const auto report = service.scan(source);

    EXPECT_EQ(report.assets_created, 0);
    EXPECT_EQ(report.already_indexed, 1);
    EXPECT_EQ(assets.list(10, 0).size(), 1);
}

TEST_F(ScanServiceTest, PairsHeicLivePhotosAfterScan) {
    create_file("IMG_0001.HEIC");
    create_file("IMG_0001.MOV");

    service.scan(source);

    EXPECT_EQ(assets.list(10, 0).size(), 1);
    const auto pairs = assets.list_live_photo_pairs();
    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].image_asset_id, assets.list(10, 0)[0].id);
}

}  // namespace geoframe::worker
