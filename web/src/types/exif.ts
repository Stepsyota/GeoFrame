export interface ExifTag {
  key: string
  value: string
}

export interface ExifPayload {
  tags: ExifTag[]
  mediaType: 'image' | 'video'
}
