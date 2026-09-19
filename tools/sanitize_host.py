#!/usr/bin/env python3
"""ASan/UBSan for BookReader code; upstream static MuPDF itself is not instrumented."""
import pathlib,subprocess,os
os.chdir(pathlib.Path(__file__).resolve().parent.parent)
files=[str(p) for p in pathlib.Path('source').glob('*/*.cpp') if p.parent.name not in ('ui','app')]
files+=['source/ui/Gestures.cpp','source/app/Worker.cpp','tests/test_main.cpp']
cflags=subprocess.check_output(['pkg-config','--cflags','sdl2','freetype2'],text=True).split()
libs=subprocess.check_output(['pkg-config','--libs','sdl2','freetype2'],text=True).split()
subprocess.run(['clang++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer',
    '-Isource','-I.deps/mupdf-1.17.0-source/include',*cflags,*files,
    '.deps/mupdf-1.17.0-source/build/release/libmupdf.a',
    '.deps/mupdf-1.17.0-source/build/release/libmupdf-third.a',*libs,
    '-lz','-lm','-pthread','-o','build-host/tests-sanitize'],check=True)
subprocess.run(['./build-host/tests-sanitize'],check=True)
