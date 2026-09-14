# GeoFrame Web

React + TypeScript интерфейс GeoFrame.

```bash
npm install
npm run dev:demo  # UI с явными demo-данными
npm run dev       # UI с запросами к GeoFrame API
```

Проверки:

```bash
npm run lint
npm test
npm run build
```

Обычный dev server проксирует `/api` и `/ws` на `https://localhost:8443`.
Production build никогда не использует demo-данные без `VITE_DEMO_MODE=true`.
