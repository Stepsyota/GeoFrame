import { Protocol } from 'pmtiles'
import { setWorkerUrl } from 'maplibre-gl'
import * as maplibregl from 'maplibre-gl'
import workerUrl from 'maplibre-gl/dist/maplibre-gl-worker.mjs?worker&url'

setWorkerUrl(workerUrl)

let pmtilesProtocol: Protocol | null = null

/** Register the pmtiles:// protocol once for the app lifetime. */
export const ensurePmtilesProtocol = () => {
  if (pmtilesProtocol) {
    return
  }
  pmtilesProtocol = new Protocol()
  maplibregl.addProtocol('pmtiles', (request, abortController) =>
    pmtilesProtocol!.tile(request, abortController),
  )
}
