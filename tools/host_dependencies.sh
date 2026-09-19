#!/bin/sh
# Optional macOS verification build; this does not produce a Switch NRO.
set -eu
cd "$(dirname "$0")/.."
command -v pkg-config >/dev/null
pkg-config --exists sdl2 freetype2
mkdir -p .deps
archive=.deps/mupdf-1.17.0-source.tar.xz
if [ ! -f "$archive" ]; then
    curl -L --fail https://mupdf.com/downloads/archive/mupdf-1.17.0-source.tar.xz -o "$archive"
fi
printf '%s  %s\n' c935fb2593d9a28d9b56b59dad6e3b0716a6790f8a257a68fa7dcb4430bc6086 "$archive" | shasum -a 256 -c -
if [ ! -d .deps/mupdf-1.17.0-source ]; then
    tar -xf "$archive" -C .deps
fi
# The old bundled HarfBuzz promotes diagnostics added by newer Apple Clang to
# errors. Relax only its diagnostic policy for host testing; do not alter logic.
python3 - <<'PY'
from pathlib import Path
p=Path('.deps/mupdf-1.17.0-source/thirdparty/harfbuzz/src/hb.hh')
s=p.read_text().replace('#pragma GCC diagnostic error','#pragma GCC diagnostic warning')
p.write_text(s)
PY
make -C .deps/mupdf-1.17.0-source -j4 build=release \
    HAVE_X11=no HAVE_GLUT=no USE_SYSTEM_FREETYPE=yes USE_SYSTEM_ZLIB=yes \
    "XCFLAGS=$(pkg-config --cflags freetype2)" libs
