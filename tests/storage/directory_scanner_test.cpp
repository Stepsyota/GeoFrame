#include "core/asset.hpp"
#include "storage/directory_scanner.hpp"
#include "storage/media_file.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace geoframe::storage {

class TempDirectory {
public:
    TempDirectory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("geoframe-storage-" + std::to_string(suffix));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

void create_file(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream{path} << "test";
}

TEST(DirectoryScanner, VisitsRegularFilesRecursively) {
    TempDirectory directory;
    create_file(directory.path / "photo.jpg");
    create_file(directory.path / "nested" / "video.mov");

    std::vector<std::filesystem::path> files;
    const auto summary =
        DirectoryScanner{}.scan(directory.path, [&](const auto& path) { files.push_back(path); });

    EXPECT_EQ(summary.files, 2);
    EXPECT_EQ(summary.filesystem_errors, 0);
    EXPECT_EQ(files.size(), 2);
}

TEST(DirectoryScanner, RejectsMissingRoot) {
    EXPECT_THROW(DirectoryScanner{}.scan("/missing/geoframe/directory", [](const auto&) {}),
                 std::invalid_argument);
}

TEST(MediaFile, DetectsSupportedExtensionCaseInsensitive) {
    EXPECT_EQ(detect_media_type("photo.HEIC"), core::MediaType::Image);
    EXPECT_EQ(detect_media_type("video.MOV"), core::MediaType::Video);
    EXPECT_FALSE(detect_media_type("document.pdf").has_value());
}

}  // namespace geoframe::storage
