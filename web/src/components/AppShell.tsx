import {
  GalleryHorizontalEnd,
  Heart,
  Images,
  Map,
  Search,
  Settings,
  Sparkles,
} from 'lucide-react'
import type { PropsWithChildren } from 'react'

const navigation = [
  { label: 'Photos', icon: Images, active: true },
  { label: 'Map', icon: Map },
  { label: 'Series', icon: GalleryHorizontalEnd },
  { label: 'Favorites', icon: Heart },
]

interface AppShellProps extends PropsWithChildren {
  total?: number
}

export const AppShell = ({ children, total }: AppShellProps) => (
  <div className="app-shell">
    <aside className="sidebar">
      <div className="brand">
        <span className="brand-mark">
          <Sparkles size={19} strokeWidth={2.2} />
        </span>
        <span>GeoFrame</span>
      </div>

      <nav className="navigation" aria-label="Main navigation">
        {navigation.map(({ label, icon: Icon, active }) => (
          <button className={`nav-item ${active ? 'active' : ''}`} key={label} type="button">
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
          <p className="eyebrow">Your library</p>
          <h1>Photos</h1>
        </div>
        <div className="topbar-actions">
          {typeof total === 'number' && <span className="asset-count">{total} assets</span>}
          <button className="icon-button" type="button" aria-label="Search library">
            <Search size={21} />
          </button>
        </div>
      </header>
      {children}
    </main>

    <nav className="bottom-navigation" aria-label="Mobile navigation">
      {navigation.slice(0, 4).map(({ label, icon: Icon, active }) => (
        <button className={`bottom-nav-item ${active ? 'active' : ''}`} key={label} type="button">
          <Icon size={21} />
          <span>{label}</span>
        </button>
      ))}
    </nav>
  </div>
)
