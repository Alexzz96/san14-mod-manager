"""Check Git-tracked publication files for accidental local/runtime material."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
paths = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode('utf-8').split('\0')
blocked_parts = {'.mod-analysis', 'build', 'dist', 'captures', 'backups', 'output', '.tools', '__pycache__'}
blocked_suffixes = {'.exe', '.dll', '.sav', '.dmp', '.bin', '.log', '.zip', '.pem', '.key'}
patterns = [
    re.compile(rb'[A-Za-z]:[/\\]Users[/\\]', re.I),
    re.compile(rb'(?:gh[pousr]_|github_pat_)[A-Za-z0-9_]{20,}'),
    re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
]
failures = []
total = 0
for name in filter(None, paths):
    path = Path(name)
    if blocked_parts.intersection(path.parts) or path.suffix.lower() in blocked_suffixes:
        failures.append(name + ': runtime/build material')
        continue
    data = (ROOT / path).read_bytes()
    total += len(data)
    if b'\0' in data or len(data) > 512_000:
        failures.append(name + ': unexpected binary or large file')
    if any(pattern.search(data) for pattern in patterns):
        failures.append(name + ': private path or credential pattern')
if failures:
    raise SystemExit('\n'.join(failures))
print(f'Publication audit passed: {len(list(filter(None, paths)))} tracked files, {total} bytes')
