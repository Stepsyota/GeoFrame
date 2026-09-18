import Supercluster from 'supercluster'
import type { PointFeature } from 'supercluster'

import type { MapFeature, MapFeatureProperties } from '../types/map'

/** Pixel radius for grouping markers — similar to Apple Photos map. */
const CLUSTER_RADIUS = 72
const CLUSTER_MAX_ZOOM = 22
/** Max GPS grouping radius at city-level zoom (meters). */
export const GEO_CLUSTER_METERS_MAX = 22
/** Min GPS grouping radius when fully zoomed in (meters). */
export const GEO_CLUSTER_METERS_MIN = 10

/** Smoothly tighten geo grouping as the user zooms in. */
export const geoClusterRadiusForZoom = (zoom: number): number => {
  const t = Math.min(1, Math.max(0, (zoom - 8) / 10))
  return GEO_CLUSTER_METERS_MAX - (GEO_CLUSTER_METERS_MAX - GEO_CLUSTER_METERS_MIN) * t
}

interface ClusterProps {
  assetId: number
  assetIds: number[]
  thumbnailUrl: string | null
  previewUrl: string | null
  capturedAt: string | null
  mediaType: 'image' | 'video'
  favorite: boolean
}

const captureSortKey = (capturedAt: string | null | undefined): string =>
  capturedAt ?? '9999-12-31T23:59:59Z'

/** Pick the earliest photo in a group for the cluster cover image. */
export const pickCoverProperties = (
  group: MapFeatureProperties[],
): MapFeatureProperties => {
  const assetIds = [...new Set(group.flatMap((props) => props.assetIds))]
  const cover = group.reduce((best, current) =>
    captureSortKey(current.capturedAt) < captureSortKey(best.capturedAt) ? current : best,
  )
  return {
    cluster: assetIds.length > 1,
    pointCount: assetIds.length,
    assetIds,
    assetId: cover.assetId ?? assetIds[0],
    capturedAt: cover.capturedAt ?? null,
    mediaType: cover.mediaType ?? 'image',
    favorite: group.some((props) => props.favorite),
    thumbnailUrl: cover.thumbnailUrl ?? null,
    previewUrl: cover.previewUrl ?? null,
  }
}

type ClusterPoint = PointFeature<ClusterProps>

const toClusterProps = (props: MapFeatureProperties): ClusterProps => ({
  assetId: props.assetId ?? props.assetIds[0] ?? 0,
  assetIds: [...props.assetIds],
  thumbnailUrl: props.thumbnailUrl ?? null,
  previewUrl: props.previewUrl ?? null,
  capturedAt: props.capturedAt ?? null,
  mediaType: props.mediaType ?? 'image',
  favorite: props.favorite ?? false,
})

const toMapFeature = (feature: ClusterPoint): MapFeature => {
  const [lon, lat] = feature.geometry.coordinates
  const props = feature.properties
  const isCluster = Boolean(
    props && typeof props === 'object' && 'cluster' in props && props.cluster === true,
  )
  const pointCount =
    props && typeof props === 'object' && 'point_count' in props && typeof props.point_count === 'number'
      ? props.point_count
      : 1
  const clusterProps = props as ClusterProps & { cluster_id?: number }

  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [lon, lat] },
    properties: {
      cluster: isCluster,
      pointCount,
      assetIds: clusterProps.assetIds,
      assetId: clusterProps.assetId,
      mediaType: clusterProps.mediaType,
      favorite: clusterProps.favorite,
      capturedAt: clusterProps.capturedAt,
      thumbnailUrl: clusterProps.thumbnailUrl,
      previewUrl: clusterProps.previewUrl,
      clusterId: isCluster ? clusterProps.cluster_id : undefined,
    },
  }
}

export const haversineMeters = (
  lon1: number,
  lat1: number,
  lon2: number,
  lat2: number,
): number => {
  const earthRadius = 6_371_000
  const toRadians = (degrees: number) => (degrees * Math.PI) / 180
  const dLat = toRadians(lat2 - lat1)
  const dLon = toRadians(lon2 - lon1)
  const a =
    Math.sin(dLat / 2) ** 2 +
    Math.cos(toRadians(lat1)) * Math.cos(toRadians(lat2)) * Math.sin(dLon / 2) ** 2
  return 2 * earthRadius * Math.asin(Math.sqrt(a))
}

const mergeFeatureGroup = (group: MapFeature[]): MapFeature => {
  if (group.length === 1) {
    return group[0]
  }

  let weightSum = 0
  let lonSum = 0
  let latSum = 0

  for (const feature of group) {
    const weight = feature.properties.pointCount
    const [lon, lat] = feature.geometry.coordinates
    lonSum += lon * weight
    latSum += lat * weight
    weightSum += weight
  }

  return {
    type: 'Feature',
    geometry: {
      type: 'Point',
      coordinates: [lonSum / weightSum, latSum / weightSum],
    },
    properties: pickCoverProperties(group.map((feature) => feature.properties)),
  }
}

/** Group markers that share almost the same GPS fix, even at high zoom. */
export const mergeNearbyFeatures = (
  features: MapFeature[],
  radiusMeters: number,
): MapFeature[] => {
  if (features.length <= 1) {
    return features
  }

  const parent = features.map((_, index) => index)
  const find = (index: number): number => {
    if (parent[index] === index) {
      return index
    }
    parent[index] = find(parent[index])
    return parent[index]
  }
  const unite = (left: number, right: number) => {
    const rootLeft = find(left)
    const rootRight = find(right)
    if (rootLeft !== rootRight) {
      parent[rootRight] = rootLeft
    }
  }

  for (let i = 0; i < features.length; i += 1) {
    const [lon1, lat1] = features[i].geometry.coordinates
    for (let j = i + 1; j < features.length; j += 1) {
      const [lon2, lat2] = features[j].geometry.coordinates
      if (haversineMeters(lon1, lat1, lon2, lat2) <= radiusMeters) {
        unite(i, j)
      }
    }
  }

  const groups = new Map<number, MapFeature[]>()
  for (let i = 0; i < features.length; i += 1) {
    const root = find(i)
    const bucket = groups.get(root) ?? []
    bucket.push(features[i])
    groups.set(root, bucket)
  }

  return [...groups.values()].map(mergeFeatureGroup)
}

export class MapClusterIndex {
  private index = new Supercluster<ClusterProps, ClusterProps>({
    radius: CLUSTER_RADIUS,
    maxZoom: CLUSTER_MAX_ZOOM,
    minPoints: 2,
    map: (props) => ({
      assetId: props.assetId,
      assetIds: [...props.assetIds],
      thumbnailUrl: props.thumbnailUrl,
      previewUrl: props.previewUrl,
      capturedAt: props.capturedAt,
      mediaType: props.mediaType,
      favorite: props.favorite,
    }),
    reduce: (accumulated, props) => {
      accumulated.assetIds.push(...props.assetIds)
      const useProps =
        props.thumbnailUrl &&
        (!accumulated.thumbnailUrl ||
          captureSortKey(props.capturedAt) < captureSortKey(accumulated.capturedAt))
      if (useProps) {
        accumulated.thumbnailUrl = props.thumbnailUrl
        accumulated.previewUrl = props.previewUrl
        accumulated.assetId = props.assetId
        accumulated.mediaType = props.mediaType
        accumulated.capturedAt = props.capturedAt
      }
      accumulated.favorite = accumulated.favorite || props.favorite
    },
  })

  private pointCount = 0

  load(features: MapFeature[]) {
    this.pointCount = features.length
    const points: ClusterPoint[] = features.map((feature) => ({
      type: 'Feature',
      geometry: feature.geometry,
      properties: toClusterProps(feature.properties),
    }))
    this.index.load(points)
  }

  query(bounds: [number, number, number, number], zoom: number): MapFeature[] {
    const clusters = this.index.getClusters(bounds, Math.floor(zoom)).map(toMapFeature)
    return mergeNearbyFeatures(clusters, geoClusterRadiusForZoom(zoom))
  }

  expansionZoom(clusterId: number): number {
    return this.index.getClusterExpansionZoom(clusterId)
  }

  totalPoints(): number {
    return this.pointCount
  }
}
