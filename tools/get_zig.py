"""Download the pinned Windows x64 compiler and verify official SHA-256."""
from pathlib import Path
import hashlib
import json
import urllib.request
import zipfile
ROOT=Path(__file__).resolve().parents[1]
target=ROOT/'.tools'; target.mkdir(exist_ok=True)
with urllib.request.urlopen('https://ziglang.org/download/index.json', timeout=60) as response:
    metadata=json.load(response)['0.15.2']['x86_64-windows']
url=metadata['tarball']
if not url.startswith('https://ziglang.org/download/0.15.2/'):
    raise SystemExit('Unexpected compiler download origin')
with urllib.request.urlopen(url, timeout=60) as response:
    data=response.read()
if hashlib.sha256(data).hexdigest()!=metadata['shasum']:
    raise SystemExit('Compiler checksum mismatch')
archive=target/'zig-0.15.2.zip'; archive.write_bytes(data)
with zipfile.ZipFile(archive) as source:
    for member in source.infolist():
        resolved=(target/member.filename).resolve()
        if not resolved.is_relative_to(target.resolve()):
            raise SystemExit('Unsafe compiler archive path')
    source.extractall(target)
extracted=target/Path(url).name.removesuffix('.zip')
destination=target/'zig'
if destination.exists():
    raise SystemExit('Compiler folder already exists; use existing compiler')
extracted.rename(destination)
print('Zig 0.15.2 ready')
