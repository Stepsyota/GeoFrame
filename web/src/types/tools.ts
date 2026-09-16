import type { AssetSummary } from './asset'

export interface DuplicateGroup {
  sha256: string
  assetIds: number[]
  assets: AssetSummary[]
}

export interface DuplicateCollection {
  groups: DuplicateGroup[]
  total: number
}

export interface PhotoSeries {
  id: number
  assetIds: number[]
  assets: AssetSummary[]
}

export interface SeriesCollection {
  series: PhotoSeries[]
  total: number
  gapSeconds: number
}
