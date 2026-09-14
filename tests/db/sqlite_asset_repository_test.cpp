#include "core/asset.hpp"
#include "db/database.hpp"
#include "db/migration_runner.hpp"
#include "db/sqlite_asset_repository.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace geoframe::db {

class SqliteAssetRepositoryTest : public testing::Test {
protected:
    SqliteAssetRepositoryTest() : database(":memory:"), repository(database) {
        MigrationRunner{database}.migrate();
    }

    static core::NewAsset image(std::string path = "/library/IMG_0001.HEIC") {
        return core::NewAsset{
            .source_path = std::move(path),
            .original_filename = "IMG_0001.HEIC",
            .media_type = core::MediaType::Image,
            .size_bytes = 4'096,
            .sha256 = std::string(64, 'a'),
            .captured_at = "2026-09-14T20:30:00",
            .width = 4'032,
            .height = 3'024,
            .location =
                core::GeoPoint{
                    .latitude = 53.9,
                    .longitude = 27.5667,
                    .altitude = 220.5,
                },
            .camera = "iPhone",
        };
    }

    Database database;
    SqliteAssetRepository repository;
};

TEST_F(SqliteAssetRepositoryTest, MigrationIsIdempotent) {
    MigrationRunner{database}.migrate();

    EXPECT_EQ(database.user_version(), 4);
}

TEST_F(SqliteAssetRepositoryTest, CreatesAndFindsAsset) {
    const auto created = repository.create(image());
    const auto found = repository.find_by_id(created.id);

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->source_path, "/library/IMG_0001.HEIC");
    EXPECT_EQ(found->original_filename, "IMG_0001.HEIC");
    EXPECT_EQ(found->media_type, core::MediaType::Image);
    EXPECT_EQ(found->size_bytes, 4'096);
    EXPECT_EQ(found->sha256, std::string(64, 'a'));
    EXPECT_EQ(found->captured_at, "2026-09-14T20:30:00");
    ASSERT_TRUE(found->location.has_value());
    EXPECT_DOUBLE_EQ(found->location->latitude, 53.9);
    EXPECT_DOUBLE_EQ(found->location->longitude, 27.5667);
    EXPECT_EQ(found->camera, "iPhone");
    EXPECT_FALSE(found->favorite);
    EXPECT_EQ(found->status, core::AssetStatus::Active);
}

TEST_F(SqliteAssetRepositoryTest, FindsAssetBySourcePath) {
    repository.create(image());

    const auto found = repository.find_by_source_path("/library/IMG_0001.HEIC");

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->original_filename, "IMG_0001.HEIC");
}

TEST_F(SqliteAssetRepositoryTest, ListsAssetsWithPagination) {
    repository.create(image("/library/IMG_0001.HEIC"));
    auto second = image("/library/IMG_0002.HEIC");
    second.sha256 = std::string(64, 'b');
    repository.create(second);

    const auto assets = repository.list(1, 1);

    ASSERT_EQ(assets.size(), 1);
    EXPECT_EQ(assets.front().source_path, "/library/IMG_0002.HEIC");
}

TEST_F(SqliteAssetRepositoryTest, UpdatesFavoriteAndStatus) {
    const auto created = repository.create(image());

    repository.set_favorite(created.id, true);
    repository.set_status(created.id, core::AssetStatus::Trashed);

    const auto updated = repository.find_by_id(created.id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_TRUE(updated->favorite);
    EXPECT_EQ(updated->status, core::AssetStatus::Trashed);
}

TEST_F(SqliteAssetRepositoryTest, UpdatesNormalizedMetadata) {
    const auto created = repository.create(image());

    repository.set_metadata(
        created.id,
        core::AssetMetadata{
            .captured_at = "2026-09-15T10:00:00",
            .width = 1920,
            .height = 1080,
            .location =
                core::GeoPoint{
                    .latitude = 50.0,
                    .longitude = 20.0,
                    .altitude = std::nullopt,
                },
            .camera = "Test camera",
        });

    const auto updated = repository.find_by_id(created.id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->captured_at, "2026-09-15T10:00:00");
    EXPECT_EQ(updated->width, 1920);
    EXPECT_EQ(updated->height, 1080);
    EXPECT_EQ(updated->camera, "Test camera");
}

TEST_F(SqliteAssetRepositoryTest, UpdatesGeneratedImagePaths) {
    const auto created = repository.create(image());

    repository.set_thumbnail_path(created.id, "/cache/thumbnails/1.jpg");
    repository.set_preview_path(created.id, "/cache/previews/1.jpg");

    const auto updated = repository.find_by_id(created.id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->thumbnail_path, "/cache/thumbnails/1.jpg");
    EXPECT_EQ(updated->preview_path, "/cache/previews/1.jpg");
}

TEST_F(SqliteAssetRepositoryTest, RejectsRelativeSourcePath) {
    EXPECT_THROW(repository.create(image("IMG_0001.HEIC")), std::invalid_argument);
}

TEST_F(SqliteAssetRepositoryTest, RejectsDuplicateSourcePath) {
    repository.create(image());

    auto duplicate = image();
    duplicate.sha256 = std::string(64, 'b');
    EXPECT_THROW(repository.create(duplicate), std::runtime_error);
}

TEST_F(SqliteAssetRepositoryTest, AllowsDuplicateHash) {
    repository.create(image());
    auto duplicate = image("/library/IMG_0002.HEIC");

    EXPECT_NO_THROW(repository.create(duplicate));
}

}  // namespace geoframe::db
