#!/usr/bin/env python3
"""Create an SD package only from a real elf2nro output; never ship a placeholder."""
import pathlib, zipfile
root=pathlib.Path(__file__).resolve().parent.parent
nro=root/'BookReader.nro'
if not nro.exists():
    raise SystemExit('BookReader.nro is missing. Install devkitPro, run make, then make package.')
with nro.open('rb') as f:
    f.seek(0x10)
    if f.read(4)!=b'NRO0':raise SystemExit('Invalid NRO header; package refused.')
with zipfile.ZipFile(root/'BookReader.zip','w',zipfile.ZIP_DEFLATED) as z:
    z.write(nro,'switch/BookReader/BookReader.nro')
    for name in ('books','fonts','cache','cache/covers','config'):
        z.writestr('switch/BookReader/'+name+'/',b'')
    for name in ('README.md','README_RU.md','CHANGELOG.md','LICENSE','THIRD_PARTY.md','BUILD_V2.md'):
        z.write(root/name,'switch/BookReader/'+name)
    for name in ('home-en-portrait.png','reader-en-portrait.png','settings-en-portrait.png'):
        z.write(root/'screenshots'/name,'switch/BookReader/screenshots/'+name)
print(root/'BookReader.zip')
