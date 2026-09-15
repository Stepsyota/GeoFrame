import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    host: true,
    proxy: {
      // Backend port is read from VITE_BACKEND_PORT env var (default 8080).
      // Use --skip-tls for dev so Vite can proxy plain HTTP without
      // fighting Node.js's strict self-signed-cert validation.
      //
      //   geoframe serve --skip-tls --port 8080 --source ./Photos --data-dir ./local
      //   npm run dev          ← proxies to http://localhost:8080
      '/api': {
        target: `http://localhost:${process.env.VITE_BACKEND_PORT ?? 8080}`,
        changeOrigin: true,
      },
      '/ws': {
        target: `ws://localhost:${process.env.VITE_BACKEND_PORT ?? 8080}`,
        ws: true,
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
})
