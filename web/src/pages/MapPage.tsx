import { AlertCircle, MapPin } from 'lucide-react'
import * as maplibregl from 'maplibre-gl'
import { useEffect, useRef, useState } from 'react'

import { getMapClusters } from '../api/map'
import { AppShell, type AppPage } from '../components/AppShell'
import type { MapFeatureProperties } from '../types/map'

import 'maplibre-gl/dist/maplibre-gl.css'

const MAP_STYLE = 'https://demotiles.maplibre.org/style.json'

interface MapPageProps {
  onNavigate: (page: AppPage) => void
}

export const MapPage = ({ onNavigate }: MapPageProps) => {
  const containerRef = useRef<HTMLDivElement | null>(null)
  const mapRef = useRef<maplibregl.Map | null>(null)
  const popupRef = useRef<maplibregl.Popup | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [geoCount, setGeoCount] = useState(0)
  const [loaded, setLoaded] = useState(false)

  useEffect(() => {
    if (!containerRef.current) {
      return
    }

    const map = new maplibregl.Map({
      container: containerRef.current,
      style: MAP_STYLE,
      center: [27.56, 53.9],
      zoom: 4,
      attributionControl: false,
    })
    map.addControl(new maplibregl.NavigationControl({ showCompass: false }), 'top-right')
    map.addControl(new maplibregl.AttributionControl({ compact: true }), 'bottom-right')

    map.on('load', () => {
      map.addSource('geoframe-clusters', {
        type: 'geojson',
        data: { type: 'FeatureCollection', features: [] },
      })

      map.addLayer({
        id: 'cluster-circles',
        type: 'circle',
        source: 'geoframe-clusters',
        paint: {
          'circle-color': [
            'case',
            ['get', 'cluster'],
            '#5f7a4d',
            '#7c975f',
          ],
          'circle-radius': [
            'case',
            ['get', 'cluster'],
            ['min', 34, ['+', 16, ['*', ['get', 'pointCount'], 1.5]]],
            9,
          ],
          'circle-stroke-width': 2,
          'circle-stroke-color': '#10120f',
        },
      })

      map.addLayer({
        id: 'cluster-count',
        type: 'symbol',
        source: 'geoframe-clusters',
        filter: ['==', ['get', 'cluster'], true],
        layout: {
          'text-field': ['to-string', ['get', 'pointCount']],
          'text-size': 13,
          'text-font': ['Open Sans Bold'],
        },
        paint: {
          'text-color': '#ecf0e9',
        },
      })

      const loadClusters = async () => {
        try {
          const zoom = Math.round(map.getZoom())
          const data = await getMapClusters(zoom)
          const source = map.getSource('geoframe-clusters') as maplibregl.GeoJSONSource
          source.setData(data)
          setGeoCount(data.features.reduce((sum, feature) => sum + feature.properties.pointCount, 0))
          setError(null)
          setLoaded(true)

          if (data.features.length > 0 && map.getZoom() <= 4) {
            const bounds = new maplibregl.LngLatBounds()
            data.features.forEach((feature) => {
              bounds.extend(feature.geometry.coordinates as [number, number])
            })
            map.fitBounds(bounds, { padding: 80, maxZoom: 10, duration: 0 })
          }
        } catch (err: unknown) {
          const message = err instanceof Error ? err.message : 'Unknown map API error'
          setError(message)
        }
      }

      void loadClusters()
      map.on('zoomend', () => {
        void loadClusters()
      })

      map.on('click', 'cluster-circles', (event: maplibregl.MapLayerMouseEvent) => {
        const feature = event.features?.[0]
        if (!feature) {
          return
        }
        const props = feature.properties as MapFeatureProperties
        const geometry = feature.geometry as { type: 'Point'; coordinates: [number, number] }
        const coordinates = geometry.coordinates.slice() as [number, number]

        if (props.cluster) {
          map.easeTo({
            center: coordinates,
            zoom: Math.min(map.getZoom() + 2, 16),
          })
          return
        }

        popupRef.current?.remove()
        const thumbnail = props.thumbnailUrl
          ? `<img src="${props.thumbnailUrl}" alt="" class="map-popup-thumb" />`
          : '<div class="map-popup-fallback">No preview</div>'

        popupRef.current = new maplibregl.Popup({ offset: 16, maxWidth: '220px' })
          .setLngLat(coordinates)
          .setHTML(`<div class="map-popup">${thumbnail}<p>${props.pointCount} photo</p></div>`)
          .addTo(map)
      })

      map.on('mouseenter', 'cluster-circles', () => {
        map.getCanvas().style.cursor = 'pointer'
      })
      map.on('mouseleave', 'cluster-circles', () => {
        map.getCanvas().style.cursor = ''
      })
    })

    mapRef.current = map
    return () => {
      popupRef.current?.remove()
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
    </AppShell>
  )
}
