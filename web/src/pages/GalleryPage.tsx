import { AlertCircle, ImageOff } from 'lucide-react'

import { AppShell } from '../components/AppShell'
import { MediaCard } from '../components/MediaCard'
import { useAssets } from '../hooks/useAssets'
import type { AssetSummary } from '../types/asset'

/** Returns a sortable "YYYY-MM-DD" string (or "unknown") for grouping. */
const dayKey = (asset: AssetSummary): string => {
  const ca = asset.capturedAt
  if (!ca || typeof ca !== 'string') return 'unknown'
  // Normalise EXIF "YYYY:MM:DD ..." → "YYYY-MM-DD"
  return ca.slice(0, 10).replace(/:/g, '-')
}

const dayTitle = (key: unknown): string => {
  if (typeof key !== 'string' || key === 'unknown') return 'Date unknown'
  const date = new Date(`${key}T12:00:00`)
  if (isNaN(date.getTime())) return key // unparsable → show raw
  return new Intl.DateTimeFormat('en', {
    weekday: 'long',
    day: 'numeric',
    month: 'long',
    year: 'numeric',
  }).format(date)
}

const groupByDay = (assets: AssetSummary[] | undefined): Map<string, AssetSummary[]> => {
  const groups = new Map<string, AssetSummary[]>()
  if (!Array.isArray(assets)) return groups
  assets.forEach((asset) => {
    const key = dayKey(asset)
    const group = groups.get(key)
    if (group) {
      group.push(asset)
    } else {
      groups.set(key, [asset])
    }
  })
  return groups
}

const GallerySkeleton = () => (
  <div className="gallery-skeleton" aria-label="Loading photos">
    {Array.from({ length: 10 }, (_, index) => (
      <span key={index} />
    ))}
  </div>
)

export const GalleryPage = () => {
  const { data, loading, error } = useAssets()

  return (
    <AppShell total={data?.total}>
      {loading && <GallerySkeleton />}

      {error && (
        <section className="state-card" role="alert">
          <AlertCircle size={30} />
          <div>
            <h2>Could not load your library</h2>
            <p>{error}</p>
            <p className="state-hint">
              Make sure GeoFrame backend is running, or use{' '}
              <code>npm run dev:demo</code> for demo mode.
            </p>
          </div>
        </section>
      )}

      {!loading && !error && data && (data.items ?? []).length === 0 && (
        <section className="state-card">
          <ImageOff size={30} />
          <div>
            <h2>Your library is empty</h2>
            <p>Run a scan to start indexing your photos.</p>
          </div>
        </section>
      )}

      {data &&
        Array.from(groupByDay(data.items)).map(([day, assets]) => (
          <section className="day-section" key={day}>
            <div className="day-heading">
              <h2>{dayTitle(day)}</h2>
              <span>{assets.length}</span>
            </div>
            <div className="media-grid">
              {assets.map((asset) => (
                <MediaCard asset={asset} key={asset.id} />
              ))}
            </div>
          </section>
        ))}
    </AppShell>
  )
}
