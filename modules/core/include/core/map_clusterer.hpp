#pragma once

#include "core/geo_asset.hpp"

#include <cstdint>
#include <vector>

namespace geoframe::core {

/** Clustered or single map marker produced from geo assets. */
struct MapCluster {
    double latitude;
    double longitude;
    std::vector<std::int64_t> asset_ids;
    bool is_cluster = false;
};

/**
 * @brief Grid-based clustering for map markers.
 *
 * @param assets Geo-located assets to cluster.
 * @param zoom Map zoom level (0–22). Higher zoom → smaller cells → fewer clusters.
 */
std::vector<MapCluster> cluster_geo_assets(const std::vector<GeoAsset>& assets, int zoom);

}  // namespace geoframe::core
