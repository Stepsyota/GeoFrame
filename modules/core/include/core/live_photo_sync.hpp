#pragma once

#include "core/asset_repository.hpp"

#include <cstddef>

namespace geoframe::core {

/** Detect new Live Photo pairs and persist them. Returns number of links created. */
std::size_t sync_live_photo_pairs(IAssetRepository& assets);

}  // namespace geoframe::core
