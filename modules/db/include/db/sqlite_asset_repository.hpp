#pragma once

#include "core/asset_repository.hpp"
#include "db/database.hpp"

namespace geoframe::db {

/**
 * @brief SQLite-реализация хранилища медиафайлов.
 */
class SqliteAssetRepository : public core::IAssetRepository {
public:
    explicit SqliteAssetRepository(Database& database);

    core::Asset create(const core::NewAsset& asset) override;
    std::optional<core::Asset> find_by_id(std::int64_t id) override;
    std::optional<core::Asset> find_by_source_path(
        const std::filesystem::path& source_path) override;
    std::vector<core::Asset> list(std::size_t limit, std::size_t offset,
                                  std::optional<core::AssetStatus> status = std::nullopt) override;
    std::int64_t count(core::AssetStatus status = core::AssetStatus::Active) override;
    void set_sha256(std::int64_t id, std::string_view sha256) override;
    void set_metadata(std::int64_t id, const core::AssetMetadata& metadata) override;
    void set_video_metadata(std::int64_t id, const core::VideoMetadata& metadata) override;
    void set_thumbnail_path(std::int64_t id, const std::filesystem::path& path) override;
    void set_preview_path(std::int64_t id, const std::filesystem::path& path) override;
    void set_favorite(std::int64_t id, bool favorite) override;
    void set_status(std::int64_t id, core::AssetStatus status) override;
    std::vector<core::GeoAsset> list_geo_points(
        core::AssetStatus status = core::AssetStatus::Active) override;

private:
    Database& database;
};

}  // namespace geoframe::db
