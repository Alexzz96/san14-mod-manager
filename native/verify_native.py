"""Validate native artifacts in this Python process, never in the game."""
import argparse
import ctypes as C
import configparser
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import random
import struct
import shutil
import subprocess
import sys
import time
import tempfile
import uuid

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'reference'))
from build_limit import Building,evaluate

sys.stdout.reconfigure(encoding='utf-8')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game-dir', type=Path, help='Optional local compatible game directory; never uploaded')
args=parser.parse_args()
GAME=args.game_dir.resolve() if args.game_dir else None
BUILD=HERE/'build' 
class Cell(C.Structure):
    _fields_=[('type',C.c_int),('owner',C.c_int)]
class Decision(C.Structure):
    _fields_=[(x,C.c_int) for x in ('allowed','reason','count','exact','reads')]

production=C.CDLL(str(BUILD/'dinput8.dll'))
test=C.CDLL(str(BUILD/'plugin_test.dll'))
live_switch=test.S14TestLiveSwitch
live_switch.argtypes=[C.POINTER(C.c_int)];live_switch.restype=C.c_int
switch_results=(C.c_int*5)();assert live_switch(switch_results)==1
assert list(switch_results)==[0,1,0,1,1],list(switch_results)
decode_cell=test.S14TestMapCell
decode_cell.argtypes=[C.POINTER(C.c_ubyte),C.POINTER(C.c_ubyte),C.POINTER(Cell)];decode_cell.restype=None
for type_id in (9,10):
    for territory in range(256):
        hex_record=(C.c_ubyte*32)();building_record=(C.c_ubyte*32)()
        hex_record[0x14]=territory;building_record[0x12]=type_id;building_record[0x13]=(territory+23)%256
        decoded=Cell();decode_cell(hex_record,building_record,C.byref(decoded))
        assert (decoded.type,decoded.owner)==(type_id,territory if territory<=51 else 0)
correlation=test.S14TestCorrelation
correlation.argtypes=[C.c_int]*6;correlation.restype=C.c_int
assert correlation(32665,9,1,32665,9,1)==1
assert correlation(32665,9,1,32665,9,2)==0
assert correlation(32665,9,1,32664,9,1)==0
verified_prologues=[]
if GAME:
    game_hash=test.S14TestHash
    game_hash.argtypes=[C.c_wchar_p];game_hash.restype=C.c_int
    assert game_hash(str(GAME/'SAN14PK_SC.exe'))==1
    assert game_hash(str(BUILD/'test_native.exe'))==0
    
    # Compare the exact constants compiled into the plugin with the original PE.
    # The two hook targets save different first registers (RBP versus RBX).
    binary=(GAME/'SAN14PK_SC.exe').read_bytes()
    assert hashlib.sha256(binary).hexdigest()=='e6ae68925c266a19b05641913e60bf7d97d5eb4754901c3e82a5362d05ff7372'
    nt=struct.unpack_from('<I',binary,0x3c)[0]
    sections=struct.unpack_from('<H',binary,nt+6)[0]
    section_table=nt+24+struct.unpack_from('<H',binary,nt+20)[0]
    def rva_bytes(rva,length):
        for index in range(sections):
            offset=section_table+40*index
            virtual_size,virtual_address,raw_size,raw_offset=struct.unpack_from('<4I',binary,offset+8)
            if virtual_address<=rva and rva+length<=virtual_address+raw_size:
                start=raw_offset+rva-virtual_address
                return binary[start:start+length]
        raise AssertionError(f'RVA {rva:#x} is outside file-backed sections')
    prologue=test.S14TestPrologue
    prologue.argtypes=[C.c_int,C.POINTER(C.c_ubyte)];prologue.restype=C.c_int
    for creation in (0,1):
        signature=(C.c_ubyte*15)()
        rva=prologue(creation,signature)
        assert bytes(signature)==rva_bytes(rva,15),(hex(rva),bytes(signature).hex(),rva_bytes(rva,15).hex())
        verified_prologues.append({'rva':hex(rva),'bytes':bytes(signature).hex()})
    ui_prologue=test.S14TestUIPrologue
    ui_prologue.argtypes=[C.c_int,C.POINTER(C.c_ubyte),C.POINTER(C.c_int)];ui_prologue.restype=C.c_int
    hook_bytes=test.S14TestHookBytes
    hook_bytes.argtypes=[C.POINTER(C.c_ubyte),C.c_int];hook_bytes.restype=C.c_int
    for index in range(3):
        signature=(C.c_ubyte*32)();length=C.c_int()
        rva=ui_prologue(index,signature,C.byref(length))
        assert bytes(signature)[:length.value]==rva_bytes(rva,length.value)
        code=(C.c_ubyte*64).from_buffer_copy(rva_bytes(rva,64))
        assert hook_bytes(code,64)==1,f'MinHook rejected private copy of {rva:#x}'
        verified_prologues.append({'rva':hex(rva),'bytes':bytes(signature)[:length.value].hex(),
                                   'private_trampoline':'passed'})
    vtable=struct.unpack('<16Q',rva_bytes(0x133f170,128))
    assert (vtable[1],vtable[2],vtable[5],vtable[15])==tuple(0x140000000+r for r in (0x702ed0,0x701f00,0x707050,0x722490))
    # Each phase key must be an actual call to the appropriate original checker.
    wrapper_returns=(0x6eebe0,0x7089fd,0x70d9bc,0x722571,0x724406,
                     0x733cd,0x7370a,0x74245,0x74605,0x70744c,0x26015d)
    for ret in wrapper_returns+(0x70d9fa,0x37894d):
        instruction=rva_bytes(ret-5,5)
        assert instruction[0]==0xe8
        destination=ret+struct.unpack('<i',instruction[1:])[0]
        assert destination==(0x251440 if ret in wrapper_returns else 0x251610),(hex(ret),hex(destination))
fn=production.S14EvaluateBoard
fn.argtypes=[C.POINTER(Cell),C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.POINTER(Decision)]
fn.restype=None
types={9:'earth_wall',10:'stone_wall'}

def native(cells,width,height,tile,type_id,owner,original=True,continuation=False):
    board=(Cell*(width*height))()
    for position,building in cells.items():
        board[position]=Cell(9 if building.kind=='earth_wall' else (10 if building.kind=='stone_wall' else 4),building.owner)
    result=Decision()
    fn(board,width,height,tile,type_id,owner,original,continuation,C.byref(result))
    return result

def assert_same(cells,width,height,tile,type_id,owner,original=True,continuation=False):
    expected=evaluate(cells,tile,Building(types.get(type_id,'other'),owner),
                      original_allowed=original,continuation=continuation,width=width,height=height)
    actual=native(cells,width,height,tile,type_id,owner,original,continuation)
    assert bool(actual.allowed)==expected.allowed
    assert (None if actual.count<0 else actual.count)==expected.component_count
    assert bool(actual.exact)==expected.count_is_exact
    assert actual.reads==expected.neighbor_reads and actual.reads<=30

rng=random.Random(20261007)
random_cases=2000
for _ in range(random_cases):
    tile=rng.randrange(64)
    cells={i:rng.choice((Building('earth_wall',0),Building('stone_wall',0),
           Building('earth_wall',1),Building('other',0))) for i in range(64)
           if i!=tile and rng.random()<.75}
    assert_same(cells,8,8,tile,rng.choice((9,10,4)),rng.choice((0,1)),original=rng.random()>.1)

# The same variables used for check/create correlation must be per thread.
tls=test.S14TestTLS
tls.argtypes=[C.c_int]; tls.restype=C.c_int
def tls_stress(marker):
    for _ in range(1000): assert tls(marker)==marker
with ThreadPoolExecutor(max_workers=8) as executor:
    list(executor.map(tls_stress,range(8)))
queue=test.S14TestQueue
queue.argtypes=[C.c_int]; queue.restype=C.c_int
started=time.perf_counter(); dropped=queue(1024); elapsed=time.perf_counter()-started
assert dropped>=768 and elapsed<1.0,(dropped,elapsed)

# Exercise the actual proxy export against the system DirectInput DLL.
kernel=C.WinDLL('kernel32',use_last_error=True)
kernel.GetModuleHandleW.argtypes=[C.c_wchar_p]; kernel.GetModuleHandleW.restype=C.c_void_p
proxy=C.WinDLL(str(BUILD/'dinput8.dll'))
create=proxy.DirectInput8Create
create.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p,C.POINTER(C.c_void_p),C.c_void_p]
create.restype=C.c_int32
iid=C.create_string_buffer(uuid.UUID('BF798031-483A-4DA2-AA99-5D64ED369700').bytes_le)
interface=C.c_void_p()
hr=create(kernel.GetModuleHandleW(None),0x0800,iid,C.byref(interface),None)
assert hr==0 and interface.value,(hr,interface.value)
vtable=C.cast(interface,C.POINTER(C.POINTER(C.c_void_p))).contents
release=C.WINFUNCTYPE(C.c_uint32,C.c_void_p)(vtable[2])
release(interface)

stress=subprocess.run([str(BUILD/'test_native.exe')],capture_output=True,text=True,check=True,
                      timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
metrics=json.loads(stress.stdout)
ui_test=subprocess.run([str(BUILD/'test_interaction.exe')],cwd=BUILD,capture_output=True,text=True,check=True,
                       timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
ui_metrics=json.loads(ui_test.stdout)
manager_test=subprocess.run([str(BUILD/'test_manager.exe')],cwd=BUILD,capture_output=True,text=True,check=True,
                            timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
manager_metrics=json.loads(manager_test.stdout)
package=C.CDLL(str(BUILD/'package_test.dll'))
install=package.S14TestInstall; install.argtypes=[C.c_wchar_p,C.c_wchar_p,C.c_wchar_p];install.restype=C.c_int
remove=package.S14TestRemove; remove.argtypes=[C.c_wchar_p,C.c_wchar_p];remove.restype=C.c_int
owned=package.S14TestOwned; owned.argtypes=[C.c_wchar_p];owned.restype=C.c_int
running=package.S14TestRunning; running.argtypes=[C.c_wchar_p];running.restype=C.c_int
error=C.create_unicode_buffer(192)
installer_metrics={'status':'skipped','reason':'--game-dir not supplied'}
if GAME:
    actual_root=GAME
    actual_hash=hashlib.sha256((actual_root/'dinput8.dll').read_bytes()).hexdigest() if (actual_root/'dinput8.dll').exists() else None
    busy_guard=False
    if running(str(actual_root)):
        assert install(str(actual_root),str(BUILD/'SAN14ModManager.exe'),error)==0
        after_hash=hashlib.sha256((actual_root/'dinput8.dll').read_bytes()).hexdigest() if (actual_root/'dinput8.dll').exists() else None
        assert after_hash==actual_hash
        busy_guard=True
    with tempfile.TemporaryDirectory(prefix='SAN14-manager-测试-') as name:
        sandbox=Path(name).resolve()
        # Verify the recursive-cleanup target before exercising this owned sandbox.
        assert sandbox.parent==Path(tempfile.gettempdir()).resolve() and sandbox!=actual_root.resolve()
        game=sandbox/'SAN14PK_SC.exe';shutil.copyfile(actual_root/'SAN14PK_SC.exe',game)
        dll=sandbox/'dinput8.dll';dll.write_bytes(b'other-proxy-must-survive')
        assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0
        assert dll.read_bytes()==b'other-proxy-must-survive'
        dll.unlink()
        game.write_bytes(b'unsupported-game')
        assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0 and not dll.exists()
        cli=subprocess.run([str(BUILD/'SAN14ModManager.exe'),'--install',str(sandbox)],capture_output=True,timeout=10,
                           creationflags=subprocess.CREATE_NO_WINDOW)
        assert cli.returncode==2 and not dll.exists(),(cli.returncode,cli.stderr)
        shutil.copyfile(actual_root/'SAN14PK_SC.exe',game)
        assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==1,error.value
        assert owned(str(sandbox))==1 and dll.read_bytes()==(BUILD/'dinput8.dll').read_bytes()
        assert (sandbox/'SAN14ModManager.exe').read_bytes()==(BUILD/'SAN14ModManager.exe').read_bytes()
        config=sandbox/'SAN14BuildLimit.ini'
        config.write_text('[Manager]\nEnabled=1\n[Features]\nWallClusterLimit=0\nLimitHint=1\n[Future]\nUnknownFeature=keep-me\n',encoding='ascii')
        # Hold only a sandbox manager file open to force a commit failure. The
        # native installer must restore the previous DLL and leave its receipt.
        old_dll=(BUILD/'dinput8.dll').read_bytes()+b'previous-sandbox-version';dll.write_bytes(old_dll)
        receipt=sandbox/'SAN14ModManager/installation.ini'
        metadata=configparser.ConfigParser();metadata.optionxform=str;metadata.read(receipt)
        metadata.set('Install','DllSHA256',hashlib.sha256(old_dll).hexdigest())
        with receipt.open('w',encoding='ascii') as output: metadata.write(output)
        create_file=kernel.CreateFileW
        create_file.argtypes=[C.c_wchar_p,C.c_uint32,C.c_uint32,C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p];create_file.restype=C.c_void_p
        close_handle=kernel.CloseHandle;close_handle.argtypes=[C.c_void_p];close_handle.restype=C.c_int
        held=create_file(str(sandbox/'SAN14ModManager.exe'),0x80000000,1,None,3,0,None)
        assert held not in (None,C.c_void_p(-1).value)
        try:
            assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0
            assert dll.read_bytes()==old_dll and owned(str(sandbox))==1
        finally: assert close_handle(held)
        assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==1,error.value
        assert 'UnknownFeature=keep-me' in config.read_text() and 'WallClusterLimit=0' in config.read_text()
        assert (sandbox/'SAN14ModManager/backups/dinput8.previous.dll').read_bytes()==old_dll
        dll.write_bytes(dll.read_bytes()+b'changed')
        assert owned(str(sandbox))==0 and install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0
        assert remove(str(sandbox),error)==0
        shutil.copyfile(BUILD/'dinput8.dll',dll)
        assert remove(str(sandbox),error)==1,error.value
        assert not dll.exists() and config.exists() and (sandbox/'SAN14ModManager.exe').exists()
        assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==1,error.value
        assert hashlib.sha256(game.read_bytes()).hexdigest()=='e6ae68925c266a19b05641913e60bf7d97d5eb4754901c3e82a5362d05ff7372'
    installer_metrics={'install_update_remove_reinstall':'passed','unicode_game_folder':'passed',
                       'unknown_proxy_preserved':True,'unsupported_game_rejected':True,
                       'changed_owned_dll_rejected':True,'configuration_preserved':True,
                       'previous_dll_backup':True,'live_game_update_guard_tested':busy_guard,
                       'locked_manager_rolls_back_plugin':True,
                       'game_binary_unchanged':True}
report={'status':'passed','native_game_hash_check':'passed' if GAME else 'skipped','verified_game_prologues':verified_prologues,
        'territory_adapter_cases':512,'creation_correlation':'passed',
        'random_reference_cases':random_cases,'private_snapshots_required':False,
        'tls_thread_count':8,'tls_calls':8000,'full_queue_dropped':dropped,
        'queue_test_ms':round(elapsed*1000,3),'directinput_proxy_hresult':hr,
        'synthetic_hook_and_rule':metrics,
        'interaction':ui_metrics,'verified_phase_call_sites':len(wrapper_returns)+2 if GAME else 0,
        'manager':manager_metrics,'installer':installer_metrics,
        'private_actual_check_off_on':list(switch_results),
        'manager_exe_sha256':hashlib.sha256((BUILD/'SAN14ModManager.exe').read_bytes()).hexdigest(),
        'production_dll_sha256':hashlib.sha256((BUILD/'dinput8.dll').read_bytes()).hexdigest(),
        'game_process_touched':False}
(BUILD/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
