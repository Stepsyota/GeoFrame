#pragma once

#include "core/asset.hpp"

#include <cstdint>

namespace geoframe::core {

/** Minimal asset record for map rendering. */
struct GeoAsset {
    std::int64_t id;
    double latitude;
    double longitude;
    MediaType media_type;
    bool favorite = false;
};

}  // namespace geoframe::core
