#include "core/version.hpp"

#include <gtest/gtest.h>

TEST(CoreVersion, IsNotEmpty) {
    EXPECT_FALSE(geoframe::core::version().empty());
}
