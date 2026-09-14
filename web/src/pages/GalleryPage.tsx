import { AlertCircle, ImageOff } from 'lucide-react'

import { AppShell } from '../components/AppShell'
import { MediaCard } from '../components/MediaCard'
import { useAssets } from '../hooks/useAssets'
import type { AssetSummary } from '../types/asset'

const dayKey = (asset: AssetSummary) => asset.capturedAt?.slice(0, 10) ?? 'unknown'

const dayTitle = (key: string) => {
  if (key === 'unknown') {
    return 'Date unknown'
  }
  return new Intl.DateTimeFormat('en', {
    weekday: 'long',
    day: 'numeric',
    month: 'long',
    year: 'numeric',
  }).format(new Date(`${key}T12:00:00`))
}

const groupByDay = (assets: AssetSummary[]) => {
  const groups = new Map<string, AssetSummary[]>()
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
              Start GeoFrame backend or run <code>npm run dev:demo</code>.
            </p>
          </div>
        </section>
      )}

      {data?.items.length === 0 && (
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
