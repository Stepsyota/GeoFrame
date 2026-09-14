#pragma once

#include <filesystem>
#include <string>

namespace geoframe::media {

/**
 * @brief Потоково вычисляет SHA-256 файла через OpenSSL EVP.
 *
 * @throws std::runtime_error если файл не удалось прочитать или OpenSSL вернул ошибку.
 */
std::string sha256_file(const std::filesystem::path& path);

}  // namespace geoframe::media
