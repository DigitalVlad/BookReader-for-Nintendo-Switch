#!/usr/bin/env python3
"""Generate an original geometric book icon as PPM; convert using macOS sips."""
from pathlib import Path
w=h=256
rgb=bytearray()
for y in range(h):
    for x in range(w):
        c=(22,31,39)
        if 38<=x<218 and 46<=y<207:c=(47,154,148)
        if 49<=x<123 and 59<=y<190:c=(228,222,204)
        if 132<=x<207 and 59<=y<190:c=(245,237,212)
        if 63<=x<110 and any(a<=y<a+5 for a in (81,100,119,138,157)):c=(117,139,139)
        if 146<=x<192 and any(a<=y<a+5 for a in (81,100,119,138,157)):c=(117,139,139)
        rgb.extend(c)
Path('assets/icon.ppm').write_bytes(f'P6\n{w} {h}\n255\n'.encode()+rgb)
