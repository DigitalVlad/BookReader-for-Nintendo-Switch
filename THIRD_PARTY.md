# Licensing and dependencies

BookReader source and original icon: AGPL-3.0-or-later; see LICENSE.

The Switch build links externally installed devkitPro portlibs. No Nintendo SDK
or proprietary firmware/font data is included in this repository or packages.
Shared fonts are accessed on the user's console using libnx pl:u.

- MuPDF 1.17.0: Artifex, AGPL-3.0-or-later (or separate commercial license).
  https://mupdf.com/licensing/
- MuPDF bundled libraries/fonts: retain their upstream notices when distributing
  a complete corresponding source bundle; see the MuPDF source archive's
  `thirdparty/` licenses and `resources/fonts/` notices.
- libnx: ISC. https://github.com/switchbrew/libnx
- SDL2: zlib license. https://github.com/libsdl-org/SDL/tree/SDL2
- FreeType: FreeType License / GPL-2.0. https://freetype.org/license.html
- JPEG, zlib, bzip2, libpng and graphics dependencies: the licenses installed
  with the respective devkitPro packages apply.

When distributing a linked NRO, provide corresponding source and license notices
for BookReader and linked AGPL components under the terms of the AGPL. This source
archive alone does not replace the corresponding source for MuPDF/portlibs.
System shared fonts must not be copied into the release package. Users may place
appropriately licensed TTF/OTF/TTC fonts in their own fonts directory.
