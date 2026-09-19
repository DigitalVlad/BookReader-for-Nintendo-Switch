#!/usr/bin/env python3
"""Original public-domain test documents; no third-party books."""
import pathlib, zipfile, struct, zlib
out=pathlib.Path(__file__).parent/'fixtures'
out.mkdir(exist_ok=True)
text=('Привет, мир! Русская книга. English text. 中文 日本語. 🙂\n'+'Long line with wrapping. '*30+'\n')*120
(out/'utf8.txt').write_text(text,encoding='utf-8')
(out/'utf16le.txt').write_bytes(b'\xff\xfe'+text.encode('utf-16le'))
(out/'utf16be.txt').write_bytes(b'\xfe\xff'+text.encode('utf-16be'))
(out/'empty.txt').write_bytes(b'')
(out/'invalid.txt').write_bytes(b'\xf0\x28\x8c\x28\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80')
(out/'invalid.pdf').write_bytes(b'not a PDF')
(out/'invalid.epub').write_bytes(b'not a ZIP')
(out/'invalid.fb2').write_text('<html>wrong</html>')
html='<html><head><meta charset="utf-8"/><title>Тестовая книга</title></head><body><h1>Глава первая</h1>'+('<p><strong>Привет!</strong> English 中文 日本語. <em>Italic</em></p>'*150)+'<h2>Глава вторая</h2><ul><li>One</li><li>Two</li></ul><img src="image.png"/></body></html>'
(out/'book.html').write_text(html)
(out/'book.fb2').write_text('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Русская FB2</book-title><author><first-name>Иван</first-name><last-name>Тестов</last-name></author></title-info></description><body><section><title><p>Глава первая</p></title>'+('<p>Привет, мир! <strong>Жирный</strong> <emphasis>Курсив</emphasis></p>'*150)+'</section><section><title><p>Глава вторая</p></title><p>Конец.</p></section></body></FictionBook>')
def png():
    def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
    raw=b''.join(b'\0'+bytes([30,150,120])*80 for _ in range(120))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',80,120,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
image=png();(out/'image.png').write_bytes(image)
import base64
fb2=(out/'book.fb2').read_text().replace('<FictionBook xmlns=', '<FictionBook xmlns:l="http://www.w3.org/1999/xlink" xmlns=')
fb2=fb2.replace('<p>Конец.</p>','<p>Конец.</p><image l:href="#picture"/>').replace('</FictionBook>', '<binary id="picture" content-type="image/png">'+base64.b64encode(image).decode()+'</binary></FictionBook>')
(out/'book.fb2').write_text(fb2)
for version in [2,3]:
    with zipfile.ZipFile(out/f'book{version}.epub','w') as z:
        z.writestr('mimetype','application/epub+zip',compress_type=zipfile.ZIP_STORED)
        z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        z.writestr('OEBPS/book.opf',f'<package xmlns="http://www.idpf.org/2007/opf" version="{version}.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">book-test</dc:identifier><dc:title>EPUB {version} Русский</dc:title><dc:creator>Test Author</dc:creator><dc:language>ru</dc:language><meta name="cover" content="cover"/></metadata><manifest><item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/><item id="ch2" href="ch2.xhtml" media-type="application/xhtml+xml"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="cover" href="image.png" media-type="image/png" properties="cover-image"/></manifest><spine'+(' toc="ncx"' if version==2 else '')+'><itemref idref="ch1"/><itemref idref="ch2"/></spine></package>')
        z.writestr('OEBPS/ch1.xhtml',html);z.writestr('OEBPS/ch2.xhtml','<html><head><title>Second</title></head><body><h1>Second chapter</h1><p>End.</p></body></html>');z.writestr('OEBPS/image.png',image)
        z.writestr('OEBPS/toc.ncx','<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/"><navMap><navPoint id="one"><navLabel><text>Chapter 1</text></navLabel><content src="ch1.xhtml"/></navPoint><navPoint id="two"><navLabel><text>Chapter 2</text></navLabel><content src="ch2.xhtml"/></navPoint></navMap></ncx>')
        if version==3:z.writestr('OEBPS/nav.xhtml','<html xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol><li><a href="ch1.xhtml">Chapter 1</a></li><li><a href="ch2.xhtml">Chapter 2</a></li></ol></nav></body></html>')
        else:z.writestr('OEBPS/nav.xhtml','<html><body></body></html>')
with zipfile.ZipFile(out/'comic.cbz','w') as z:
    z.writestr('001.png',image);z.writestr('002.png',image)
objects=[b'<< /Type /Catalog /Pages 2 0 R >>',b'<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>',b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 6 0 R >>',b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 792 612] /Resources << /Font << /F1 5 0 R >> >> /Contents 7 0 R >>',b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
for line in [b'PDF first page',b'PDF second landscape page']:
    content=b'BT /F1 26 Tf 50 500 Td ('+line+b') Tj ET'
    objects.append(b'<< /Length '+str(len(content)).encode()+b' >>\nstream\n'+content+b'\nendstream')
pdf=bytearray(b'%PDF-1.4\n');offsets=[0]
for i,obj in enumerate(objects,1):offsets.append(len(pdf));pdf+=f'{i} 0 obj\n'.encode()+obj+b'\nendobj\n'
xref=len(pdf);pdf+=f'xref\n0 {len(offsets)}\n0000000000 65535 f \n'.encode()
for x in offsets[1:]:pdf+=f'{x:010} 00000 n \n'.encode()
pdf+=f'trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode();(out/'book.pdf').write_bytes(pdf)
print('Fixtures generated:',out)
