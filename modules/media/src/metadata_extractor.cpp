#include "media/metadata_extractor.hpp"

#include <exiv2/exiv2.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace geoframe::media {

namespace {

const Exiv2::Exifdatum* find_tag(const Exiv2::ExifData& exif, const std::string& key) {
    const auto position = exif.findKey(Exiv2::ExifKey{key});
    return position == exif.end() ? nullptr : &*position;
}

std::optional<std::string> capture_time(const Exiv2::ExifData& exif) {
    const auto* value = find_tag(exif, "Exif.Photo.DateTimeOriginal");
    if (value == nullptr) {
        return std::nullopt;
    }

    std::string result = value->toString();
    if (result.size() >= 19 && result[4] == ':' && result[7] == ':') {
        result[4] = '-';
        result[7] = '-';
        result[10] = 'T';
    }
    return result.empty() ? std::nullopt : std::optional{result};
}

std::optional<std::string> camera_name(const Exiv2::ExifData& exif) {
    const auto* make_tag = find_tag(exif, "Exif.Image.Make");
    const auto* model_tag = find_tag(exif, "Exif.Image.Model");
    const std::string make = make_tag == nullptr ? "" : make_tag->toString();
    const std::string model = model_tag == nullptr ? "" : model_tag->toString();

    if (model.empty()) {
        return make.empty() ? std::nullopt : std::optional{make};
    }
    if (make.empty() || model.starts_with(make)) {
        return model;
    }
    return make + " " + model;
}

std::optional<double> coordinate(const Exiv2::ExifData& exif, const std::string& value_key,
                                 const std::string& reference_key) {
    const auto* value = find_tag(exif, value_key);
    const auto* reference = find_tag(exif, reference_key);
    if (value == nullptr || reference == nullptr || value->count() < 3) {
        return std::nullopt;
    }

    double result =
        value->toFloat(0) + value->toFloat(1) / 60.0 + value->toFloat(2) / 3600.0;
    std::string direction = reference->toString();
    std::ranges::transform(direction, direction.begin(), [](const unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    if (direction.starts_with('S') || direction.starts_with('W')) {
        result = -result;
    }
    return result;
}

std::optional<core::GeoPoint> location(const Exiv2::ExifData& exif) {
    const auto latitude =
        coordinate(exif, "Exif.GPSInfo.GPSLatitude", "Exif.GPSInfo.GPSLatitudeRef");
    const auto longitude =
        coordinate(exif, "Exif.GPSInfo.GPSLongitude", "Exif.GPSInfo.GPSLongitudeRef");
    if (!latitude.has_value() || !longitude.has_value()) {
        return std::nullopt;
    }

    std::optional<double> altitude;
    if (const auto* value = find_tag(exif, "Exif.GPSInfo.GPSAltitude"); value != nullptr) {
        altitude = value->toFloat();
        if (const auto* reference = find_tag(exif, "Exif.GPSInfo.GPSAltitudeRef");
            reference != nullptr && reference->toUint32() == 1) {
            *altitude = -*altitude;
        }
    }

    return core::GeoPoint{
        .latitude = *latitude,
        .longitude = *longitude,
        .altitude = altitude,
    };
}

std::optional<int> dimension(const std::uint32_t value) {
    if (value == 0 || value > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

}  // namespace

core::AssetMetadata extract_image_metadata(const std::filesystem::path& path) {
    try {
        auto image = Exiv2::ImageFactory::open(path.string());
        if (!image) {
            throw std::runtime_error("Exiv2 cannot open image: " + path.string());
        }
        image->readMetadata();

        const auto& exif = image->exifData();
        return core::AssetMetadata{
            .captured_at = capture_time(exif),
            .width = dimension(image->pixelWidth()),
            .height = dimension(image->pixelHeight()),
            .location = location(exif),
            .camera = camera_name(exif),
        };
    } catch (const Exiv2::Error& error) {
        throw std::runtime_error("Cannot extract image metadata from " + path.string() + ": " +
                                 error.what());
    }
}

}  // namespace geoframe::media
