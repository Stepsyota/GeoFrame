import { demoAssetPage } from '../demo/assets'
import type { AssetDetail, AssetListOptions, AssetPage } from '../types/asset'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

const filterDemoPage = (options?: AssetListOptions): AssetPage => {
  let items = demoAssetPage.items
  if (options?.status) {
    items = items.filter((item) => item.status === options.status)
  }
  if (options?.favorite === true) {
    items = items.filter((item) => item.favorite)
  } else if (options?.favorite === false) {
    items = items.filter((item) => !item.favorite)
  }
  return {
    items,
    total: items.length,
    limit: demoAssetPage.limit,
    offset: 0,
  }
}

export const getAssets = async (
  limit = 50,
  offset = 0,
  signal?: AbortSignal,
  options?: AssetListOptions,
): Promise<AssetPage> => {
  if (demoMode) {
    return filterDemoPage(options)
  }

  const query = new URLSearchParams({
    limit: String(limit),
    offset: String(offset),
    status: options?.status ?? 'active',
  })
  if (options?.favorite === true) {
    query.set('favorite', 'true')
  } else if (options?.favorite === false) {
    query.set('favorite', 'false')
  }

  const response = await fetch(`/api/assets?${query}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame API returned ${response.status}`)
  }
  return (await response.json()) as AssetPage
}

export const getAssetById = async (id: number, signal?: AbortSignal): Promise<AssetDetail> => {
  if (demoMode) {
    const asset = demoAssetPage.items.find((item) => item.id === id)
    if (!asset) {
      throw new Error(`Demo asset ${id} not found`)
    }
    return {
      ...asset,
      camera: asset.id % 2 === 0 ? 'Apple iPhone 15 Pro' : 'Canon EOS R6',
      gps: asset.id % 3 === 0 ? { lat: 53.9, lon: 27.56, alt: 220 } : null,
      sizeBytes: 4_800_000,
      sha256: null,
      durationSeconds: asset.mediaType === 'video' ? 12.4 : null,
      videoCodec: asset.mediaType === 'video' ? 'h264' : null,
    }
  }

  const response = await fetch(`/api/assets/${id}`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame API returned ${response.status}`)
  }

  return (await response.json()) as AssetDetail
}

export const setAssetFavorite = async (id: number, favorite: boolean): Promise<boolean> => {
  if (demoMode) {
    const asset = demoAssetPage.items.find((item) => item.id === id)
    if (!asset) {
      throw new Error(`Demo asset ${id} not found`)
    }
    asset.favorite = favorite
    return favorite
  }

  const response = await fetch(`/api/assets/${id}/favorite`, {
    method: 'POST',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ favorite }),
  })

  if (!response.ok) {
    throw new Error(`GeoFrame API returned ${response.status}`)
  }

  const body = (await response.json()) as { favorite: boolean }
  return body.favorite
}

export const trashAsset = async (id: number): Promise<void> => {
  if (demoMode) {
    const asset = demoAssetPage.items.find((item) => item.id === id)
    if (!asset) {
      throw new Error(`Demo asset ${id} not found`)
    }
    asset.status = 'trashed'
    return
  }

  const response = await fetch(`/api/assets/${id}/trash`, {
    method: 'POST',
    headers: { Accept: 'application/json' },
  })

  if (!response.ok) {
    throw new Error(`GeoFrame API returned ${response.status}`)
  }
}
