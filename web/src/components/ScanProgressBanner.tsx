import { LoaderCircle } from 'lucide-react'

import type { ScanProgressEvent } from '../types/progress'

interface ScanProgressBannerProps {
  progress: ScanProgressEvent
}

const stageLabel = (progress: ScanProgressEvent): string => {
  if (progress.scanning) {
    return 'Scanning library'
  }
  if (progress.hashPercent < 100) {
    return 'Hashing files'
  }
  if (progress.metadataPercent < 100) {
    return 'Reading metadata'
  }
  if (progress.thumbnailsPercent < 100) {
    return 'Generating thumbnails'
  }
  if (progress.previewsPercent < 100) {
    return 'Generating previews'
  }
  return 'Processing library'
}

export const ScanProgressBanner = ({ progress }: ScanProgressBannerProps) => {
  const label = stageLabel(progress)
  const overall = Math.round(
    (progress.hashPercent + progress.metadataPercent + progress.thumbnailsPercent
      + progress.previewsPercent) / 4,
  )

  return (
    <section className="scan-progress" aria-live="polite">
      <div className="scan-progress-header">
        <LoaderCircle className="scan-progress-spinner" size={18} />
        <div>
          <p className="scan-progress-title">{label}</p>
          {progress.currentFile && (
            <p className="scan-progress-file">{progress.currentFile}</p>
          )}
        </div>
        <span className="scan-progress-percent">{overall}%</span>
      </div>

      <div className="scan-progress-track" aria-hidden="true">
        <span className="scan-progress-fill" style={{ width: `${overall}%` }} />
      </div>

      <div className="scan-progress-stats">
        <span>Files {progress.filesDone}</span>
        <span>Metadata {progress.metadataPercent}%</span>
        <span>Thumbnails {progress.thumbnailsPercent}%</span>
      </div>
    </section>
  )
}
