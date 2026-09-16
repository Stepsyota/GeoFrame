import { render, screen } from '@testing-library/react'
import { vi } from 'vitest'

import { App } from './App'
import { getAssets } from './api/assets'
import { demoAssetPage } from './demo/assets'

vi.mock('./api/assets', () => ({
  getAssets: vi.fn(),
}))

describe('App', () => {
  it('renders assets returned by the API', async () => {
    vi.mocked(getAssets).mockResolvedValue(demoAssetPage)
    render(<App />)
    // Gallery is the default page

    expect(await screen.findByLabelText('Open IMG_4821.HEIC')).not.toBeNull()
    expect(screen.getByText('12 assets')).not.toBeNull()
  })
})
