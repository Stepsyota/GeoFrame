#include "core/series_detector.hpp"

#include <gtest/gtest.h>

namespace geoframe::core {

TEST(SeriesDetectorTest, ParsesExifAndIsoTimestamps) {
    EXPECT_EQ(parse_captured_epoch("2025:05:24 10:09:30"), 1'748'081'370LL);
    EXPECT_EQ(parse_captured_epoch("2025-05-24T10:09:30"), 1'748'081'370LL);
}

TEST(SeriesDetectorTest, GroupsPhotosWithinThreeSeconds) {
    const std::vector<TimedAsset> assets{
        {.id = 1, .captured_at = "2025-05-24T10:09:30"},
        {.id = 2, .captured_at = "2025-05-24T10:09:31"},
        {.id = 3, .captured_at = "2025-05-24T10:09:50"},
        {.id = 4, .captured_at = "2025-05-24T10:09:51"},
    };

    const auto series = detect_photo_series(assets, 3);
    EXPECT_EQ(series.size(), 2U);
    EXPECT_EQ(series[0].asset_ids, std::vector<std::int64_t>({1, 2}));
    EXPECT_EQ(series[1].asset_ids, std::vector<std::int64_t>({3, 4}));
}

TEST(SeriesDetectorTest, RequiresAtLeastTwoAssets) {
    const std::vector<TimedAsset> assets{
        {.id = 1, .captured_at = "2025-05-24T10:09:30"},
        {.id = 2, .captured_at = "2025-05-24T10:09:40"},
    };

    EXPECT_TRUE(detect_photo_series(assets, 3).empty());
}

}  // namespace geoframe::core
