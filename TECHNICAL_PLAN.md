# BookReader — технический план

Дополнение 0.2.0: словарь English/Русский компилируется в приложение; выбор
сохраняется в существующем INI. Весь UI рисуется в SDL target texture размером
1280×720 или 720×1280. OrientationTransform задаёт поворот при презентации и
обратное преобразование touch; все экраны получают размеры из Settings.
PDF/EPUB readers и библиотеки остаются без изменений. Проверки и итоговая
сборка описаны в BUILD_V2.md.

## Проверка стека (19 сентября 2026)

Проект находится в отдельной папке, поскольку родительская папка содержит другую
игру. На macOS ARM64 host-проверки выполняются Clang/SDL2/FreeType, а итоговая
Switch-сборка проверена в официальном ARM64-образе `devkitpro/devkita64:20260219`.

- C++17, devkitA64, libnx, Makefile на основе официального application template.
- SDL2: официальный `switch-sdl2`, масштабируемый логический viewport 1280×720.
- FreeType: официальный `switch-freetype`, системные shared fonts libnx как
  fallback, пользовательские TTF/OTF из `fonts/`.
- MuPDF: **существует официальный `switch-mupdf`**, рецепт версии 1.17.0
  содержит `OS=switch`, патч libnx и зависимости FreeType/JPEG/zlib.
  Это существенно реалистичнее PDFium/Poppler с их большим деревом зависимостей.
  Используется API 1.17; более новые API не предполагаются. Лицензия AGPL-3.0.
- PDF — постраничная отрисовка MuPDF; EPUB — его ZIP/OPF/XHTML/CSS renderer,
  изображения и оглавление; HTML — тот же ограниченный HTML renderer.
- TXT — собственный потоковый UTF-8/UTF-16 decoder и индекс страниц на SD;
  FB2 — XML parser MuPDF и преобразование в HTML; CBZ — MuPDF.
- Один формат конфигурации: INI с экранированием UTF-8 путей, атомарной заменой,
  резервной копией и debounce. SQLite и дополнительная ZIP/XML библиотека не нужны.

Источники:

- https://github.com/devkitPro/pacman-packages/blob/master/switch/mupdf/PKGBUILD
- https://github.com/devkitPro/pacman-packages/blob/master/switch/mupdf/mupdf.patch
- https://github.com/devkitPro/pacman-packages/blob/master/switch/SDL2/PKGBUILD
- https://github.com/switchbrew/switch-examples/tree/master/templates/application
- https://github.com/ArtifexSoftware/mupdf/tree/1.17.0/include/mupdf/fitz
- https://devkitpro.org/wiki/Getting_Started

## Последовательность

1. Shell приложения, SDL/FreeType, Joy-Con, библиотека файлов; Makefile.
2. Потоковый TXT, UTF, позиции, тесты на host.
3. EPUB через MuPDF, reflow, главы, оглавление, CSS.
4. PDF: ограниченные bitmap, zoom/pan, LRU максимум 3 страницы.
5. FB2 и HTML, явные границы поддержки; CBZ без RAR.
6. INI, recent/favorites, metadata, дисковый cache обложек.
7. Touch state machine, настройки, темы, ориентация, итоговые проверки.

После этапов запускать `make clean` и `make`. Если локальный toolchain недоступен,
использовать `tools/build_container.sh`; host binary не выдавать за NRO и не
создавать фиктивный файл `.nro`.

## Память и надёжность

MuPDF allocator имеет жёсткий бюджет; store ограничен отдельно. Растры страницы
ограничены числом пикселей и стороной текстуры, масштаб UI ограничен 1–4.
Открытие документа/рендер/индексация выполняются worker-потоком; SDL остаётся на
главном потоке. Очередь ограниченная, устаревшие результаты не отображаются.
TXT читается блоками, индекс и кэш ограничены. EPUB/HTML/FB2 имеют ограничения
входного размера; сложная перепаковка EPUB может занимать время и память.
Ошибки MuPDF перехватываются через fz_try/fz_catch без C++ RAII внутри longjmp.

## Честные ограничения первого выпуска

Без DRM, JavaScript, полноценного браузерного CSS, MOBI/AZW/AZW3, RTF, CBR, DOCX.
Markdown читается как plain text. FB2: текст, metadata, секции и встроенные PNG/JPEG с ограничением размера;
остальные изображения заменяются подписью.
Тёмная тема PDF меняет фон интерфейса, но не инвертирует документ.
Яркость — только затемнение приложения, не изменение системной настройки.
Пользователь подтвердил запуск текущей сборки на физическом Nintendo Switch OLED
и успешное открытие книг. Отдельные сценарии dock/suspend не заявляются как
исчерпывающе проверенные.

## Итог реализации

План реализован в исходниках. Native NRO собран и проверен по структуре; пользователь
подтвердил запуск на Switch OLED и открытие книг. Также проверены полная host-сборка,
117 integration checks, SDL event-loop и ASan/UBSan для кода приложения. Подробности:
BUILD_REPORT.md.
