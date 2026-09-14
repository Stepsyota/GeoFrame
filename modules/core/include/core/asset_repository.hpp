#pragma once

#include "core/asset.hpp"

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
    virtual std::vector<Asset> list(std::size_t limit, std::size_t offset) = 0;
    virtual void set_sha256(std::int64_t id, std::string_view sha256) = 0;
    virtual void set_metadata(std::int64_t id, const AssetMetadata& metadata) = 0;
    virtual void set_favorite(std::int64_t id, bool favorite) = 0;
    virtual void set_status(std::int64_t id, AssetStatus status) = 0;
};

}  // namespace geoframe::core
