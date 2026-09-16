#include "core/live_photo_sync.hpp"

#include "core/live_photo_pairer.hpp"

namespace geoframe::core {

std::size_t sync_live_photo_pairs(IAssetRepository& assets) {
    const auto existing_links = assets.list_live_photo_pairs();
    for (const auto& link : existing_links) {
        const auto image = assets.find_by_id(link.image_asset_id);
        const auto video = assets.find_by_id(link.video_asset_id);
        if (!image.has_value() || !video.has_value()
            || !is_valid_live_photo_pair(image->source_path, video->duration_seconds)) {
            assets.unlink_live_photo(link.image_asset_id);
        }
    }

    const auto candidates = assets.list_pairing_candidates(AssetStatus::Active);
    const auto remaining_links = assets.list_live_photo_pairs();
    std::vector<LivePhotoPair> existing;
    existing.reserve(remaining_links.size());
    for (const auto& link : remaining_links) {
        existing.push_back(LivePhotoPair{
            .image_asset_id = link.image_asset_id,
            .video_asset_id = link.video_asset_id,
        });
    }
    const auto detected = detect_live_photo_pairs(candidates, existing);

    std::size_t created = 0;
    for (const auto& pair : detected) {
        assets.link_live_photo(pair.image_asset_id, pair.video_asset_id);
        ++created;
    }
    return created;
}

}  // namespace geoframe::core
