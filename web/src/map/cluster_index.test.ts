import { describe, expect, it } from 'vitest'

import { haversineMeters, mergeNearbyFeatures, pickCoverProperties } from './cluster_index'
import type { MapFeature } from '../types/map'

const point = (
  id: number,
  lon: number,
  lat: number,
  capturedAt: string | null = null,
  thumbnailUrl: string | null = null,
): MapFeature => ({
  type: 'Feature',
  geometry: { type: 'Point', coordinates: [lon, lat] },
  properties: {
    cluster: false,
    pointCount: 1,
    assetIds: [id],
    assetId: id,
    mediaType: 'image',
    favorite: false,
    capturedAt,
    thumbnailUrl,
    previewUrl: thumbnailUrl,
  },
})

describe('mergeNearbyFeatures', () => {
  it('merges points within the geo radius', () => {
    const merged = mergeNearbyFeatures(
      [point(1, 27.5615, 53.9023), point(2, 27.56152, 53.90232)],
      20,
    )

    expect(merged).toHaveLength(1)
    expect(merged[0].properties.cluster).toBe(true)
    expect(merged[0].properties.pointCount).toBe(2)
    expect(merged[0].properties.assetIds).toEqual([1, 2])
  })

  it('keeps distant points separate', () => {
    const merged = mergeNearbyFeatures(
      [point(1, 27.56, 53.9), point(2, 27.57, 53.91)],
      20,
    )

    expect(merged).toHaveLength(2)
  })
})

describe('pickCoverProperties', () => {
  it('uses the earliest captured photo as the cluster cover', () => {
    const cover = pickCoverProperties([
      point(2, 0, 0, '2025-05-24T10:09:40', '/api/media/2/thumbnail').properties,
      point(1, 0, 0, '2025-05-24T10:09:30', '/api/media/1/thumbnail').properties,
    ])

    expect(cover.assetId).toBe(1)
    expect(cover.thumbnailUrl).toBe('/api/media/1/thumbnail')
    expect(cover.pointCount).toBe(2)
  })
})

describe('haversineMeters', () => {
  it('returns a small distance for nearby coordinates', () => {
    const meters = haversineMeters(27.5615, 53.9023, 27.56152, 53.90232)
    expect(meters).toBeLessThan(5)
  })
})
