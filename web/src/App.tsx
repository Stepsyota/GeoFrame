import { useState } from 'react'

import type { AppPage } from './components/AppShell'
import { ErrorBoundary } from './components/ErrorBoundary'
import { DuplicatesPage } from './pages/DuplicatesPage'
import { GalleryPage } from './pages/GalleryPage'
import { MapPage } from './pages/MapPage'
import { SeriesPage } from './pages/SeriesPage'

export const App = () => {
  const [page, setPage] = useState<AppPage>('photos')

  return (
    <ErrorBoundary>
      {page === 'map' ? (
        <MapPage onNavigate={setPage} />
      ) : page === 'duplicates' ? (
        <DuplicatesPage onNavigate={setPage} />
      ) : page === 'series' ? (
        <SeriesPage onNavigate={setPage} />
      ) : (
        <GalleryPage onNavigate={setPage} />
      )}
    </ErrorBoundary>
  )
}
