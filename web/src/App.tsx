import { ErrorBoundary } from './components/ErrorBoundary'
import { GalleryPage } from './pages/GalleryPage'

export const App = () => (
  <ErrorBoundary>
    <GalleryPage />
  </ErrorBoundary>
)
