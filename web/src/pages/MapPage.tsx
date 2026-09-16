import { AlertCircle, MapPin } from 'lucide-react'
import * as maplibregl from 'maplibre-gl'
import { useEffect, useRef, useState } from 'react'

import '../maplibre_setup'
import { getAssetById } from '../api/assets'
import { getMapClusters } from '../api/map'
import { AppShell, type AppPage } from '../components/AppShell'
import { Lightbox } from '../components/Lightbox'
import { PhotoMarkerManager } from '../map/photo_markers'
import type { AssetSummary } from '../types/asset'
import type { MapFeature } from '../types/map'

import 'maplibre-gl/dist/maplibre-gl.css'

const MAP_STYLE = 'https://demotiles.maplibre.org/style.json'

interface MapPageProps {
  onNavigate: (page: AppPage) => void
}

export const MapPage = ({ onNavigate }: MapPageProps) => {
  const containerRef = useRef<HTMLDivElement | null>(null)
  const mapRef = useRef<maplibregl.Map | null>(null)
  const markerManagerRef = useRef(new PhotoMarkerManager())
  const [error, setError] = useState<string | null>(null)
  const [geoCount, setGeoCount] = useState(0)
  const [loaded, setLoaded] = useState(false)
  const [selectedAsset, setSelectedAsset] = useState<AssetSummary | null>(null)

  useEffect(() => {
    if (!containerRef.current) {
      return
    }

    const markerManager = markerManagerRef.current

    const map = new maplibregl.Map({
      container: containerRef.current,
      style: MAP_STYLE,
      center: [27.56, 53.9],
      zoom: 4,
      attributionControl: false,
    })
    map.addControl(new maplibregl.NavigationControl({ showCompass: false }), 'top-right')
    map.addControl(new maplibregl.AttributionControl({ compact: true }), 'bottom-right')

    let didInitialFit = false

    const handleFeatureClick = (feature: MapFeature) => {
      const coordinates = feature.geometry.coordinates

      if (feature.properties.cluster) {
        map.easeTo({
          center: coordinates,
          zoom: Math.min(map.getZoom() + 2, 16),
        })
        return
      }

      const assetId = feature.properties.assetId
      if (!assetId) {
        return
      }

      void getAssetById(assetId)
        .then((asset) => setSelectedAsset(asset))
        .catch((err: unknown) => {
          const message = err instanceof Error ? err.message : 'Could not open photo'
          setError(message)
        })
    }

    const loadClusters = async () => {
      try {
        const zoom = Math.round(map.getZoom())
        const data = await getMapClusters(zoom)
        markerManager.sync(map, data.features, handleFeatureClick)
        setGeoCount(data.features.reduce((sum, feature) => sum + feature.properties.pointCount, 0))
        setError(null)
        setLoaded(true)

        if (!didInitialFit && data.features.length > 0) {
          didInitialFit = true
          const bounds = new maplibregl.LngLatBounds()
          data.features.forEach((feature) => {
            bounds.extend(feature.geometry.coordinates)
          })
          map.fitBounds(bounds, { padding: 80, maxZoom: 10 })
        }
      } catch (err: unknown) {
        const message = err instanceof Error ? err.message : 'Unknown map API error'
        setError(message)
      }
    }

    map.on('load', () => {
      map.resize()
      void loadClusters()
      map.on('zoomend', () => {
        void loadClusters()
      })
    })

    const onResize = () => map.resize()
    window.addEventListener('resize', onResize)

    mapRef.current = map
    return () => {
      window.removeEventListener('resize', onResize)
      markerManager.clear()
      map.remove()
      mapRef.current = null
    }
  }, [])

  return (
    <AppShell page="map" onNavigate={onNavigate} total={geoCount} title="Map" eyebrow="Locations">
      {error && (
        <section className="state-card map-error" role="alert">
          <AlertCircle size={30} />
          <div>
            <h2>Could not load map data</h2>
            <p>{error}</p>
          </div>
        </section>
      )}

      {loaded && !error && geoCount === 0 && (
        <section className="state-card map-empty">
          <MapPin size={30} />
          <div>
            <h2>No geotagged photos yet</h2>
            <p>Photos with GPS coordinates will appear here after indexing.</p>
          </div>
        </section>
      )}

      <div className="map-page">
        <div className="map-canvas" ref={containerRef} aria-label="Photo map" />
      </div>

      {selectedAsset && (
        <Lightbox
          asset={selectedAsset}
          hasPrev={false}
          hasNext={false}
          onClose={() => setSelectedAsset(null)}
          onPrev={() => undefined}
          onNext={() => undefined}
        />
      )}
    </AppShell>
  )
}
