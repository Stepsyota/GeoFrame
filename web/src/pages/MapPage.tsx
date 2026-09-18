import { AlertCircle, MapPin } from 'lucide-react'
import * as maplibregl from 'maplibre-gl'
import { useEffect, useRef, useState } from 'react'

import { ensurePmtilesProtocol } from '../maplibre_setup'
import { getAssetById } from '../api/assets'
import { getMapBasemap, getMapPoints } from '../api/map'
import { AppShell, type AppPage } from '../components/AppShell'
import { Lightbox } from '../components/Lightbox'
import { MapClusterIndex } from '../map/cluster_index'
import { MAP_CREATE_OPTIONS } from '../map/map_performance'
import { PhotoMarkerManager } from '../map/photo_markers'
import type { AssetSummary } from '../types/asset'
import type { MapFeature } from '../types/map'

import 'maplibre-gl/dist/maplibre-gl.css'

interface MapPageProps {
  onNavigate: (page: AppPage) => void
}

export const MapPage = ({ onNavigate }: MapPageProps) => {
  const containerRef = useRef<HTMLDivElement | null>(null)
  const mapRef = useRef<maplibregl.Map | null>(null)
  const clusterIndexRef = useRef(new MapClusterIndex())
  const markerManagerRef = useRef(new PhotoMarkerManager())
  const [error, setError] = useState<string | null>(null)
  const [geoCount, setGeoCount] = useState(0)
  const [loaded, setLoaded] = useState(false)
  const [selectedAsset, setSelectedAsset] = useState<AssetSummary | null>(null)

  useEffect(() => {
    if (!containerRef.current) {
      return
    }

    let cancelled = false
    let syncFrame = 0
    let zoomSyncTimer: number | undefined
    let onResize: (() => void) | null = null

    const markerManager = markerManagerRef.current
    const clusterIndex = clusterIndexRef.current

    const initMap = async () => {
      const basemap = await getMapBasemap()
      if (cancelled || !containerRef.current) {
        return
      }

      if (basemap.kind === 'pmtiles' || basemap.kind === 'hybrid') {
        ensurePmtilesProtocol()
      }

      const map = new maplibregl.Map({
        container: containerRef.current,
        style: basemap.kind === 'carto' ? basemap.styleUrl : basemap.style,
        center: [27.56, 53.9],
        zoom: 4,
        pitch: 0,
        bearing: 0,
        maxPitch: 0,
        pitchWithRotate: false,
        dragRotate: false,
        touchPitch: false,
        ...MAP_CREATE_OPTIONS,
        attributionControl: false,
        cooperativeGestures: false,
      })
      map.addControl(new maplibregl.NavigationControl({ showCompass: false }), 'top-right')
      map.addControl(new maplibregl.AttributionControl({ compact: true }), 'bottom-right')

      let didInitialFit = false
      let pointsLoaded = false

      const handleFeatureClick = (feature: MapFeature) => {
        const coordinates = feature.geometry.coordinates

        if (feature.properties.cluster) {
          const clusterId = feature.properties.clusterId
          const nextZoom = clusterId !== undefined
            ? clusterIndex.expansionZoom(clusterId)
            : Math.min(map.getZoom() + 2, 18)
          map.easeTo({
            center: coordinates,
            zoom: nextZoom,
            duration: 450,
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

      const syncMarkers = () => {
        if (!pointsLoaded) {
          return
        }
        const bounds = map.getBounds()
        const bbox: [number, number, number, number] = [
          bounds.getWest(),
          bounds.getSouth(),
          bounds.getEast(),
          bounds.getNorth(),
        ]
        const zoom = map.getZoom()
        const features = clusterIndex.query(bbox, zoom)
        markerManager.sync(map, features, handleFeatureClick, zoom)
      }

      const scheduleSync = () => {
        cancelAnimationFrame(syncFrame)
        syncFrame = requestAnimationFrame(syncMarkers)
      }

      const scheduleZoomSync = () => {
        if (zoomSyncTimer !== undefined) {
          return
        }
        zoomSyncTimer = window.setTimeout(() => {
          zoomSyncTimer = undefined
          scheduleSync()
        }, 100)
      }

      const applyPoints = (data: Awaited<ReturnType<typeof getMapPoints>>) => {
        clusterIndex.load(data.features)
        pointsLoaded = true
        setGeoCount(clusterIndex.totalPoints())
        setError(null)
        setLoaded(true)
        scheduleSync()

        if (!didInitialFit && data.features.length > 0 && map.isStyleLoaded()) {
          didInitialFit = true
          const bounds = new maplibregl.LngLatBounds()
          data.features.forEach((feature) => {
            bounds.extend(feature.geometry.coordinates)
          })
          map.fitBounds(bounds, { padding: 80, maxZoom: 12, duration: 0 })
        }
      }

      try {
        const data = await getMapPoints()
        if (!cancelled) {
          applyPoints(data)
        }
      } catch (err: unknown) {
        if (!cancelled) {
          const message = err instanceof Error ? err.message : 'Unknown map API error'
          setError(message)
        }
      }

      map.on('load', () => {
        map.resize()
        scheduleSync()
      })
      map.on('zoom', scheduleZoomSync)
      map.on('moveend', scheduleSync)
      map.on('error', (event) => {
        const message = event.error?.message ?? 'Could not render map tiles'
        setError(message)
      })

      onResize = () => map.resize()
      window.addEventListener('resize', onResize)

      mapRef.current = map
    }

    void initMap().catch((err: unknown) => {
      const message = err instanceof Error ? err.message : 'Could not load map'
      setError(message)
    })

    return () => {
      cancelled = true
      if (zoomSyncTimer !== undefined) {
        window.clearTimeout(zoomSyncTimer)
      }
      cancelAnimationFrame(syncFrame)
      if (onResize) {
        window.removeEventListener('resize', onResize)
      }
      markerManager.clear()
      if (mapRef.current) {
        mapRef.current.remove()
        mapRef.current = null
      }
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
