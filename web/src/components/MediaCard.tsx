import { Heart, Play } from 'lucide-react'

import type { AssetSummary } from '../types/asset'

interface MediaCardProps {
  asset: AssetSummary
  onOpen?: (asset: AssetSummary) => void
}

export const MediaCard = ({ asset, onOpen }: MediaCardProps) => (
  <button
    className="media-card"
    type="button"
    aria-label={`Open ${asset.originalFilename}`}
    onClick={() => onOpen?.(asset)}
  >
    {asset.thumbnailUrl ? (
      <img src={asset.thumbnailUrl} alt="" loading="lazy" />
    ) : (
      <span className="media-placeholder">{asset.originalFilename}</span>
    )}

    <span className="media-card-shade" />
    {asset.mediaType === 'video' && (
      <span className="media-badge">
        <Play size={13} fill="currentColor" />
        Video
      </span>
    )}
    {asset.favorite && (
      <span className="favorite-badge" aria-label="Favorite">
        <Heart size={16} fill="currentColor" />
      </span>
    )}
  </button>
)
