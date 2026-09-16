#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace geoframe::core {

struct HashedAsset {
    std::int64_t id;
    std::string sha256;
};

struct DuplicateGroup {
    std::string sha256;
    std::vector<std::int64_t> asset_ids;
};

/** Group assets that share the same SHA-256 hash (exact duplicates only). */
std::vector<DuplicateGroup> group_duplicate_hashes(const std::vector<HashedAsset>& assets);

}  // namespace geoframe::core
