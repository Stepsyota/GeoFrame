#include "core/duplicate_grouper.hpp"

#include <gtest/gtest.h>

namespace geoframe::core {

TEST(DuplicateGrouperTest, GroupsAssetsWithSameHash) {
    const std::vector<HashedAsset> assets{
        {.id = 1, .sha256 = "aaa"},
        {.id = 2, .sha256 = "aaa"},
        {.id = 3, .sha256 = "bbb"},
        {.id = 4, .sha256 = "bbb"},
        {.id = 5, .sha256 = "bbb"},
        {.id = 6, .sha256 = "ccc"},
    };

    const auto groups = group_duplicate_hashes(assets);
    EXPECT_EQ(groups.size(), 2U);
    EXPECT_EQ(groups[0].sha256, "bbb");
    EXPECT_EQ(groups[0].asset_ids, std::vector<std::int64_t>({3, 4, 5}));
    EXPECT_EQ(groups[1].sha256, "aaa");
    EXPECT_EQ(groups[1].asset_ids, std::vector<std::int64_t>({1, 2}));
}

TEST(DuplicateGrouperTest, IgnoresUniqueHashes) {
    const std::vector<HashedAsset> assets{
        {.id = 1, .sha256 = "solo"},
        {.id = 2, .sha256 = ""},
    };

    EXPECT_TRUE(group_duplicate_hashes(assets).empty());
}

}  // namespace geoframe::core
