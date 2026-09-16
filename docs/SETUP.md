# GeoFrame — запуск и разработка

Руководство охватывает три сценария:
1. **Быстрый старт** — запустить GeoFrame локально с реальными данными.
2. **Разработка** — сборка, тесты, frontend dev.
3. **Справочник команд** — все опции CLI и npm-скрипты.

---

## Содержание

- [Зависимости](#зависимости)
- [Сборка C++ backend](#сборка-c-backend)
- [Запуск сервера](#запуск-сервера)
- [Сканирование библиотеки](#сканирование-библиотеки)
- [Frontend (React)](#frontend-react)
- [Разработка: цикл сборки и тестов](#разработка-цикл-сборки-и-тестов)
- [Справочник CLI](#справочник-cli)
- [Структура данных](#структура-данных)
- [Что сгенерируется автоматически](#что-сгенерируется-автоматически)

---

## Зависимости

### Системные пакеты (Linux)

На Fedora/RHEL:

```bash
sudo dnf install -y \
    cmake ninja-build gcc-c++ \
    openssl-devel sqlite-devel \
    ffmpeg ffprobe \
    exiv2-devel            # опционально, иначе FetchContent скачает сам
```

На Ubuntu/Debian:

```bash
sudo apt install -y \
    cmake ninja-build g++ \
    libssl-dev libsqlite3-dev \
    ffmpeg libexiv2-dev
```

> **Boost и зависимости** скачиваются автоматически через CMake FetchContent при первой сборке (~100 МБ, единоразово).

### Runtime

| Инструмент | Назначение |
|------------|------------|
| `ffmpeg`   | Создание thumbnail/preview и постеров видео |
| `ffprobe`  | Извлечение метаданных видеофайлов |
| `openssl`  | Генерация TLS-сертификата |

Убедитесь, что `ffmpeg` и `ffprobe` в `PATH`:

```bash
ffmpeg -version
ffprobe -version
```

### Frontend (опционально, только для разработки UI)

- Node.js 20+
- npm 10+

---

## Сборка C++ backend

```bash
# Клонировать репозиторий
git clone <repo-url> GeoFrame
cd GeoFrame

# Сконфигурировать (Debug — для разработки, Release — для запуска)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Собрать (первый раз скачает Boost — ~5 минут при медленном интернете)
cmake --build build --parallel

# Бинарник появится здесь:
# build/apps/geoframe/geoframe
```

Для разработки (с тестами, быстрее компиляция):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

---

## Запуск сервера

```bash
./build/apps/geoframe/geoframe serve \
    --source /path/to/your/photos \
    --data-dir ~/.local/share/geoframe \
    --port 8443
```

После запуска:

1. GeoFrame создаст `data-dir` если его нет.
2. Сгенерирует TLS-сертификат в `data-dir/certs/` (единоразово).
3. Автоматически проиндексирует `--source` папку (в фоне).
4. Откроет HTTPS-сервер на указанном порту.

Открыть в браузере: [https://localhost:8443](https://localhost:8443)

> **Самоподписанный сертификат** — браузер покажет предупреждение. Нажмите «Принять риск» / «Перейти на сайт» — это нормально для локального доступа.

### HTTP без TLS (dev-режим)

```bash
./build/apps/geoframe/geoframe serve \
    --source /path/to/photos \
    --skip-tls \
    --port 8080
```

Открыть: [http://localhost:8080](http://localhost:8080)

### Опции сервера

| Флаг | По умолчанию | Описание |
|------|-------------|----------|
| `--source <path>` | — | Папка с фото/видео (обязательна при первом запуске) |
| `--data-dir <path>` | `~/.local/share/geoframe` | Где хранить БД, кэш, сертификаты |
| `--host <addr>` | `0.0.0.0` | Адрес для прослушивания |
| `--port <n>` | `8443` | HTTPS-порт |
| `--threads <n>` | `CPU_count - 1` | Число worker-потоков |
| `--skip-tls` | `false` | HTTP вместо HTTPS (только для dev) |
| `--web-dir <path>` | `./web/dist` | Папка со сборкой React UI (см. [Production](#production-одним-процессом)) |

### Production: одним процессом

Соберите UI и запустите сервер — GeoFrame раздаёт и API, и статику:

```bash
# 1. Собрать backend (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 2. Собрать frontend
cd web
npm install
npm run build
cd ..

# 3. Запустить (из корня репозитория, чтобы нашёлся web/dist)
./build/apps/geoframe/geoframe serve \
    --source /path/to/your/photos \
    --data-dir ~/.local/share/geoframe \
    --web-dir web/dist \
    --port 8443
```

Открыть: [https://localhost:8443](https://localhost:8443) (или `--skip-tls --port 8080` для HTTP).

Для LAN-доступа с телефона сервер слушает `0.0.0.0` по умолчанию — откройте `https://<IP-вашего-ПК>:8443`.

Путь к UI можно задать через переменную окружения `GEOFRAME_WEB_DIR` вместо `--web-dir`.

---

## Сканирование библиотеки

Для разового сканирования без запуска HTTP-сервера:

```bash
./build/apps/geoframe/geoframe scan /path/to/photos \
    --data-dir ~/.local/share/geoframe
```

Команда:
- Рекурсивно обходит папку.
- Создаёт assets для новых файлов (`.jpg .jpeg .png .heic .mov`).
- Ставит задачи в очередь (SHA-256 → EXIF → thumbnail → preview).
- Выводит отчёт и возвращается.

Чтобы обработка задач завершилась — запустите `serve`, который поднимет worker pool.

---

## Статус библиотеки

```bash
./build/apps/geoframe/geoframe status \
    --data-dir ~/.local/share/geoframe
```

Пример вывода:

```
Data dir:      /home/user/.local/share/geoframe
Assets active: 12453
Assets trashed:8
Disk free:     143234 MB
```

---

## Frontend (React)

### Установка зависимостей

```bash
cd web
npm install
```

### Demo-режим (без backend)

Показывает интерфейс с демонстрационными данными — backend не нужен:

```bash
cd web
npm run dev:demo
```

Откроется на [http://localhost:5173](http://localhost:5173).

### Подключение к реальному серверу

Запустите backend (с `--skip-tls` или с настоящим сертом), затем:

```bash
cd web
npm run dev
```

Vite проксирует `/api` и `/ws` на `https://localhost:8443`.

### Production build

Собирает React в статические файлы, которые GeoFrame раздаёт сам:

```bash
cd web
npm run build
# Результат в web/dist/
```

Полный сценарий запуска — в разделе [Production: одним процессом](#production-одним-процессом) выше.
По умолчанию GeoFrame ищет `web/dist/` относительно текущей рабочей директории.

### Frontend-команды

| Команда | Описание |
|---------|----------|
| `npm run dev` | Dev-сервер с проксированием на backend |
| `npm run dev:demo` | Dev-сервер без backend (демо-данные) |
| `npm run build` | Production build в `web/dist/` |
| `npm run lint` | ESLint проверка |
| `npm test` | Vitest (unit + component тесты) |
| `npm run preview` | Предпросмотр production build |

---

## Разработка: цикл сборки и тестов

### C++ тесты

```bash
# Собрать и запустить все тесты
cmake --build build --parallel && ctest --test-dir build --output-on-failure

# Только конкретный тест
ctest --test-dir build -R SqliteAssetRepositoryTest --output-on-failure
```

Тесты расположены в `tests/`:

| Набор | Тестирует |
|-------|-----------|
| `core/` | Версия, базовые типы |
| `db/` | SQLite-репозитории, миграции |
| `storage/` | Scanner, определение типа файла |
| `media/` | SHA-256, EXIF-extractor, preview, process runner |
| `worker/` | ScanService, WorkerPool, полный image pipeline |

### Форматирование кода

```bash
# Проверить (как в CI)
find modules apps tests -name "*.cpp" -o -name "*.hpp" | \
    xargs clang-format --dry-run --Werror

# Исправить
find modules apps tests -name "*.cpp" -o -name "*.hpp" | \
    xargs clang-format -i
```

### Full check (build + tests + lint)

```bash
cmake --build build --parallel \
  && ctest --test-dir build --output-on-failure \
  && cd web && npm run lint && npm test && npm run build
```

---

## Справочник CLI

```
geoframe serve [options]      — HTTPS-сервер + worker pool
geoframe scan <path> [opts]   — проиндексировать директорию
geoframe status [options]     — статистика библиотеки
geoframe --version            — версия
geoframe --help               — эта справка
```

Общие опции (`serve`, `scan`, `status`):

```
--data-dir <path>     data dir (default: ~/.local/share/geoframe)
--source <path>       source folder (нужен для serve и как позиционный для scan)
```

Опции `serve`:

```
--host <addr>         0.0.0.0
--port <n>            8443
--threads <n>         hardware_concurrency - 1
--skip-tls            HTTP вместо HTTPS
```

---

## Структура данных

```
~/.local/share/geoframe/         ← data dir (настраивается через --data-dir)
├── geoframe.db                  ← SQLite: все метаданные, jobs, статусы
├── cache/
│   ├── thumbnails/              ← сгенерированные уменьшенные копии (≤500px)
│   └── previews/                ← preview-версии для web (≤2000px)
└── certs/
    ├── server.crt               ← TLS-сертификат (генерируется при первом запуске)
    └── server.key               ← приватный ключ
```

**Что бэкапить:**
- `source folder` — оригинальные файлы (главное).
- `geoframe.db` — ваши избранные, статусы и дополнительные метаданные.
- `thumbnails/` и `previews/` — можно пересоздать командой `scan`.

---

## Что сгенерируется автоматически

| Что | Когда |
|-----|-------|
| `data-dir` | При первом `serve` или `scan` |
| `geoframe.db` + миграции | При первом запуске |
| `certs/server.crt` + `server.key` | При первом `serve` (TLS) |
| `cache/thumbnails/` | По ходу обработки в worker pool |
| `cache/previews/` | По ходу обработки в worker pool |

---

## Типичные проблемы

**«Сертификат не доверенный» в браузере**
Нормальное поведение для self-signed. Нажмите «Принять риск» — сайт откроется.

**`ffmpeg: command not found`**
Установите `ffmpeg` (см. [Зависимости](#зависимости)). Он нужен для генерации thumbnail/preview.

**Первая сборка занимает несколько минут**
CMake скачивает Boost, Exiv2, spdlog, nlohmann/json. Последующие сборки — быстрые (кэш).

**`geoframe status` говорит «No GeoFrame database found»**
Запустите `geoframe serve` или `geoframe scan` хотя бы раз.

**Сканирование прошло, но thumbnails не появились**
Thumbnail генерируются в фоне worker pool. Запустите `geoframe serve` — обработка продолжится с места остановки.
