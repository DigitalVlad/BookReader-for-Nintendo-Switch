#!/usr/bin/env python3
"""Package only redistributable project sources, documentation and test fixtures."""
from pathlib import Path
import zipfile
root=Path(__file__).resolve().parent.parent
excluded={'.deps','build','build-host','sdcard','.git','__pycache__','v1-source'}
archive=root/'BookReader-source.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(root.rglob('*')):
        rel=p.relative_to(root)
        if any(part in excluded for part in rel.parts):continue
        if p.suffix in {'.zip','.nro','.elf','.nacp','.map','.lst','.pyc','.bmp'}:continue
        if p.name=='.DS_Store':continue
        if p.is_file():z.write(p,'BookReader/'+str(rel))
    z.writestr('BookReader/include/',b'')
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert 'BookReader/Makefile' in z.namelist()
    print(f'{archive}: {len(z.namelist())} entries, {archive.stat().st_size} bytes')
