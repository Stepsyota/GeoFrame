import type { ExifPayload } from '../types/exif'

const demoMode = import.meta.env.VITE_DEMO_MODE === 'true'

const demoExif: ExifPayload = {
  mediaType: 'image',
  tags: [
    { key: 'Exif.Image.Make', value: 'Apple' },
    { key: 'Exif.Image.Model', value: 'iPhone 15 Pro' },
    { key: 'Exif.Photo.DateTimeOriginal', value: '2026:09:14 20:30:00' },
    { key: 'Exif.Photo.ExposureTime', value: '1/120 sec.' },
    { key: 'Exif.Photo.FNumber', value: 'F1.8' },
    { key: 'Exif.Photo.ISOSpeedRatings', value: '64' },
    { key: 'Exif.Photo.FocalLength', value: '6.8 mm' },
  ],
}

export const getAssetExif = async (id: number, signal?: AbortSignal): Promise<ExifPayload> => {
  if (demoMode) {
    return demoExif
  }

  const response = await fetch(`/api/assets/${id}/exif`, {
    headers: { Accept: 'application/json' },
    signal,
  })

  if (!response.ok) {
    throw new Error(`GeoFrame EXIF API returned ${response.status}`)
  }

  const body = (await response.json()) as ExifPayload
  return {
    tags: Array.isArray(body.tags) ? body.tags : [],
    mediaType: body.mediaType === 'video' ? 'video' : 'image',
  }
}
