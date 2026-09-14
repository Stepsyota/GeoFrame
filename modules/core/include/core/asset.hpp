#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace geoframe::core {

enum class MediaType {
    Image,
    Video,
};

enum class AssetStatus {
    Active,
    Trashed,
};

struct GeoPoint {
    double latitude;
    double longitude;
    std::optional<double> altitude;
};

struct AssetMetadata {
    std::optional<std::string> captured_at;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<GeoPoint> location;
    std::optional<std::string> camera;
};

/**
 * @brief Данные нового медиафайла, найденного при сканировании библиотеки.
 */
struct NewAsset {
    std::filesystem::path source_path;
    std::string original_filename;
    MediaType media_type;
    std::uint64_t size_bytes;
    std::optional<std::string> sha256;
    std::optional<std::string> captured_at;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<GeoPoint> location;
    std::optional<std::string> camera;
};

/**
 * @brief Проиндексированный медиафайл и его нормализованные метаданные.
 */
struct Asset : NewAsset {
    std::int64_t id;
    AssetStatus status = AssetStatus::Active;
    bool favorite = false;
};

}  // namespace geoframe::core
