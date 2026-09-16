import { AlertCircle, GalleryHorizontalEnd } from 'lucide-react'
import { useEffect, useMemo, useState } from 'react'

import { getSeries } from '../api/series'
import { AppShell, type AppPage } from '../components/AppShell'
import { AssetStrip } from '../components/AssetStrip'
import { Lightbox } from '../components/Lightbox'
import type { AssetSummary } from '../types/asset'
import type { PhotoSeries } from '../types/tools'

const seriesTitle = (entry: PhotoSeries) => {
  const first = entry.assets[0]?.capturedAt
  const last = entry.assets[entry.assets.length - 1]?.capturedAt
  if (!first || !last) {
    return `Series ${entry.id}`
  }
  if (first === last) {
    return first.replace('T', ' ')
  }
  return `${first.replace('T', ' ')} → ${last.replace('T', ' ')}`
}

interface SeriesPageProps {
  onNavigate: (page: AppPage) => void
}

export const SeriesPage = ({ onNavigate }: SeriesPageProps) => {
  const [series, setSeries] = useState<PhotoSeries[]>([])
  const [total, setTotal] = useState(0)
  const [gapSeconds, setGapSeconds] = useState(3)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [selectedId, setSelectedId] = useState<number | null>(null)

  useEffect(() => {
    const controller = new AbortController()

    void getSeries(controller.signal)
      .then((data) => {
        setSeries(data.series)
        setTotal(data.total)
        setGapSeconds(data.gapSeconds)
        setError(null)
      })
      .catch((err: unknown) => {
        if (err instanceof DOMException && err.name === 'AbortError') {
          return
        }
        const message = err instanceof Error ? err.message : 'Unknown series API error'
        setError(message)
      })
      .finally(() => setLoading(false))

    return () => controller.abort()
  }, [])

  const assets = useMemo(() => series.flatMap((entry) => entry.assets), [series])
  const selectedIndex = selectedId === null ? -1 : assets.findIndex((asset) => asset.id === selectedId)
  const selectedAsset = selectedIndex >= 0 ? assets[selectedIndex] : null

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
    <AppShell
      page="series"
      onNavigate={onNavigate}
      total={total}
      title="Series"
      eyebrow={`Burst groups · ${gapSeconds}s gap`}
    >
      {loading && <p className="tools-loading">Looking for burst sequences…</p>}

      {error && (
        <section className="state-card" role="alert">
          <AlertCircle size={30} />
          <div>
            <h2>Could not load series</h2>
            <p>{error}</p>
          </div>
        </section>
      )}

      {!loading && !error && series.length === 0 && (
        <section className="state-card">
          <GalleryHorizontalEnd size={30} />
          <div>
            <h2>No photo series found</h2>
            <p>Burst shots taken within {gapSeconds} seconds of each other will appear here.</p>
          </div>
        </section>
      )}

      {!loading &&
        !error &&
        series.map((entry) => (
          <section className="tool-group-card" key={entry.id}>
            <div className="tool-group-heading">
              <div>
                <h2>{seriesTitle(entry)}</h2>
                <p className="tool-group-meta">{entry.assets.length} photos in sequence</p>
              </div>
              <span className="tool-group-count">{entry.assets.length}</span>
            </div>
            <AssetStrip assets={entry.assets} onOpen={openAsset} />
          </section>
        ))}

      {selectedAsset && (
        <Lightbox
          asset={selectedAsset}
          hasPrev={selectedIndex > 0}
          hasNext={selectedIndex >= 0 && selectedIndex < assets.length - 1}
          onClose={closeLightbox}
          onPrev={goPrev}
          onNext={goNext}
        />
      )}
    </AppShell>
  )
}
