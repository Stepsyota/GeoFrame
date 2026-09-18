import { describe, expect, it } from 'vitest'

import { markerScaleForZoom, MARKER_FULL_SIZE_ZOOM, MARKER_MIN_SIZE_ZOOM } from './photo_markers'

describe('markerScaleForZoom', () => {
  it('uses the minimum scale at low zoom', () => {
    expect(markerScaleForZoom(MARKER_MIN_SIZE_ZOOM)).toBeCloseTo(0.42, 2)
  })

  it('reaches full scale near the max zoom', () => {
    expect(markerScaleForZoom(MARKER_FULL_SIZE_ZOOM)).toBeCloseTo(1, 2)
  })

  it('grows smoothly between low and high zoom', () => {
    const low = markerScaleForZoom(6)
    const mid = markerScaleForZoom(11)
    const high = markerScaleForZoom(15)
    expect(low).toBeLessThan(mid)
    expect(mid).toBeLessThan(high)
  })
})
