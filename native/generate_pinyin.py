"""Regenerate the checked-in MIT pinyin table; normal builds need no pypinyin.

Optional tooling dependency: pip install pypinyin==0.55.0
"""
from pathlib import Path
import hashlib
import json
import re

import pypinyin
from pypinyin import Style, pinyin
from pypinyin.constants import PINYIN_DICT

assert pypinyin.__version__ == '0.55.0'
HERE = Path(__file__).resolve().parent / 'vendor/pinyin'
pool = bytearray()
offsets = {}
rows = []
for code in sorted(PINYIN_DICT):
    if not 0x3400 <= code <= 0xffff:
        continue
    variants = []
    for value in pinyin(chr(code), style=Style.NORMAL, heteronym=True)[0]:
        value = value.replace('ü', 'v')
        if re.fullmatch('[a-z]+', value) and value not in variants:
            variants.append(value)
    if not variants:
        continue
    value = '|'.join(variants).encode('ascii')
    if value not in offsets:
        offsets[value] = len(pool)
        pool.extend(value + b'\0')
    rows.append((code, offsets[value]))
text = '/* Generated from pypinyin 0.55.0 (MIT); offline BMP readings, including heteronyms. */\n'
text += 'static const char reading_pool[]=\n'
for value in bytes(pool).split(b'\0')[:-1]:
    text += '"' + value.decode('ascii') + '\\0"\n'
text += ';\nstatic const struct { unsigned short code; unsigned int offset; } reading_index[]={\n'
text += ''.join(f'{{0x{code:04x},{offset}u}},\n' for code, offset in rows)
text += '};\n'
target = HERE / 'readings.h'
target.write_text(text, encoding='ascii')
manifest = json.loads((HERE / 'UPSTREAM.json').read_text(encoding='utf-8'))
manifest['readings_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
(HERE / 'UPSTREAM.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({'characters': len(rows), 'table_sha256': manifest['readings_sha256']}))
