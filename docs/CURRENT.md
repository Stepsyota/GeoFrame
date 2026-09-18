# GeoFrame — текущая задача

Документ фиксирует **что делаем сейчас**: пивот от Telegram-бота к self-hosted серверу и границы MVP.

## Контекст

Проект начинался как Telegram-бот для публикации фото в темы по дням. Концепция изменилась:

| Было | Стало |
|------|-------|
| Telegram-бот | HTTP-сервер + Web UI |
| Загрузка через Telegram | Импорт из папки / USB (позже) |
| Фото как контент для чата | Фото как личная медиабиблиотека |
| Облако через Telegram | Self-hosted на своём железе |

**Telegram полностью убираем** — он только добавляет ограничений.

## Цель текущего этапа

Подготовить фундамент self-hosted photo manager:

1. Зафиксировать продуктовое видение (этот набор документов) ✅
2. Зафиксировать архитектуру (C++ modular monolith + React) ✅
3. Удалить Python-прототип, начать C++ с нуля
4. Реализовать MVP

## Python-прототип

Удалён. Вся логика (scanner, metadata, hash, CRUD) перенесена в C++ модули.

## Технологический стек (зафиксирован)

| Слой | Технология |
|------|------------|
| Core | C++20, CMake, GTest |
| HTTP | Boost.Beast/Asio (REST + WebSocket + TLS) |
| DB | SQLite (абстракция → PostgreSQL позже) |
| Media | Exiv2, OpenSSL, ffmpeg (subprocess) |
| Logging | spdlog |
| UI | React + TypeScript + Vite + MapLibre |

Подробности: [ARCHITECTURE.md](ARCHITECTURE.md), [CODING.md](CODING.md).

## MVP — границы первой версии

### Входит в MVP

#### Библиотека (index mode)

- [x] Указать source folder — GeoFrame индексирует, **не копирует** оригиналы (CLI `--source`; путь в Settings UI)
- [x] GeoFrame data dir (по умолчанию `~/.local/share/geoframe/`, настраивается; отображается в Settings UI)
- [x] Cache: `thumbnails/`, `previews/` в data dir (пути и размер в Settings)
- [x] Повторный scan: не создавать asset повторно для того же `source_path`
- [ ] Не отслеживать внешние изменения файлов

#### Индексация

- [x] Фоновая обработка; UI доступен сразу (WorkerPool запускается до HTTP server)
- [x] Прогресс сканирования (файлы, % metadata, % thumbnails — WebSocket `/ws/events`)
- [x] Дата съёмки изображения из EXIF; если нет — `unknown` (не fallback на filesystem)
- [x] SHA-256 каждого оригинала
- [x] Нормализованные метаданные изображений и видео (Exiv2 / ffprobe)
- [x] Thumbnail и Preview изображений с настраиваемым max size; видео: poster frame

#### Медиа

- [x] JPEG, HEIC, PNG, MOV (расширить позже)
- [x] Видео: poster frame, длительность, базовые метаданные (ffprobe + ffmpeg)
- [x] Live Photo как один объект (HEIC + MOV) — pairing при scan, скрытие MOV в галерее, воспроизведение в lightbox
- [x] Неизвестные файлы: учитывать в scan report, не удалять

#### Web UI

- [x] React/Vite-каркас галереи (mobile-first, desktop тоже; demo mode)
- [x] Подключение галереи к реальному HTTP API (dev proxy + production static serve)
- [x] Просмотр фото: дата, место, камера, полный EXIF, скачать оригинал
- [x] Избранное
- [x] Удаление → Trash
- [x] Карта с кластерами (GeoJSON endpoint + MapLibre)
- [x] Гибридная офлайн-карта (PMTiles z0–N локально + онлайн Carto на близком зуме)
- [x] CLI: `geoframe map download world` (extract из Protomaps planet, `--dry-run`, `--max-zoom`)

#### Инструменты

- [x] Точные дубликаты (SHA-256)
- [x] Серии (фото в пределах ~3 сек друг от друга)
- [x] Очистка корзины с подтверждением (`CLEAR TRASH`)

#### Инфраструктура

- [x] Один бинарник `geoframe` (modular monolith)
- [x] SQLite для метаданных (встроенная, self-bootstrap)
- [x] HTTPS (self-signed cert при первом запуске, OpenSSL)
- [x] Локальная сеть (LAN) — сервер на 0.0.0.0:8443
- [x] CLI (`geoframe serve`, `geoframe scan`, `geoframe status`)
- [x] Предупреждение при заполнении диска (< 1 GiB)

### Не входит в MVP

| Фича | Когда |
|------|-------|
| Web upload с телефона | Позже |
| USB-импорт с iPhone | Позже |
| Удалённый доступ извне | После MVP, но — главный конечный сценарий |
| Passkey / авторизация | После MVP |
| Sharing links | Roadmap |
| Фильтры (страна, камера, …) | После базовой галереи |
| Timeline-режим | Roadmap |
| AI / семантический поиск | Не планируется в ближайшем будущем |
| Reverse geocoding | Нужен для «докачки по стране» (см. раздел Карта ниже) |
| Докачка карты по странам с фото | Следующий этап после world z14 |
| z16–20 офлайн (вектор) | Нет в Protomaps; только свой билд или онлайн |
| Timezone | Отложено |
| RAW-форматы | Поддержка форматов — постепенно |
| Video transcoding | Отдаём оригинал, надеемся на браузер |
| Почти-дубликаты / визуально похожие | Roadmap |
| Ручное добавление GPS (modify EXIF) | Roadmap (с предупреждением) |
| Инсталлер «в один клик» | Цель, но не блокер для разработки |

## Модель данных (концепция)

### PhotoAsset

```
PhotoAsset
├── id (monotonic integer)
├── source_path (абсолютный путь к оригиналу на диске пользователя)
├── thumbnail_path (в cache GeoFrame data dir)
├── preview_path (в cache GeoFrame data dir)
├── captured_at (или unknown)
├── gps (lat, lon, altitude) или null
├── camera, dimensions, media_type
├── sha256, phash
├── normalized_metadata (в DB)
├── full_exif (on-demand из оригинала)
├── favorite (GeoFrame metadata)
└── status (active | trashed)
```

### Live Photo

```
LivePhoto
├── image_asset_id
└── video_asset_id
→ в UI: один объект с воспроизведением
```

### Series

```
PhotoSeries
├── id
├── asset_ids[]
└── detected_by: time_proximity (≤3 sec)
```

### Duplicates

```
DuplicateGroup
├── sha256
└── asset_ids[]
```

## Правила работы с файлами

```
Rename file       ❌ (не в MVP)
Move file         ❌ (не в MVP)
Modify EXIF       🔜 (добавить GPS — с подтверждением, меняет оригинал)
Soft-delete       ✅ → флаг в DB, файл на диске не трогается
Permanent delete  ✅ → отдельная операция + подтверждение, удаляет из source folder
```

GeoFrame читает source folder. Внешние изменения файлов в MVP не отслеживаются.

## Карта и офлайн-базис (актуальный план)

Зафиксировано по итогам работы над картой (сентябрь 2026). Детали установки и `/tmp` — в [SETUP.md](SETUP.md).

### Что уже реализовано

| Компонент | Описание |
|-----------|----------|
| **Гибридный режим** | Локальный PMTiles до `localMaxZoom`, выше — Carto (raster CDN или MVT через прокси) |
| **Один world-файл** | `<data-dir>/map/region.pmtiles` + `map/config.json` (`localMaxZoom`, `hybrid`) |
| **Staging на диске** | Большие загрузки в `<data-dir>/tmp/` (не в `/tmp` tmpfs); override: `GEOFRAME_TMP_DIR` |
| **HTTP Range** | Сервер отдаёт байтовые диапазоны PMTiles (MapLibre читает кусками) |
| **CLI** | `geoframe map download world [--max-zoom N] [--dry-run]` |
| **Источник** | Protomaps daily build (`build.protomaps.com/YYYYMMDD.pmtiles`), max **z15** в архиве |
| **Маркеры** | Кластеризация по гео, анимации, обложка кластера — самое раннее фото по `capturedAt` |

Текущая dev-копия: `local/map/region.pmtiles` ~**34 GB** (world **z0–13**).

### Порядок действий (согласовано)

1. **Сейчас:** докачать **мир до z14** (~**68 GB** итого, +34 GB к текущему файлу).
   ```bash
   geoframe map download world --data-dir ./local --max-zoom 14
   ```
2. **Потом:** докачивать **z15 по странам**, только если в стране есть хотя бы одно фото с GPS.
   - Без сложной кластерной логики — единица = **страна (ISO)**.
   - Для **России** (и других гигантов): не весь полигон страны, а **bbox всех фото в RU** + небольшой padding (одна формула, не DBSCAN).
3. **z16–20 офлайн:** из Protomaps **не вытащить** — в planet-архиве нет слоёв выше z15. До появления своего пайплайна — **гибрид + онлайн Carto** на близком зуме.

### Оценки размера (Protomaps, dry-run, сентябрь 2026)

| Что | Размер |
|-----|--------|
| Мир z0–13 | ~34 GB (текущее) |
| Мир z0–14 | ~**68 GB** |
| Мир z0–15 | ~**128 GB** (официальный planet) |
| Беларусь z14–15 | ~**1.0 GB** |
| Грузия z14–15 | ~**230 MB** |
| Россия, европ. часть z14–15 | ~**7 GB** |
| Россия, западнее Урала z14–15 | ~**12 GB** |

**z18–20 по стране** (оценка «если бы данные существовали», ~×4 объёма на каждый новый зум):

| Страна | z14–15 | z14–18 (оценка) |
|--------|--------|-----------------|
| Беларусь | 1 GB | ~55 GB |
| Грузия | 230 MB | ~13 GB |
| Россия, евр. часть | 7 GB | ~400 GB |

Вывод: страничная докачка на z14–15 **очень выгодна**; z18+ даже для одной страны — уже десятки–сотни GB и **недоступно** из текущего источника.

### Целевая схема хранения (следующая итерация)

```
<data-dir>/map/
  region.pmtiles          # мир z0–14 (или переименовать в world.pmtiles)
  regions/
    BY.pmtiles            # z14–15, только если есть фото в BY
    GE.pmtiles
    RU.pmtiles            # bbox по фото в России, не вся страна
  manifest.json           # список стран, bbox, bytes, updatedAt
```

Триггер докачки: после scan + metadata jobs → новая страна в GPS → одна задача / `geoframe map sync-countries` (ещё не реализовано).

### Ограничения и решения

| Проблема | Решение |
|----------|---------|
| `/tmp` на Fedora = tmpfs (RAM) | Staging в `<data-dir>/tmp/`; для cmake: `TMPDIR=$PWD/.cache/tmp` |
| Firefox OOM при карте | Упрощённый hybrid (один raster слой Carto), лимиты маркеров, без CSS transform на корне маркера |
| Carto free tier | 5M tiles/мес; для редкого зума вне офлайна обычно хватает; attribution обязателен |
| Определение страны по GPS | Natural Earth polygons (офлайн) или reverse geocoding — TBD |

### Команды (шпаргалка)

```bash
# Оценка без записи (~1–2 мин)
geoframe map download world --data-dir ./local --max-zoom 14 --dry-run

# Полная загрузка мира z14
geoframe map download world --data-dir ./local --max-zoom 14

# Сборка с большим TMPDIR
export TMPDIR="$PWD/.cache/tmp" && cmake --build build --target geoframe
```

## Следующие шаги (порядок работ)

1. **Документация** — vision, MVP, roadmap, architecture ✅
2. **CMake skeleton** — модули, GTest, partial CI ✅
3. **SQLite** — schema, migrations, Asset repository ✅
4. **Scanner + persistent job queue** — directory walk, restart-safe jobs ✅
5. **Worker pool + SHA-256 + image EXIF** — OpenSSL, Exiv2 ✅
6. **Preview generation** — thumbnails и previews через ffmpeg ✅
7. **Video pipeline** — ffprobe metadata, ffmpeg poster frame, video DB fields ✅
8. **HTTP API + CLI + TLS** — REST + Boost.Beast + self-signed cert + CLI subcommands ✅
9. **React gallery** — каркас, demo mode, real API + pagination ✅
10. **WebSocket progress** — live прогресс индексации (`/ws/events`) ✅
11. **Map** — GeoJSON endpoint + MapLibre ✅
12. **Duplicates + series** — детекция + UI ✅
13. **Удалить Python-прототип** ✅
14. **Lightbox + избранное + trash + EXIF** ✅
15. **Карта: hybrid PMTiles + маркеры + CLI download** ✅
16. **Карта: world z14** — в работе (ручная загрузка)
17. **Карта: `map sync-countries` + manifest + multi-PMTiles в UI** — следующий этап
18. **Страна по GPS** — Natural Earth / reverse geocoding для авто-докачки

## Критерий готовности MVP

> Закинул папку с фото на HDD → запустил GeoFrame → через браузер на телефоне в локальной сети вижу галерею, карту, могу открыть фото, посмотреть EXIF, найти дубликаты и разобрать серии.
