import { useEffect, useState } from 'react'

import type { ScanProgressEvent } from '../types/progress'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

/** True while the filesystem walk or in-flight worker jobs are running. */
const isActive = (event: ScanProgressEvent | null): boolean => {
  if (!event) return false
  return event.scanning || event.pendingJobs > 0
}

export const useScanProgress = () => {
  const [progress, setProgress] = useState<ScanProgressEvent | null>(null)
  const [connected, setConnected] = useState(false)

  useEffect(() => {
    if (demoMode) {
      return
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const socket = new WebSocket(`${protocol}//${window.location.host}/ws/events`)

    socket.onopen = () => setConnected(true)
    socket.onclose = () => setConnected(false)
    socket.onerror = () => setConnected(false)
    socket.onmessage = (message) => {
      try {
        const payload = JSON.parse(message.data as string) as ScanProgressEvent
        if (payload.type === 'scan_progress') {
          setProgress(payload)
        }
      } catch {
        // Ignore malformed events.
      }
    }

    return () => socket.close()
  }, [])

  return {
    progress,
    connected,
    active: isActive(progress),
  }
}
