#include "core/map_clusterer.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace geoframe::core {

namespace {

struct CellAccumulator {
    double lat_sum = 0.0;
    double lon_sum = 0.0;
    std::vector<std::int64_t> asset_ids;
};

std::string cell_key(const double latitude, const double longitude, const int zoom) {
    const auto clamped_zoom = std::clamp(zoom, 0, 22);
    const double cell_size = 360.0 / std::pow(2.0, static_cast<double>(clamped_zoom));
    const auto cell_x = static_cast<long long>(std::floor((longitude + 180.0) / cell_size));
    const auto cell_y = static_cast<long long>(std::floor((latitude + 90.0) / cell_size));
    return std::to_string(cell_x) + ':' + std::to_string(cell_y);
}

}  // namespace

std::vector<MapCluster> cluster_geo_assets(const std::vector<GeoAsset>& assets, const int zoom) {
    std::unordered_map<std::string, CellAccumulator> cells;

    for (const auto& asset : assets) {
        const auto key = cell_key(asset.latitude, asset.longitude, zoom);
        auto& cell = cells[key];
        cell.lat_sum += asset.latitude;
        cell.lon_sum += asset.longitude;
        cell.asset_ids.push_back(asset.id);
    }

    std::vector<MapCluster> clusters;
    clusters.reserve(cells.size());

    for (auto& [key, cell] : cells) {
        const auto count = static_cast<double>(cell.asset_ids.size());
        MapCluster cluster{
            .latitude = cell.lat_sum / count,
            .longitude = cell.lon_sum / count,
            .asset_ids = std::move(cell.asset_ids),
            .is_cluster = count > 1.0,
        };
        clusters.push_back(std::move(cluster));
    }

    return clusters;
}

}  // namespace geoframe::core
