import { AlertCircle, Trash2 } from 'lucide-react'
import { useEffect, useMemo, useRef, useState } from 'react'

import { clearTrash } from '../api/assets'
import { AppShell, type AppPage } from '../components/AppShell'
import { Lightbox } from '../components/Lightbox'
import { MediaCard } from '../components/MediaCard'
import { useAssets } from '../hooks/useAssets'
import type { AssetSummary } from '../types/asset'

interface TrashPageProps {
  onNavigate: (page: AppPage) => void
}

export const TrashPage = ({ onNavigate }: TrashPageProps) => {
  const { items, total, loading, loadingMore, error, hasMore, loadMore, removeItem, refresh } =
    useAssets({ status: 'trashed' })
  const [selectedId, setSelectedId] = useState<number | null>(null)
  const [showClearDialog, setShowClearDialog] = useState(false)
  const [confirmText, setConfirmText] = useState('')
  const [clearing, setClearing] = useState(false)
  const [clearError, setClearError] = useState<string | null>(null)
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

  const handleClearTrash = () => {
    if (confirmText !== 'CLEAR TRASH' || clearing) {
      return
    }
    setClearing(true)
    setClearError(null)
    void clearTrash()
      .then(() => {
        setShowClearDialog(false)
        setConfirmText('')
        refresh()
      })
      .catch((err: unknown) => {
        const message = err instanceof Error ? err.message : 'Could not empty trash'
        setClearError(message)
      })
      .finally(() => setClearing(false))
  }

  return (
    <AppShell page="trash" onNavigate={onNavigate} total={total} title="Trash" eyebrow="Deleted photos">
      {!loading && !error && total > 0 && (
        <div className="trash-toolbar">
          <p>Files stay on disk until you empty trash.</p>
          <button className="trash-clear-button" type="button" onClick={() => setShowClearDialog(true)}>
            Empty trash
          </button>
        </div>
      )}

      {loading && <p className="tools-loading">Loading trash…</p>}

      {error && (
        <section className="state-card" role="alert">
          <AlertCircle size={30} />
          <div>
            <h2>Could not load trash</h2>
            <p>{error}</p>
          </div>
        </section>
      )}

      {!loading && !error && assets.length === 0 && (
        <section className="state-card">
          <Trash2 size={30} />
          <div>
            <h2>Trash is empty</h2>
            <p>Deleted photos will appear here before permanent removal.</p>
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
          mode="trash"
          onClose={closeLightbox}
          onPrev={goPrev}
          onNext={goNext}
          onRestore={() => removeItem(selectedAsset.id)}
          onDeletePermanent={() => removeItem(selectedAsset.id)}
        />
      )}

      {showClearDialog && (
        <div className="confirm-dialog" role="dialog" aria-modal="true" aria-labelledby="clear-trash-title">
          <button
            className="confirm-dialog-backdrop"
            type="button"
            aria-label="Close"
            onClick={() => {
              if (!clearing) {
                setShowClearDialog(false)
                setConfirmText('')
                setClearError(null)
              }
            }}
          />
          <div className="confirm-dialog-card">
            <h2 id="clear-trash-title">Empty trash?</h2>
            <p>
              This permanently deletes {total} file{total === 1 ? '' : 's'} from your library folder.
              Type <strong>CLEAR TRASH</strong> to confirm.
            </p>
            <input
              className="confirm-dialog-input"
              type="text"
              value={confirmText}
              autoComplete="off"
              spellCheck={false}
              placeholder="CLEAR TRASH"
              onChange={(event) => setConfirmText(event.target.value)}
            />
            {clearError && <p className="confirm-dialog-error" role="alert">{clearError}</p>}
            <div className="confirm-dialog-actions">
              <button
                type="button"
                disabled={clearing}
                onClick={() => {
                  setShowClearDialog(false)
                  setConfirmText('')
                  setClearError(null)
                }}
              >
                Cancel
              </button>
              <button
                className="danger"
                type="button"
                disabled={clearing || confirmText !== 'CLEAR TRASH'}
                onClick={handleClearTrash}
              >
                {clearing ? 'Deleting…' : 'Delete permanently'}
              </button>
            </div>
          </div>
        </div>
      )}
    </AppShell>
  )
}
