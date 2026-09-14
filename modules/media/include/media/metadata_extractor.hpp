#pragma once

#include "core/asset.hpp"

#include <filesystem>

namespace geoframe::media {

/**
 * @brief Извлекает нормализованные EXIF-метаданные изображения.
 *
 * Отсутствующие EXIF-поля остаются std::nullopt. Время файловой системы
 * намеренно не используется как дата съёмки.
 */
core::AssetMetadata extract_image_metadata(const std::filesystem::path& path);

}  // namespace geoframe::media
