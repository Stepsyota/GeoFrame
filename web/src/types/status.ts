export interface LibraryStatus {
  source: string | null
  dataDir: string
  scanning: boolean
  assets: {
    active: number
    trashed: number
  }
  cache: {
    thumbnailsDir: string
    previewsDir: string
    thumbnailsMB: number
    previewsMB: number
    totalMB: number
  }
  disk?: {
    availableMB: number
    totalMB: number
  }
}
