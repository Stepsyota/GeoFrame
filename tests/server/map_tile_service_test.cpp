#include "server/map_tile_service.hpp"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

std::filesystem::path temp_data_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "geoframe_map_tile_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

}  // namespace

TEST(MapTileService, rejects_invalid_tile_coords) {
    geoframe::server::MapTileService service{temp_data_dir()};
    EXPECT_FALSE(service.tile(-1, 0, 0).has_value());
    EXPECT_FALSE(service.tile(0, -1, 0).has_value());
    EXPECT_FALSE(service.tile(0, 0, -1).has_value());
    EXPECT_FALSE(service.tile(0, 1, 0).has_value());
    EXPECT_FALSE(service.tile(15, 0, 0).has_value());
}

TEST(MapTileService, region_defaults_to_missing) {
    const auto data_dir = temp_data_dir();
    geoframe::server::MapTileService service{data_dir};
    EXPECT_FALSE(service.has_region());
    EXPECT_EQ(service.region_bytes(), 0U);
    EXPECT_EQ(service.region_path().filename(), "region.pmtiles");
    EXPECT_EQ(service.staging_dir(), data_dir / "tmp");
}

TEST(MapTileService, sprite_rejects_unknown_assets) {
    geoframe::server::MapTileService service{temp_data_dir()};
    EXPECT_FALSE(service.sprite("icon.png").has_value());
}

TEST(MapTileService, download_rejects_non_https_urls) {
    geoframe::server::MapTileService service{temp_data_dir()};
    EXPECT_FALSE(service.download_region("http://example.com/map.pmtiles", nullptr));
}
