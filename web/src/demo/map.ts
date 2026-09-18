import { demoAssetPage } from './assets'
import type { MapClusterCollection } from '../types/map'

const asset = (id: number) => demoAssetPage.items.find((item) => item.id === id)

const point = (
  id: number,
  lon: number,
  lat: number,
  favorite = false,
  mediaType: 'image' | 'video' = 'image',
) => ({
  type: 'Feature' as const,
  geometry: { type: 'Point' as const, coordinates: [lon, lat] as [number, number] },
  properties: {
    cluster: false,
    pointCount: 1,
    assetIds: [id],
    assetId: id,
    mediaType,
    favorite,
    capturedAt: asset(id)?.capturedAt ?? null,
    thumbnailUrl: asset(id)?.thumbnailUrl ?? null,
    previewUrl: asset(id)?.thumbnailUrl ?? null,
  },
})

export const demoMapPoints: MapClusterCollection = {
  type: 'FeatureCollection',
  features: [
    point(1, 27.5615, 53.9023, true),
    point(2, 27.5621, 53.9028),
    point(3, 27.5608, 53.9019),
    point(4, 27.5630, 53.9031),
    point(5, 27.5610, 53.9015),
    point(6, 27.5625, 53.9020),
    point(7, 27.5605, 53.9035),
    point(8, 27.5635, 53.9018),
    point(9, 30.3351, 59.9343, true),
    point(10, 37.6173, 55.7558),
  ],
}
