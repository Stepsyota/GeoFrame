import { demoSeries } from '../demo/series'
import type { SeriesCollection } from '../types/tools'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

export const getSeries = async (signal?: AbortSignal): Promise<SeriesCollection> => {
  if (demoMode) {
    return demoSeries
  }

  const query = new URLSearchParams({ status: 'active', gap: '3' })
  const response = await fetch(`/api/series?${query}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame series API returned ${response.status}`)
  }

  return (await response.json()) as SeriesCollection
}
