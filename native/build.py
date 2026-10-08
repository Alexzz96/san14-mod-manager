"""Build Windows x64 artifacts with Zig 0.15.2; no game files needed."""
from pathlib import Path
import argparse
import os
import json
import hashlib
import shutil
import subprocess

HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--zig', default=os.environ.get('ZIG_EXE') or shutil.which('zig'))
args=parser.parse_args()
if not args.zig:
    parser.error('Install Zig 0.15.2 and add zig to PATH, set ZIG_EXE, or use --zig PATH')
ZIG=Path(args.zig).resolve()
version=subprocess.check_output([str(ZIG),'version'],text=True).strip()
if version != '0.15.2':
    parser.error(f'Expected Zig 0.15.2; found {version}')
BUILD=HERE/'build'
MINHOOK=HERE/'vendor'/'minhook'
BUILD.mkdir(exist_ok=True)
sources=[MINHOOK/'src'/x for x in ('buffer.c','hook.c','trampoline.c','hde/hde64.c')]
common=[str(ZIG),'cc','-target','x86_64-windows-gnu','-std=c11','-O2','-Werror','-fno-emulated-tls',
        '-DUNICODE','-D_UNICODE','-I',str(MINHOOK/'include'),'-I',str(HERE)]

def run(extra):
    subprocess.run(common+[str(x) for x in extra],check=True,cwd=HERE)

def run_test(extra):
    # Zig's optimized C builds define NDEBUG. Assertions are the test checks and
    # sometimes invoke the operation being tested, so keep them active explicitly.
    run(['-UNDEBUG',*extra])

run_test([HERE/'test_native.c',HERE/'rule.c',*sources,'-o',BUILD/'test_native.exe'])
run_test([HERE/'test_interaction.c',HERE/'interaction.c',HERE/'toast.c',*sources,'-luser32','-lgdi32','-o',BUILD/'test_interaction.exe'])
stats_sources=[HERE/'battle_stats.c']
manager_sources=[*stats_sources,HERE/'features.c',HERE/'manager_ui.c',HERE/'package.c',HERE/'cleanup.c',HERE/'search_model.c']
update_sources=[HERE/'update.c',HERE/'update_model.c']
run_test([HERE/'test_update.c',HERE/'update_model.c','-o',BUILD/'test_update.exe'])
run_test([HERE/'test_search.c',HERE/'search_model.c','-o',BUILD/'test_search.exe'])
round_sources=[HERE/'battle_report.c',HERE/'turn_report.c',*stats_sources,'-lbcrypt']
run_test([HERE/'test_battle_stats.c',*stats_sources,'-lbcrypt','-o',BUILD/'test_battle_stats.exe'])
run_test([HERE/'test_battle_report.c',*round_sources,'-o',BUILD/'test_battle_report.exe'])
run_test([HERE/'test_search_bridge.c',HERE/'search_model.c',HERE/'battle_probe.c',*round_sources,*sources,'-o',BUILD/'test_search_bridge.exe'])
run_test([HERE/'test_battle_probe.c',*round_sources,*sources,'-o',BUILD/'test_battle_probe.exe'])
run_test([HERE/'test_battle_skills.c',*round_sources,*sources,'-o',BUILD/'test_battle_skills.exe'])
run_test([HERE/'test_battle_details.c',*round_sources,*sources,'-o',BUILD/'test_battle_details.exe'])
run_test([HERE/'test_battle_save.c',*round_sources,*sources,'-o',BUILD/'test_battle_save.exe'])
subprocess.run([str(ZIG),'rc','/fo',str(BUILD/'officer-controls.res'),str(HERE/'officer-controls.rc')],check=True,cwd=HERE)
officer_sources=[HERE/'pinyin.c',HERE/'officer_model.c',HERE/'officer_ui.c',BUILD/'officer-controls.res']
detail_sources=[HERE/'detail_model.c',HERE/'detail_ui.c']
run_test([HERE/'test_native_detail.c',HERE/'detail_model.c','-o',BUILD/'test_native_detail.exe'])
run_test([HERE/'test_detail_ui.c',HERE/'detail_model.c',*stats_sources,'-lbcrypt','-luser32','-lgdi32','-o',BUILD/'test_detail_ui.exe'])
run([HERE/'detail_probe.c',HERE/'detail_model.c','-municode','-lpsapi','-o',BUILD/'detail_probe.exe'])
run_test([HERE/'test_officers.c',*officer_sources,'-luser32','-lgdi32','-lcomctl32','-o',BUILD/'test_officers.exe'])
run([HERE/'officer_probe.c',*officer_sources,'-municode','-lpsapi','-luser32','-lgdi32','-lcomctl32','-o',BUILD/'officer_probe.exe'])
plugin_sources=[HERE/'plugin.c',HERE/'auto_search.c',HERE/'battle_probe.c',*round_sources[:2],HERE/'rule.c',HERE/'interaction.c',HERE/'toast.c',*officer_sources,*detail_sources,*manager_sources,*sources,HERE/'exports.def','-lbcrypt','-luser32','-lgdi32','-lshell32','-lcomctl32']
run(['-shared',*plugin_sources,'-o',BUILD/'dinput8.dll'])
run(['-shared','-DS14_SELFTEST',*plugin_sources,'-o',BUILD/'plugin_test.dll'])
payload=(BUILD/'dinput8.dll').read_bytes()
(BUILD/'payload.h').write_text('static const unsigned char s14_payload[]={\n'+
    '\n'.join(','.join(f'0x{x:02x}' for x in payload[i:i+32])+',' for i in range(0,len(payload),32))+
    '\n};\nstatic const char s14_payload_hash[]="'+hashlib.sha256(payload).hexdigest()+'";\n',encoding='ascii')
run(['-DS14_INSTALLER','-I',BUILD,HERE/'manager_main.c',*manager_sources,*update_sources,'-municode','-Wl,--subsystem,windows','-lwinhttp','-lbcrypt','-luser32','-lgdi32','-lshell32','-lole32','-luuid','-o',BUILD/'SAN14ModManager.exe'])
run_test([HERE/'test_manager.c',*manager_sources,'-lbcrypt','-luser32','-lgdi32','-o',BUILD/'test_manager.exe'])
run(['-shared','-DS14_INSTALLER','-I',BUILD,HERE/'test_package_exports.c',HERE/'package.c',HERE/'cleanup.c',HERE/'features.c',*stats_sources,'-lbcrypt','-o',BUILD/'package_test.dll'])
shutil.copyfile(MINHOOK/'LICENSE.txt',BUILD/'MinHook-LICENSE.txt')
commit=json.loads((MINHOOK/'UPSTREAM.json').read_text(encoding='utf-8'))['commit']
(BUILD/'build-provenance.json').write_text(json.dumps({'compiler':'Zig '+version,'minhook_commit':commit,
    'pinyin_data':json.loads((HERE/'vendor/pinyin/UPSTREAM.json').read_text(encoding='utf-8')),
    'target':'x86_64-windows-gnu','optimization':'O2','native_tests_assertions_enabled':True},indent=2),encoding='utf-8')
print(str(BUILD))
