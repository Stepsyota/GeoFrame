#include "db/sqlite_asset_repository.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace geoframe::db {

namespace {

constexpr std::string_view kAssetColumns =
    "id, source_path, original_filename, media_type, status, size_bytes, sha256, "
    "captured_at, width, height, gps_lat, gps_lon, altitude, camera, favorite, "
    "thumbnail_path, preview_path, duration_seconds, video_codec";

std::string_view to_string(const core::MediaType type) {
    switch (type) {
        case core::MediaType::Image:
            return "image";
        case core::MediaType::Video:
            return "video";
    }
    throw std::invalid_argument("Unknown media type");
}

std::string_view to_string(const core::AssetStatus status) {
    switch (status) {
        case core::AssetStatus::Active:
            return "active";
        case core::AssetStatus::Trashed:
            return "trashed";
    }
    throw std::invalid_argument("Unknown asset status");
}

core::MediaType media_type_from_string(const std::string_view value) {
    if (value == "image") {
        return core::MediaType::Image;
    }
    if (value == "video") {
        return core::MediaType::Video;
    }
    throw std::runtime_error("Unknown media type in database: " + std::string{value});
}

core::AssetStatus status_from_string(const std::string_view value) {
    if (value == "active") {
        return core::AssetStatus::Active;
    }
    if (value == "trashed") {
        return core::AssetStatus::Trashed;
    }
    throw std::runtime_error("Unknown asset status in database: " + std::string{value});
}

void bind_optional(Statement& statement, const int index,
                   const std::optional<std::string>& value) {
    if (value.has_value()) {
        statement.bind(index, *value);
    } else {
        statement.bind_null(index);
    }
}

void bind_optional(Statement& statement, const int index, const std::optional<int>& value) {
    if (value.has_value()) {
        statement.bind(index, static_cast<std::int64_t>(*value));
    } else {
        statement.bind_null(index);
    }
}

std::optional<std::string> optional_text(const Statement& statement, const int index) {
    if (statement.column_is_null(index)) {
        return std::nullopt;
    }
    return statement.column_text(index);
}

std::optional<int> optional_int(const Statement& statement, const int index) {
    if (statement.column_is_null(index)) {
        return std::nullopt;
    }
    return static_cast<int>(statement.column_int64(index));
}

core::Asset read_asset(const Statement& statement) {
    core::Asset asset;
    asset.id = statement.column_int64(0);
    asset.source_path = statement.column_text(1);
    asset.original_filename = statement.column_text(2);
    asset.media_type = media_type_from_string(statement.column_text(3));
    asset.status = status_from_string(statement.column_text(4));
    asset.size_bytes = static_cast<std::uint64_t>(statement.column_int64(5));
    asset.sha256 = optional_text(statement, 6);
    asset.captured_at = optional_text(statement, 7);
    asset.width = optional_int(statement, 8);
    asset.height = optional_int(statement, 9);

    if (!statement.column_is_null(10) && !statement.column_is_null(11)) {
        core::GeoPoint point{
            .latitude = statement.column_double(10),
            .longitude = statement.column_double(11),
            .altitude = std::nullopt,
        };
        if (!statement.column_is_null(12)) {
            point.altitude = statement.column_double(12);
        }
        asset.location = point;
    }

    asset.camera = optional_text(statement, 13);
    asset.favorite = statement.column_int64(14) != 0;
    if (!statement.column_is_null(15)) {
        asset.thumbnail_path = statement.column_text(15);
    }
    if (!statement.column_is_null(16)) {
        asset.preview_path = statement.column_text(16);
    }
    if (!statement.column_is_null(17)) {
        asset.duration_seconds = statement.column_double(17);
    }
    asset.video_codec = optional_text(statement, 18);
    return asset;
}

}  // namespace

SqliteAssetRepository::SqliteAssetRepository(Database& database) : database(database) {}

core::Asset SqliteAssetRepository::create(const core::NewAsset& asset) {
    if (!asset.source_path.is_absolute()) {
        throw std::invalid_argument("Asset source path must be absolute");
    }
    if (asset.size_bytes > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("Asset size exceeds SQLite integer range");
    }

    std::int64_t asset_id = 0;
    {
        auto statement = database.prepare(
            "INSERT INTO assets ("
            "source_path, original_filename, media_type, size_bytes, sha256, captured_at, "
            "width, height, gps_lat, gps_lon, altitude, camera"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) RETURNING id");

        statement.bind(1, asset.source_path.string());
        statement.bind(2, asset.original_filename);
        statement.bind(3, to_string(asset.media_type));
        statement.bind(4, static_cast<std::int64_t>(asset.size_bytes));
        bind_optional(statement, 5, asset.sha256);
        bind_optional(statement, 6, asset.captured_at);
        bind_optional(statement, 7, asset.width);
        bind_optional(statement, 8, asset.height);

        if (asset.location.has_value()) {
            statement.bind(9, asset.location->latitude);
            statement.bind(10, asset.location->longitude);
            if (asset.location->altitude.has_value()) {
                statement.bind(11, *asset.location->altitude);
            } else {
                statement.bind_null(11);
            }
        } else {
            statement.bind_null(9);
            statement.bind_null(10);
            statement.bind_null(11);
        }

        bind_optional(statement, 12, asset.camera);
        if (!statement.step()) {
            throw std::runtime_error("SQLite did not return created asset id");
        }
        asset_id = statement.column_int64(0);
    }

    auto created = find_by_id(asset_id);
    if (!created.has_value()) {
        throw std::runtime_error("Created asset was not found");
    }
    return *created;
}

std::optional<core::Asset> SqliteAssetRepository::find_by_id(const std::int64_t id) {
    auto statement =
        database.prepare("SELECT " + std::string{kAssetColumns} + " FROM assets WHERE id = ?");
    statement.bind(1, id);
    if (!statement.step()) {
        return std::nullopt;
    }
    return read_asset(statement);
}

std::optional<core::Asset> SqliteAssetRepository::find_by_source_path(
    const std::filesystem::path& source_path) {
    auto statement = database.prepare("SELECT " + std::string{kAssetColumns} +
                                      " FROM assets WHERE source_path = ?");
    statement.bind(1, source_path.string());
    if (!statement.step()) {
        return std::nullopt;
    }
    return read_asset(statement);
}

std::vector<core::Asset> SqliteAssetRepository::list(const std::size_t limit,
                                                     const std::size_t offset,
                                                     const std::optional<core::AssetStatus> status) {
    if (limit > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) ||
        offset > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("Pagination value exceeds SQLite integer range");
    }

    std::string sql = "SELECT " + std::string{kAssetColumns} + " FROM assets";
    if (status.has_value()) {
        sql += " WHERE status = ?";
    }
    // NULLs last so undated media doesn't crowd the top; within the same date sort newest first
    sql += " ORDER BY CASE WHEN captured_at IS NULL THEN 1 ELSE 0 END, captured_at DESC, id DESC LIMIT ? OFFSET ?";

    auto statement = database.prepare(sql);
    int param = 1;
    if (status.has_value()) {
        statement.bind(param++, to_string(*status));
    }
    statement.bind(param++, static_cast<std::int64_t>(limit));
    statement.bind(param,   static_cast<std::int64_t>(offset));

    std::vector<core::Asset> assets;
    assets.reserve(limit);
    while (statement.step()) {
        assets.push_back(read_asset(statement));
    }
    return assets;
}

void SqliteAssetRepository::set_sha256(const std::int64_t id, const std::string_view sha256) {
    const bool is_hex = std::ranges::all_of(
        sha256, [](const unsigned char value) { return std::isxdigit(value) != 0; });
    if (sha256.size() != 64 || !is_hex) {
        throw std::invalid_argument("SHA-256 must contain 64 hexadecimal characters");
    }

    auto statement = database.prepare("UPDATE assets SET sha256 = ? WHERE id = ? RETURNING id");
    statement.bind(1, sha256);
    statement.bind(2, id);
    if (!statement.step()) {
        throw std::out_of_range("Asset not found");
    }
}

void SqliteAssetRepository::set_metadata(const std::int64_t id,
                                         const core::AssetMetadata& metadata) {
    auto statement = database.prepare(
        "UPDATE assets SET captured_at = ?, width = ?, height = ?, gps_lat = ?, "
        "gps_lon = ?, altitude = ?, camera = ? WHERE id = ? RETURNING id");
    bind_optional(statement, 1, metadata.captured_at);
    bind_optional(statement, 2, metadata.width);
    bind_optional(statement, 3, metadata.height);

    if (metadata.location.has_value()) {
        statement.bind(4, metadata.location->latitude);
        statement.bind(5, metadata.location->longitude);
        if (metadata.location->altitude.has_value()) {
            statement.bind(6, *metadata.location->altitude);
        } else {
            statement.bind_null(6);
        }
    } else {
        statement.bind_null(4);
        statement.bind_null(5);
        statement.bind_null(6);
    }

    bind_optional(statement, 7, metadata.camera);
    statement.bind(8, id);
    if (!statement.step()) {
        throw std::out_of_range("Asset not found");
    }
}

void SqliteAssetRepository::set_thumbnail_path(const std::int64_t id,
                                               const std::filesystem::path& path) {
    if (!path.is_absolute()) {
        throw std::invalid_argument("Thumbnail path must be absolute");
    }
    auto statement =
        database.prepare("UPDATE assets SET thumbnail_path = ? WHERE id = ? RETURNING id");
    statement.bind(1, path.string());
    statement.bind(2, id);
    if (!statement.step()) {
        throw std::out_of_range("Asset not found");
    }
}

void SqliteAssetRepository::set_preview_path(const std::int64_t id,
                                             const std::filesystem::path& path) {
    if (!path.is_absolute()) {
        throw std::invalid_argument("Preview path must be absolute");
    }
    auto statement =
        database.prepare("UPDATE assets SET preview_path = ? WHERE id = ? RETURNING id");
    statement.bind(1, path.string());
    statement.bind(2, id);
    if (!statement.step()) {
        throw std::out_of_range("Asset not found");
    }
}

void SqliteAssetRepository::set_favorite(const std::int64_t id, const bool favorite) {
    auto statement =
        database.prepare("UPDATE assets SET favorite = ? WHERE id = ? RETURNING id");
    statement.bind(1, static_cast<std::int64_t>(favorite));
    statement.bind(2, id);
    if (!statement.step()) {
        throw std::out_of_range("Asset not found");
    }
}

void SqliteAssetRepository::set_status(const std::int64_t id, const core::AssetStatus status) {
    auto statement = database.prepare("UPDATE assets SET status = ? WHERE id = ? RETURNING id");
    statement.bind(1, to_string(status));
    statement.bind(2, id);
    if (!statement.step()) {
        throw std::out_of_range("Asset not found");
    }
}

std::vector<core::HashedAsset> SqliteAssetRepository::list_hashed_assets(
    const core::AssetStatus status) {
    auto statement = database.prepare(
        "SELECT id, sha256 FROM assets WHERE status = ? AND sha256 IS NOT NULL ORDER BY id");
    statement.bind(1, to_string(status));

    std::vector<core::HashedAsset> assets;
    while (statement.step()) {
        assets.push_back(core::HashedAsset{
            .id = statement.column_int64(0),
            .sha256 = statement.column_text(1),
        });
    }
    return assets;
}

std::vector<core::TimedAsset> SqliteAssetRepository::list_timed_assets(
    const core::AssetStatus status) {
    auto statement = database.prepare(
        "SELECT id, captured_at FROM assets WHERE status = ? AND captured_at IS NOT NULL "
        "ORDER BY captured_at, id");
    statement.bind(1, to_string(status));

    std::vector<core::TimedAsset> assets;
    while (statement.step()) {
        assets.push_back(core::TimedAsset{
            .id = statement.column_int64(0),
            .captured_at = statement.column_text(1),
        });
    }
    return assets;
}

std::vector<core::GeoAsset> SqliteAssetRepository::list_geo_points(
    const core::AssetStatus status) {
    auto statement = database.prepare(
        "SELECT id, gps_lat, gps_lon, media_type, favorite FROM assets "
        "WHERE status = ? AND gps_lat IS NOT NULL AND gps_lon IS NOT NULL "
        "ORDER BY id");

    statement.bind(1, to_string(status));

    std::vector<core::GeoAsset> points;
    while (statement.step()) {
        points.push_back(core::GeoAsset{
            .id = statement.column_int64(0),
            .latitude = statement.column_double(1),
            .longitude = statement.column_double(2),
            .media_type = media_type_from_string(statement.column_text(3)),
            .favorite = statement.column_int64(4) != 0,
        });
    }
    return points;
}

std::int64_t SqliteAssetRepository::count(const core::AssetStatus status) {
    auto statement =
        database.prepare("SELECT COUNT(*) FROM assets WHERE status = ?");
    statement.bind(1, to_string(status));
    if (!statement.step()) {
        throw std::runtime_error("COUNT query returned no rows");
    }
    return statement.column_int64(0);
}

void SqliteAssetRepository::set_video_metadata(const std::int64_t id,
                                               const core::VideoMetadata& metadata) {
    auto statement = database.prepare(
        "UPDATE assets SET duration_seconds = ?, video_codec = ?, width = ?, height = ?, "
        "captured_at = ? WHERE id = ? RETURNING id");

    if (metadata.duration_seconds.has_value()) {
        statement.bind(1, *metadata.duration_seconds);
    } else {
        statement.bind_null(1);
    }
    bind_optional(statement, 2, metadata.codec);
    bind_optional(statement, 3, metadata.width);
    bind_optional(statement, 4, metadata.height);
    bind_optional(statement, 5, metadata.captured_at);
    statement.bind(6, id);

    if (!statement.step()) {
        throw std::out_of_range("Asset not found");
    }
}

}  // namespace geoframe::db
