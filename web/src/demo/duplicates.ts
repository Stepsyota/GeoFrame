import { demoAssetPage } from './assets'
import type { DuplicateCollection } from '../types/tools'

const asset = (id: number) => {
  const item = demoAssetPage.items.find((entry) => entry.id === id)
  if (!item) {
    throw new Error(`Demo asset ${id} not found`)
  }
  return item
}

export const demoDuplicates: DuplicateCollection = {
  groups: [
    {
      sha256: 'a1b2c3d4e5f6789012345678901234567890abcd1234567890abcd12345678',
      assetIds: [4, 5],
      assets: [asset(4), asset(5)],
    },
    {
      sha256: 'fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321',
      assetIds: [1, 12],
      assets: [asset(1), asset(12)],
    },
  ],
  total: 2,
}
