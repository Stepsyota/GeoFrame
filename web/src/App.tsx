import { useState } from 'react'

import type { AppPage } from './components/AppShell'
import { ErrorBoundary } from './components/ErrorBoundary'
import { DuplicatesPage } from './pages/DuplicatesPage'
import { FavoritesPage } from './pages/FavoritesPage'
import { GalleryPage } from './pages/GalleryPage'
import { MapPage } from './pages/MapPage'
import { SeriesPage } from './pages/SeriesPage'
import { TrashPage } from './pages/TrashPage'

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
      ) : page === 'favorites' ? (
        <FavoritesPage onNavigate={setPage} />
      ) : page === 'trash' ? (
        <TrashPage onNavigate={setPage} />
      ) : (
        <GalleryPage onNavigate={setPage} />
      )}
    </ErrorBoundary>
  )
}
