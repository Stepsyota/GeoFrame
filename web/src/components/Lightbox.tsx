import { ChevronLeft, ChevronRight, X } from 'lucide-react'
import { useEffect } from 'react'

import type { AssetSummary } from '../types/asset'

interface LightboxProps {
  asset: AssetSummary
  hasPrev: boolean
  hasNext: boolean
  onClose: () => void
  onPrev: () => void
  onNext: () => void
}

const imageSrc = (asset: AssetSummary) => asset.previewUrl ?? asset.thumbnailUrl

export const Lightbox = ({
  asset,
  hasPrev,
  hasNext,
  onClose,
  onPrev,
  onNext,
}: LightboxProps) => {
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

  return (
    <div className="lightbox" role="dialog" aria-modal="true" aria-label={asset.originalFilename}>
      <button className="lightbox-backdrop" type="button" aria-label="Close" onClick={onClose} />

      <header className="lightbox-toolbar">
        <p className="lightbox-title">{asset.originalFilename}</p>
        <button className="lightbox-close" type="button" aria-label="Close" onClick={onClose}>
          <X size={22} />
        </button>
      </header>

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
    </div>
  )
}
