export interface ScanProgressEvent {
  type: 'scan_progress'
  scanning: boolean
  filesTotal: number
  filesDone: number
  metadataPercent: number
  thumbnailsPercent: number
  previewsPercent: number
  hashPercent: number
  pendingJobs: number
  currentFile: string | null
}
