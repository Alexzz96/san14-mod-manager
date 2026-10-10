"""Compile our HLSL at build time; no shader compiler DLL required by players."""
from pathlib import Path
import ctypes as C
from ctypes import wintypes as W
HERE=Path(__file__).resolve().parent
def build_shaders(output):
    k=C.WinDLL('kernel32',use_last_error=True);directory=C.create_unicode_buffer(32768)
    assert k.GetSystemDirectoryW(directory,len(directory))
    compiler=C.WinDLL(str(Path(directory.value)/'d3dcompiler_47.dll'))
    compile_=compiler.D3DCompile
    compile_.argtypes=[C.c_void_p,C.c_size_t,C.c_char_p,C.c_void_p,C.c_void_p,C.c_char_p,C.c_char_p,W.UINT,W.UINT,C.POINTER(C.c_void_p),C.POINTER(C.c_void_p)]
    compile_.restype=C.c_long
    source=(HERE/'map_halo.hlsl').read_bytes();parts=[]
    for entry in (b'vs',b'ps',b'troop_ps'):
        code=C.c_void_p();errors=C.c_void_p()
        target=b'vs_4_0' if entry==b'vs' else b'ps_4_0'
        hr=compile_(source,len(source),b'map_halo.hlsl',None,None,entry,target,1<<15,0,C.byref(code),C.byref(errors))
        def method(blob,index,restype):
            table=C.cast(blob,C.POINTER(C.POINTER(C.c_void_p))).contents
            return C.WINFUNCTYPE(restype,C.c_void_p)(table[index])(blob)
        if hr<0:
            message=C.string_at(method(errors,3,C.c_void_p),method(errors,4,C.c_size_t)).decode(errors='replace') if errors else ''
            raise RuntimeError(f'D3DCompile {hr:#x}: {message}')
        data=C.string_at(method(code,3,C.c_void_p),method(code,4,C.c_size_t))
        method(code,2,W.ULONG)
        if errors:method(errors,2,W.ULONG)
        parts.append('static const unsigned char s14_map_'+entry.decode()+'[]={'+','.join(f'0x{x:02x}' for x in data)+'};\n')
    output.write_text(''.join(parts),encoding='ascii')
