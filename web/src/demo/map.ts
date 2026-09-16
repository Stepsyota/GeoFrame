import type { MapClusterCollection } from '../types/map'

export const demoMapClusters: MapClusterCollection = {
  type: 'FeatureCollection',
  features: [
    {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [27.5615, 53.9023] },
      properties: {
        cluster: true,
        pointCount: 8,
        assetIds: [1, 2, 3, 4, 5, 6, 7, 8],
      },
    },
    {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [30.3351, 59.9343] },
      properties: {
        cluster: false,
        pointCount: 1,
        assetIds: [9],
        assetId: 9,
        mediaType: 'image',
        favorite: true,
        thumbnailUrl: null,
      },
    },
    {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [37.6173, 55.7558] },
      properties: {
        cluster: false,
        pointCount: 1,
        assetIds: [10],
        assetId: 10,
        mediaType: 'image',
        favorite: false,
        thumbnailUrl: null,
      },
    },
  ],
}
