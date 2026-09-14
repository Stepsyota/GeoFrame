# GeoFrame — архитектура

Зафиксированные технические решения. Продуктовый контекст — в [README.md](../README.md) и [CURRENT.md](CURRENT.md).

## Принципы

| Принцип | Решение |
|---------|---------|
| Ядро | C++20 — FS, вычисления, media pipeline, HTTP API |
| UI | React + TypeScript + Vite (отдельный `web/`) |
| Деплой (цель) | Один бинарник `geoframe`, пользователь делает минимум |
| Архитектурный стиль | **Modular monolith** — микросервисное мышление, монолитный деплой |
| БД (MVP) | SQLite, встроенная; абстракция для будущего PostgreSQL |
| Модель библиотеки | **Index mode** — фото остаются на диске пользователя |
| Python-прототип | Удаляется, не развивается |

---

## Высокоуровневая схема

```text
┌──────────────────────────────────────────────────────────────────┐
│                     geoframe (single binary)                      │
│                                                                   │
│  ┌─────────┐  ┌────────────────┐  ┌─────────────────────────┐  │
│  │   CLI   │  │  HTTP Server   │  │    WebSocket hub          │  │
│  │         │  │  REST + TLS    │  │    (scan / job progress)  │  │
│  │ scan    │  │  + React static│  │                           │  │
│  │ serve   │  └───────┬────────┘  └────────────┬────────────┘  │
│  │ ...     │          │                        │                 │
│  └────┬────┘          │                        │                 │
│       │               ▼                        ▼                 │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                   Application services                      │  │
│  │  AssetService │ ScanService │ TrashService │ MapService ...  │  │
│  └────────────────────────────┬───────────────────────────────┘  │
│                               │                                  │
│         ┌─────────────────────┼─────────────────────┐            │
│         ▼                     ▼                     ▼            │
│  ┌─────────────┐      ┌─────────────┐      ┌──────────────┐     │
│  │ Worker pool │      │  SQLite DB  │      │ Media engine │     │
│  │  (threads)  │      │ + migrations│      │ Exiv2/ffmpeg │     │
│  └──────┬──────┘      └─────────────┘      └──────────────┘     │
│         │                                                        │
└─────────┼────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────┐    ┌──────────────────────────────┐
│  Source library (read)      │    │  GeoFrame data dir (write)   │
│  /mnt/HDD/Photos/**         │    │  ~/.local/share/geoframe/    │
│  Оригиналы НЕ копируются    │    │  geoframe.db                 │
│                             │    │  cache/thumbnails/           │
│                             │    │  cache/previews/             │
│                             │    │  certs/                      │
└─────────────────────────────┘    └──────────────────────────────┘

          ▲                                    ▲
          │                                    │
     Browser (React)                      filesystem
```

---

## Модель библиотеки: Index mode

GeoFrame **не копирует и не перемещает** оригиналы. Пользователь указывает папку с фото — GeoFrame индексирует её.

```text
Source folder          GeoFrame data dir
/mnt/HDD/Photos/       ~/.local/share/geoframe/   (или путь из setup)
  IMG_0001.HEIC          geoframe.db
  IMG_0002.MOV           cache/thumbnails/42.jpg
  ...                    cache/previews/42.jpg
                         certs/
```

### Правила работы с файлами

| Операция | Поведение |
|----------|-----------|
| Индексация | Read-only доступ к source folder |
| Rename / move оригинала | ❌ Не в MVP |
| Modify EXIF (GPS) | 🔜 С подтверждением пользователя; изменяет оригинал |
| Soft-delete | Флаг `trashed` в DB; файл на диске не трогается |
| Permanent delete | Отдельная операция + подтверждение; **удаляет файл из source folder** |
| Внешние изменения FS | Не отслеживаем в MVP |

### Asset ID и пути

- **ID**: monotonic integer (`1, 2, 3...`)
- **Source path**: абсолютный путь в DB (`source_path`)
- **Cache paths**: `cache/thumbnails/{id}.jpg`, `cache/previews/{id}.jpg`
- Оригинальное имя файла — в metadata, не в cache path

### Path traversal

Жёсткое правило: клиент передаёт только `asset_id`. Сервер резолвит путь через DB. Никогда не принимать raw path от клиента.

---

## Модули и сборка

Каждый модуль — отдельная static library со своим `CMakeLists.txt`, `src/` и `include/`.

```text
GeoFrame/
  cmake/                    # общие CMake-функции, зависимости
  modules/
    core/                   # domain types, interfaces, errors
      CMakeLists.txt
      src/
      include/core/
    storage/                # filesystem, directory walk, paths
    media/                  # exif, hash, thumbnail, ffmpeg wrappers
    db/                     # sqlite, repositories, migrations
    worker/                 # job queue processor
    server/                 # HTTP, WebSocket, routes, TLS
    cli/                    # command parsing, entrypoints
  apps/
    geoframe/               # main binary, links all modules
      CMakeLists.txt
      src/main.cpp
  web/                      # React + TypeScript + Vite
  migrations/               # SQL migration files
  tests/                    # GTest (unit + integration)
  third_party/              # FetchContent: httplib, json, gtest, spdlog
  docs/
```

### CMake targets (MVP)

```text
geoframe_core      STATIC
geoframe_storage   STATIC  → core
geoframe_media     STATIC  → core, storage
geoframe_db        STATIC  → core
geoframe_worker    STATIC  → core, media, db, storage
geoframe_server    STATIC  → core, db
geoframe_cli       STATIC  → core, db, storage, worker, server
geoframe           EXEC    → all
```

Модули общаются через C++ interfaces. JSON (nlohmann) — для HTTP API и внутренних сообщений между слоями, где удобно.

---

## База данных

### SQLite (MVP)

- Один файл: `{data_dir}/geoframe.db`
- WAL mode для concurrent readers + один writer
- GeoFrame создаёт и мигрирует DB при первом запуске

### Абстракция для PostgreSQL

```text
IAssetRepository
IJobRepository
IDatabaseConnection
  ├── SqliteConnection    (MVP)
  └── PgConnection        (future)
```

Переход на PostgreSQL — новая реализация интерфейсов + те же SQL-миграции (с адаптацией типов). Не «временное решение», а осознанный выбор под single-binary.

### Миграции

```text
migrations/
  001_initial.sql
  002_add_phash.sql
  ...
```

При старте: проверить `PRAGMA user_version` → применить недостающие. Всё в C++, без внешних инструментов.

### EXIF storage

- **В DB**: нормализованные поля (`captured_at`, `gps_lat`, `camera`, `width`, `height`, ...)
- **On-demand**: полный EXIF читается из оригинала при запросе `GET /api/assets/{id}/exif` (может быть медленно — это ок)

### Основные таблицы (концепт)

```sql
assets          -- id, source_path, sha256, status, captured_at, gps, ...
asset_images    -- phash, media-specific
asset_videos    -- duration, codec, ...
live_photos     -- image_asset_id + video_asset_id
jobs            -- id, asset_id, type, status, error, created_at
favorites       -- asset_id (или flag в assets)
duplicate_groups / series_groups  -- MVP phase
```

---

## Job pipeline

Фоновая обработка через таблицу `jobs` + worker thread pool.

```text
Scan → discover file → create asset (pending)
  → [hash] → [metadata] → [thumbnail] → [preview] → [phash] → [series_detect] → done
```

### Job types

| Type | Описание |
|------|----------|
| `hash` | SHA-256 через OpenSSL |
| `metadata` | EXIF/GPS через Exiv2 |
| `thumbnail` | 300–500 px |
| `preview` | 1600–2500 px |
| `phash` | Perceptual hash |
| `series_detect` | Группировка по time proximity |

### Worker pool

- **Threads** (не processes) — shared memory, проще с SQLite WAL
- HTTP server — отдельный thread(s), не блокируется workers
- При рестарте: jobs в статусе `pending` / `processing` → продолжить
- Параллелизм по умолчанию: `hardware_concurrency() - 1`, настраивается в config

### Прогресс

WebSocket endpoint `/ws/events` пушит:

```json
{
  "type": "scan_progress",
  "files_total": 51203,
  "files_done": 34821,
  "metadata_pct": 78,
  "thumbnails_pct": 52,
  "current_file": "IMG_58392.HEIC"
}
```

---

## HTTP API

### Стек

- **Boost.Beast + Boost.Asio** — HTTP + WebSocket + TLS
- Beast изолирован внутри `server`: сложность сетевого кода не протекает в application layer
- **REST + JSON**
- Self-signed TLS cert генерируется при первом запуске → `certs/`

### Endpoints (MVP)

```text
GET  /api/assets                    # list (pagination)
GET  /api/assets/{id}               # single asset metadata
GET  /api/assets/{id}/exif          # full EXIF on-demand
GET  /api/assets/{id}/thumbnail     # stream image
GET  /api/assets/{id}/preview       # stream image
GET  /api/assets/{id}/original       # stream original file
POST /api/assets/{id}/favorite      # toggle
POST /api/assets/{id}/trash         # soft-delete
POST /api/assets/{id}/restore       # undo trash
DELETE /api/assets/{id}             # permanent delete (with confirmation header/body)
GET  /api/map/clusters              # GeoJSON clusters
GET  /api/duplicates                # duplicate groups
GET  /api/series                    # photo series
POST /api/scan                      # trigger scan
GET  /api/status                    # library stats, disk usage
WS   /ws/events                     # live progress
```

Static React build отдаётся из `web/dist/` тем же HTTP server.

---

## Media engine

### Библиотеки и инструменты

| Задача | Инструмент |
|--------|------------|
| SHA-256 | OpenSSL |
| EXIF (фото) | Exiv2 (primary) |
| Видео metadata | ffprobe (subprocess) |
| Thumbnail / preview (фото) | ffmpeg (subprocess), позже libvips |
| Видео poster frame | ffmpeg `-ss 0 -vframes 1` |
| HEIC | ffmpeg / libheif через ffmpeg |
| phash | C++ implementation (MVP) |
| Exotic metadata fallback | exiftool (subprocess, optional) |

Внешние бинарники (ffmpeg, ffprobe) — bundled в инсталлер.

### Поддерживаемые форматы (MVP)

- JPEG, HEIC, PNG, MOV
- Live Photo: HEIC + MOV → один логический asset
- Неизвестные файлы: warning в scan report, не удаляем

---

## Frontend

```text
web/
  src/
    components/
    pages/          # Gallery, AssetView, Map, Duplicates, Series
    api/            # fetch wrappers
    hooks/          # useWebSocket, etc.
  package.json
  vite.config.ts
  tsconfig.json
```

| Технология | Выбор |
|------------|-------|
| Framework | React |
| Language | TypeScript |
| Build | Vite |
| Map | MapLibre GL |
| Styling | TBD (CSS modules / Tailwind) |

Кластеризация для карты — на C++ (GeoJSON endpoint), MapLibre только рендерит.

---

## CLI

```text
geoframe serve [--config config.toml]     # start HTTP + workers
geoframe scan <path>                      # index directory
geoframe status                           # library stats
geoframe trash list                       # trashed assets
geoframe trash empty                      # permanent delete all trashed (confirmation)
```

В MVP `serve` — основной режим. Остальные команды — для отладки и автоматизации.

---

## Конфигурация

Оба варианта: CLI flags + TOML файл.

```toml
# ~/.config/geoframe/config.toml  (или --config path)

[data]
dir = "~/.local/share/geoframe"

[library]
source = "/mnt/HDD/Photos"

[server]
host = "0.0.0.0"
port = 8443
tls = true

[worker]
threads = 7   # default: hardware_concurrency() - 1

[media]
thumbnail_max_px = 500
preview_max_px = 2000
```

При первом запуске без config — interactive setup (выбор source folder + data dir).

---

## Безопасность (MVP)

| Аспект | Решение |
|--------|---------|
| Auth | Нет в MVP |
| Network | `0.0.0.0` — вся LAN |
| TLS | Self-signed cert, auto-generated |
| Path traversal | Только asset_id → DB lookup |
| Permanent delete | Требует явного подтверждения |
| EXIF modify | 🔜 Требует подтверждения + warning |

---

## Зависимости

### C++ (build)

| Dependency | Способ |
|------------|--------|
| Boost.Beast / Asio | system package (позже — package manager) |
| nlohmann/json | FetchContent |
| spdlog | FetchContent |
| GTest | FetchContent |
| OpenSSL | system package |
| Exiv2 | system package; pinned FetchContent fallback |
| SQLite3 | system package (libsqlite3-dev) |

### Runtime (bundled)

| Binary | Назначение |
|--------|------------|
| ffmpeg | thumbnails, previews, HEIC, video posters |
| ffprobe | video metadata |

### Frontend (dev)

| Dependency | Способ |
|------------|--------|
| Node.js 20+ | dev only |
| React, Vite, MapLibre | npm |

Python/Node — только для dev/build frontend, не в runtime C++.

---

## Порядок реализации

```text
 1. CMake skeleton + module structure + GTest + CI (partial)
 2. SQLite schema + migrations + Asset repository
 3. Directory scanner + job queue
 4. Hash (OpenSSL) + metadata (Exiv2)
 5. Thumbnail/preview (ffmpeg)
 6. HTTP API (assets list, single asset, stream preview)
 7. React gallery (minimal)
 8. WebSocket progress
 9. Map endpoint (GeoJSON) + MapLibre UI
10. Duplicates + series detection + UI
```

Целевой срок «первого wow»: 1–2 месяца.

---

## Что сознательно НЕ в MVP

- PostgreSQL (через абстракцию — легко добавить)
- Auth / passkey
- Web upload
- USB import
- Remote access извне дома
- Video transcoding
- Reverse geocoding
- RAW formats
- Отдельный reverse proxy (HTTP/WebSocket/TLS обслуживает GeoFrame)
- Отдельные процессы / Docker / настоящие микросервисы

---

## Связанные документы

- [CURRENT.md](CURRENT.md) — границы MVP, чеклист
- [ROADMAP.md](ROADMAP.md) — будущие фичи
- [CODING.md](CODING.md) — стиль кода, error handling, PR workflow
