/** Voyager / Protomaps light — matches gutters while tiles load. */
export const BASEMAP_BACKGROUND = '#fbf8f3'

/** MapLibre options for smoother pan/zoom with PMTiles + raster fallback. */
export const MAP_CREATE_OPTIONS = {
  fadeDuration: 0,
  /** Keep parent-zoom tiles visible while zooming in (less gray flash). */
  cancelPendingTileRequestsWhileZooming: false,
  refreshExpiredTiles: false,
  maxTileCacheZoomLevels: 6,
  maxTileCacheSize: 160,
  renderWorldCopies: false,
} as const
