# GeoFrame — coding guidelines

Правила для C++ кода. Цель — чистые PR для code review, предсказуемый стиль.

## Язык и стандарт

- **C++20** (не выше, пока не нужно)
- Compiler: GCC 13+ / Clang 16+ (Linux MVP)
- Warnings: `-Wall -Wextra -Wpedantic`; treat warnings as errors в CI (когда CI полный)

## Стиль

- **clang-format** — единый стиль, конфиг в репо (`.clang-format`)
- **clang-tidy** — warn-only на старте, постепенно ужесточать
- Заголовки используют расширение `.hpp`, реализации — `.cpp`
- Именование:
  - `snake_case` — функции, переменные, файлы
  - `PascalCase` — классы, structs (domain types)
  - `kConstantName` — constexpr constants
  - `IInterfaceName` — абстрактные интерфейсы
- Заголовки: `#pragma once`
- Include order: corresponding header → project → third-party → standard
- Код живёт в `geoframe::<module>`; глобальный namespace не используем

## Ownership и память

- **RAII везде** — никакого raw `new`/`delete`
- `std::unique_ptr` для owned resources
- `std::shared_ptr` — только когда реально нужен shared ownership (редко)
- `std::string_view` для read-only string params
- `std::span` для read-only buffers
- Запрет: C-style casts → `static_cast` / `reinterpret_cast`

## Error handling

На старте используем исключения — этот подход уже знаком по проекту `Task` и не
требует преждевременно создавать собственный `Result<T>`.

```text
Ошибка IO / DB / media operation  → exception с контекстом
Допустимое отсутствие значения    → std::optional<T>
Успешный результат pipeline       → конкретный domain struct
Граница CLI / worker / HTTP       → catch + log + перевод в exit/status/JSON
```

### Правила

- Бросать наиболее конкретное стандартное или domain-исключение
- Добавлять контекст на границе слоя; не терять исходную причину
- Не использовать exceptions как обычное ветвление
- Не логировать одну ошибку на каждом уровне: логирует boundary, который её обработал
- Каждый worker task обязан ловить исключения, чтобы не завершить процесс
- `noexcept` — только где это действительно гарантировано
- Собственный `Result<T>` вводим только при появлении доказанной необходимости

## Логирование

- **spdlog** — единственный logger
- Уровни: `trace` / `debug` / `info` / `warn` / `error` / `critical`
- В production default: `info`
- Формат: `[2026-09-14 21:00:00.123] [info] [scanner] Found 51203 files`
- Не логировать полные пути пользователя на уровне `info` (privacy) — только на `debug`

## Модули и зависимости

```text
Разрешено:  cli → server → worker → media → storage → core
            cli → server → db → core
Запрещено:  core → media (core не знает о media)
            media → server (media не знает о HTTP)
```

- Каждый модуль экспортирует публичный API через `include/{module}/`
- Внутренние детали — в `src/`, не в include
- Межмодульное общение — через interfaces в `core`

## Подход из Task

GeoFrame сохраняет удачные паттерны из
[`Stepsyota/Task`](https://github.com/Stepsyota/Task):

- маленькие компоненты с одной ответственностью;
- отдельный orchestration-слой, который связывает scanner, processor и output;
- зависимости передаются явно через конструктор;
- `std::filesystem::path` для путей;
- streaming-обработка файлов через callback, без обязательного накопления всей библиотеки;
- RAII-обёртки для C API и системных ресурсов;
- подробный Doxygen для публичных классов и функций;
- исключения обрабатываются на верхней границе приложения.

Не переносим механически недостатки небольшого проекта: GeoFrame использует namespaces,
автоформатирование, модульные CMake targets и не создаёт абстракции без реального сценария.

## Тесты

- **GoogleTest** + при необходимости GoogleMock
- Структура зеркалит модули:

```text
tests/
  core/
  storage/
  media/
  db/
  integration/
```

- Unit tests — для чистой логики (hash, path resolve, series detection)
- Integration tests — scanner + sqlite in temp dir
- Тестовые данные: `tests/fixtures/` (маленькие sample файлы)
- Именование: `TEST(ModuleName, ScenarioName)`

## Pull requests

- **Маленькие PR**: одна фича / один модуль, < 400 строк diff
- Каждый PR: что сделано, как тестировать, screenshot если UI
- PR без тестов для новой логики — только с обоснованием
- Не смешивать refactor и feature в одном PR

## CI (частично с первого C++ коммита)

Минимум:

```yaml
- cmake build (Release)
- ctest
- clang-format --dry-run
```

Позже:

```yaml
- GCC + Clang matrix
- clang-tidy
- ASan build
```

## Комментарии

- Код должен быть self-explanatory
- Комментарии — для non-obvious business rules и invariants
- Не комментировать очевидное
- Публичные API в headers — краткий doc comment

## JSON

- **nlohmann/json** для сериализации
- API response types — явные `to_json()` / `from_json()` функции, не размазанные по коду
- Не парсить JSON вручную

## Subprocess (ffmpeg, exiftool)

- Обёртка `ProcessRunner` в `media/`:
  - timeout
  - capture stderr
  - `Result<ProcessOutput>` 
- Никогда не конструировать shell command через string concatenation с user input
- Пути — через `std::filesystem::path`

## SQLite

- Raw C API (`sqlite3.h`) через thin wrapper class `Database`
- Prepared statements всегда (никакого string concat SQL)
- Транзакции для batch operations
- WAL mode: `PRAGMA journal_mode=WAL`

## Git

- Conventional commits желательны: `feat:`, `fix:`, `refactor:`, `docs:`, `test:`
- Не коммитить: `build/`, `.env`, бинарники, `web/node_modules/`
- `third_party/` — только через FetchContent или git submodule с версией
