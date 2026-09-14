#pragma once

#include "core/asset.hpp"

#include <filesystem>
#include <optional>

namespace geoframe::storage {

/**
 * @brief Определяет поддерживаемый тип медиа по расширению файла.
 */
std::optional<core::MediaType> detect_media_type(const std::filesystem::path& path);

}  // namespace geoframe::storage
