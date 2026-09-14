import type { AssetPage, AssetSummary } from '../types/asset'

const thumbnail = (label: string, from: string, to: string) => {
  const svg = `
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 600">
      <defs>
        <linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
          <stop stop-color="${from}" />
          <stop offset="1" stop-color="${to}" />
        </linearGradient>
      </defs>
      <rect width="800" height="600" fill="url(#g)" />
      <circle cx="650" cy="120" r="64" fill="rgba(255,255,255,.35)" />
      <path d="M0 470 210 250l130 135 110-100 350 315H0Z" fill="rgba(20,24,20,.35)" />
      <text x="38" y="64" fill="white" font-family="system-ui" font-size="28">${label}</text>
    </svg>`
  return `data:image/svg+xml,${encodeURIComponent(svg)}`
}

const item = (
  id: number,
  name: string,
  capturedAt: string,
  colors: [string, string],
  favorite = false,
): AssetSummary => ({
  id,
  originalFilename: name,
  mediaType: 'image',
  status: 'active',
  capturedAt,
  width: 4032,
  height: 3024,
  favorite,
  thumbnailUrl: thumbnail(name.replace(/\..+$/, ''), ...colors),
})

const items = [
  item(1, 'IMG_4821.HEIC', '2026-09-13T18:42:00', ['#d38754', '#51483c'], true),
  item(2, 'IMG_4822.HEIC', '2026-09-13T18:43:12', ['#91a889', '#31483d']),
  item(3, 'IMG_4823.HEIC', '2026-09-13T18:43:14', ['#7897ab', '#283640']),
  item(4, 'IMG_4818.HEIC', '2026-09-13T16:20:00', ['#c3a879', '#5a452e']),
  item(5, 'IMG_4812.HEIC', '2026-09-13T12:08:34', ['#897f72', '#272b26']),
  item(6, 'IMG_4799.HEIC', '2026-09-12T20:17:02', ['#7d6b8d', '#24202b'], true),
  item(7, 'IMG_4790.HEIC', '2026-09-12T17:04:11', ['#b98e68', '#57402e']),
  item(8, 'IMG_4784.HEIC', '2026-09-12T15:34:51', ['#6f948d', '#223936']),
  item(9, 'IMG_4765.HEIC', '2026-09-11T19:52:17', ['#bd765f', '#452d2a']),
  item(10, 'IMG_4758.HEIC', '2026-09-11T18:15:03', ['#778dba', '#252c46']),
  item(11, 'IMG_4741.HEIC', '2026-09-11T11:27:44', ['#a5a276', '#36372a']),
  item(12, 'IMG_4722.HEIC', '2026-09-10T09:01:19', ['#9b8270', '#372e2a']),
]

export const demoAssetPage: AssetPage = {
  items,
  total: items.length,
  limit: 50,
  offset: 0,
}
