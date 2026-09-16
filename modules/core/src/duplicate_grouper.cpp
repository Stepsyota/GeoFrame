#include "core/duplicate_grouper.hpp"

#include <algorithm>
#include <unordered_map>

namespace geoframe::core {

std::vector<DuplicateGroup> group_duplicate_hashes(const std::vector<HashedAsset>& assets) {
    std::unordered_map<std::string, std::vector<std::int64_t>> buckets;
    buckets.reserve(assets.size());

    for (const auto& asset : assets) {
        if (asset.sha256.empty()) {
            continue;
        }
        buckets[asset.sha256].push_back(asset.id);
    }

    std::vector<DuplicateGroup> groups;
    for (auto& [sha256, asset_ids] : buckets) {
        if (asset_ids.size() < 2) {
            continue;
        }
        std::sort(asset_ids.begin(), asset_ids.end());
        groups.push_back(DuplicateGroup{
            .sha256 = sha256,
            .asset_ids = std::move(asset_ids),
        });
    }

    std::sort(groups.begin(), groups.end(),
              [](const DuplicateGroup& left, const DuplicateGroup& right) {
                  return left.asset_ids.size() > right.asset_ids.size();
              });

    return groups;
}

}  // namespace geoframe::core
