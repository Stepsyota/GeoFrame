#include "media/sha256.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace geoframe::media {

class Sha256Test : public testing::Test {
protected:
    Sha256Test() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("geoframe-hash-" + std::to_string(suffix));
    }

    ~Sha256Test() override {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::filesystem::path path;
};

TEST_F(Sha256Test, CalculatesKnownDigest) {
    std::ofstream{path, std::ios::binary} << "test";

    EXPECT_EQ(sha256_file(path),
              "9f86d081884c7d659a2feaa0c55ad015"
              "a3bf4f1b2b0b822cd15d6c15b0f00a08");
}

TEST_F(Sha256Test, RejectsMissingFile) {
    EXPECT_THROW(sha256_file(path), std::runtime_error);
}

}  // namespace geoframe::media
