#include "media/metadata_extractor.hpp"

#include <exiv2/exiv2.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

namespace geoframe::media {

class MetadataExtractorTest : public testing::Test {
protected:
    MetadataExtractorTest() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path =
            std::filesystem::temp_directory_path() / ("geoframe-metadata-" + std::to_string(suffix));
    }

    ~MetadataExtractorTest() override {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    void create_image_with_exif() {
        auto image = Exiv2::ImageFactory::create(Exiv2::ImageType::jpeg, path.string());
        auto& exif = image->exifData();
        exif["Exif.Photo.DateTimeOriginal"] = "2026:09:14 20:30:00";
        exif["Exif.Image.Make"] = "Apple";
        exif["Exif.Image.Model"] = "iPhone 15 Pro";
        exif["Exif.GPSInfo.GPSLatitudeRef"] = "N";
        exif["Exif.GPSInfo.GPSLatitude"] = "53/1 54/1 0/1";
        exif["Exif.GPSInfo.GPSLongitudeRef"] = "E";
        exif["Exif.GPSInfo.GPSLongitude"] = "27/1 34/1 0/1";
        exif["Exif.GPSInfo.GPSAltitudeRef"] = "0";
        exif["Exif.GPSInfo.GPSAltitude"] = "220/1";
        image->writeMetadata();
    }

    std::filesystem::path path;
};

TEST_F(MetadataExtractorTest, ExtractsNormalizedExif) {
    create_image_with_exif();

    const auto metadata = extract_image_metadata(path);

    EXPECT_EQ(metadata.captured_at, "2026-09-14T20:30:00");
    EXPECT_EQ(metadata.camera, "Apple iPhone 15 Pro");
    ASSERT_TRUE(metadata.location.has_value());
    EXPECT_DOUBLE_EQ(metadata.location->latitude, 53.9);
    EXPECT_NEAR(metadata.location->longitude, 27.566666, 0.000001);
    EXPECT_EQ(metadata.location->altitude, 220.0);
}

TEST_F(MetadataExtractorTest, RejectsMissingFile) {
    EXPECT_THROW(extract_image_metadata(path), std::runtime_error);
}

TEST_F(MetadataExtractorTest, ExtractsFullExifTagList) {
    create_image_with_exif();

    const auto tags = extract_image_exif(path);
    EXPECT_GE(tags.size(), 5U);
    EXPECT_TRUE(std::ranges::is_sorted(tags, {}, &ExifTag::key));

    const auto find_tag = [&](const std::string& key) {
        return std::ranges::find_if(tags, [&](const ExifTag& tag) { return tag.key == key; });
    };

    const auto make = find_tag("Exif.Image.Make");
    ASSERT_NE(make, tags.end());
    EXPECT_EQ(make->value, "Apple");
}

}  // namespace geoframe::media
