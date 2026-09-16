import { demoAssetPage } from './assets'
import type { SeriesCollection } from '../types/tools'

const asset = (id: number) => {
  const item = demoAssetPage.items.find((entry) => entry.id === id)
  if (!item) {
    throw new Error(`Demo asset ${id} not found`)
  }
  return item
}

export const demoSeries: SeriesCollection = {
  series: [
    {
      id: 1,
      assetIds: [2, 3],
      assets: [asset(2), asset(3)],
    },
    {
      id: 2,
      assetIds: [6, 7, 8],
      assets: [asset(6), asset(7), asset(8)],
    },
  ],
  total: 2,
  gapSeconds: 3,
}
