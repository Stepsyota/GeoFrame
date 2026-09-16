import type { AssetSummary } from '../types/asset'
import { MediaCard } from './MediaCard'

interface AssetStripProps {
  assets: AssetSummary[]
  onOpen: (asset: AssetSummary) => void
}

export const AssetStrip = ({ assets, onOpen }: AssetStripProps) => (
  <div className="asset-strip">
    {assets.map((asset) => (
      <MediaCard asset={asset} key={asset.id} onOpen={onOpen} />
    ))}
  </div>
)
