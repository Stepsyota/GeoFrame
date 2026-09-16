#pragma once

#include "core/asset.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace geoframe::media {

struct ExifTag {
    std::string key;
    std::string value;
};

/**
 * @brief Извлекает нормализованные EXIF-метаданные изображения.
 *
 * Отсутствующие EXIF-поля остаются std::nullopt. Время файловой системы
 * намеренно не используется как дата съёмки.
 */
core::AssetMetadata extract_image_metadata(const std::filesystem::path& path);

/** Returns all EXIF tags from the original file, sorted by key. */
std::vector<ExifTag> extract_image_exif(const std::filesystem::path& path);

}  // namespace geoframe::media
