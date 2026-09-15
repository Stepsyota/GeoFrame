#pragma once

#include <filesystem>
#include <string>

namespace geoframe::media {

/**
 * @brief Генерирует JPEG-копию изображения с ограничением max_size по длинной стороне.
 *
 * Стратегия:
 *   1. ffmpeg  — быстро, поддерживает JPEG/PNG/WebP/TIFF и (с libheif) HEIC
 *   2. convert — ImageMagick; поддерживает HEIC, RAW, TIFF, WebP и многое другое
 *   3. vips    — самый широкий охват форматов как последний резерв
 *
 * @throws std::runtime_error если все три инструмента провалились.
 */
void generate_image_preview(const std::filesystem::path& source,
                            const std::filesystem::path& destination, int max_size,
                            const std::string& ffmpeg_binary = "ffmpeg");

}  // namespace geoframe::media
