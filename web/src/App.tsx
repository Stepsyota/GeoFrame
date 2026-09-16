import { useState } from 'react'

import type { AppPage } from './components/AppShell'
import { ErrorBoundary } from './components/ErrorBoundary'
import { GalleryPage } from './pages/GalleryPage'
import { MapPage } from './pages/MapPage'

export const App = () => {
  const [page, setPage] = useState<AppPage>('photos')

  return (
    <ErrorBoundary>
      {page === 'map' ? (
        <MapPage onNavigate={setPage} />
      ) : (
        <GalleryPage onNavigate={setPage} />
      )}
    </ErrorBoundary>
  )
}
