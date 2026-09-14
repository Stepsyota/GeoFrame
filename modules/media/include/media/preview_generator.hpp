#pragma once

#include <filesystem>
#include <string>

namespace geoframe::media {

/**
 * @brief Генерирует JPEG-копию изображения с ограничением по ширине и высоте.
 */
void generate_image_preview(const std::filesystem::path& source,
                            const std::filesystem::path& destination, int max_size,
                            const std::string& ffmpeg_binary = "ffmpeg");

}  // namespace geoframe::media
