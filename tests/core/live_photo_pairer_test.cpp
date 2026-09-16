#include "core/live_photo_pairer.hpp"

#include <gtest/gtest.h>

namespace geoframe::core {

TEST(LivePhotoPairerTest, MatchesHeicAndMovWithSameStem) {
    const std::vector<PairingCandidate> candidates = {
        {.id = 1,
         .source_path = "/photos/IMG_0001.HEIC",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 2,
         .source_path = "/photos/IMG_0001.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = 2.1},
    };

    const auto pairs = detect_live_photo_pairs(candidates, {});

    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].image_asset_id, 1);
    EXPECT_EQ(pairs[0].video_asset_id, 2);
}

TEST(LivePhotoPairerTest, MatchesHeicEvenWhenMovDurationUnknown) {
    const std::vector<PairingCandidate> candidates = {
        {.id = 1,
         .source_path = "/photos/IMG_0001.HEIC",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 2,
         .source_path = "/photos/IMG_0001.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = std::nullopt},
    };

    const auto pairs = detect_live_photo_pairs(candidates, {});

    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].image_asset_id, 1);
    EXPECT_EQ(pairs[0].video_asset_id, 2);
}

TEST(LivePhotoPairerTest, SkipsLongMovEvenWithHeic) {
    const std::vector<PairingCandidate> candidates = {
        {.id = 1,
         .source_path = "/photos/IMG_0001.HEIC",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 2,
         .source_path = "/photos/IMG_0001.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = 12.5},
    };

    const auto pairs = detect_live_photo_pairs(candidates, {});

    EXPECT_TRUE(pairs.empty());
}

TEST(LivePhotoPairerTest, MatchesJpegWithShortMov) {
    const std::vector<PairingCandidate> candidates = {
        {.id = 1,
         .source_path = "/photos/IMG_0001.JPG",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 2,
         .source_path = "/photos/IMG_0001.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = 2.8},
    };

    const auto pairs = detect_live_photo_pairs(candidates, {});

    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].image_asset_id, 1);
    EXPECT_EQ(pairs[0].video_asset_id, 2);
}

TEST(LivePhotoPairerTest, SkipsJpegWhenMovDurationUnknown) {
    const std::vector<PairingCandidate> candidates = {
        {.id = 1,
         .source_path = "/photos/IMG_0001.JPG",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 2,
         .source_path = "/photos/IMG_0001.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = std::nullopt},
    };

    EXPECT_TRUE(detect_live_photo_pairs(candidates, {}).empty());
}

TEST(LivePhotoPairerTest, PrefersHeicWhenMultipleImagesShareStem) {
    const std::vector<PairingCandidate> candidates = {
        {.id = 1,
         .source_path = "/photos/IMG_0001.JPG",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 2,
         .source_path = "/photos/IMG_0001.HEIC",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 3,
         .source_path = "/photos/IMG_0001.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = 1.5},
    };

    const auto pairs = detect_live_photo_pairs(candidates, {});

    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].image_asset_id, 2);
    EXPECT_EQ(pairs[0].video_asset_id, 3);
}

TEST(LivePhotoPairerTest, SkipsAlreadyPairedAssets) {
    const std::vector<PairingCandidate> candidates = {
        {.id = 1,
         .source_path = "/photos/IMG_0001.HEIC",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 2,
         .source_path = "/photos/IMG_0001.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = 1.5},
        {.id = 3,
         .source_path = "/photos/IMG_0002.HEIC",
         .media_type = MediaType::Image,
         .duration_seconds = std::nullopt},
        {.id = 4,
         .source_path = "/photos/IMG_0002.MOV",
         .media_type = MediaType::Video,
         .duration_seconds = 1.5},
    };
    const std::vector<LivePhotoPair> existing = {
        {.image_asset_id = 1, .video_asset_id = 2},
    };

    const auto pairs = detect_live_photo_pairs(candidates, existing);

    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].image_asset_id, 3);
    EXPECT_EQ(pairs[0].video_asset_id, 4);
}

}  // namespace geoframe::core
