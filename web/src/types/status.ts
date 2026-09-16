export interface LibraryStatus {
  source: string | null
  dataDir: string
  scanning: boolean
  assets: {
    active: number
    trashed: number
  }
  disk?: {
    availableMB: number
    totalMB: number
  }
}
