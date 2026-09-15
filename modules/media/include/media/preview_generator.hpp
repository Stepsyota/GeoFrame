#pragma once

#include <filesystem>
#include <string>

namespace geoframe::media {

/**
 * @brief Генерирует JPEG-копию изображения с ограничением max_size по длинной стороне.
 *
 * Стратегия:
 *   HEIC/HEIF/AVIF: heif-convert (полный кадр) → ffmpeg scale
 *   Остальное:      ffmpeg scale → ImageMagick → vips
 *
 * @throws std::runtime_error если все три инструмента провалились.
 */
void generate_image_preview(const std::filesystem::path& source,
                            const std::filesystem::path& destination, int max_size,
                            const std::string& ffmpeg_binary = "ffmpeg");

}  // namespace geoframe::media
