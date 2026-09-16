#include "core/map_clusterer.hpp"

#include <algorithm>
#include <gtest/gtest.h>

namespace geoframe::core {

TEST(MapClustererTest, GroupsNearbyPointsAtLowZoom) {
    const std::vector<GeoAsset> assets{
        {.id = 1, .latitude = 53.9, .longitude = 27.56, .media_type = MediaType::Image},
        {.id = 2, .latitude = 53.91, .longitude = 27.57, .media_type = MediaType::Image},
        {.id = 3, .latitude = 40.7, .longitude = -74.0, .media_type = MediaType::Video},
    };

    const auto clusters = cluster_geo_assets(assets, 5);
    EXPECT_EQ(clusters.size(), 2U);

    const auto merged = std::find_if(clusters.begin(), clusters.end(),
                                    [](const MapCluster& cluster) { return cluster.is_cluster; });
    ASSERT_NE(merged, clusters.end());
    EXPECT_EQ(merged->asset_ids.size(), 2U);
}

TEST(MapClustererTest, KeepsSeparatePointsAtHighZoom) {
    const std::vector<GeoAsset> assets{
        {.id = 1, .latitude = 53.9, .longitude = 27.56, .media_type = MediaType::Image},
        {.id = 2, .latitude = 53.91, .longitude = 27.57, .media_type = MediaType::Image},
    };

    const auto clusters = cluster_geo_assets(assets, 16);
    EXPECT_EQ(clusters.size(), 2U);
    EXPECT_FALSE(clusters[0].is_cluster);
    EXPECT_FALSE(clusters[1].is_cluster);
}

}  // namespace geoframe::core
