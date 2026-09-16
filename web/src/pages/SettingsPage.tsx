import { AlertCircle, FolderOpen, HardDrive, RefreshCw } from 'lucide-react'
import { useEffect, useState } from 'react'

import { getLibraryStatus, triggerLibraryScan } from '../api/status'
import { AppShell, type AppPage } from '../components/AppShell'
import { ScanProgressBanner } from '../components/ScanProgressBanner'
import { useScanProgress } from '../hooks/useScanProgress'
import type { LibraryStatus } from '../types/status'

const formatMegabytes = (value: number): string => {
  if (value >= 1024) {
    return `${(value / 1024).toFixed(1)} GB`
  }
  return `${value} MB`
}

interface SettingsPageProps {
  onNavigate: (page: AppPage) => void
}

export const SettingsPage = ({ onNavigate }: SettingsPageProps) => {
  const [status, setStatus] = useState<LibraryStatus | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [scanError, setScanError] = useState<string | null>(null)
  const [scanning, setScanning] = useState(false)
  const { progress, active: progressActive } = useScanProgress()

  useEffect(() => {
    const controller = new AbortController()

    void getLibraryStatus(controller.signal)
      .then((data) => {
        setStatus(data)
        setError(null)
      })
      .catch((err: unknown) => {
        if (err instanceof DOMException && err.name === 'AbortError') {
          return
        }
        const message = err instanceof Error ? err.message : 'Unknown status API error'
        setError(message)
      })
      .finally(() => setLoading(false))

    return () => controller.abort()
  }, [])

  useEffect(() => {
    if (!status?.scanning && !scanning) {
      return
    }
    const controller = new AbortController()
    const timer = window.setInterval(() => {
      void getLibraryStatus(controller.signal)
        .then((data) => setStatus(data))
        .catch(() => {
          // Keep the last known status on refresh errors.
        })
    }, 1000)

    return () => {
      controller.abort()
      window.clearInterval(timer)
    }
  }, [status?.scanning, scanning])

  const handleRescan = async () => {
    setScanError(null)
    setScanning(true)
    try {
      await triggerLibraryScan()
      const data = await getLibraryStatus()
      setStatus(data)
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Could not start scan'
      setScanError(message)
    } finally {
      setScanning(false)
    }
  }

  const scanBusy = scanning || status?.scanning === true
  const canRescan = Boolean(status?.source) && !scanBusy

  return (
    <AppShell page="settings" onNavigate={onNavigate} title="Settings" eyebrow="Library">
      {loading && <p className="tools-loading">Loading settings…</p>}

      {error && (
        <section className="state-card" role="alert">
          <AlertCircle size={30} />
          <div>
            <h2>Could not load settings</h2>
            <p>{error}</p>
          </div>
        </section>
      )}

      {!loading && !error && status && (
        <div className="settings-page">
          {progress && (progressActive || status.scanning) && <ScanProgressBanner progress={progress} />}

          <section className="settings-card">
            <div className="settings-card-header">
              <FolderOpen size={22} />
              <h2>Library paths</h2>
            </div>
            <dl className="settings-list">
              <div>
                <dt>Source folder</dt>
                <dd>{status.source ?? 'Not configured — restart server with --source'}</dd>
              </div>
              <div>
                <dt>GeoFrame data dir</dt>
                <dd>{status.dataDir}</dd>
              </div>
              <div>
                <dt>Thumbnails cache</dt>
                <dd>{status.cache.thumbnailsDir}</dd>
              </div>
              <div>
                <dt>Previews cache</dt>
                <dd>{status.cache.previewsDir}</dd>
              </div>
            </dl>
          </section>

          <section className="settings-card">
            <div className="settings-card-header">
              <HardDrive size={22} />
              <h2>Storage</h2>
            </div>
            <dl className="settings-list">
              <div>
                <dt>Indexed assets</dt>
                <dd>{status.assets.active} active, {status.assets.trashed} in trash</dd>
              </div>
              <div>
                <dt>Generated cache</dt>
                <dd>
                  {formatMegabytes(status.cache.totalMB)} total ({formatMegabytes(status.cache.thumbnailsMB)}{' '}
                  thumbnails, {formatMegabytes(status.cache.previewsMB)} previews)
                </dd>
              </div>
              {status.disk && (
                <div>
                  <dt>Disk space</dt>
                  <dd>
                    {formatMegabytes(status.disk.availableMB)} free of{' '}
                    {formatMegabytes(status.disk.totalMB)}
                  </dd>
                </div>
              )}
            </dl>
          </section>

          <section className="settings-card">
            <div className="settings-card-header">
              <RefreshCw size={22} />
              <h2>Indexing</h2>
            </div>
            <p className="settings-note">
              Rescan walks the source folder and enqueues new files. Existing assets are not duplicated.
            </p>
            {scanError && (
              <p className="settings-error" role="alert">{scanError}</p>
            )}
            <button
              className="settings-action"
              type="button"
              disabled={!canRescan}
              onClick={() => void handleRescan()}
            >
              <RefreshCw size={18} className={scanBusy ? 'scan-progress-spinner' : undefined} />
              {scanBusy ? 'Scan in progress…' : 'Rescan library'}
            </button>
          </section>
        </div>
      )}
    </AppShell>
  )
}
