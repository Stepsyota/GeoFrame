import { layers, namedFlavor } from '@protomaps/basemaps'
import type { LayerSpecification, StyleSpecification } from 'maplibre-gl'

import { BASEMAP_BACKGROUND } from './map_performance'

const backgroundLayer = (): LayerSpecification => ({
  id: 'geoframe-background',
  type: 'background',
  paint: { 'background-color': BASEMAP_BACKGROUND },
})

const PROTOMAPS_GLYPHS =
  'https://protomaps.github.io/basemaps-assets/fonts/{fontstack}/{range}.pbf'
const PROTOMAPS_SPRITE = 'https://protomaps.github.io/basemaps-assets/sprites/v4/light'

/** Carto CDN fallback when no local region file is installed. */
export const CARTO_STYLE_URL = 'https://basemaps.cartocdn.com/gl/voyager-gl-style/style.json'

/** Build a MapLibre style backed by a local PMTiles file only. */
export const createPmtilesStyle = (regionUrl: string): StyleSpecification => {
  const absolute = new URL(regionUrl, window.location.origin).href

  return {
    version: 8,
    glyphs: PROTOMAPS_GLYPHS,
    sprite: PROTOMAPS_SPRITE,
    sources: {
      protomaps: {
        type: 'vector',
        url: `pmtiles://${absolute}`,
        attribution:
          '<a href="https://protomaps.com">Protomaps</a> © <a href="https://openstreetmap.org">OpenStreetMap</a>',
      },
    },
    layers: [backgroundLayer(), ...layers('protomaps', namedFlavor('light'), { lang: 'en' })],
  }
}

/** Carto Voyager raster tiles for zoom levels above the offline PMTiles archive. */
const CARTO_RASTER_TILES =
  'https://basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}.png'

/**
 * Hybrid basemap: local world PMTiles up to @p localMaxZoom, Carto raster beyond that.
 * Uses a single raster layer instead of duplicating the full Carto vector style (~90 layers).
 */
export const createHybridStyle = (
  regionUrl: string,
  localMaxZoom: number,
): StyleSpecification => {
  const absolute = new URL(regionUrl, window.location.origin).href
  const remoteMinZoom = localMaxZoom + 1

  const localLayers = layers('protomaps', namedFlavor('light'), { lang: 'en' }).map((layer) => ({
    ...layer,
    maxzoom: remoteMinZoom,
  }))

  const remoteLayer: LayerSpecification = {
    id: 'carto-raster',
    type: 'raster',
    source: 'carto-raster',
    minzoom: remoteMinZoom,
    paint: {
      'raster-fade-duration': 0,
      'raster-resampling': 'linear',
    },
  }

  return {
    version: 8,
    glyphs: PROTOMAPS_GLYPHS,
    sprite: PROTOMAPS_SPRITE,
    sources: {
      protomaps: {
        type: 'vector',
        url: `pmtiles://${absolute}`,
        attribution:
          '<a href="https://protomaps.com">Protomaps</a> © <a href="https://openstreetmap.org">OpenStreetMap</a>',
      },
      'carto-raster': {
        type: 'raster',
        tiles: [CARTO_RASTER_TILES],
        tileSize: 256,
        minzoom: remoteMinZoom,
        maxzoom: 20,
        attribution: '© CARTO © OpenStreetMap',
      },
    },
    layers: [backgroundLayer(), ...localLayers, remoteLayer],
  }
}
