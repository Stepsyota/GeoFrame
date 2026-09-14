export type MediaType = 'image' | 'video'
export type AssetStatus = 'active' | 'trashed'

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
}

export interface AssetPage {
  items: AssetSummary[]
  total: number
  limit: number
  offset: number
}
