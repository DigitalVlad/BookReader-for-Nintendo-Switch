#!/usr/bin/env python3
"""Validate NRO segment bounds and embedded icon/NACP; does not emulate Switch."""
from pathlib import Path
import struct,hashlib
root=Path(__file__).resolve().parent.parent
p=root/'BookReader.nro'
b=p.read_bytes()
u32=lambda off:struct.unpack_from('<I',b,off)[0]
u64=lambda off:struct.unpack_from('<Q',b,off)[0]
assert b[0x10:0x14]==b'NRO0','NRO magic missing'
size=u32(0x18)
assert 0x80<=size<=len(b),'Invalid executable size'
for name,off in [('text',0x20),('rodata',0x28),('data',0x30)]:
    start,length=u32(off),u32(off+4)
    assert start+length<=size,f'{name} exceeds executable bounds'
    assert start%0x1000==0,f'{name} is not page-aligned'
    print(f'{name}: offset={start:#x}, size={length:#x}')
assert b[size:size+4]==b'ASET','NRO assets missing'
for name,off in [('icon',8),('nacp',24)]:
    start,length=u64(size+off),u64(size+off+8)
    assert length>0 and size+start+length<=len(b),f'Invalid {name}'
    data=b[size+start:size+start+length]
    if name=='icon':assert data[:2]==b'\xff\xd8','Icon is not JPEG'
    else:
        assert len(data)==0x4000,'NACP must be 16 KiB'
        assert data[:0x200].split(b'\0',1)[0]==b'BookReader'
        assert data[0x200:0x300].split(b'\0',1)[0]==b'Homebrew'
        assert data[0x3060:0x3070].split(b'\0',1)[0] == b'0.2.0','App version missing'
    print(f'{name}: {length} bytes')
print(f'PASS: {p.name}, {len(b)} bytes')
print('SHA256:',hashlib.sha256(b).hexdigest())
