"""Exercise actual startup validation in private allocations, never the game."""
import ctypes as C
import struct


def run_entry_tests(test):
    kernel=C.WinDLL('kernel32',use_last_error=True)
    allocate=kernel.VirtualAlloc
    allocate.argtypes=[C.c_void_p,C.c_size_t,C.c_uint32,C.c_uint32];allocate.restype=C.c_void_p
    free=kernel.VirtualFree;free.argtypes=[C.c_void_p,C.c_size_t,C.c_uint32];free.restype=C.c_int
    protect=kernel.VirtualProtect
    protect.argtypes=[C.c_void_p,C.c_size_t,C.c_uint32,C.POINTER(C.c_uint32)];protect.restype=C.c_int
    size=0x2000000
    base=allocate(None,size,0x3000,4)
    assert base
    prepare=test.S14TestPrepareImage;prepare.argtypes=[C.c_void_p];prepare.restype=C.c_int
    prologue=test.S14TestPrologue
    prologue.argtypes=[C.c_int,C.POINTER(C.c_ubyte)];prologue.restype=C.c_int
    ui=test.S14TestUIPrologue
    ui.argtypes=[C.c_int,C.POINTER(C.c_ubyte),C.POINTER(C.c_int)];ui.restype=C.c_int

    def put(offset,data):C.memmove(base+offset,data,len(data))
    def get(offset,length):return C.string_at(base+offset,length)
    try:
        put(0,b'MZ');put(0x3c,struct.pack('<I',0x80));put(0x80,b'PE\0\0')
        put(0x84,struct.pack('<H',0x8664));put(0x98,struct.pack('<H',0x20b))
        put(0xd0,struct.pack('<I',size))
        for index in (0,1):
            data=(C.c_ubyte*15)();offset=prologue(index,data);put(offset,bytes(data))
        for index in range(3):
            data=(C.c_ubyte*32)();length=C.c_int();offset=ui(index,data,C.byref(length))
            put(offset,bytes(data)[:length.value])
        for slot,rva in [(1,0x702ed0),(2,0x701f00),(5,0x707050),(15,0x722490)]:
            put(0x133f170+slot*8,struct.pack('<Q',base+rva))
        put(0x20cdcd,b'\x48\x8b\x05'+struct.pack('<i',0x1fc91d0-0x20cdd4))
        returns=[0x6eebe0,0x7089fd,0x70d9bc,0x722571,0x724406,
                 0x733cd,0x7370a,0x74245,0x74605,0x70744c,0x26015d,0x70d9fa,0x37894d]
        for index,rva in enumerate(returns):
            target=0x251440 if index<11 else 0x251610
            put(rva-5,b'\xe8'+struct.pack('<i',target-rva))
        assert prepare(base)==1
        # No executable fingerprint/timestamp checks: unrelated changes pass.
        put(0x88,struct.pack('<I',123456789));put(0x2000,b'different-game-build')
        assert prepare(base)==1
        rejected=0
        for offset,data in [(0xd0,struct.pack('<I',4096)),(0x251610,b'\xcc'),
                            (returns[0]-1,b'\x7f'),(0x133f178,b'\0'*8),
                            (0x20cdd0,b'\0'*4),(0x84,b'\0\0'),(0x3c,b'\xff'*4)]:
            previous=get(offset,len(data));put(offset,data)
            assert prepare(base)==0,hex(offset)
            put(offset,previous);rejected+=1
        prior=C.c_uint32()
        assert protect(base+0x251000,4096,1,C.byref(prior))
        assert prepare(base)==0
        restored=C.c_uint32();assert protect(base+0x251000,4096,prior.value,C.byref(restored))
        assert prepare(base)==1
        return {'valid_layout_accepted':True,'different_fingerprint_accepted':True,
                'bad_layout_cases_rejected':rejected+1,'out_of_bounds_and_unreadable_safe':True,
                'game_process_touched':False}
    finally:
        assert free(base,0,0x8000)
