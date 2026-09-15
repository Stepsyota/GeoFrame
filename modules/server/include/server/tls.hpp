#pragma once

#include <filesystem>

namespace geoframe::server {

/**
 * @brief Генерирует self-signed TLS cert+key при первом запуске.
 *
 * Если файлы уже существуют — пропускает генерацию. Использует OpenSSL.
 *
 * @param cert_path  путь к cert.pem
 * @param key_path   путь к key.pem
 * @throws std::runtime_error при ошибке генерации
 */
void ensure_self_signed_cert(const std::filesystem::path& cert_path,
                             const std::filesystem::path& key_path);

}  // namespace geoframe::server
