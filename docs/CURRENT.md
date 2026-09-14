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

## Что было в Python-прототипе (reference, затем удалить)

```
app/                  # scanner, metadata, hash, CRUD — логика переносится в C++
tests/                # тестовые сценарии — reference для GTest
```

Python-код не развиваем. Используем как reference при портировании, затем удаляем.

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

- [ ] Указать source folder — GeoFrame индексирует, **не копирует** оригиналы
- [ ] GeoFrame data dir (по умолчанию `~/.local/share/geoframe/`, настраивается)
- [ ] Cache: `thumbnails/`, `previews/` в data dir
- [ ] Повторный scan: только новые файлы (по hash)
- [ ] Не отслеживать внешние изменения файлов

#### Индексация

- [ ] Фоновая обработка; UI доступен сразу
- [ ] Прогресс сканирования (файлы, фото, видео, live photos, % metadata, % thumbnails)
- [ ] Дата съёмки из EXIF; если нет — `unknown` (не fallback на filesystem)
- [ ] SHA-256 каждого оригинала
- [ ] Нормализованные метаданные + raw metadata dump
- [ ] Thumbnail (300–500 px) и Preview (1600–2500 px)

#### Медиа

- [ ] JPEG, HEIC, PNG, MOV (расширить позже)
- [ ] Видео: poster frame, длительность, базовые метаданные
- [ ] Live Photo как один объект (HEIC + MOV)
- [ ] Неизвестные файлы: предупреждение, не удалять

#### Web UI

- [ ] Галерея (mobile-first, desktop тоже)
- [ ] Просмотр фото: дата, место, камера, полный EXIF, скачать оригинал
- [ ] Избранное
- [ ] Удаление → Trash
- [ ] Карта с кластерами (центральная фича MVP)

#### Инструменты

- [ ] Точные дубликаты (SHA-256)
- [ ] Серии (фото в пределах ~3 сек друг от друга)
- [ ] Очистка корзины с подтверждением (`CLEAR TRASH`)

#### Инфраструктура

- [x] Один бинарник `geoframe` (modular monolith)
- [x] SQLite для метаданных (встроенная, self-bootstrap)
- [ ] HTTPS (self-signed cert при первом запуске)
- [ ] Локальная сеть (LAN)
- [ ] CLI (`geoframe serve`, `geoframe scan`, ...)
- [ ] Предупреждение при заполнении диска

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
| Reverse geocoding | Решить позже |
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

## Следующие шаги (порядок работ)

1. **Документация** — vision, MVP, roadmap, architecture ✅
2. **CMake skeleton** — модули, GTest, partial CI ✅
3. **SQLite** — schema, migrations, Asset repository ✅
4. **Scanner + persistent job queue** — directory walk, restart-safe jobs ✅
5. **Worker pool + media engine** — threads, hash, EXIF, thumbnails (ffmpeg)
6. **HTTP API** — REST + WebSocket + TLS
7. **React gallery** — минимальный UI
8. **Map** — GeoJSON endpoint + MapLibre
9. **Duplicates + series** — детекция + UI
10. **Удалить Python-прототип**

## Критерий готовности MVP

> Закинул папку с фото на HDD → запустил GeoFrame → через браузер на телефоне в локальной сети вижу галерею, карту, могу открыть фото, посмотреть EXIF, найти дубликаты и разобрать серии.
