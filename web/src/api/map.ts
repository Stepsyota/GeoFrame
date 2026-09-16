import { demoMapClusters } from '../demo/map'
import type { MapClusterCollection } from '../types/map'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

export const getMapClusters = async (
  zoom: number,
  signal?: AbortSignal,
): Promise<MapClusterCollection> => {
  if (demoMode) {
    return demoMapClusters
  }

  const query = new URLSearchParams({
    zoom: String(zoom),
    status: 'active',
  })
  const response = await fetch(`/api/map/clusters?${query}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame map API returned ${response.status}`)
  }

  return (await response.json()) as MapClusterCollection
}
