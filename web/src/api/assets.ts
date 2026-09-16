import { demoAssetPage } from '../demo/assets'
import type { AssetPage, AssetSummary } from '../types/asset'

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

export const getAssetById = async (id: number, signal?: AbortSignal): Promise<AssetSummary> => {
  if (demoMode) {
    const asset = demoAssetPage.items.find((item) => item.id === id)
    if (!asset) {
      throw new Error(`Demo asset ${id} not found`)
    }
    return asset
  }

  const response = await fetch(`/api/assets/${id}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame API returned ${response.status}`)
  }

  return (await response.json()) as AssetSummary
}
