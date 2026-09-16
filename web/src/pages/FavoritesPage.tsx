import { AlertCircle, Heart } from 'lucide-react'
import { useEffect, useMemo, useRef, useState } from 'react'

import { AppShell, type AppPage } from '../components/AppShell'
import { Lightbox } from '../components/Lightbox'
import { MediaCard } from '../components/MediaCard'
import { useAssets } from '../hooks/useAssets'
import type { AssetSummary } from '../types/asset'

interface FavoritesPageProps {
  onNavigate: (page: AppPage) => void
}

export const FavoritesPage = ({ onNavigate }: FavoritesPageProps) => {
  const { items, total, loading, loadingMore, error, hasMore, loadMore, updateItem, removeItem } =
    useAssets({ favorite: true })
  const [selectedId, setSelectedId] = useState<number | null>(null)
  const loadMoreRef = useRef<HTMLDivElement | null>(null)

  const assets = useMemo(() => items, [items])
  const selectedIndex = selectedId === null ? -1 : assets.findIndex((asset) => asset.id === selectedId)
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
    if (selectedIndex > 0) {
      setSelectedId(assets[selectedIndex - 1].id)
    }
  }
  const goNext = () => {
    if (selectedIndex >= 0 && selectedIndex < assets.length - 1) {
      setSelectedId(assets[selectedIndex + 1].id)
    }
  }

  return (
    <AppShell page="favorites" onNavigate={onNavigate} total={total} title="Favorites" eyebrow="Saved photos">
      {loading && <p className="tools-loading">Loading favorites…</p>}

      {error && (
        <section className="state-card" role="alert">
          <AlertCircle size={30} />
          <div>
            <h2>Could not load favorites</h2>
            <p>{error}</p>
          </div>
        </section>
      )}

      {!loading && !error && assets.length === 0 && (
        <section className="state-card">
          <Heart size={30} />
          <div>
            <h2>No favorites yet</h2>
            <p>Tap the heart in the photo viewer to save shots here.</p>
          </div>
        </section>
      )}

      {assets.length > 0 && (
        <div className="media-grid">
          {assets.map((asset) => (
            <MediaCard asset={asset} key={asset.id} onOpen={openAsset} />
          ))}
        </div>
      )}

      {hasMore && !loading && (
        <div className="load-more-sentinel" ref={loadMoreRef} aria-hidden="true">
          {loadingMore && <p className="tools-loading">Loading more…</p>}
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
          onFavoriteChange={(favorite) => {
            if (!favorite) {
              removeItem(selectedAsset.id)
              closeLightbox()
              return
            }
            updateItem(selectedAsset.id, { favorite })
          }}
          onTrash={() => removeItem(selectedAsset.id)}
        />
      )}
    </AppShell>
  )
}
