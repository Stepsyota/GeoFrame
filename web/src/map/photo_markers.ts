import * as maplibregl from 'maplibre-gl'

import type { MapFeature, MapFeatureProperties } from '../types/map'

const toAbsoluteUrl = (url: string) => new URL(url, window.location.origin).href

const appendMarkerImage = (
  button: HTMLButtonElement,
  primary: string,
  fallback?: string | null,
) => {
  const image = document.createElement('img')
  image.alt = ''
  image.draggable = false
  image.decoding = 'async'
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
}

export const createPhotoMarkerElement = (props: MapFeatureProperties): HTMLButtonElement => {
  const button = document.createElement('button')
  button.type = 'button'
  button.className = props.cluster ? 'map-photo-marker map-photo-cluster' : 'map-photo-marker'
  button.setAttribute(
    'aria-label',
    props.cluster ? `Open cluster of ${props.pointCount} photos` : 'Open photo',
  )

  const imageUrl = props.thumbnailUrl ?? props.previewUrl ?? null
  if (imageUrl) {
    appendMarkerImage(button, imageUrl, props.previewUrl)
  } else {
    const fallback = document.createElement('span')
    fallback.className = 'map-photo-fallback'
    fallback.textContent = props.cluster ? String(props.pointCount) : 'Photo'
    button.appendChild(fallback)
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

  return button
}

export class PhotoMarkerManager {
  private markers: Array<{ marker: maplibregl.Marker; cleanup: () => void }> = []

  sync(
    map: maplibregl.Map,
    features: MapFeature[],
    onFeatureClick: (feature: MapFeature) => void,
  ) {
    this.clear()

    for (const feature of features) {
      const button = createPhotoMarkerElement(feature.properties)
      const onClick = (event: MouseEvent) => {
        event.stopPropagation()
        onFeatureClick(feature)
      }
      button.addEventListener('click', onClick)

      // MapLibre positions the root element via inline transform — keep it free
      // of CSS transform/transition so markers stay locked to map coordinates.
      const root = document.createElement('div')
      root.className = 'map-photo-marker-root'
      root.appendChild(button)

      const marker = new maplibregl.Marker({ element: root, anchor: 'center' })
        .setLngLat(feature.geometry.coordinates)
        .addTo(map)

      this.markers.push({
        marker,
        cleanup: () => button.removeEventListener('click', onClick),
      })
    }
  }

  clear() {
    for (const entry of this.markers) {
      entry.cleanup()
      entry.marker.remove()
    }
    this.markers = []
  }
}
