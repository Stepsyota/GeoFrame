import { useCallback, useEffect, useRef, useState } from 'react'

import { getAssets } from '../api/assets'
import type { AssetPage, AssetSummary } from '../types/asset'

const PAGE_SIZE = 50

interface AssetsState {
  items: AssetSummary[]
  total: number
  loading: boolean
  loadingMore: boolean
  error: string | null
  hasMore: boolean
  loadMore: () => void
}

export const useAssets = (): AssetsState => {
  const [items, setItems] = useState<AssetSummary[]>([])
  const [total, setTotal] = useState(0)
  const [loading, setLoading] = useState(true)
  const [loadingMore, setLoadingMore] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const offsetRef = useRef(0)
  const loadingMoreRef = useRef(false)

  const applyPage = useCallback((page: AssetPage, append: boolean) => {
    if (!page || !Array.isArray(page.items)) {
      throw new Error('Unexpected API response shape')
    }
    setItems((prev) => (append ? [...prev, ...page.items] : page.items))
    setTotal(page.total)
    offsetRef.current = append ? offsetRef.current + page.items.length : page.items.length
  }, [])

  useEffect(() => {
    const controller = new AbortController()
    offsetRef.current = 0

    getAssets(PAGE_SIZE, 0, controller.signal)
      .then((page) => {
        applyPage(page, false)
        setLoading(false)
        setError(null)
      })
      .catch((err: unknown) => {
        if (!controller.signal.aborted) {
          const message = err instanceof Error ? err.message : 'Unknown API error'
          setError(message)
          setLoading(false)
        }
      })

    return () => controller.abort()
  }, [applyPage])

  const loadMore = useCallback(() => {
    if (loadingMoreRef.current || loading || items.length >= total) {
      return
    }

    loadingMoreRef.current = true
    setLoadingMore(true)

    getAssets(PAGE_SIZE, offsetRef.current)
      .then((page) => {
        applyPage(page, true)
      })
      .catch((err: unknown) => {
        const message = err instanceof Error ? err.message : 'Unknown API error'
        setError(message)
      })
      .finally(() => {
        loadingMoreRef.current = false
        setLoadingMore(false)
      })
  }, [applyPage, items.length, loading, total])

  return {
    items,
    total,
    loading,
    loadingMore,
    error,
    hasMore: items.length < total,
    loadMore,
  }
}
