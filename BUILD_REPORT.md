# Отчёт проверок — 2026-09-19

Этот документ фиксирует исходную сборку 0.1.0. Актуальная сборка 0.2.0 и новые
проверки описаны в [BUILD_V2.md](BUILD_V2.md). Указанный ниже SHA-256 относится
теперь к `BookReader_backup.nro`, а не к обновлённому `BookReader.nro`.

## Что действительно выполнено

Окружение host-проверок: macOS ARM64, Apple Clang 21.0.0, C++17; SDL2 и FreeType
из Homebrew. MuPDF 1.17.0 собран из официального source archive. SHA-256 совпал
с рецептом `switch-mupdf`: `c935fb2593d9a28d9b56b59dad6e3b0716a6790f8a257a68fa7dcb4430bc6086`.
Итоговый NRO собран в официальном ARM64-контейнере
`devkitpro/devkita64:20260219` (OCI digest
`sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`).

| Проверка | Результат |
| --- | --- |
| `make clean` (Switch Makefile) | Выполнено |
| `make clean && make -j4` в официальном devkitPro container | Успешно; создан настоящий `BookReader.nro` |
| `python3 tools/verify_nro.py` | PASS: NRO0, сегменты, NACP, JPEG icon, ARM64 ELF metadata |
| Версии container toolchain | `devkitA64 r29.2-1`, `libnx 4.12.0-1`, `switch-mupdf 1.17.0-1`, `switch-sdl2 2.28.5-4`, `switch-freetype 2.13.3-1` |
| Чистая host-сборка `make -f Makefile.host clean`, затем `make host` | Успешно; реальный исполняемый файл macOS |
| `make test` | 117 проверок пройдено |
| `make -f Makefile.host gui-test` | Полный event loop SDL: slider, double tap zoom, страница, favorite, сохранение при выходе — пройдено |
| `python3 tools/sanitize_host.py` | 117 проверок пройдено с ASan/UBSan для BookReader-кода |
| SDL dummy driver, главный экран и открытие EPUB/TXT | Успешно; кадры отрисованы и сохранены |
| Визуальная проверка EPUB | Кириллица, китайские и японские глифы видны, bold/italic работают |
| Установка/запуск на Switch OLED | Пользователь подтвердил запуск приложения и успешное открытие книг |

MuPDF и остальные предварительно собранные сторонние static libraries не
инструментированы sanitizer: эти проверки не доказывают отсутствие ошибок внутри
всех сторонних библиотек. macOS ASan не равнозначен проверке утечек на Switch.

## Что покрывают тесты

- Настоящие двухстраничный PDF (portrait + landscape), EPUB2 с NCX, EPUB3 с nav,
  HTML, FB2, CBZ. Рендер в RGBA и реальный подсчёт страниц.
- OPF metadata, EPUB cover, главы, FB2 title/author/sections, embedded PNG fixture.
- UTF-8, UTF-16LE/BE с BOM, surrogate-pair fixture, невалидные последовательности,
  пустой TXT, длинные строки, совпадение числа страниц для разных кодировок.
- TXT byte offsets и переиндексация после изменения размера шрифта.
- PDF zoom до 4× с проверкой пределов bitmap; worker zoom 2× → 1× → next page.
- Тёмные поля EPUB/HTML/FB2, пересчёт reflow, закрытие/повторное открытие reader.
- Повреждённые PDF/EPUB/FB2, отсутствующий файл, unsupported format.
- INI Unicode/escape, большое смещение, favorite, recovery из `.bak`.
- Ограниченный allocator, Unicode case-fold, рекурсивное сканирование без symlinks.
- Tap/swipe/long press/pinch и подавление ложного tap после pinch.
- Реальные SDL input events через основной цикл приложения; настройки и прогресс
  перечитаны из созданного на диске INI после выхода.

Сообщения MuPDF `cannot recognize archive`, `no objects found` в test output —
ожидаемые ошибки повреждённых fixtures. `unknown epub version: 3.0` — предупреждение
MuPDF 1.17; тестовый EPUB3 spine рендерится, а nav обрабатывает адаптер приложения.
Это не означает поддержку всех возможностей стандарта EPUB3.

## Этапы разработки и проверка сборки

План записан до основных исходников. Реализованы функции milestones 1–7:
application shell/library → TXT → EPUB → PDF → FB2/HTML → persistence/covers →
touch/themes. Итоговая cross-сборка всего проекта выполнена в официальном
devkitPro image, затем структура NRO проверена отдельным скриптом.

## Что остаётся для подтверждения готового релиза

1. Проверить полученный NRO на Switch: Joy-Con, две точки touch, dock/undock,
   suspend/resume/Home, applet vs full-memory mode, SD без свободного места.
2. Пройти реальные тяжёлые/сканированные PDF и большие EPUB на устройстве;
   проверить память и скорость на microSD.
3. Проверить установочный SD-архив на устройстве.

`BookReader.nro` — 44 279 095 байт, SHA-256
`dea0c7430aac3be8a81a9294cdd5c7a418687fc383fb9bce1c409e510fc98e17`.
`BookReader.zip` содержит этот NRO и готовую структуру
`switch/BookReader/{books,fonts,cache,config}`. `BookReader-source.zip` содержит
исходники, тесты и документацию.
