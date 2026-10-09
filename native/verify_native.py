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
from verify_entry import run_entry_tests
from verify_cleanup import run_cleanup_tests

sys.stdout.reconfigure(encoding='utf-8')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game-dir', type=Path, help='Optional local game directory for static checks; never uploaded')
parser.add_argument('--legacy-manager', type=Path, help='Optional verified previous release installer to exercise migration')
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
entry_metrics=run_entry_tests(test)
verified_prologues=[]
if GAME:
    # Compare the exact constants compiled into the plugin with the original PE.
    # The two hook targets save different first registers (RBP versus RBX).
    binary=(GAME/'SAN14PK_SC.exe').read_bytes()
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
    search_prologue=test.S14TestSearchPrologue
    search_prologue.argtypes=[C.c_int,C.POINTER(C.c_ubyte),C.POINTER(C.c_int)];search_prologue.restype=C.c_int
    for index in range(8):
        signature=(C.c_ubyte*32)();length=C.c_int()
        rva=search_prologue(index,signature,C.byref(length))
        assert bytes(signature)[:length.value]==rva_bytes(rva,length.value)
        code=(C.c_ubyte*64).from_buffer_copy(rva_bytes(rva,64))
        assert hook_bytes(code,64)==1,f'MinHook rejected private search copy of {rva:#x}'
        verified_prologues.append({'rva':hex(rva),'bytes':bytes(signature)[:length.value].hex(),'private_trampoline':'passed','feature':'auto_search'})
    vtable=struct.unpack('<16Q',rva_bytes(0x133f170,128))
    battle_prologue=test.S14TestBattlePrologue
    battle_prologue.argtypes=[C.c_int,C.POINTER(C.c_ubyte)];battle_prologue.restype=C.c_int
    for index in range(16):
        signature=(C.c_ubyte*16)();rva=battle_prologue(index,signature)
        assert bytes(signature)==rva_bytes(rva,16),(hex(rva),'battle entry mismatch')
        code=(C.c_ubyte*64).from_buffer_copy(rva_bytes(rva,64))
        assert hook_bytes(code,64)==1,f'MinHook rejected private battle copy of {rva:#x}'
        verified_prologues.append({'rva':hex(rva),'bytes':bytes(signature).hex(),'private_trampoline':'passed','feature':'battle_observation'})
    duel_anchor=test.S14TestDuelAnchor;duel_anchor.argtypes=[C.c_int,C.POINTER(C.c_ubyte),C.POINTER(C.c_int)];duel_anchor.restype=C.c_int
    for index in range(3):
        signature=(C.c_ubyte*32)();length=C.c_int();rva=duel_anchor(index,signature,C.byref(length))
        assert bytes(signature)[:length.value]==rva_bytes(rva,length.value)
        verified_prologues.append({'rva':hex(rva),'bytes':bytes(signature)[:length.value].hex(),'feature':'ordinary_duel_result_mapping'})
    assert (vtable[1],vtable[2],vtable[5],vtable[15])==tuple(0x140000000+r for r in (0x702ed0,0x701f00,0x707050,0x722490))
    capture_anchor=test.S14TestCaptureAnchor;capture_anchor.argtypes=[C.c_int,C.POINTER(C.c_ubyte),C.POINTER(C.c_int)];capture_anchor.restype=C.c_int
    for index in range(3):
        signature=(C.c_ubyte*32)();length=C.c_int();rva=capture_anchor(index,signature,C.byref(length))
        assert bytes(signature)[:length.value]==rva_bytes(rva,length.value)
        verified_prologues.append({'rva':hex(rva),'bytes':bytes(signature)[:length.value].hex(),'feature':'capture_report_actor_mapping'})
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
update_test=subprocess.run([str(BUILD/'test_update.exe')],capture_output=True,text=True,check=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
update_metrics=json.loads(update_test.stdout);assert update_metrics['update_model']=='passed'
officer_test=subprocess.run([str(BUILD/'test_officers.exe')],cwd=BUILD,capture_output=True,text=True,check=True,
                            timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
officer_metrics=json.loads(officer_test.stdout)
search_metrics={}
for name in ('test_search','test_search_bridge'):
    result=subprocess.run([str(BUILD/(name+'.exe'))],cwd=BUILD,capture_output=True,text=True,check=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
    search_metrics[name]=json.loads(result.stdout)
if GAME:
    # Execute the complete city constructor and predicate plus the unchanged
    # target-table store from this PE
    # inside the isolated fixture. Their relative globals stay in its synthetic
    # image; no game is loaded and no running process is accessed.
    assert struct.unpack('<Q',rva_bytes(0x129fb48,8))[0]==0x1402f6130
    constructor=rva_bytes(0x2034e0,147)
    predicate=rva_bytes(0x2f6130,55)
    target_store=rva_bytes(0x65dcc1,19)
    with tempfile.TemporaryDirectory(prefix='SAN14-city-ABI-') as name:
        fixture=Path(name)/'city-iterator.bin'
        fixture.write_bytes(struct.pack('<III',len(constructor),len(predicate),len(target_store))+constructor+predicate+target_store)
        result=subprocess.run([str(BUILD/'test_search_bridge.exe'),str(fixture)],cwd=BUILD,capture_output=True,text=True,check=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
        search_metrics['private_native_city_iterator']=json.loads(result.stdout)
        assert search_metrics['private_native_city_iterator']['private_native_iterator_fixture']
        assert search_metrics['private_native_city_iterator']['private_native_target_store']
        legacy=subprocess.run([str(BUILD/'test_search_bridge.exe'),str(fixture),'--reproduce-040'],cwd=BUILD,capture_output=True,text=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
        assert legacy.returncode==71,('Expected the recorded null RDX access violation',legacy.returncode,legacy.stdout,legacy.stderr)
        search_metrics['legacy_040_null_rdx_reproduced']=True
        legacy=subprocess.run([str(BUILD/'test_search_bridge.exe'),str(fixture),'--reproduce-041'],cwd=BUILD,capture_output=True,text=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
        assert legacy.returncode==74,('Expected the recorded null target-table write at person 393',legacy.returncode,legacy.stdout,legacy.stderr)
        search_metrics['legacy_041_null_target_table_reproduced']=True
package=C.CDLL(str(BUILD/'package_test.dll'))
install=package.S14TestInstall; install.argtypes=[C.c_wchar_p,C.c_wchar_p,C.c_wchar_p];install.restype=C.c_int
remove=package.S14TestRemove; remove.argtypes=[C.c_wchar_p,C.c_wchar_p];remove.restype=C.c_int
owned=package.S14TestOwned; owned.argtypes=[C.c_wchar_p];owned.restype=C.c_int
running=package.S14TestRunning; running.argtypes=[C.c_wchar_p];running.restype=C.c_int
error=C.create_unicode_buffer(192)
busy_guard=False
if GAME and running(str(GAME)):
    actual_hash=hashlib.sha256((GAME/'dinput8.dll').read_bytes()).hexdigest() if (GAME/'dinput8.dll').exists() else None
    assert install(str(GAME),str(BUILD/'SAN14ModManager.exe'),error)==0
    after_hash=hashlib.sha256((GAME/'dinput8.dll').read_bytes()).hexdigest() if (GAME/'dinput8.dll').exists() else None
    assert actual_hash==after_hash
    busy_guard=True
game_source=GAME/'SAN14PK_SC.exe' if GAME else BUILD/'test_native.exe'
input_hash=hashlib.sha256(game_source.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='SAN14-manager-测试-') as name:
    sandbox=Path(name).resolve()
    # Verify the recursive-cleanup target before exercising this owned sandbox.
    assert sandbox.parent==Path(tempfile.gettempdir()).resolve() and (GAME is None or sandbox!=GAME.resolve())
    # This is our independent native test binary under the process name checked
    # by the installer, never the real game. Keep its handle after termination
    # to exercise an exited process lingering in a Windows process snapshot.
    fixture_root=sandbox/'process-fixture';fixture_root.mkdir()
    fixture_app=fixture_root/'SAN14PK_SC.exe'
    shutil.copyfile(BUILD/'test_native.exe',fixture_app)
    fixture_process=subprocess.Popen([str(fixture_app)],creationflags=0x00000004|subprocess.CREATE_NO_WINDOW) # Win32 CREATE_SUSPENDED
    try:
        assert running(str(fixture_root))==1,'A live fixture must block installation'
        fixture_process.terminate();fixture_process.wait(timeout=5)
        assert running(str(fixture_root))==0,'An exited process must not block installation'
    finally:
        if fixture_process.poll() is None:fixture_process.terminate();fixture_process.wait(timeout=5)
        fixture_process._handle.Close()
    search_metrics['installer_live_vs_exited_process']=True
    game=sandbox/'SAN14PK_SC.exe';shutil.copyfile(game_source,game)
    dll=sandbox/'dinput8.dll';dll.write_bytes(b'other-proxy-must-survive')
    assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0
    assert dll.read_bytes()==b'other-proxy-must-survive'
    dll.unlink()
    app=sandbox/'SAN14ModManager.exe'
    app.write_bytes(b'other-program-must-survive')
    assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0 and not dll.exists()
    assert app.read_bytes()==b'other-program-must-survive'
    app.unlink()
    game.unlink()
    assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0 and not dll.exists()
    cli=subprocess.run([str(BUILD/'SAN14ModManager.exe'),'--install',str(sandbox)],capture_output=True,timeout=10,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    assert cli.returncode==2 and not dll.exists(),(cli.returncode,cli.stderr)
    shutil.copyfile(game_source,game)
    # Manual copying is valid even before an installation receipt exists.
    shutil.copyfile(BUILD/'SAN14ModManager.exe',app)
    assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==1,error.value
    assert owned(str(sandbox))==1 and dll.read_bytes()==(BUILD/'dinput8.dll').read_bytes()
    assert (sandbox/'SAN14ModManager.exe').read_bytes()==(BUILD/'SAN14ModManager.exe').read_bytes()
    config=sandbox/'SAN14BuildLimit.ini'
    config.write_text('[Manager]\nEnabled=1\n[Features]\nWallClusterLimit=0\nLimitHint=1\nAutoSearch=0\n[AutoSearch]\nExecutors=0\n[Observation]\nBattleEvents=0\n[Future]\nUnknownFeature=keep-me\n',encoding='ascii')
    # Hold only a sandbox manager file open to force a commit failure. The
    # native installer must restore the previous DLL and leave its receipt.
    old_dll=(BUILD/'dinput8.dll').read_bytes()+b'previous-sandbox-version';dll.write_bytes(old_dll)
    old_manager=app.read_bytes()+b'previous-sandbox-manager';app.write_bytes(old_manager)
    receipt=sandbox/'SAN14ModManager/installation.ini'
    metadata=configparser.ConfigParser();metadata.optionxform=str;metadata.read(receipt)
    metadata.set('Install','DllSHA256',hashlib.sha256(old_dll).hexdigest())
    metadata.set('Install','ManagerSHA256',hashlib.sha256(old_manager).hexdigest())
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
    assert all(setting in config.read_text() for setting in ('UnknownFeature=keep-me','WallClusterLimit=0','AutoSearch=0','Executors=0','BattleEvents=0'))
    assert (sandbox/'SAN14ModManager/backups/dinput8.previous.dll').read_bytes()==old_dll
    dll.write_bytes(dll.read_bytes()+b'changed')
    assert owned(str(sandbox))==0 and install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==0
    assert remove(str(sandbox),error)==0
    shutil.copyfile(BUILD/'dinput8.dll',dll)
    assert remove(str(sandbox),error)==1,error.value
    assert not dll.exists() and config.exists() and (sandbox/'SAN14ModManager.exe').exists()
    assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==1,error.value
    # An identical target EXE may be running/locked: it must not be replaced.
    held=create_file(str(app),0x80000000,1,None,3,0,None)
    assert held not in (None,C.c_void_p(-1).value)
    try:
        assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==1,error.value
    finally: assert close_handle(held)
    cli=subprocess.run([str(app),'--install',str(sandbox)],capture_output=True,timeout=10,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    assert cli.returncode==0,(cli.returncode,cli.stderr)
    assert hashlib.sha256(game.read_bytes()).hexdigest()==input_hash
legacy_tested=False
if args.legacy_manager:
    previous=args.legacy_manager.resolve().read_bytes()
    assert hashlib.sha256(previous).hexdigest() in {
        '4b46d2d923a65f70af7a514c4757a3dbfdea957aee433a4f9a78a5c5f8aedb94',
        '7447aac1a8b218ba0c1aa54d6a945c0140dc603ff943264c9b41bbf7fce9c5e8',
        'c96de53337ba48b1fc749857564bba0aa67431663121ee572f56c72e7e742387',
        'f9eb8e46c8fe40da83b41b38da92b911c5f4e065840f2703ffce057e568da113',
        '3d9497be3e12c93c7b3b71ba2203acbbab1288518120a37a85537ee9d5796418',
        'c38c1e8edd187928e9eebb59ba0ad9ca0bb346c78828630e602ac07d4231f58a',
        '4e7199ce2cbdb86492225327b6c54eec9ba6e96778c26c2e438ad8e324e59a82',
        '900a6ab7c55f54393e8c1cbffa3d8916a17181469fe37b8d7d1397a82810d7bd',
        'a12bde5634abcb4f3841d76852da523968f48fef73beab782c744a382c67764a',
        '4063432b33717b6d9ce4cfecd1d23e096926ab607e1187a7af43baa3e7bdf0ba',
        'deab45bf5bbb0538f5a4ed9492e50ad0676347189b7bbb674dcd8f6cb472f8f2',
        '1d32fff1c27f155b46494cd1f762474ed32eec6a613a1b88614691b7ff749030',
        '7be3eb8f46556e4603702dbf9aedf2443ff84f56c5cfc794aedc793e8824c692',
        '570c30fd02bc3ddc647889770c6bece79280fd6ed65a145b84e4c920721a0829',
        '4d94a1e3c7a18e4a08ff139d2bba4975630214a23e6a7c4574cbb3d29a2eb6f0',
        '1c3af86e9bcc940642f3f322ae865581355c936709bc8b6d17e41f8354df787f',
        '23f4186fad6c651640e9fbc208a387e8d00051ad24813fd62659fa81cd218de4',
        '4c962ddce7353a51f23e5d207763b163ec78105d5c631f489dfd00d7ff5f0df0',
        '958518645460caa9735a60c96c9de8508c6bed8f005f7dd577cb6f2647712a88',
        'c1d2dae333edfbe222908519085c43573756cb16f412db857bd74e5920bc98f9',
        '522230d684952cdbfb2db0864796363d94c690b8d042d417bcc0e42bae3f0f1f',
        'd1cdb01676e600797701917daca041817c1ac0fc16bf3ef2afb92859cd80c4ad',
        '0c9b5bddfec9f59e32ee04e8f6bec14bc3092d4a58f16a3c96ac7c1fd9371451',
        '68f2cfbffe0fc5233d5b210b5d2929332498c37c73c3ea36887d901feded7991'}
    with tempfile.TemporaryDirectory(prefix='SAN14-manager-migration-') as name:
        sandbox=Path(name).resolve()
        assert sandbox.parent==Path(tempfile.gettempdir()).resolve()
        shutil.copyfile(game_source,sandbox/'SAN14PK_SC.exe')
        (sandbox/'SAN14ModManager.exe').write_bytes(previous)
        assert install(str(sandbox),str(BUILD/'SAN14ModManager.exe'),error)==1,error.value
        assert (sandbox/'SAN14ModManager/backups/manager.previous.exe').read_bytes()==previous
        assert owned(str(sandbox))==1
        assert (sandbox/'SAN14ModManager.exe').read_bytes()==(BUILD/'SAN14ModManager.exe').read_bytes()
        legacy_tested=True
installer_metrics={'install_update_remove_reinstall':'passed','unicode_game_folder':'passed',
                   'unknown_proxy_preserved':True,'missing_game_rejected':True,'unlisted_game_fingerprint_accepted':True,
                   'unknown_manager_preserved':True,'copied_current_manager_adopted':True,
                   'locked_identical_manager_reinstall':True,'cli_install_from_game_directory':True,
                   'previous_release_without_receipt_migrated':legacy_tested,
                   'changed_owned_dll_rejected':True,'configuration_preserved':True,
                   'previous_dll_backup':True,'live_game_update_guard_tested':busy_guard,
                   'locked_manager_rolls_back_plugin':True,
                   'game_binary_unchanged':True}
cleanup_metrics=run_cleanup_tests(package,BUILD,args.legacy_manager)
battle_metrics={}
for name in ('test_battle_probe','test_battle_skills','test_battle_details','test_battle_save','test_battle_special'):
    with tempfile.TemporaryDirectory(prefix='S14-battle-verify-') as root:
        result=subprocess.run([str(BUILD/(name+'.exe')),root],capture_output=True,text=True,check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
        battle_fixture_metrics=json.loads(result.stdout);assert battle_fixture_metrics['status']=='passed'
        lines=[json.loads(l) for p in Path(root).rglob('*.jsonl') for l in p.read_text(encoding='utf-8').splitlines()]
        records=[e for e in lines if e['event']=='battle_observation']
        assert len(records)==battle_fixture_metrics['written_events']
        assert all(len(bytes.fromhex(o['raw_hex']))==o['raw_size'] for e in records for o in (e[k] for k in ('source','target','target_after','source_after','other')))
        if name=='test_battle_skills':
            effects=[e for e in records if e['kind']=='skill_effect_dispatch' and e['tactic_id']==5]
            assert effects and all(e['tactic_name']=='火矢' and e['effect_categories']==[17,0] and e['tactic_parameters']==[3,40,10,130,130,163,160,40,40,8] for e in effects)
        if name=='test_battle_details':
            ratios=[e for e in records if e['kind']=='wounded_generation_ratio']
            assert len(ratios)==7 and all(e['schema_version']==7 for e in records)
            assert {e['wound_rate_float_bits'] for e in ratios}>={'0x80000000','0x7fc12345','0x7f800000','0xff800000'}
            abnormal=[e for e in records if e['kind']=='army_abnormal_application']
            assert len(abnormal)==4 and abnormal[0]['target']['id']==2 and abnormal[0]['source']['id']==1
            assert abnormal[0]['abnormal_before']==0 and abnormal[0]['abnormal_after']==15
            troops=[e for e in records if e['kind']=='troop_change']
            assert all(e['player_force_id']==1 and e['source_force_id']==1 and e['target_force_id']==2 and e['target']['raw_force']==0 for e in troops)
        battle_metrics[name]=battle_fixture_metrics
analysis_test=subprocess.run([sys.executable,str(HERE.parent/'tools/test_summarize_battle.py')],capture_output=True,text=True,check=True,timeout=20)
battle_metrics['analysis_tests']=analysis_test.stderr.strip()
round_result=subprocess.run([str(BUILD/'test_battle_report.exe')],capture_output=True,text=True,check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
battle_metrics['turn_report']=json.loads(round_result.stdout)
assert battle_metrics['turn_report']['status']=='passed'
report_ui_command=[str(BUILD/'test_report_ui.exe')]+([str(GAME)] if GAME else [])
result=subprocess.run(report_ui_command,cwd=BUILD,capture_output=True,text=True,check=True,
                      timeout=45,creationflags=subprocess.CREATE_NO_WINDOW)
battle_metrics['turn_report_ui']=json.loads(result.stdout)
assert battle_metrics['turn_report_ui']['status']=='passed'
portrait_command=[str(BUILD/'test_portrait.exe')]+([str(GAME),str(BUILD)] if GAME else [])
result=subprocess.run(portrait_command,cwd=BUILD,capture_output=True,text=True,check=True,
                      timeout=60,creationflags=subprocess.CREATE_NO_WINDOW)
battle_metrics['portraits']=json.loads(result.stdout)
assert battle_metrics['portraits']['status']=='passed'
with tempfile.TemporaryDirectory(prefix='S14-stats-') as folder:
    stats_result=subprocess.run([str(BUILD/'test_battle_stats.exe'),folder],capture_output=True,text=True,check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
    battle_metrics['persistent_stats']=json.loads(stats_result.stdout)
    reopen=subprocess.run([str(BUILD/'test_battle_stats.exe'),folder,'--reopen'],capture_output=True,text=True,check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
    assert json.loads(reopen.stdout)['cross_process_restore']
    battle_metrics['persistent_stats']['cross_process_restore']=True
battle_metrics['in_game_acceptance']='pending'
with tempfile.TemporaryDirectory(prefix='S14-timeline-') as folder:
    result=subprocess.run([str(BUILD/'test_timeline.exe'),folder],capture_output=True,text=True,check=True,timeout=25,creationflags=subprocess.CREATE_NO_WINDOW)
    battle_metrics['timeline']=json.loads(result.stdout)
    reopen=subprocess.run([str(BUILD/'test_timeline.exe'),folder,'--reopen'],capture_output=True,text=True,check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
    assert json.loads(reopen.stdout)['cross_process_restore']
    battle_metrics['timeline']['cross_process_restore']=True
with tempfile.TemporaryDirectory(prefix='S14-special-') as folder:
    result=subprocess.run([str(BUILD/'test_special_stats.exe'),folder],capture_output=True,text=True,check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
    battle_metrics['special_stats']=json.loads(result.stdout)
    reopen=subprocess.run([str(BUILD/'test_special_stats.exe'),folder,'--reopen'],capture_output=True,text=True,check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
    assert json.loads(reopen.stdout)['cross_process_restore']
    battle_metrics['special_stats']['cross_process_restore']=True
    battle_metrics['special_stats']['in_game_semantics']='pending'
detail_result=subprocess.run([str(BUILD/'test_native_detail.exe')],capture_output=True,text=True,check=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
detail_metrics=json.loads(detail_result.stdout);assert detail_metrics['status']=='passed'
detail_ui_result=subprocess.run([str(BUILD/'test_detail_ui.exe')],capture_output=True,text=True,check=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
detail_metrics['ui']=json.loads(detail_ui_result.stdout);assert detail_metrics['ui']['status']=='passed'
detail_metrics['game_process_touched']=False;detail_metrics['in_game_acceptance']='pending'
if GAME:
    for rva,hexcode in ((0x58bae9,'488b87e002000048899840010000'),(0x80a264,'48899168010000'),(0xf4d4,'488d05b56e9d01')):
        assert rva_bytes(rva,len(bytes.fromhex(hexcode)))==bytes.fromhex(hexcode)
    detail_metrics['static_adapter_anchors_verified']=3
report={'status':'passed','game_version_check':False,'game_sha256_check':False,'hook_entry_validation':entry_metrics,'verified_game_prologues':verified_prologues,
        'territory_adapter_cases':512,'creation_correlation':'passed',
        'random_reference_cases':random_cases,'private_snapshots_required':False,
        'tls_thread_count':8,'tls_calls':8000,'full_queue_dropped':dropped,
        'queue_test_ms':round(elapsed*1000,3),'directinput_proxy_hresult':hr,
        'synthetic_hook_and_rule':metrics,
        'interaction':ui_metrics,'verified_phase_call_sites':len(wrapper_returns)+2 if GAME else 0,
        'manager':manager_metrics,'github_update':update_metrics,'officers':officer_metrics,'native_officer_detail':detail_metrics,'auto_search':search_metrics,'battle_observation':battle_metrics,'installer':installer_metrics,'cleanup':cleanup_metrics,
        'private_actual_check_off_on':list(switch_results),
        'manager_exe_sha256':hashlib.sha256((BUILD/'SAN14ModManager.exe').read_bytes()).hexdigest(),
        'production_dll_sha256':hashlib.sha256((BUILD/'dinput8.dll').read_bytes()).hexdigest(),
        'game_process_touched':False}
(BUILD/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
