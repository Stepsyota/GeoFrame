#include "media/metadata_extractor.hpp"
#include "media/preview_generator.hpp"
#include "media/process_runner.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace geoframe::media {

class PreviewGeneratorTest : public testing::Test {
protected:
    PreviewGeneratorTest() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        directory =
            std::filesystem::temp_directory_path() / ("geoframe-preview-" + std::to_string(suffix));
        std::filesystem::create_directories(directory);
        source = directory / "source.ppm";
        destination = directory / "cache" / "preview.jpg";
        std::ofstream{source} << "P3\n2 1\n255\n255 0 0 0 255 0\n";
    }

    ~PreviewGeneratorTest() override {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }

    std::filesystem::path directory;
    std::filesystem::path source;
    std::filesystem::path destination;
};

TEST(ProcessRunner, CapturesOutput) {
    const auto result = run_process({"ffmpeg", "-version"}, std::chrono::seconds{10});

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.output.find("ffmpeg version"), std::string::npos);
}

TEST(ProcessRunner, RejectsEmptyArguments) {
    EXPECT_THROW(run_process({}, std::chrono::seconds{1}), std::invalid_argument);
}

TEST_F(PreviewGeneratorTest, GeneratesLimitedJpegPreview) {
    generate_image_preview(source, destination, 1);

    ASSERT_TRUE(std::filesystem::is_regular_file(destination));
    const auto metadata = extract_image_metadata(destination);
    EXPECT_EQ(metadata.width, 1);
    EXPECT_EQ(metadata.height, 1);
}

TEST_F(PreviewGeneratorTest, RejectsInvalidSize) {
    EXPECT_THROW(generate_image_preview(source, destination, 0), std::invalid_argument);
}

}  // namespace geoframe::media
