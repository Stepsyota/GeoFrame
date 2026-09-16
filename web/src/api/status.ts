import type { LibraryStatus } from '../types/status'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

const demoStatus: LibraryStatus = {
  source: '/demo/photos',
  dataDir: '~/.local/share/geoframe',
  scanning: false,
  assets: { active: 42, trashed: 3 },
  cache: {
    thumbnailsDir: '~/.local/share/geoframe/cache/thumbnails',
    previewsDir: '~/.local/share/geoframe/cache/previews',
    thumbnailsMB: 120,
    previewsMB: 840,
    totalMB: 960,
  },
  disk: { availableMB: 128_000, totalMB: 512_000 },
}

export const getLibraryStatus = async (signal?: AbortSignal): Promise<LibraryStatus> => {
  if (demoMode) {
    return demoStatus
  }

  const response = await fetch('/api/status', {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame API returned ${response.status}`)
  }

  return (await response.json()) as LibraryStatus
}

export const triggerLibraryScan = async (source?: string): Promise<{ status: string; source: string }> => {
  if (demoMode) {
    return { status: 'accepted', source: source ?? '/demo/photos' }
  }

  const response = await fetch('/api/scan', {
    method: 'POST',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json',
    },
    body: source ? JSON.stringify({ source }) : undefined,
  })

  if (!response.ok) {
    const payload = (await response.json().catch(() => null)) as { error?: string } | null
    throw new Error(payload?.error ?? `GeoFrame API returned ${response.status}`)
  }

  return (await response.json()) as { status: string; source: string }
}
