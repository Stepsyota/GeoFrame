export type MediaType = 'image' | 'video'
export type AssetStatus = 'active' | 'trashed'

export interface AssetGps {
  lat: number
  lon: number
  alt: number | null
}

export interface AssetSummary {
  id: number
  originalFilename: string
  mediaType: MediaType
  status: AssetStatus
  capturedAt: string | null
  width: number | null
  height: number | null
  favorite: boolean
  thumbnailUrl: string | null
  previewUrl: string | null
}

export interface AssetDetail extends AssetSummary {
  camera: string | null
  gps: AssetGps | null
  sizeBytes: number
  sha256: string | null
  durationSeconds: number | null
  videoCodec: string | null
}

export interface AssetListOptions {
  status?: AssetStatus
  favorite?: boolean
}

export interface AssetPage {
  items: AssetSummary[]
  total: number
  limit: number
  offset: number
}
