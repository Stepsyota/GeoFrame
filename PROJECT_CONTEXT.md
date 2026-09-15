# GeoFrame — контекст проекта для продолжения работы

Этот файл — основная точка входа для следующей модели или разработчика. Он описывает
не только целевую архитектуру, но и фактическое состояние репозитория на
15 сентября 2026 года.

Перед изменениями также прочитать:

- [`README.md`](README.md) — общая идея продукта;
- [`docs/CURRENT.md`](docs/CURRENT.md) — границы MVP и текущий план;
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — целевая архитектура;
- [`docs/CODING.md`](docs/CODING.md) — правила C++ и code review;
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — отложенные возможности.

## 1. Что строим

GeoFrame — self-hosted сервер и умная галерея для фото и видео.

Основной пользовательский сценарий:

> Пользователь указывает папку на HDD с фотографиями и видео, запускает один
> исполняемый файл GeoFrame и открывает библиотеку в браузере с компьютера или
> телефона.

GeoFrame:

- индексирует существующую папку без копирования оригиналов;
- извлекает EXIF/GPS и технические метаданные;
- вычисляет SHA-256 и в будущем perceptual hash;
- создаёт thumbnail и preview;
- показывает галерею и фотографии на карте;
- помогает разбирать точные дубликаты и серии снимков;
- предоставляет локальный HTTP API и Web UI;
- в конечном итоге обеспечивает безопасный доступ извне дома.

GeoFrame больше не является Telegram-ботом. Весь Telegram-код относится к старому
Python-прототипу и не должен развиваться.

## 2. Продуктовые принципы

### Self-hosted

Файлы и метаданные находятся на железе пользователя. GeoFrame не является SaaS и
не зависит от внешнего облачного хранилища.

### Index mode

Оригиналы остаются в выбранной пользователем source folder:

```text
/mnt/HDD/Photos/
  IMG_0001.HEIC
  IMG_0002.MOV
```

GeoFrame хранит отдельно только БД и производные данные:

```text
~/.local/share/geoframe/
  geoframe.db
  cache/
    thumbnails/
    previews/
  certs/
```

Импорт сейчас не означает copy или move.

### Работа с оригиналами

- Rename/move — не в MVP.
- Soft-delete — только статус `trashed` в БД, оригинал остаётся на месте.
- Permanent delete — будущая отдельная операция с явным подтверждением.
- Изменение EXIF (например, добавление GPS) возможно позже, только с предупреждением.
- Внешние изменения source folder в MVP не отслеживаются.
- Клиент никогда не передаёт raw filesystem path в API. Только `asset_id`, затем
  сервер получает путь из БД.

### Дата съёмки

Использовать только `Exif.Photo.DateTimeOriginal`. Если её нет — дата неизвестна.
Filesystem creation/modification time не является fallback, потому что это дата
копирования, а не съёмки.

### Решение остаётся за человеком

- Точные дубликаты определяются по SHA-256, но пользователь выбирает, что удалить.
- Серия — снимки с промежутком примерно до трёх секунд; пользователь выбирает кадры.
- GeoFrame не выбирает «лучший кадр» автоматически.
- AI и semantic search не входят в ближайший MVP.

### Медиа

- Фото, видео и Live Photo — полноценные объекты.
- Live Photo (`HEIC/JPEG + MOV`) должна выглядеть как один логический asset.
- Видео важно не меньше фотографий, но его metadata/preview pipeline ещё не сделан.
- Оригинал никогда не пережимается. Web UI использует preview, оригинал скачивается
  отдельно.

### Интерфейс

- Mobile-first, но desktop — также первоклассный сценарий.
- Карта является центральной возможностью, а не декоративным экраном.
- Полный EXIF должен быть доступен on-demand, без обязательного хранения dump в БД.

### Бэкапы

GeoFrame не является backup-системой. Пользователь отвечает за сохранность source
folder. Позже документация должна явно разделить:

- originals/source folder — критично;
- `geoframe.db` — пользовательские метаданные и состояние;
- thumbnails/previews — можно пересоздать.

## 3. Зафиксированный стек

Backend/core:

- C++20;
- CMake;
- GoogleTest;
- SQLite для MVP;
- OpenSSL EVP для SHA-256;
- Exiv2 для EXIF;
- FFmpeg/FFprobe как runtime tools;
- reproc++ для безопасного cross-platform запуска subprocess без shell;
- Boost.Beast + Boost.Asio для будущих HTTP, WebSocket и TLS;
- nlohmann/json для API DTO — ещё не подключён;
- spdlog для логирования — ещё не подключён.

Frontend:

- React 19;
- TypeScript 6;
- Vite 8;
- plain CSS без UI framework;
- MapLibre запланирован, но ещё не установлен;
- Vitest + Testing Library;
- ESLint.

Python и Node.js допустимы для разработки/build tooling. В runtime целевого backend
Python не нужен. React build позднее должен отдаваться тем же бинарником GeoFrame.

## 4. Архитектурный стиль

GeoFrame — modular monolith:

```text
geoframe (один процесс и один deployable binary)
├── cli
├── server
├── worker
├── media
├── storage
├── db
└── core
```

Границы похожи на микросервисы, но для пользователя нет Docker Compose, supervisor
или набора отдельных процессов.

Все C++ модули — отдельные static library targets. У каждого модуля собственные:

```text
modules/<name>/
  CMakeLists.txt
  include/<name>/
  src/
```

Лишнего каталога `include/geoframe/` быть не должно. Заголовки используют `.hpp`.

## 5. Структура репозитория

```text
apps/geoframe/        main executable
cmake/                CMake helpers и шаблон встроенных migrations
migrations/           SQL migrations
modules/
  core/               domain structs и repository interfaces
  storage/            scanner и определение типа файла
  media/              SHA-256, EXIF, process runner, preview generation
  db/                 SQLite RAII, migrations, repositories
  worker/             scan service, dispatcher, handlers, worker pool
  server/             пока placeholder
  cli/                пока только базовый CLI / version
tests/                 C++ unit и integration tests
web/                   React приложение
app/                   старый Python/Telegram prototype
requirements.txt       зависимости старого Python prototype
```

Python-прототип пока физически присутствует. Не смешивать его удаление с несвязанной
фичей; удалить отдельным коммитом после переноса нужных сценариев.

## 6. Что уже реализовано

### CMake и качество

- C++20 project skeleton.
- Отдельные static targets для модулей.
- Warnings: `-Wall -Wextra -Wpedantic`.
- `.clang-format`.
- Базовый GitHub Actions workflow: build, CTest, format.
- `geoframe --version` выводит `GeoFrame 0.1.0`.

Локально `clang-format` ранее отсутствовал, поэтому format-check выполнялся только
в CI. Проверить его наличие перед следующей локальной проверкой.

### Core

Основные domain-типы:

- `NewAsset`;
- `Asset`;
- `AssetMetadata`;
- `GeoPoint`;
- `MediaType`;
- `AssetStatus`;
- `Job`, `JobType`, `JobStatus`;
- `IAssetRepository`;
- `IJobRepository`.

Asset ID — monotonic SQLite integer.

### SQLite

Реализованы:

- RAII `Database`;
- RAII `Statement`;
- RAII `Transaction`;
- prepared statements;
- foreign keys;
- WAL;
- busy timeout;
- `SQLITE_OPEN_FULLMUTEX`;
- встроенный migration runner через `PRAGMA user_version`;
- `SqliteAssetRepository`;
- `SqliteJobRepository`.

Актуальные migrations:

```text
001_initial.sql                 assets
002_jobs.sql                    persistent job queue
003_allow_duplicate_hashes.sql  SHA-256 больше не UNIQUE
004_asset_previews.sql          thumbnail_path и preview_path
```

Одинаковые SHA-256 должны быть разрешены: именно они формируют группы точных
дубликатов. Не возвращать `UNIQUE` для `assets.sha256`.

Asset repository умеет:

- create;
- find by id;
- find by source path;
- paginated list;
- set SHA-256;
- set normalized metadata;
- set thumbnail/preview paths;
- favorite;
- active/trashed status.

Job repository умеет:

- идемпотентный enqueue (`UNIQUE(asset_id, type)`);
- атомарный `claim_next()` через `UPDATE ... RETURNING`;
- mark done/failed;
- восстановление `processing → pending` после рестарта;
- чтение job по ID.

### Scanner

`DirectoryScanner`:

- рекурсивный;
- streaming/callback, без накопления списка файлов;
- пропускает symlinks;
- использует `skip_permission_denied`;
- считает filesystem errors.

Поддерживаемые расширения на этапе обнаружения:

```text
.jpg .jpeg .png .heic .mov
```

Определение пока основано на расширении. Реальный media parser позже должен проверять
содержимое.

`ScanService`:

- нормализует absolute source path;
- не создаёт asset повторно для известного `source_path`;
- создаёт asset для нового поддерживаемого файла;
- ставит первый `hash` job;
- считает unsupported/existing/created/errors.

### Media pipeline

Реализован streaming SHA-256 через OpenSSL EVP с буфером 64 KiB.

EXIF extractor через Exiv2 извлекает:

- `DateTimeOriginal`, нормализованный в `YYYY-MM-DDTHH:MM:SS`;
- реальные размеры изображения;
- camera make/model;
- GPS latitude/longitude;
- altitude.

Если EXIF отсутствует, optional-поля остаются пустыми.

Preview generator:

- запускает FFmpeg через reproc++, не через shell;
- ограничивает обе стороны изображения;
- создаёт директории кэша;
- пишет во временный JPEG;
- публикует готовый файл rename-операцией;
- сохраняет путь в SQLite;
- имеет timeout.

Текущий image pipeline:

```text
scan
  → hash
  → metadata
  → thumbnail
  → preview
```

Компоненты:

- `WorkerPool` на `std::jthread`;
- `JobDispatcher`;
- `HashJobHandler`;
- `MetadataJobHandler`;
- `PreviewJobHandler`.

Dispatcher ставит следующий этап только если соответствующий handler зарегистрирован.
Для video после hash пока нет следующего этапа.

Worker:

- ловит исключения job handler;
- переводит job в `failed` и сохраняет error;
- не завершает весь процесс из-за одного плохого файла;
- сохраняет fatal repository error для `rethrow_if_failed()`;
- восстанавливает прерванные jobs при старте.

### React UI

В `web/` готов первый самостоятельный UI slice:

- responsive desktop sidebar;
- mobile bottom navigation;
- gallery grid;
- группировка assets по дню;
- loading skeleton;
- empty state;
- API error state;
- media/favorite/video badges;
- типизированный API client;
- явный demo mode;
- production build без скрытого fallback на mock data.

Команды:

```bash
cd web
npm install
npm run dev:demo  # интерфейс с demo assets
npm run dev       # запросы к настоящему /api
npm run lint
npm test
npm run build
```

Vite dev proxy ожидает backend:

```text
https://localhost:8443
```

и проксирует:

```text
/api
/ws
```

## 7. Контракт, который уже ожидает frontend

Запрос:

```http
GET /api/assets?limit=50&offset=0&status=active
Accept: application/json
```

Ожидаемый ответ:

```json
{
  "items": [
    {
      "id": 1,
      "originalFilename": "IMG_4821.HEIC",
      "mediaType": "image",
      "status": "active",
      "capturedAt": "2026-09-13T18:42:00",
      "width": 4032,
      "height": 3024,
      "favorite": false,
      "thumbnailUrl": "/api/assets/1/thumbnail"
    }
  ],
  "total": 1,
  "limit": 50,
  "offset": 0
}
```

Поля с неизвестными значениями передаются как `null`. API использует camelCase,
внутренний C++ — snake_case.

Этот контракт пока существует только на frontend. C++ HTTP server и JSON DTO ещё
не реализованы.

## 8. HTTP/WebSocket — следующее крупное направление

Выбран Boost.Beast + Boost.Asio:

- REST/JSON;
- настоящий WebSocket для live progress;
- TLS;
- один процесс и один бинарник;
- Beast должен быть изолирован внутри `modules/server`.

WebSocket важен как продуктовое требование. Не заменять его SSE только потому, что
реализовать SSE проще.

Планируемый endpoint:

```text
WS /ws/events
```

Пример события:

```json
{
  "type": "scan_progress",
  "filesTotal": 51203,
  "filesDone": 34821,
  "metadataPercent": 78,
  "thumbnailsPercent": 52,
  "currentFile": "IMG_58392.HEIC"
}
```

Минимальный следующий vertical slice:

1. Подключить Boost.Beast/Asio и nlohmann/json.
2. Реализовать HTTP application/router abstraction внутри `server`.
3. Реализовать `GET /api/assets`.
4. Реализовать `GET /api/assets/{id}/thumbnail`.
5. Запустить сервер пока без полного setup wizard.
6. Подключить существующую React gallery к реальным данным.
7. Только затем добавить WebSocket progress.

TLS остаётся требованием MVP. Для LAN запланирован self-signed certificate,
генерируемый при первом запуске. Конкретный certificate bootstrap ещё не реализован.

## 9. Что ещё не реализовано

Backend/runtime:

- реальный CLI `serve`, `scan`, `status`, trash;
- config TOML и выбор source/data directory;
- application composition root;
- HTTP server;
- REST routes;
- JSON DTO;
- WebSocket;
- TLS/cert generation;
- static serving React build;
- progress/event bus;
- spdlog;
- disk-space warning.

Media:

- video metadata через ffprobe;
- video poster;
- browser-compatible video handling;
- Live Photo pairing;
- Apple Burst pairing;
- full EXIF on-demand endpoint/dump;
- perceptual hash;
- exact duplicate groups API;
- series detection;
- RAW;
- reverse geocoding;
- timezone.

UI:

- реальный backend connection;
- asset detail view;
- full EXIF view;
- original download;
- favorite mutation;
- trash/restore/permanent delete;
- MapLibre screen;
- duplicates screen;
- series screen;
- scan progress WebSocket UI;
- routing между экранами.

Infrastructure:

- frontend CI job;
- GCC/Clang matrix;
- clang-tidy;
- ASan;
- installer;
- bundled ffmpeg/ffprobe;
- cross-platform packaging.

## 10. Тесты и команды проверки

C++:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Последнее подтверждённое состояние перед созданием этого файла:

- C++: 33/33 tests passed;
- frontend: 1/1 test passed;
- frontend ESLint passed;
- frontend production build passed;
- npm audit: 0 vulnerabilities.

Exiv2:

- сначала ищется installed CMake package;
- если development package отсутствует, загружается pinned `v0.28.9`;
- первая конфигурация FetchContent может занять несколько минут;
- CMake helper явно задаёт `cxx_std_20` для GeoFrame targets, потому что Exiv2
  внутренне выводит/использует C++17 для собственных targets.

Runtime для media tests требует доступный `ffmpeg`.

Frontend:

```bash
npm --prefix web install
npm --prefix web run lint
npm --prefix web test
npm --prefix web run build
```

## 11. Стиль кода и предпочтения владельца

Главный язык владельца проекта — C++. Главная личная цель проекта — развивать C++
и навыки code review. Производительность также важна, но не требуется писать UI на C++.

Ориентир по авторскому стилю:

[`Stepsyota/Task`](https://github.com/Stepsyota/Task)

Сохранять:

- небольшие компоненты с одной ответственностью;
- явные orchestration-классы;
- constructor injection;
- `std::filesystem::path`;
- streaming processing;
- RAII вокруг C API и ресурсов;
- исключения с обработкой на границе приложения;
- подробный Doxygen для публичного API;
- понятный код без лишней абстракции.

Дополнительные правила GeoFrame:

- namespace `geoframe::<module>`;
- `.hpp` для headers;
- `snake_case` для функций/переменных;
- `PascalCase` для классов;
- никаких raw `new/delete`;
- prepared statements;
- никакого shell command concatenation;
- user paths не логировать на `info`;
- не вводить generic `Result<T>` без доказанной необходимости.

## 12. Git workflow

Владелец самостоятельно выполняет `git add` и `git commit`. Агент не должен коммитить,
если пользователь отдельно этого не попросил.

После каждого законченного этапа дать готовый commit message в авторском стиле:

```text
ADD: short English description
FIX: short English description
UPDATE: short English description
```

Недавние этапы:

```text
ADD: C++ project structure and architecture docs
ADD: SQLite database and asset repository
ADD: directory scanner and persistent job queue
ADD: worker pool and SHA256 file processing
ADD: EXIF metadata extraction and job pipeline
ADD: thumbnail and preview generation pipeline
ADD: React gallery UI with demo mode
```

Предпочтительны небольшие тематические коммиты. Не смешивать feature, cleanup старого
Python и несвязанный refactor.

## 13. Важное состояние working tree

На момент создания этого файла последние семь этапов уже присутствуют в истории.
При этом перед созданием файла в working tree оставались изменения:

```text
M .gitignore
M README.md
M docs/ARCHITECTURE.md
M docs/CURRENT.md
```

Это накопленные актуализации документации/ignore rules, а не изменения, которые
нужно автоматически откатывать. Перед следующим коммитом проверить свежий
`git status`.

## 14. Явные запреты и ловушки

- Не возвращать Telegram в архитектуру.
- Не менять index mode на копирование файлов без отдельного продуктового решения.
- Не использовать filesystem timestamp как дату съёмки.
- Не делать SHA-256 уникальным.
- Не принимать filesystem path от HTTP-клиента.
- Не удалять оригинал при обычном trash.
- Не заменять WebSocket на SSE.
- Не добавлять Postgres «на будущее»: SQLite — осознанный MVP backend.
- Не начинать микросервисы/Docker: runtime — modular monolith.
- Не добавлять AI ради AI.
- Не скрывать mock fallback в production frontend.
- Не считать Python-прототип актуальной реализацией.

## 15. Рекомендуемый следующий шаг

Начать минимальный HTTP vertical slice:

```text
SQLite AssetRepository
        ↓
Asset DTO / JSON
        ↓
Boost.Beast route GET /api/assets
        ↓
React getAssets()
        ↓
реальная галерея
```

Сначала доказать end-to-end путь на HTTP без усложнения TLS/WebSocket internals.
При этом границы server сразу проектировать так, чтобы TLS и WebSocket добавлялись
без переписывания application services.
