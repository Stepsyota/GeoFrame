#include "storage/media_file.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace geoframe::storage {

namespace {

constexpr std::array kImageExtensions{".jpg", ".jpeg", ".png", ".heic"};
constexpr std::array kVideoExtensions{".mov"};

template <std::size_t Size>
bool contains(const std::array<const char*, Size>& values, const std::string& value) {
    return std::ranges::any_of(values, [&](const char* candidate) { return value == candidate; });
}

}  // namespace

std::optional<core::MediaType> detect_media_type(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](const unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    if (contains(kImageExtensions, extension)) {
        return core::MediaType::Image;
    }
    if (contains(kVideoExtensions, extension)) {
        return core::MediaType::Video;
    }
    return std::nullopt;
}

}  // namespace geoframe::storage
