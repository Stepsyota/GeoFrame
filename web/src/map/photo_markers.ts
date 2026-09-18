import * as maplibregl from 'maplibre-gl'

import type { MapFeature, MapFeatureProperties } from '../types/map'

/** Keep DOM marker count bounded to avoid browser OOM with large libraries. */
export const MAX_VISIBLE_MARKERS = 120
/** Zoom at which markers reach full rendered size. */
export const MARKER_FULL_SIZE_ZOOM = 16
/** Zoom below which markers are at minimum scale. */
export const MARKER_MIN_SIZE_ZOOM = 3
const MARKER_SCALE_MIN = 0.42
const MARKER_SCALE_MAX = 1
const MARKER_TRANSITION_MS = 380

const smoothstep = (value: number) => value * value * (3 - 2 * value)

/** Scale factor 0.42…1.0 — markers grow smoothly as the user zooms in. */
export const markerScaleForZoom = (zoom: number): number => {
  const span = MARKER_FULL_SIZE_ZOOM - MARKER_MIN_SIZE_ZOOM
  const t = span <= 0 ? 1 : Math.min(1, Math.max(0, (zoom - MARKER_MIN_SIZE_ZOOM) / span))
  return MARKER_SCALE_MIN + (MARKER_SCALE_MAX - MARKER_SCALE_MIN) * smoothstep(t)
}

const applyMarkerScale = (button: HTMLButtonElement, zoom: number) => {
  const scale = markerScaleForZoom(zoom)
  button.style.setProperty('--marker-scale', scale.toFixed(3))
  button.classList.toggle('map-photo-marker-compact', scale < 0.55)
}

const toAbsoluteUrl = (url: string) => new URL(url, window.location.origin).href

export const selectVisibleFeatures = (features: MapFeature[]): MapFeature[] => {
  if (features.length <= MAX_VISIBLE_MARKERS) {
    return features
  }
  return [...features]
    .sort((left, right) => right.properties.pointCount - left.properties.pointCount)
    .slice(0, MAX_VISIBLE_MARKERS)
}

const featureKey = (feature: MapFeature): string => {
  const { cluster, clusterId, assetId, assetIds, pointCount } = feature.properties
  if (cluster && clusterId !== undefined) {
    return `c:${clusterId}`
  }
  if (cluster) {
    const [lon, lat] = feature.geometry.coordinates
    return `c:${lon.toFixed(5)},${lat.toFixed(5)}:${pointCount}`
  }
  return `a:${assetId ?? assetIds[0] ?? 0}`
}

const appendMarkerImage = (
  button: HTMLButtonElement,
  primary: string,
  fallback?: string | null,
) => {
  const image = document.createElement('img')
  image.alt = ''
  image.draggable = false
  image.decoding = 'async'
  image.loading = 'lazy'
  image.className = 'map-photo-image'

  const load = (url: string) => {
    image.src = toAbsoluteUrl(url)
  }

  image.addEventListener('error', () => {
    if (fallback && fallback !== primary) {
      load(fallback)
    }
  })

  load(primary)
  button.appendChild(image)
  return image
}

const shouldShowThumbnail = (props: MapFeatureProperties) =>
  Boolean(props.thumbnailUrl ?? props.previewUrl)

const appendDotFallback = (button: HTMLButtonElement, props: MapFeatureProperties) => {
  const fallback = document.createElement('span')
  fallback.className = props.cluster
    ? 'map-photo-fallback map-photo-dot-cluster'
    : 'map-photo-fallback map-photo-dot'
  button.appendChild(fallback)
}

export const createPhotoMarkerElement = (
  props: MapFeatureProperties,
  zoom: number,
): HTMLButtonElement => {
  const button = document.createElement('button')
  button.type = 'button'
  button.className = props.cluster ? 'map-photo-marker map-photo-cluster' : 'map-photo-marker'
  if (!shouldShowThumbnail(props)) {
    button.classList.add('map-photo-marker-dot')
  }
  button.setAttribute(
    'aria-label',
    props.cluster ? `Open cluster of ${props.pointCount} photos` : 'Open photo',
  )

  const imageUrl = props.thumbnailUrl ?? props.previewUrl ?? null
  if (shouldShowThumbnail(props) && imageUrl) {
    appendMarkerImage(button, imageUrl, props.previewUrl)
  } else {
    appendDotFallback(button, props)
  }

  if (props.cluster && props.pointCount > 1) {
    const badge = document.createElement('span')
    badge.className = 'map-photo-badge'
    badge.textContent = String(props.pointCount)
    button.appendChild(badge)
  }

  if (props.favorite) {
    const favorite = document.createElement('span')
    favorite.className = 'map-photo-favorite'
    favorite.textContent = '♥'
    button.appendChild(favorite)
  }

  if (props.mediaType === 'video') {
    const video = document.createElement('span')
    video.className = 'map-photo-video'
    video.textContent = '▶'
    button.appendChild(video)
  }

  applyMarkerScale(button, zoom)
  return button
}

const updateMarkerElement = (
  button: HTMLButtonElement,
  props: MapFeatureProperties,
  imageUrl: string | null,
  zoom: number,
) => {
  const showThumbnail = shouldShowThumbnail(props)
  button.classList.toggle('map-photo-marker-dot', !showThumbnail)

  button.setAttribute(
    'aria-label',
    props.cluster ? `Open cluster of ${props.pointCount} photos` : 'Open photo',
  )

  const badge = button.querySelector<HTMLElement>('.map-photo-badge')
  if (props.cluster && props.pointCount > 1) {
    if (badge) {
      badge.textContent = String(props.pointCount)
    } else {
      const next = document.createElement('span')
      next.className = 'map-photo-badge'
      next.textContent = String(props.pointCount)
      button.appendChild(next)
    }
  } else if (badge) {
    badge.remove()
  }

  const nextImageUrl = props.thumbnailUrl ?? props.previewUrl ?? null
  const image = button.querySelector<HTMLImageElement>('.map-photo-image')
  const fallback = button.querySelector<HTMLElement>('.map-photo-fallback')

  if (showThumbnail && nextImageUrl) {
    if (!image) {
      if (fallback) {
        fallback.remove()
      }
      appendMarkerImage(button, nextImageUrl, props.previewUrl)
    } else if (nextImageUrl !== imageUrl) {
      image.src = toAbsoluteUrl(nextImageUrl)
    }
  } else {
    if (image) {
      image.remove()
    }
    if (!fallback) {
      appendDotFallback(button, props)
    }
  }

  applyMarkerScale(button, zoom)
}

interface MarkerEntry {
  marker: maplibregl.Marker
  root: HTMLDivElement
  button: HTMLButtonElement
  imageUrl: string | null
  cleanup: () => void
}

const playEnterAnimation = (button: HTMLButtonElement) => {
  button.classList.add('map-photo-marker-enter')
  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      button.classList.remove('map-photo-marker-enter')
    })
  })
}

export class PhotoMarkerManager {
  private entries = new Map<string, MarkerEntry>()
  private leaving = new Map<string, MarkerEntry>()
  private leaveTimers = new Map<string, number>()

  sync(
    map: maplibregl.Map,
    features: MapFeature[],
    onFeatureClick: (feature: MapFeature) => void,
    zoom: number,
  ) {
    const nextKeys = new Set<string>()
    const visible = selectVisibleFeatures(features)

    for (const feature of visible) {
      const key = featureKey(feature)
      nextKeys.add(key)

      const pendingLeave = this.leaving.get(key)
      if (pendingLeave) {
        const timer = this.leaveTimers.get(key)
        if (timer !== undefined) {
          window.clearTimeout(timer)
          this.leaveTimers.delete(key)
        }
        pendingLeave.button.classList.remove('map-photo-marker-leaving')
        this.leaving.delete(key)
        this.entries.set(key, pendingLeave)
      }

      const existing = this.entries.get(key)
      if (existing) {
        existing.marker.setLngLat(feature.geometry.coordinates)
        updateMarkerElement(existing.button, feature.properties, existing.imageUrl, zoom)
        existing.imageUrl = feature.properties.thumbnailUrl ?? feature.properties.previewUrl ?? null
        continue
      }

      const button = createPhotoMarkerElement(feature.properties, zoom)
      playEnterAnimation(button)
      const onClick = (event: MouseEvent) => {
        event.stopPropagation()
        onFeatureClick(feature)
      }
      button.addEventListener('click', onClick)

      const root = document.createElement('div')
      root.className = 'map-photo-marker-root'
      root.appendChild(button)

      const marker = new maplibregl.Marker({ element: root, anchor: 'bottom' })
        .setLngLat(feature.geometry.coordinates)
        .addTo(map)

      this.entries.set(key, {
        marker,
        root,
        button,
        imageUrl: feature.properties.thumbnailUrl ?? feature.properties.previewUrl ?? null,
        cleanup: () => button.removeEventListener('click', onClick),
      })
    }

    for (const [key, entry] of this.entries) {
      if (nextKeys.has(key) || this.leaving.has(key)) {
        continue
      }
      this.entries.delete(key)
      entry.button.classList.add('map-photo-marker-leaving')
      this.leaving.set(key, entry)
      const timer = window.setTimeout(() => {
        this.leaveTimers.delete(key)
        if (!this.leaving.has(key)) {
          return
        }
        entry.cleanup()
        entry.marker.remove()
        this.leaving.delete(key)
      }, MARKER_TRANSITION_MS)
      this.leaveTimers.set(key, timer)
    }
  }

  clear() {
    for (const timer of this.leaveTimers.values()) {
      window.clearTimeout(timer)
    }
    this.leaveTimers.clear()
    for (const entry of this.leaving.values()) {
      entry.cleanup()
      entry.marker.remove()
    }
    this.leaving.clear()
    for (const entry of this.entries.values()) {
      entry.cleanup()
      entry.marker.remove()
    }
    this.entries.clear()
  }
}
