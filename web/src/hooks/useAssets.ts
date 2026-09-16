import { useCallback, useEffect, useRef, useState } from 'react'

import { getAssets } from '../api/assets'
import type { AssetListOptions, AssetPage, AssetSummary } from '../types/asset'

const PAGE_SIZE = 50

interface AssetsState {
  items: AssetSummary[]
  total: number
  loading: boolean
  loadingMore: boolean
  error: string | null
  hasMore: boolean
  loadMore: () => void
  refresh: () => void
  updateItem: (id: number, patch: Partial<AssetSummary>) => void
  removeItem: (id: number) => void
}

export const useAssets = (options?: AssetListOptions): AssetsState => {
  const [items, setItems] = useState<AssetSummary[]>([])
  const [total, setTotal] = useState(0)
  const [loading, setLoading] = useState(true)
  const [loadingMore, setLoadingMore] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const offsetRef = useRef(0)
  const loadingMoreRef = useRef(false)
  const listStatus = options?.status
  const listFavorite = options?.favorite

  const applyPage = useCallback((page: AssetPage, append: boolean) => {
    if (!page || !Array.isArray(page.items)) {
      throw new Error('Unexpected API response shape')
    }
    setItems((prev) => (append ? [...prev, ...page.items] : page.items))
    setTotal(page.total)
    offsetRef.current = append ? offsetRef.current + page.items.length : page.items.length
  }, [])

  const loadInitial = useCallback(
    (signal?: AbortSignal) => {
      offsetRef.current = 0
      return getAssets(PAGE_SIZE, 0, signal, {
        status: listStatus,
        favorite: listFavorite,
      }).then((page) => {
        applyPage(page, false)
        setLoading(false)
        setError(null)
      })
    },
    [applyPage, listFavorite, listStatus],
  )

  useEffect(() => {
    const controller = new AbortController()

    loadInitial(controller.signal).catch((err: unknown) => {
      if (!controller.signal.aborted) {
        const message = err instanceof Error ? err.message : 'Unknown API error'
        setError(message)
        setLoading(false)
      }
    })

    return () => controller.abort()
  }, [loadInitial])

  const loadMore = useCallback(() => {
    if (loadingMoreRef.current || loading || items.length >= total) {
      return
    }

    loadingMoreRef.current = true
    setLoadingMore(true)

    getAssets(PAGE_SIZE, offsetRef.current, undefined, {
      status: listStatus,
      favorite: listFavorite,
    })
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
  }, [applyPage, items.length, listFavorite, listStatus, loading, total])

  const refresh = useCallback(() => {
    setLoading(true)
    void loadInitial().catch((err: unknown) => {
      const message = err instanceof Error ? err.message : 'Unknown API error'
      setError(message)
      setLoading(false)
    })
  }, [loadInitial])

  const updateItem = useCallback((id: number, patch: Partial<AssetSummary>) => {
    setItems((prev) => prev.map((item) => (item.id === id ? { ...item, ...patch } : item)))
  }, [])

  const removeItem = useCallback((id: number) => {
    setItems((prev) => prev.filter((item) => item.id !== id))
    setTotal((prev) => Math.max(0, prev - 1))
  }, [])

  return {
    items,
    total,
    loading,
    loadingMore,
    error,
    hasMore: items.length < total,
    loadMore,
    refresh,
    updateItem,
    removeItem,
  }
}
