import { Component, type ErrorInfo, type ReactNode } from 'react'

interface Props {
  children: ReactNode
}

interface State {
  error: Error | null
}

/**
 * Catches render-time errors and shows a readable message instead of a blank page.
 * Without this, React 19 silently unmounts the whole tree on any render error.
 */
export class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props)
    this.state = { error: null }
  }

  static getDerivedStateFromError(error: Error): State {
    return { error }
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error('[GeoFrame] Render error:', error, info.componentStack)
  }

  render() {
    if (this.state.error) {
      return (
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            minHeight: '100vh',
            gap: '12px',
            color: '#a9b0a4',
            fontFamily: 'inherit',
          }}
        >
          <p style={{ margin: 0, fontSize: 15 }}>
            <strong style={{ color: '#e5e9e1' }}>GeoFrame encountered an error</strong>
          </p>
          <pre
            style={{
              margin: 0,
              padding: '14px 18px',
              borderRadius: 10,
              background: '#191c17',
              border: '1px solid #30362d',
              fontSize: 12,
              color: '#c97070',
              maxWidth: '80vw',
              overflow: 'auto',
            }}
          >
            {this.state.error.message}
          </pre>
          <p style={{ margin: 0, fontSize: 13, color: '#7e8879' }}>
            Check browser console for details.
          </p>
          <button
            type="button"
            onClick={() => this.setState({ error: null })}
            style={{
              padding: '8px 18px',
              border: '1px solid #4b5643',
              borderRadius: 8,
              background: '#20251d',
              color: '#d9edbe',
              cursor: 'pointer',
              fontSize: 13,
            }}
          >
            Try again
          </button>
        </div>
      )
    }

    return this.props.children
  }
}
