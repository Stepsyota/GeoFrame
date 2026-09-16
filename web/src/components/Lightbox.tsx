import { ChevronLeft, ChevronRight, Download, Heart, RotateCcw, Trash2, X } from 'lucide-react'
import { useEffect, useState } from 'react'

import {
  deleteAssetPermanently,
  getAssetById,
  restoreAsset,
  setAssetFavorite,
  trashAsset,
} from '../api/assets'
import { getAssetExif } from '../api/exif'
import type { AssetDetail, AssetSummary } from '../types/asset'
import type { ExifTag } from '../types/exif'

interface LightboxProps {
  asset: AssetSummary
  hasPrev: boolean
  hasNext: boolean
  onClose: () => void
  onPrev: () => void
  onNext: () => void
  onFavoriteChange?: (favorite: boolean) => void
  onTrash?: () => void
  mode?: 'library' | 'trash'
  onRestore?: () => void
  onDeletePermanent?: () => void
}

const imageSrc = (asset: AssetSummary) => asset.previewUrl ?? asset.thumbnailUrl

const formatBytes = (bytes: number) => {
  if (bytes < 1024) {
    return `${bytes} B`
  }
  if (bytes < 1024 * 1024) {
    return `${(bytes / 1024).toFixed(1)} KB`
  }
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
}

const formatCapturedAt = (value: string | null) => {
  if (!value) {
    return 'Unknown'
  }
  const date = new Date(value)
  if (Number.isNaN(date.getTime())) {
    return value
  }
  return new Intl.DateTimeFormat('en', {
    day: 'numeric',
    month: 'long',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  }).format(date)
}

export const Lightbox = ({
  asset,
  hasPrev,
  hasNext,
  onClose,
  onPrev,
  onNext,
  onFavoriteChange,
  onTrash,
  mode = 'library',
  onRestore,
  onDeletePermanent,
}: LightboxProps) => {
  const [detail, setDetail] = useState<AssetDetail | null>(null)
  const [exifTags, setExifTags] = useState<ExifTag[]>([])
  const [exifError, setExifError] = useState<string | null>(null)
  const [pendingFavorite, setPendingFavorite] = useState<boolean | null>(null)
  const [busy, setBusy] = useState(false)
  const favorite = pendingFavorite ?? asset.favorite

  useEffect(() => {
    const controller = new AbortController()

    void getAssetById(asset.id, controller.signal)
      .then((loaded) => setDetail(loaded))
      .catch(() => setDetail(null))

    return () => controller.abort()
  }, [asset.id])

  useEffect(() => {
    if (asset.mediaType !== 'image') {
      return
    }

    const controller = new AbortController()

    void getAssetExif(asset.id, controller.signal)
      .then((payload) => {
        setExifTags(payload.tags)
        setExifError(null)
      })
      .catch((err: unknown) => {
        if (err instanceof DOMException && err.name === 'AbortError') {
          return
        }
        const message = err instanceof Error ? err.message : 'Could not load EXIF'
        setExifTags([])
        setExifError(message)
      })

    return () => controller.abort()
  }, [asset.id, asset.mediaType])

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose()
      if (event.key === 'ArrowLeft' && hasPrev) onPrev()
      if (event.key === 'ArrowRight' && hasNext) onNext()
    }
    document.addEventListener('keydown', onKey)
    document.body.style.overflow = 'hidden'
    return () => {
      document.removeEventListener('keydown', onKey)
      document.body.style.overflow = ''
    }
  }, [hasNext, hasPrev, onClose, onNext, onPrev])

  const src = imageSrc(asset)
  const info = detail ?? asset

  const toggleFavorite = () => {
    if (busy) {
      return
    }
    setBusy(true)
    const next = !favorite
    setPendingFavorite(next)
    void setAssetFavorite(asset.id, next)
      .then((value) => {
        setPendingFavorite(null)
        onFavoriteChange?.(value)
      })
      .catch(() => setPendingFavorite(null))
      .finally(() => setBusy(false))
  }

  const moveToTrash = () => {
    if (busy) {
      return
    }
    setBusy(true)
    void trashAsset(asset.id)
      .then(() => {
        onTrash?.()
        onClose()
      })
      .finally(() => setBusy(false))
  }

  const restoreFromTrash = () => {
    if (busy) {
      return
    }
    setBusy(true)
    void restoreAsset(asset.id)
      .then(() => {
        onRestore?.()
        onClose()
      })
      .finally(() => setBusy(false))
  }

  const deletePermanent = () => {
    if (busy || !window.confirm('Delete this file permanently? This cannot be undone.')) {
      return
    }
    setBusy(true)
    void deleteAssetPermanently(asset.id)
      .then(() => {
        onDeletePermanent?.()
        onClose()
      })
      .finally(() => setBusy(false))
  }

  return (
    <div className="lightbox" role="dialog" aria-modal="true" aria-label={asset.originalFilename}>
      <button className="lightbox-backdrop" type="button" aria-label="Close" onClick={onClose} />

      <header className="lightbox-toolbar">
        <p className="lightbox-title">{asset.originalFilename}</p>
        <div className="lightbox-actions">
          {mode === 'library' && (
            <button
              className={`lightbox-action ${favorite ? 'active' : ''}`}
              type="button"
              aria-label={favorite ? 'Remove from favorites' : 'Add to favorites'}
              disabled={busy}
              onClick={toggleFavorite}
            >
              <Heart size={20} fill={favorite ? 'currentColor' : 'none'} />
            </button>
          )}
          <a
            className="lightbox-action"
            href={`/api/assets/${asset.id}/original`}
            download={asset.originalFilename}
            aria-label="Download original"
          >
            <Download size={20} />
          </a>
          {mode === 'trash' ? (
            <>
              <button
                className="lightbox-action"
                type="button"
                aria-label="Restore photo"
                disabled={busy}
                onClick={restoreFromTrash}
              >
                <RotateCcw size={20} />
              </button>
              <button
                className="lightbox-action danger"
                type="button"
                aria-label="Delete permanently"
                disabled={busy}
                onClick={deletePermanent}
              >
                <Trash2 size={20} />
              </button>
            </>
          ) : (
            <button
              className="lightbox-action danger"
              type="button"
              aria-label="Move to trash"
              disabled={busy}
              onClick={moveToTrash}
            >
              <Trash2 size={20} />
            </button>
          )}
          <button className="lightbox-close" type="button" aria-label="Close" onClick={onClose}>
            <X size={22} />
          </button>
        </div>
      </header>

      <div className="lightbox-body">
        <div className="lightbox-stage">
          {hasPrev && (
            <button className="lightbox-nav lightbox-nav-prev" type="button" aria-label="Previous" onClick={onPrev}>
              <ChevronLeft size={28} />
            </button>
          )}

          {asset.mediaType === 'video' ? (
            <video
              className="lightbox-media"
              controls
              playsInline
              poster={asset.thumbnailUrl ?? undefined}
              src={`/api/assets/${asset.id}/original`}
            />
          ) : src ? (
            <img className="lightbox-media" src={src} alt={asset.originalFilename} />
          ) : (
            <p className="lightbox-fallback">Preview not available</p>
          )}

          {hasNext && (
            <button className="lightbox-nav lightbox-nav-next" type="button" aria-label="Next" onClick={onNext}>
              <ChevronRight size={28} />
            </button>
          )}
        </div>

        <aside className="lightbox-panel">
          <h2>Details</h2>
          <dl className="lightbox-meta">
            <div>
              <dt>Captured</dt>
              <dd>{formatCapturedAt(info.capturedAt)}</dd>
            </div>
            {detail?.camera && (
              <div>
                <dt>Camera</dt>
                <dd>{detail.camera}</dd>
              </div>
            )}
            {info.width && info.height && (
              <div>
                <dt>Dimensions</dt>
                <dd>{info.width} × {info.height}</dd>
              </div>
            )}
            {detail?.sizeBytes && (
              <div>
                <dt>File size</dt>
                <dd>{formatBytes(detail.sizeBytes)}</dd>
              </div>
            )}
            {detail?.gps && (
              <div>
                <dt>Location</dt>
                <dd>
                  {detail.gps.lat.toFixed(5)}, {detail.gps.lon.toFixed(5)}
                  {detail.gps.alt !== null ? ` · ${detail.gps.alt.toFixed(0)} m` : ''}
                </dd>
              </div>
            )}
            {detail?.sha256 && (
              <div>
                <dt>SHA-256</dt>
                <dd className="lightbox-hash">{detail.sha256.slice(0, 16)}…</dd>
              </div>
            )}
          </dl>

          {asset.mediaType === 'image' && (
            <section className="lightbox-exif">
              <h3>EXIF</h3>
              {exifError && <p className="lightbox-exif-error">{exifError}</p>}
              {!exifError && exifTags.length === 0 && (
                <p className="lightbox-exif-empty">No EXIF tags found.</p>
              )}
              {exifTags.length > 0 && (
                <dl className="lightbox-exif-list">
                  {exifTags.map((tag) => (
                    <div key={tag.key}>
                      <dt>{tag.key}</dt>
                      <dd>{tag.value}</dd>
                    </div>
                  ))}
                </dl>
              )}
            </section>
          )}
        </aside>
      </div>
    </div>
  )
}
