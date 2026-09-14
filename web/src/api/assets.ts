import { demoAssetPage } from '../demo/assets'
import type { AssetPage } from '../types/asset'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

export const getAssets = async (
  limit = 50,
  offset = 0,
  signal?: AbortSignal,
): Promise<AssetPage> => {
  if (demoMode) {
    return demoAssetPage
  }

  const query = new URLSearchParams({
    limit: String(limit),
    offset: String(offset),
    status: 'active',
  })
  const response = await fetch(`/api/assets?${query}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame API returned ${response.status}`)
  }
  return (await response.json()) as AssetPage
}
