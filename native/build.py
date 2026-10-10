"""Build Windows x64 artifacts with Zig 0.15.2; no game files needed."""
from pathlib import Path
from compile_map_shaders import build_shaders
import argparse
import os
import json
import hashlib
import shutil
import subprocess
import re
import sys

HERE=Path(__file__).resolve().parent
source_version=(HERE.parent/'VERSION').read_text(encoding='utf-8').strip()
native_version=re.search(r'#define S14_MANAGER_VERSION L"([^"]+)"',(HERE/'features.h').read_text(encoding='utf-8'))
if not native_version or native_version.group(1)!=source_version:
    raise SystemExit('VERSION and native/features.h must match before building')
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
subprocess.run([sys.executable,str(HERE.parent/'tools/troop_catalog.py')],check=True)
build_shaders(BUILD/'map_shaders.h')
sources=[MINHOOK/'src'/x for x in ('buffer.c','hook.c','trampoline.c','hde/hde64.c')]
common=[str(ZIG),'cc','-target','x86_64-windows-gnu','-std=c11','-O2','-Werror','-fno-emulated-tls',
        '-DUNICODE','-D_UNICODE','-DMINIZ_NO_ZLIB_APIS','-DMINIZ_NO_ARCHIVE_APIS','-DMINIZ_NO_DEFLATE_APIS','-DMINIZ_NO_TIME','-I',str(MINHOOK/'include'),'-I',str(HERE),'-I',str(BUILD)]

def run(extra):
    subprocess.run(common+[str(HERE/'career_affix.c'),str(HERE/'ai_affix.c'),'-lbcrypt']+[str(x) for x in extra],check=True,cwd=HERE)

def run_test(extra):
    # Zig's optimized C builds define NDEBUG. Assertions are the test checks and
    # sometimes invoke the operation being tested, so keep them active explicitly.
    run(['-UNDEBUG',HERE/'troop_stub.c',*extra])

troop_sources=[HERE/'troop_registry.c']
subprocess.run(common+['-UNDEBUG',str(HERE/'test_ai_affix.c'),'-lbcrypt','-o',str(BUILD/'test_ai_affix.exe')],check=True,cwd=HERE)
run(['-UNDEBUG',HERE/'test_personality_edit.c',HERE/'personality_model.c','-o',BUILD/'test_personality_edit.exe'])
run_test([HERE/'test_officer_capture.c',HERE/'officer_model.c',HERE/'pinyin.c',HERE/'battle_stats.c',HERE/'special_stats.c',HERE/'battle_place.c',HERE/'battle_timeline.c','-lbcrypt','-o',BUILD/'test_officer_capture.exe'])
run(['-UNDEBUG',HERE/'test_troop_runtime.c',HERE/'troop_store.c',*troop_sources,*sources,'-luser32','-lgdi32','-o',BUILD/'test_troop_runtime.exe'])
run_test([HERE/'test_troop_registry.c',*troop_sources,'-o',BUILD/'test_troop_registry.exe'])
run([HERE/'troop_catalog_cli.c',*troop_sources,'-o',BUILD/'troop_catalog.exe'])
run_test([HERE/'test_native.c',HERE/'rule.c',*sources,'-o',BUILD/'test_native.exe'])
run_test([HERE/'test_interaction.c',HERE/'interaction.c',HERE/'toast.c',*sources,'-luser32','-lgdi32','-o',BUILD/'test_interaction.exe'])
timeline_sources=[HERE/'battle_place.c',HERE/'battle_timeline.c']
stats_sources=[HERE/'battle_stats.c',HERE/'special_stats.c',*timeline_sources]
manager_sources=[HERE/'troop_store.c',*stats_sources,HERE/'features.c',HERE/'manager_ui.c',HERE/'package.c',HERE/'cleanup.c',HERE/'search_model.c']
update_sources=[HERE/'update.c',HERE/'update_model.c']
run_test([HERE/'test_update.c',HERE/'update_model.c','-o',BUILD/'test_update.exe'])
run_test([HERE/'test_search.c',HERE/'search_model.c','-o',BUILD/'test_search.exe'])
round_sources=[HERE/'battle_report.c',HERE/'turn_report.c',HERE/'report_model.c',*stats_sources,'-lbcrypt']
run_test([HERE/'test_battle_stats.c',*stats_sources,'-lbcrypt','-o',BUILD/'test_battle_stats.exe'])
run_test([HERE/'test_special_stats.c',HERE/'special_stats.c','-o',BUILD/'test_special_stats.exe'])
run_test([HERE/'test_timeline.c',*stats_sources,'-lbcrypt','-o',BUILD/'test_timeline.exe'])
run_test([HERE/'test_battle_report.c',*round_sources,'-o',BUILD/'test_battle_report.exe'])
run_test([HERE/'test_search_bridge.c',HERE/'search_model.c',HERE/'battle_probe.c',*round_sources,*sources,'-o',BUILD/'test_search_bridge.exe'])
run_test([HERE/'test_battle_probe.c',*round_sources,*sources,'-o',BUILD/'test_battle_probe.exe'])
run_test([HERE/'test_battle_skills.c',*round_sources,*sources,'-o',BUILD/'test_battle_skills.exe'])
run_test([HERE/'test_battle_details.c',*round_sources,*sources,'-o',BUILD/'test_battle_details.exe'])
run_test([HERE/'test_battle_save.c',*round_sources,*sources,'-o',BUILD/'test_battle_save.exe'])
run_test([HERE/'test_affix_bridge.c',*round_sources,*sources,'-o',BUILD/'test_affix_bridge.exe'])
run_test([HERE/'test_battle_special.c',*round_sources,*sources,'-o',BUILD/'test_battle_special.exe'])
subprocess.run([str(ZIG),'rc','/fo',str(BUILD/'officer-controls.res'),str(HERE/'officer-controls.rc')],check=True,cwd=HERE)
officer_sources=[HERE/'pinyin.c',HERE/'officer_model.c',HERE/'officer_ui.c',HERE/'officer_history.c',HERE/'personality_ui.c',HERE/'personality_model.c',BUILD/'officer-controls.res']
detail_sources=[HERE/'detail_model.c',HERE/'detail_ui.c']
army_sources=[HERE/'army_model.c',HERE/'army_observer.c',HERE/'army_ui.c',HERE/'army_buff.c']
map_sources=[HERE/'map_effects_model.c',HERE/'map_effects_ui.c',HERE/'map_render.c']
run_test([HERE/'test_map_effects.c',*map_sources,*sources,'-ld3d11','-ldxgi','-luser32','-lgdi32','-o',BUILD/'test_map_effects.exe'])
run_test([HERE/'test_map_render.c',map_sources[0],*sources,'-ld3d11','-ldxgi','-luser32','-o',BUILD/'test_map_render.exe'])
run([HERE/'map_effects_probe.c',map_sources[0],'-municode','-lpsapi','-o',BUILD/'map_effects_probe.exe'])
run_test([HERE/'test_army_buff.c',*sources,'-o',BUILD/'test_army_buff.exe'])
run_test([HERE/'test_affix.c',*stats_sources,*sources,'-lbcrypt','-o',BUILD/'test_affix.exe'])
run_test([HERE/'test_army.c',HERE/'army_model.c',HERE/'detail_model.c',*timeline_sources[:1],*sources,'-luser32','-lgdi32','-o',BUILD/'test_army.exe'])
run([HERE/'army_probe.c',HERE/'army_model.c',*timeline_sources[:1],'-municode','-lpsapi','-o',BUILD/'army_probe.exe'])
run_test([HERE/'test_native_detail.c',HERE/'detail_model.c','-o',BUILD/'test_native_detail.exe'])
run_test([HERE/'test_detail_ui.c',HERE/'detail_model.c',*stats_sources,'-lbcrypt','-luser32','-lgdi32','-o',BUILD/'test_detail_ui.exe'])
run([HERE/'detail_probe.c',HERE/'detail_model.c','-municode','-lpsapi','-o',BUILD/'detail_probe.exe'])
run_test([HERE/'test_officers.c',*officer_sources,HERE/'special_stats.c',*timeline_sources,'-luser32','-lgdi32','-lcomctl32','-o',BUILD/'test_officers.exe'])
run([HERE/'troop_stub.c',HERE/'officer_probe.c',*officer_sources,HERE/'special_stats.c',*timeline_sources,'-municode','-lpsapi','-luser32','-lgdi32','-lcomctl32','-o',BUILD/'officer_probe.exe'])
portrait_sources=[HERE/'portrait.c',HERE/'vendor/miniz/miniz_tinfl.c']
report_sources=[HERE/'report_ui.c',HERE/'report_model.c',*portrait_sources]
run_test([HERE/'test_report_ui.c',*report_sources[1:],'-municode','-luser32','-lgdi32','-o',BUILD/'test_report_ui.exe'])
run_test([HERE/'test_portrait.c',*portrait_sources,'-municode','-luser32','-lgdi32','-o',BUILD/'test_portrait.exe'])
plugin_sources=[HERE/'plugin.c',HERE/'personality_edit.c',*troop_sources,HERE/'troop_runtime.c',HERE/'troop_ui.c',HERE/'affix_names.c',HERE/'auto_search.c',HERE/'battle_probe.c',*round_sources[:2],*report_sources,HERE/'rule.c',HERE/'interaction.c',HERE/'toast.c',*officer_sources,*detail_sources,*army_sources,*map_sources,*manager_sources,*sources,HERE/'exports.def','-ld3d11','-ldxgi','-lbcrypt','-luser32','-lgdi32','-lshell32','-lcomctl32']
run(['-shared',*plugin_sources,'-o',BUILD/'dinput8.dll'])
run(['-shared','-DS14_SELFTEST',*plugin_sources,'-o',BUILD/'plugin_test.dll'])
payload=(BUILD/'dinput8.dll').read_bytes()
(BUILD/'payload.h').write_text('static const unsigned char s14_payload[]={\n'+
    '\n'.join(','.join(f'0x{x:02x}' for x in payload[i:i+32])+',' for i in range(0,len(payload),32))+
    '\n};\nstatic const char s14_payload_hash[]="'+hashlib.sha256(payload).hexdigest()+'";\n',encoding='ascii')
run(['-DS14_INSTALLER','-I',BUILD,HERE/'manager_main.c',*manager_sources,*update_sources,'-municode','-Wl,--subsystem,windows','-lwinhttp','-lbcrypt','-luser32','-lgdi32','-lshell32','-lole32','-luuid','-o',BUILD/'SAN14ModManager.exe'])
run_test([HERE/'test_manager.c',*manager_sources,'-lbcrypt','-luser32','-lgdi32','-o',BUILD/'test_manager.exe'])
run(['-shared','-DS14_INSTALLER','-I',BUILD,HERE/'test_package_exports.c',HERE/'troop_store.c',HERE/'package.c',HERE/'cleanup.c',HERE/'features.c',*stats_sources,'-lbcrypt','-o',BUILD/'package_test.dll'])
shutil.copyfile(MINHOOK/'LICENSE.txt',BUILD/'MinHook-LICENSE.txt')
shutil.copyfile(HERE/'vendor/miniz/LICENSE',BUILD/'Miniz-MIT-LICENSE.txt')
commit=json.loads((MINHOOK/'UPSTREAM.json').read_text(encoding='utf-8'))['commit']
(BUILD/'build-provenance.json').write_text(json.dumps({'compiler':'Zig '+version,'minhook_commit':commit,
    'portrait_decompressor':json.loads((HERE/'vendor/miniz/UPSTREAM.json').read_text(encoding='utf-8-sig')),
    'pinyin_data':json.loads((HERE/'vendor/pinyin/UPSTREAM.json').read_text(encoding='utf-8')),
    'target':'x86_64-windows-gnu','optimization':'O2','native_tests_assertions_enabled':True},indent=2),encoding='utf-8')
print(str(BUILD))
