# Changelog

## Unreleased

- Opt-in Wi-Fi web upload server on port 8080 (`−` on the Home screen).
- Streams PDF/EPUB/TXT/FB2/HTML/CBZ/MD to `/switch/BookReader/books/`,
  validates extension and file signature, sanitizes names, enforces
  per-format limits and rescans the library after each upload.
- `+` remains Settings; `−` on other screens still opens Settings.
- English/Russian UI strings for the upload panel and notifications.

## 0.2.0 — 2026-09-19

- English / Русский UI with centralized UTF-8 translations, localized dialogs
  and persistent language selection.
- Immediate whole-application Landscape / Portrait switching with adaptive screens.
- Shared clockwise render transform and inverse touch transform; rotated D-pad/sticks.
- Portrait search keyboard, sliders, contents, list selection and reading gestures.
- Cyrillic glyph checks including Ё/ё; existing shared/user font fallback preserved.
- PDF and EPUB backend sources unchanged; existing reflow position mechanism retained.
- Original NRO preserved as BookReader_backup.nro; new release BookReader_v2.nro.
- SDL integration tests cover rotated touch, layout bounds, pinch/pan, PDF paging,
  EPUB reflow and restoration of Russian/portrait in a fresh App instance.

## 0.1.0 — 2026-09-19 (first Switch binary)

- C++17/libnx application lifecycle, SDL2 interface and official NACP/NRO Makefile.
- Recursive library, search, five sort orders, recent 20, favorites, context menu.
- Streaming UTF-8/UTF-16 TXT with bounded on-disk pagination index.
- MuPDF PDF rendering, bounded LRU, zoom/pan, landscape and portrait pages.
- EPUB OPF/XHTML/CSS/NCX plus EPUB3 nav adapter and extracted cover artwork.
- HTML and XML-based FB2 conversion with section TOC, metadata and PNG/JPEG data images.
- CBZ image pages and plain-text MD adapter.
- Light/Sepia/Dark themes, font/spacing/margin/alignment settings and app dimming.
- Touch state machine, two-finger pinch, delayed single tap/double tap,
  thresholded swipes, long press, list scrolling and sliders.
- Continuous text scrolling with neighboring pages and decaying inertia.
- Debounced INI persistence, backup recovery and per-book view state.
- 15-second position checkpoints during stick/continuous scrolling, avoiding
  writes when state has not changed.
- Disk thumbnail cache, safe input/raster/allocation limits and worker-based I/O.
- macOS host build, official devkitPro container build, NRO/NACP/icon verification,
  fixtures, 117 integration checks, SDL event-loop test, sanitizer helper and
  source archive.

### Known limits

- The maintainer launched the NRO on a physical Nintendo Switch OLED and confirmed
  that books open successfully.
- Simplified reflow; EPUB position after style changes follows page percentage.
- Library metadata/cover are populated after first opening.
- No DRM/passwords/MOBI/AZW/AZW3/RTF/CBR/DOCX; Markdown is plain text.
- Raster cap can reduce detail at maximum PDF zoom. Fixed-layout EPUB is not guaranteed.
- MuPDF parsing/rendering is not immediately cancellable during an individual call.
