import { AlertCircle, ImageOff } from 'lucide-react'
import { useEffect, useMemo, useRef, useState } from 'react'

import { AppShell, type AppPage } from '../components/AppShell'
import { Lightbox } from '../components/Lightbox'
import { MediaCard } from '../components/MediaCard'
import { ScanProgressBanner } from '../components/ScanProgressBanner'
import { useAssets } from '../hooks/useAssets'
import { useScanProgress } from '../hooks/useScanProgress'
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

interface GalleryPageProps {
  onNavigate: (page: AppPage) => void
}

export const GalleryPage = ({ onNavigate }: GalleryPageProps) => {
  const { items, total, loading, loadingMore, error, hasMore, loadMore, updateItem, removeItem } =
    useAssets()
  const { progress, active: progressActive } = useScanProgress()
  const [selectedId, setSelectedId] = useState<number | null>(null)
  const loadMoreRef = useRef<HTMLDivElement | null>(null)

  const assets = useMemo(() => items, [items])
  const selectedIndex = selectedId === null ? -1 : assets.findIndex((a) => a.id === selectedId)
  const selectedAsset = selectedIndex >= 0 ? assets[selectedIndex] : null

  useEffect(() => {
    if (!hasMore || loading || loadingMore) {
      return
    }
    const node = loadMoreRef.current
    if (!node) {
      return
    }

    const observer = new IntersectionObserver(
      (entries) => {
        if (entries.some((entry) => entry.isIntersecting)) {
          loadMore()
        }
      },
      { rootMargin: '400px' },
    )
    observer.observe(node)
    return () => observer.disconnect()
  }, [hasMore, loadMore, loading, loadingMore])

  const openAsset = (asset: AssetSummary) => setSelectedId(asset.id)
  const closeLightbox = () => setSelectedId(null)
  const goPrev = () => {
    if (selectedIndex > 0) setSelectedId(assets[selectedIndex - 1].id)
  }
  const goNext = () => {
    if (selectedIndex >= 0 && selectedIndex < assets.length - 1) {
      setSelectedId(assets[selectedIndex + 1].id)
    }
  }

  return (
    <AppShell
      page="photos"
      onNavigate={onNavigate}
      total={total}
      banner={progressActive && progress ? <ScanProgressBanner progress={progress} /> : null}
    >
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

      {!loading && !error && assets.length === 0 && (
        <section className="state-card">
          <ImageOff size={30} />
          <div>
            <h2>Your library is empty</h2>
            <p>Run a scan to start indexing your photos.</p>
          </div>
        </section>
      )}

      {assets.length > 0 &&
        Array.from(groupByDay(assets)).map(([day, dayAssets]) => (
          <section className="day-section" key={day}>
            <div className="day-heading">
              <h2>{dayTitle(day)}</h2>
              <span>{dayAssets.length}</span>
            </div>
            <div className="media-grid">
              {dayAssets.map((asset) => (
                <MediaCard asset={asset} key={asset.id} onOpen={openAsset} />
              ))}
            </div>
          </section>
        ))}

      {hasMore && !loading && (
        <div className="load-more-sentinel" ref={loadMoreRef} aria-hidden="true">
          {loadingMore && <GallerySkeleton />}
        </div>
      )}

      {selectedAsset && (
        <Lightbox
          asset={selectedAsset}
          hasPrev={selectedIndex > 0}
          hasNext={selectedIndex >= 0 && selectedIndex < assets.length - 1}
          onClose={closeLightbox}
          onPrev={goPrev}
          onNext={goNext}
          onFavoriteChange={(favorite) => updateItem(selectedAsset.id, { favorite })}
          onTrash={() => removeItem(selectedAsset.id)}
        />
      )}
    </AppShell>
  )
}
