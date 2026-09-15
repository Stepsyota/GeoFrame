import { useEffect, useState } from 'react'

import { getAssets } from '../api/assets'
import type { AssetPage } from '../types/asset'

interface AssetsState {
  data: AssetPage | null
  loading: boolean
  error: string | null
}

export const useAssets = (): AssetsState => {
  const [state, setState] = useState<AssetsState>({
    data: null,
    loading: true,
    error: null,
  })

  useEffect(() => {
    const controller = new AbortController()

    // Reset to loading on every (re-)mount, including StrictMode's double-invoke
    setState({ data: null, loading: true, error: null })

    getAssets(50, 0, controller.signal)
      .then((page) => {
        // Validate that we got a proper page object before setting state
        if (!page || !Array.isArray(page.items)) {
          throw new Error('Unexpected API response shape')
        }
        setState({ data: page, loading: false, error: null })
      })
      .catch((error: unknown) => {
        if (!controller.signal.aborted) {
          const message = error instanceof Error ? error.message : 'Unknown API error'
          setState({ data: null, loading: false, error: message })
        }
      })

    return () => controller.abort()
  }, [])

  return state
}
