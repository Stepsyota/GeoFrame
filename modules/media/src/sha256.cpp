#include "media/sha256.hpp"

#include <openssl/evp.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace geoframe::media {

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for SHA-256: " + path.string());
    }

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context{EVP_MD_CTX_new(),
                                                                   EVP_MD_CTX_free};
    if (!context) {
        throw std::runtime_error("OpenSSL: EVP_MD_CTX_new failed");
    }
    if (EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("OpenSSL: EVP_DigestInit_ex failed");
    }

    std::array<char, 64 * 1024> buffer;
    while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || file.gcount()) {
        if (EVP_DigestUpdate(context.get(), buffer.data(),
                             static_cast<std::size_t>(file.gcount())) != 1) {
            throw std::runtime_error("OpenSSL: EVP_DigestUpdate failed for " + path.string());
        }
    }
    if (!file.eof()) {
        throw std::runtime_error("Cannot read file for SHA-256: " + path.string());
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest;
    unsigned int digest_size = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1) {
        throw std::runtime_error("OpenSSL: EVP_DigestFinal_ex failed");
    }

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < digest_size; ++index) {
        result << std::setw(2) << static_cast<unsigned int>(digest[index]);
    }
    return result.str();
}

}  // namespace geoframe::media
