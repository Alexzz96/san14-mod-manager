"""Package verified mod-only installer; source ZIPs are not installers."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile

HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=HERE/'build')
parser.add_argument('--dist-dir', type=Path, default=HERE/'dist')
args=parser.parse_args()
BUILD=args.build_dir.resolve()
DIST=args.dist_dir.resolve(); DIST.mkdir(parents=True,exist_ok=True)
version=(HERE.parent/'VERSION').read_text(encoding='utf-8').strip()
verification=json.loads((BUILD/'verification.json').read_text(encoding='utf-8'))
if verification.get('status') != 'passed':
    raise SystemExit('Native verification must pass before packaging')
payloads={}
for name,key in [('SAN14ModManager.exe','manager_exe_sha256'),('dinput8.dll','production_dll_sha256')]:
    data=(BUILD/name).read_bytes()
    if hashlib.sha256(data).hexdigest() != verification[key]:
        raise SystemExit('Verified artifact changed: '+name)
    payloads[name]=data
license_data=(HERE/'vendor/minhook/LICENSE.txt').read_bytes()
package=DIST/f'SAN14ModManager-{version}-windows-x64.zip'
guide=(HERE.parent/'docs/USER_GUIDE.md').read_bytes()
config=b'[Rule]\nMode=0\n[Manager]\nEnabled=1\n[Features]\nWallClusterLimit=0\nLimitHint=1\nDiagnostics=0\nAutoSearch=0\n[AutoSearch]\nExecutors=0\nReturnDays=0\nPriority=0\n'
config+=b'[Views]\nOfficers=1\nNativeOfficerStats=1\n'
config+=b'[Observation]\nBattleEvents=0\n'
manifest={'version':version,'release_status':'prerelease','platform':'windows-x64',
          'target_executable':'SAN14PK_SC.exe','game_version_check':False,
          'game_sha256_check':False,'runtime_hook_validation':True,
          'manager_exe_sha256':verification['manager_exe_sha256'],
          'embedded_dll_sha256':verification['production_dll_sha256'],
          'manager_in_game_acceptance':'pending'}
manifest.update({'ui_theme':'warm-paper-terracotta','complete_uninstall':True,
                 'uninstall_from_external_manager':True,'unknown_files_preserved':True})
manifest.update({'auto_search':True,'auto_search_start_toast_ms':2000,
                 'auto_search_result_types':['person','item','tactic_book','money'],
                 'auto_search_native_settings':True,'auto_search_in_game_acceptance':'pending',
                 'publication':'github_release'})
manifest.update({'search_actor_location':True,'search_failure_details':True,
                 'search_result_popup_scroll':True,'search_result_popup_dismiss':'click',
                 'search_result_popup_auto_timeout':False})
assert verification['officers']['status']=='passed'
manifest.update({'officer_view':True,'officer_view_hotkey':'F','officer_view_read_only':True,
                 'officer_view_refresh_ms':None,'officer_view_refresh_policy':'on_open',
                 'officer_view_search':['name','courtesy','personality'],
                 'officer_view_ability_basis':'base','officer_view_career_provider_version':1,
                 'officer_view_in_game_acceptance':'pending','battle_observer_included':True,
                 'battle_observer_schema_version':7,'battle_observer_default_enabled':False,
                 'battle_observer_in_game_acceptance':'pending','battle_career_totals_verified':False})
manifest.update({'end_of_turn_report':True,'end_of_turn_report_tabs':['exploration','battle'],
                 'battle_report_scope':'player_force','battle_report_in_game_acceptance':'pending',
                 'battle_report_reopen':'F10 exploration and turn report',
                 'battle_report_permanent_career_totals':False})
assert verification['battle_observation']['persistent_stats']['status']=='passed'
manifest.update({'officer_battle_stats':True,'officer_battle_stats_sortable':True,
                 'officer_battle_stats_fields':['enemy_troop_loss_including_wounded','units_routed','own_units_defeated','own_troop_loss','officers_injured'],
                 'officer_battle_stats_scope':'all_supported_factions_main_commander',
                 'officer_battle_stats_persistence':'native_save_content_sha256_checkpoint',
                 'officer_battle_stats_history':'from_collection_start',
                 'officer_battle_stats_in_game_acceptance':'pending'})
manifest.update({'officer_view_dismiss':['close_button','titlebar_close','Escape','F_from_game'],
                 'officer_view_auto_hide_on_focus_loss':False,'officer_view_topmost':False})
manifest.update({'officer_view_search_forms':['Chinese','pinyin','pinyin_initials'],
                 'officer_view_polyphonic_search':True,'officer_view_theme':'warm-paper-terracotta',
                 'officer_view_character_fields':['ambition','bond','loyalty'],
                 'officer_view_inner_grade_range':[1,5],'officer_view_schema_version':2})
assert verification['native_officer_detail']['status']=='passed'
manifest.update({'native_officer_detail_stats':True,'native_officer_detail_read_only':True,
                 'native_officer_detail_extra_game_hooks':0,'native_officer_detail_position':'above_native_detail',
                 'native_officer_detail_click_through':True,'native_officer_detail_follows_selection':True,
                 'native_officer_detail_in_game_acceptance':'pending'})
files={'SAN14ModManager.exe':payloads['SAN14ModManager.exe'],
       '使用说明.md':guide,'MinHook-LICENSE.txt':license_data,'默认配置示例.ini':config,
       'Pinyin-MIT-LICENSE.txt':(HERE/'vendor/pinyin/LICENSE.txt').read_bytes(),
       'release.json':(json.dumps(manifest,indent=2)+'\n').encode()}
assert verification['github_update']['update_model']=='passed'
manifest.update({'wall_cluster_limit_default_enabled':False,'existing_settings_preserved':True,
                 'github_update':True,'github_update_repository':'Alexzz96/san14-mod-manager',
                 'github_update_trigger':'manual_check','github_update_prereleases':True,
                 'github_update_integrity':'GitHub asset SHA256 digest',
                 'github_update_requires_game_exit':True,
                 'native_officer_detail_prior_build_accepted':'0.7.1',
                 'officer_battle_stats_prior_build_accepted':'0.7.0',
                 'save_restart_and_rollback_in_game_acceptance':'pending'})
files['release.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['SHA256.txt']=''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in files.items()).encode('utf-8')
with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED) as output:
    for name,data in files.items():
        entry=zipfile.ZipInfo(name,(2026,10,7,0,0,0)); entry.compress_type=zipfile.ZIP_DEFLATED
        output.writestr(entry,data)
digest=hashlib.sha256(package.read_bytes()).hexdigest()
(DIST/'SHA256SUMS.txt').write_text(digest+'  '+package.name+'\n',encoding='ascii')
installer=DIST/f'SAN14ModManager-{version}-windows-x64.exe'
installer.write_bytes(payloads['SAN14ModManager.exe'])
with (DIST/'SHA256SUMS.txt').open('a',encoding='ascii') as output:
    output.write(hashlib.sha256(installer.read_bytes()).hexdigest()+'  '+installer.name+'\n')
print(json.dumps({'package':str(package),'sha256':digest,'entries':list(files)},ensure_ascii=True))
