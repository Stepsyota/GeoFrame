#pragma once

#include "core/asset.hpp"

#include <filesystem>

namespace geoframe::media {

/**
 * @brief Извлекает метаданные видеофайла через ffprobe.
 *
 * @throws std::runtime_error если ffprobe недоступен или завершился с ошибкой.
 */
core::VideoMetadata extract_video_metadata(const std::filesystem::path& path);

/**
 * @brief Создаёт JPEG-постер (первый кадр) из видеофайла через ffmpeg.
 *
 * @param source  абсолютный путь к видеофайлу
 * @param dest    абсолютный путь к целевому JPEG (директория должна существовать)
 * @param max_px  максимальная ширина/высота постера
 * @throws std::runtime_error при ошибке генерации
 */
void generate_video_poster(const std::filesystem::path& source,
                           const std::filesystem::path& dest, int max_px);

}  // namespace geoframe::media
