import { demoMapPoints } from '../demo/map'
import { getLibraryStatus } from './status'
import { CARTO_STYLE_URL, createHybridStyle, createPmtilesStyle } from '../map/basemap'
import type { StyleSpecification } from 'maplibre-gl'
import type { MapClusterCollection } from '../types/map'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'
const POINTS_CACHE_KEY = 'geoframe.map.points.v2'
const POINTS_CACHE_TTL_MS = 15 * 60 * 1000

interface CachedPoints {
  savedAt: number
  data: MapClusterCollection
}

const readPointsCache = (): MapClusterCollection | null => {
  try {
    const raw = sessionStorage.getItem(POINTS_CACHE_KEY)
    if (!raw) {
      return null
    }
    const cached = JSON.parse(raw) as CachedPoints
    if (Date.now() - cached.savedAt > POINTS_CACHE_TTL_MS) {
      sessionStorage.removeItem(POINTS_CACHE_KEY)
      return null
    }
    return cached.data
  } catch {
    return null
  }
}

const writePointsCache = (data: MapClusterCollection) => {
  try {
    const cached: CachedPoints = { savedAt: Date.now(), data }
    sessionStorage.setItem(POINTS_CACHE_KEY, JSON.stringify(cached))
  } catch {
    // Storage full or unavailable — ignore.
  }
}

export const getMapPoints = async (signal?: AbortSignal): Promise<MapClusterCollection> => {
  if (demoMode) {
    return demoMapPoints
  }

  const cached = readPointsCache()
  if (cached) {
    return cached
  }

  const query = new URLSearchParams({ status: 'active' })
  const response = await fetch(`/api/map/points?${query}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame map API returned ${response.status}`)
  }

  const data = (await response.json()) as MapClusterCollection
  writePointsCache(data)
  return data
}

export type MapBasemap =
  | { kind: 'hybrid'; style: StyleSpecification }
  | { kind: 'pmtiles'; style: StyleSpecification }
  | { kind: 'carto'; styleUrl: string }

export const getMapBasemap = async (signal?: AbortSignal): Promise<MapBasemap> => {
  if (demoMode) {
    return { kind: 'carto', styleUrl: CARTO_STYLE_URL }
  }

  const status = await getLibraryStatus(signal)
  if (status.map?.regionPmtiles) {
    const localMaxZoom = status.map.localMaxZoom ?? 13
    if (status.map.basemap === 'hybrid') {
      return {
        kind: 'hybrid',
        style: createHybridStyle('/api/map/region.pmtiles', localMaxZoom),
      }
    }
    return { kind: 'pmtiles', style: createPmtilesStyle('/api/map/region.pmtiles') }
  }

  return { kind: 'carto', styleUrl: '/map/style.json' }
}
