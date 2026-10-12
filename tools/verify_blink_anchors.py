"""Read-only static validation of blink guards and private MinHook trampolines."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--private-hooks', action='store_true', help='Check private byte copies in this process; never attach to the game')
    parser.add_argument('--build-dir', type=Path, default=ROOT/'native/build')
    parser.add_argument('--report', type=Path, help='Save the read-only evidence report')
    args = parser.parse_args()
    data = args.exe.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0'
    count, optional = struct.unpack_from('<H', data, pe+6)[0], struct.unpack_from('<H', data, pe+20)[0]
    sections = []
    for i in range(count):
        size, rva, raw_size, offset = struct.unpack_from('<4I', data, pe+24+optional+i*40+8)
        sections.append((rva, raw_size, offset))
    def read(rva, size):
        for start, raw_size, offset in sections:
            if start <= rva and rva+size <= start+raw_size:
                result = data[offset+rva-start:offset+rva-start+size]
                assert len(result) == size
                return result
        raise ValueError(f'Unmapped RVA {rva:#x}')
    rows = re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+),UINT64_C\((0x[0-9a-f]+)\)\}', (ROOT/'native/blink_anchors.h').read_text())
    assert len(rows) == 18
    for addr, length, expected in rows:
        digest = 14695981039346656037
        for value in read(int(addr, 16), int(length, 16)):
            digest = ((digest ^ value)*1099511628211) & ((1 << 64)-1)
        assert digest == int(expected, 16), f'Anchor mismatch {addr}'
    dependency_count = 0
    if args.private_hooks:
        test = C.CDLL(str(args.build_dir.resolve()/'plugin_test.dll'))
        hook = test.S14TestHookBytes
        hook.argtypes = [C.POINTER(C.c_ubyte), C.c_int]
        hook.restype = C.c_int
        for rva in (0x2482466, 0x280570):
            code = (C.c_ubyte*64).from_buffer_copy(read(rva, 64))
            assert hook(code, 64) == 1, f'Private trampoline rejected {rva:#x}'
        # Blink reuses existing battle/name/create hooks. Confirm their current
        # EXE entries as well, without attempting installation in the game.
        battle = test.S14TestBattlePrologue
        battle.argtypes = [C.c_int, C.POINTER(C.c_ubyte)]
        battle.restype = C.c_int
        for index in range(16):
            signature = (C.c_ubyte*16)()
            rva = battle(index, signature)
            assert bytes(signature) == read(rva, 16), f'Battle dependency {rva:#x}'
            dependency_count += 1
        adapter = (ROOT/'native/troop_runtime.c').read_text(encoding='utf8')
        rvas = [int(x, 16) for x in re.search(r'static const uintptr_t rva\[\]=\{([^}]+)\};', adapter)[1].split(',')]
        table = re.search(r'static const unsigned char signatures\[\]\[12\]=\{(.*?)\};', adapter, re.S)[1]
        signatures = [bytes(int(x,16) for x in re.findall(r'0x[0-9a-f]+', row)) for row in re.findall(r'\{([^}]+)\}',table)]
        assert len(rvas) == len(signatures) == 25
        for rva, signature in zip(rvas, signatures):
            assert signature == read(rva, 12), f'Troop dependency {rva:#x}'
            dependency_count += 1
    result = {'status':'passed', 'anchors':len(rows), 'reused_dependency_entries':dependency_count, 'private_trampolines':'passed' if args.private_hooks else 'not_run', 'exe_sha256':hashlib.sha256(data).hexdigest(), 'game_process_touched':False, 'in_game_acceptance':'pending'}
    if args.report:
        args.report.write_text(json.dumps(result, indent=2)+'\n', encoding='utf8')
    print(json.dumps(result))

if __name__ == '__main__':
    main()
