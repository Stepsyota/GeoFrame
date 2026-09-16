import { AlertCircle, Copy } from 'lucide-react'
import { useEffect, useMemo, useState } from 'react'

import { getDuplicates } from '../api/duplicates'
import { AppShell, type AppPage } from '../components/AppShell'
import { AssetStrip } from '../components/AssetStrip'
import { Lightbox } from '../components/Lightbox'
import type { AssetSummary } from '../types/asset'
import type { DuplicateGroup } from '../types/tools'

const shortHash = (sha256: string) => `${sha256.slice(0, 12)}…`

interface DuplicatesPageProps {
  onNavigate: (page: AppPage) => void
}

export const DuplicatesPage = ({ onNavigate }: DuplicatesPageProps) => {
  const [groups, setGroups] = useState<DuplicateGroup[]>([])
  const [total, setTotal] = useState(0)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [selectedId, setSelectedId] = useState<number | null>(null)

  useEffect(() => {
    const controller = new AbortController()

    void getDuplicates(controller.signal)
      .then((data) => {
        setGroups(data.groups)
        setTotal(data.total)
        setError(null)
      })
      .catch((err: unknown) => {
        if (err instanceof DOMException && err.name === 'AbortError') {
          return
        }
        const message = err instanceof Error ? err.message : 'Unknown duplicates API error'
        setError(message)
      })
      .finally(() => setLoading(false))

    return () => controller.abort()
  }, [])

  const assets = useMemo(() => groups.flatMap((group) => group.assets), [groups])
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
      page="duplicates"
      onNavigate={onNavigate}
      total={total}
      title="Duplicates"
      eyebrow="Exact matches"
    >
      {loading && <p className="tools-loading">Scanning for duplicate hashes…</p>}

      {error && (
        <section className="state-card" role="alert">
          <AlertCircle size={30} />
          <div>
            <h2>Could not load duplicates</h2>
            <p>{error}</p>
          </div>
        </section>
      )}

      {!loading && !error && groups.length === 0 && (
        <section className="state-card">
          <Copy size={30} />
          <div>
            <h2>No exact duplicates found</h2>
            <p>Photos with identical SHA-256 hashes will appear here.</p>
          </div>
        </section>
      )}

      {!loading &&
        !error &&
        groups.map((group) => (
          <section className="tool-group-card" key={group.sha256}>
            <div className="tool-group-heading">
              <div>
                <h2>{group.assets.length} identical files</h2>
                <p className="tool-group-meta">SHA-256 {shortHash(group.sha256)}</p>
              </div>
              <span className="tool-group-count">{group.assets.length}</span>
            </div>
            <AssetStrip assets={group.assets} onOpen={openAsset} />
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
