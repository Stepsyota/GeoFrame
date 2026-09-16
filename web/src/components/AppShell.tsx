import {
  GalleryHorizontalEnd,
  Heart,
  Images,
  Map,
  Search,
  Settings,
  Sparkles,
} from 'lucide-react'
import type { PropsWithChildren, ReactNode } from 'react'

export type AppPage = 'photos' | 'map' | 'series' | 'favorites'

const navigation: Array<{ id: AppPage; label: string; icon: typeof Images; enabled: boolean }> = [
  { id: 'photos', label: 'Photos', icon: Images, enabled: true },
  { id: 'map', label: 'Map', icon: Map, enabled: true },
  { id: 'series', label: 'Series', icon: GalleryHorizontalEnd, enabled: false },
  { id: 'favorites', label: 'Favorites', icon: Heart, enabled: false },
]

interface AppShellProps extends PropsWithChildren {
  page: AppPage
  onNavigate: (page: AppPage) => void
  total?: number
  title?: string
  eyebrow?: string
  banner?: ReactNode
}

export const AppShell = ({
  children,
  page,
  onNavigate,
  total,
  title = 'Photos',
  eyebrow = 'Your library',
  banner,
}: AppShellProps) => (
  <div className="app-shell">
    <aside className="sidebar">
      <div className="brand">
        <span className="brand-mark">
          <Sparkles size={19} strokeWidth={2.2} />
        </span>
        <span>GeoFrame</span>
      </div>

      <nav className="navigation" aria-label="Main navigation">
        {navigation.map(({ id, label, icon: Icon, enabled }) => (
          <button
            className={`nav-item ${page === id ? 'active' : ''}`}
            key={label}
            type="button"
            disabled={!enabled}
            onClick={() => enabled && onNavigate(id)}
          >
            <Icon size={19} />
            <span>{label}</span>
          </button>
        ))}
      </nav>

      <button className="nav-item settings" type="button">
        <Settings size={19} />
        <span>Settings</span>
      </button>
    </aside>

    <main className="main-content">
      <header className="topbar">
        <div>
          <p className="eyebrow">{eyebrow}</p>
          <h1>{title}</h1>
        </div>
        <div className="topbar-actions">
          {typeof total === 'number' && <span className="asset-count">{total} assets</span>}
          <button className="icon-button" type="button" aria-label="Search library">
            <Search size={21} />
          </button>
        </div>
      </header>
      {banner}
      {children}
    </main>

    <nav className="bottom-navigation" aria-label="Mobile navigation">
      {navigation.filter((item) => item.enabled).map(({ id, label, icon: Icon }) => (
        <button
          className={`bottom-nav-item ${page === id ? 'active' : ''}`}
          key={label}
          type="button"
          onClick={() => onNavigate(id)}
        >
          <Icon size={21} />
          <span>{label}</span>
        </button>
      ))}
    </nav>
  </div>
)
