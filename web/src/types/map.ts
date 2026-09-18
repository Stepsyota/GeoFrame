export interface MapFeatureProperties {
  cluster: boolean
  pointCount: number
  assetIds: number[]
  assetId?: number
  clusterId?: number
  mediaType?: 'image' | 'video'
  favorite?: boolean
  capturedAt?: string | null
  thumbnailUrl?: string | null
  previewUrl?: string | null
}

export interface MapFeature {
  type: 'Feature'
  geometry: {
    type: 'Point'
    coordinates: [number, number]
  }
  properties: MapFeatureProperties
}

export interface MapClusterCollection {
  type: 'FeatureCollection'
  features: MapFeature[]
}
