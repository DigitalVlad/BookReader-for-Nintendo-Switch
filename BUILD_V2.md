# BookReader 0.2.0 — BUILD SUCCESS

Дата: 19 сентября 2026. Изменён существующий проект; оригинал сохранён как
`BookReader_backup.nro`. Новый файл — `BookReader_v2.nro`, идентичен
`BookReader.nro` после чистой сборки. Пользователь запустил эту сборку на
физическом Nintendo Switch OLED и подтвердил открытие книг.

## Результат

- Language: English / Русский, переключение сразу, сохранение `language=en|ru`.
- Orientation: Horizontal / Vertical (Landscape / Portrait), 1280×720 / 720×1280.
- Весь интерфейс поворачивается на 90° по часовой стрелке; держать консоль с правым
  Joy-Con сверху. Touch использует обратное преобразование того же OrientationTransform.
- Настройки, главное меню, библиотека, recent, favorites, поиск, контекстное меню,
  оглавление, reader overlay и диалоги адаптированы к ширине/высоте viewport.
- D-pad и стики учитывают поворот. A/B/X/Y/L/R/ZL/ZR сохранены, + открывает настройки.
- Проверяются все кириллические глифы А–Я, а–я, Ё/ё; используется существующий
  FreeType fallback с пользовательскими и libnx shared fonts. Дополнительный
  проприетарный шрифт не распространяется.
- PDF: backend unchanged. EPUB: working implementation preserved.
  Все файлы `source/readers/` побайтово совпадают с исходной копией.
- Существующий reflow EPUB получает новые размеры из Settings, восстанавливая
  близкую позицию по проценту. Абсолютное совпадение слова при смене вёрстки
  существующий механизм не гарантирует. PDF сохраняет номер страницы.

## Фактическая сборка

В официальном ARM64 image `devkitpro/devkita64:20260219` выполнено:

```sh
make clean
make
```

Exit code: 0.
devkitA64 r29.2-1, libnx 4.12.0-1, switch-mupdf 1.17.0-1,
switch-sdl2 2.28.5-4, switch-freetype 2.13.3-1.
Сохранились предупреждения старых libnx/MuPDF headers и существующего MuPDF
longjmp-кода. Этот backend не менялся в рамках задачи.

NACP: BookReader / Homebrew / **0.2.0**. ELF: ARM aarch64, static PIE.
NRO: **44 311 863 байта**, SHA-256:
`f6736a7c694de90fb8722f20f7fb82fcfe9bb8b1c0c353b66dfa90740944c792`.

Backup 0.1.0: 44 279 095 байт, SHA-256:
`dea0c7430aac3be8a81a9294cdd5c7a418687fc383fb9bce1c409e510fc98e17`.

## Проверки

- `make test`: 117 проверок настоящих PDF/EPUB/TXT/HTML/FB2/CBZ и хранения — PASS.
- `make -f Makefile.host gui-test`: прежний SDL event-loop regression — PASS.
- `make -f Makefile.host v2-test`: 612 проверок плюс восстановление русского
  языка и Portrait в новом App — PASS.
- Та же новая UI-проверка собрана с ASan/UBSan для исходников приложения:
  PASS без сообщений sanitizer. Готовые upstream static libraries не инструментированы.
- Проверены границы и минимальный размер кнопок; тап по повёрнутым кнопкам,
  кириллическая клавиатура с ё, slider drag, листание списков, оглавление,
  свайп PDF, двухпальцевый pinch и drag увеличенного PDF.
- Проверены все кириллические кодовые точки через FreeType; сохранённые
  реальные SDL-кадры визуально проверены. Английские кадры опубликованы в
  каталоге `screenshots/`.
- Физический кадр точно совпадает с повёрнутой логической поверхностью,
  включая каждый пиксель; это не только проверка обратимости формул.
- NRO0, сегменты, ASET, JPEG icon, NACP и версия проверены `tools/verify_nro.py`.
- Пользователь подтвердил запуск приложения на OLED и успешное открытие книг.

## Modified files

- `source/app/App.cpp`, `source/app/App.hpp`
- `source/config/Config.cpp`, `source/utils/Types.hpp`
- `source/render/Fonts.cpp`, `source/render/Fonts.hpp`
- `source/ui/Components.cpp`, `source/ui/LibraryScreen.cpp`
- `source/ui/ReaderScreen.cpp`, `source/ui/SettingsScreen.cpp`
- `Makefile`, `Makefile.host`, `tools/verify_nro.py`
- `tools/package.py`, `tools/package_source.py`
- `README.md`, `CHANGELOG.md`, `BUILD_REPORT.md`

## Added files

- `source/ui/Localization.hpp`, `source/ui/Localization.cpp`
- `source/ui/OrientationTransform.hpp`
- `tests/v2_ui.cpp`
- `BUILD_V2.md`, `screenshots/`

## Установка

Распаковать `BookReader_v2.zip` в корень microSD. Внутри используется стандартное
имя `switch/BookReader/BookReader.nro`. Либо вручную скопировать
`BookReader_v2.nro` как `/switch/BookReader/BookReader.nro`.
Книги и старый `config/state.ini` удалять не требуется.
