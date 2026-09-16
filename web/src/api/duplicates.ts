import { demoDuplicates } from '../demo/duplicates'
import type { DuplicateCollection } from '../types/tools'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

export const getDuplicates = async (signal?: AbortSignal): Promise<DuplicateCollection> => {
  if (demoMode) {
    return demoDuplicates
  }

  const query = new URLSearchParams({ status: 'active' })
  const response = await fetch(`/api/duplicates?${query}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame duplicates API returned ${response.status}`)
  }

  return (await response.json()) as DuplicateCollection
}
