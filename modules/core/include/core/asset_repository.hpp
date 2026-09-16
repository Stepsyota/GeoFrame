#pragma once

#include "core/asset.hpp"
#include "core/duplicate_grouper.hpp"
#include "core/geo_asset.hpp"
#include "core/series_detector.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace geoframe::core {

/**
 * @brief Хранилище проиндексированных медиафайлов.
 */
class IAssetRepository {
public:
    virtual ~IAssetRepository() = default;

    virtual Asset create(const NewAsset& asset) = 0;
    virtual std::optional<Asset> find_by_id(std::int64_t id) = 0;
    virtual std::optional<Asset> find_by_source_path(
        const std::filesystem::path& source_path) = 0;
    virtual std::vector<Asset> list(std::size_t limit, std::size_t offset,
                                    std::optional<AssetStatus> status = std::nullopt,
                                    std::optional<bool> favorite = std::nullopt) = 0;
    virtual std::int64_t count(AssetStatus status = AssetStatus::Active,
                               std::optional<bool> favorite = std::nullopt) = 0;
    virtual void set_sha256(std::int64_t id, std::string_view sha256) = 0;
    virtual void set_metadata(std::int64_t id, const AssetMetadata& metadata) = 0;
    virtual void set_video_metadata(std::int64_t id, const VideoMetadata& metadata) = 0;
    virtual void set_thumbnail_path(std::int64_t id, const std::filesystem::path& path) = 0;
    virtual void set_preview_path(std::int64_t id, const std::filesystem::path& path) = 0;
    virtual void set_favorite(std::int64_t id, bool favorite) = 0;
    virtual void set_status(std::int64_t id, AssetStatus status) = 0;
    virtual std::vector<GeoAsset> list_geo_points(AssetStatus status = AssetStatus::Active) = 0;
    virtual std::vector<HashedAsset> list_hashed_assets(
        AssetStatus status = AssetStatus::Active) = 0;
    virtual std::vector<TimedAsset> list_timed_assets(
        AssetStatus status = AssetStatus::Active) = 0;
    virtual void erase(std::int64_t id) = 0;
};

}  // namespace geoframe::core
