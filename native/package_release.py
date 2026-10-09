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
parser.add_argument('--local', action='store_true', help='Package local observations without publishing')
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
config=b'[Rule]\nMode=0\n[Manager]\nEnabled=1\n[Features]\nWallClusterLimit=0\nLimitHint=1\nDiagnostics=0\nAutoSearch=1\n[AutoSearch]\nExecutors=1\nReturnDays=0\nPriority=0\n'
config+=b'[Views]\nEnabled=1\nOfficers=1\nNativeOfficerStats=1\n'
config+=b'[Observation]\nBattleEvents=1\n'
manifest={'version':version,'release_status':'prerelease','platform':'windows-x64',
          'target_executable':'SAN14PK_SC.exe','game_version_check':False,
          'game_sha256_check':False,'runtime_hook_validation':True,
          'manager_exe_sha256':verification['manager_exe_sha256'],
          'embedded_dll_sha256':verification['production_dll_sha256'],
          'manager_in_game_acceptance':'pending'}
manifest.update({'ui_theme':'warm-paper-terracotta','complete_uninstall':True,
                 'uninstall_from_external_manager':True,'unknown_files_preserved':True})
assert verification['manager']['manager_tabs']==['mods','settings']
assert all(verification['manager'][key] for key in ('expandable_mod_list','stable_mod_sort',
    'child_preferences_preserved','views_parent_runtime_gate','scroll_and_keyboard_reveal'))
manifest.update({'manager_tabs':['mods','settings'],'manager_mod_list':True,
                 'manager_mod_count':5,'manager_mod_expandable_children':True,
                 'manager_mod_sort':['default','name','enabled_first'],
                 'manager_child_preferences_preserved':True,'manager_views_parent_default_enabled':True,
                 'manager_expanded_group_border':True,'manager_parent_child_divider':True,
                 'manager_install_and_update_tab':'settings',
                 'manager_report_entry':'mods / auto_search / expand / previous_turn_report'})
manifest.update({'auto_search':True,'auto_search_start_toast_ms':2000,
                 'auto_search_result_types':['person','item','tactic_book','money'],
                 'auto_search_native_settings':True,'auto_search_in_game_acceptance':'pending',
                 'publication':'local_only' if args.local else 'github_release'})
manifest.update({'search_actor_location':True,'search_failure_details':True,
                 'search_result_popup_scroll':True,'search_result_popup_dismiss':'click',
                 'search_result_popup_auto_timeout':False})
assert verification['officers']['status']=='passed'
manifest.update({'officer_view':True,'officer_view_hotkey':'F','officer_view_read_only':True,
                 'officer_view_refresh_ms':None,'officer_view_refresh_policy':'on_open',
                 'officer_view_search':['name','courtesy','personality'],
                 'officer_view_ability_basis':'base','officer_view_career_provider_version':1,
                 'officer_view_in_game_acceptance':'pending','battle_observer_included':True,
                 'battle_observer_schema_version':7,'battle_observer_default_enabled':True,
                 'battle_observer_in_game_acceptance':'pending','battle_career_totals_verified':False})
assert verification['battle_observation']['turn_report_ui']['status']=='passed'
assert verification['battle_observation']['portraits']['status']=='passed'
manifest.update({'end_of_turn_report':True,'end_of_turn_report_tabs':[],
                 'end_of_turn_report_view':'native_portrait_cards',
                 'end_of_turn_report_exploration_totals':['money','people','items','tactic_books'],
                 'end_of_turn_report_exploration_pinned':True,
                 'end_of_turn_report_details':'click_total_or_officer_card',
                 'end_of_turn_report_close':'close_button_or_Escape',
                 'end_of_turn_report_portraits':'asynchronous_local_game_resources',
                 'bundled_game_artwork':False,
                 'battle_report_scope':'player_force','battle_report_in_game_acceptance':'pending',
                 'battle_report_reopen':'F10 mods / auto search / expand / previous turn report',
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
assert all(verification['officers'][key] for key in ('two_tabs','tab_message_pump',
    'pinned_identity_columns','synchronized_rows_and_scroll','unknown_counts_display_zero','archive_panel_removed',
    'no_horizontal_scroll','four_character_battle_headers','kda_exact_sort_and_zero_denominator','officer_timeline_menu_and_window'))
manifest.update({'officer_view_tabs':['battle_stats','original_data'],
                 'officer_view_tab_hotkey':'Tab','officer_view_default_tab':'battle_stats',
                 'officer_view_fixed_columns':['name','force','location'],
                 'officer_view_sync_rows_and_scroll':True,'officer_view_archive_panel':False,
                 'officer_view_unknown_counts_display':0,'officer_view_count_sort_basis':'displayed_numeric_value'})
assert verification['battle_observation']['timeline']['status']=='passed'
assert verification['battle_observation']['timeline']['cross_process_restore']
assert verification['battle_observation']['test_battle_special']['capture_attribution_guard']
assert verification['cleanup']['timeline_checkpoint_cleanup']
manifest.update({'officer_view_horizontal_scroll':False,'officer_view_battle_header_characters':4,
                 'officer_view_battle_columns':['武将姓名','效命势力','驻军所在','斩敌总数','士卒折损','杀损比值',
                     '破敌次数','所部覆灭','斩杀敌将','击伤敌将','单挑制胜','单挑败北','擒获次数','被俘次数'],
                 'officer_view_hidden_counts':['duels','unique_captives'],
                 'officer_kda_formula':'enemy_troop_loss_including_wounded / own_troop_loss',
                 'officer_kda_zero_denominator':{'positive_enemy_loss':'infinity','zero_enemy_loss':'0.00'},
                 'officer_timeline':True,'officer_timeline_entry':'officer context menu or selected row button',
                 'officer_timeline_sidecar':'.s14timeline','officer_timeline_schema_version':1,
                 'officer_timeline_global_capacity':16384,'officer_timeline_fields':['date','city','area','opponent','result'],
                 'officer_timeline_persistence':'native_save_content_sha256_checkpoint',
                 'officer_timeline_refresh_policy':'F_on_open','officer_timeline_legacy_place_invented':False,
                 'officer_timeline_in_game_acceptance':'pending',
                 'capture_attribution':'matched_native_report_and_status_on_supported_capture_path',
                 'capture_in_game_acceptance':'pending'})
assert verification['native_officer_detail']['status']=='passed'
manifest.update({'native_officer_detail_stats':True,'native_officer_detail_read_only':True,
                 'native_officer_detail_extra_game_hooks':0,'native_officer_detail_position':'above_native_detail',
                 'native_officer_detail_click_through':True,'native_officer_detail_follows_selection':True,
                 'native_officer_detail_in_game_acceptance':'pending'})
files={'SAN14ModManager.exe':payloads['SAN14ModManager.exe'],
       '使用说明.md':guide,'MinHook-LICENSE.txt':license_data,'默认配置示例.ini':config,
       'Pinyin-MIT-LICENSE.txt':(HERE/'vendor/pinyin/LICENSE.txt').read_bytes(),
       'Miniz-MIT-LICENSE.txt':(HERE/'vendor/miniz/LICENSE').read_bytes(),
       'release.json':(json.dumps(manifest,indent=2)+'\n').encode()}
assert verification['github_update']['update_model']=='passed'
manifest.update({'wall_cluster_limit_default_enabled':False,'existing_settings_preserved':True,
                 'auto_search_default_enabled':True,'auto_search_default_executors':1,
                 'github_update':True,'github_update_repository':'Alexzz96/san14-mod-manager',
                 'github_update_trigger':'manual_check','github_update_prereleases':True,
                 'github_update_integrity':'GitHub asset SHA256 digest',
                 'github_update_requires_game_exit':True,
                 'native_officer_detail_prior_build_accepted':'0.7.1',
                 'officer_battle_stats_prior_build_accepted':'0.7.0',
                 'save_restart_and_rollback_in_game_acceptance':'pending'})
files['release.json']=(json.dumps(manifest,indent=2)+'\n').encode()
assert verification['battle_observation']['special_stats']['status']=='passed'
manifest.update({'special_events_observation':True,'special_events':['duel_settlement_candidate','capture_settlement_candidate'],
                 'special_events_semantics':'ordinary_duel_and_report_matched_capture_verified_other_paths_pending','ordinary_duel_cumulative':True,'ordinary_duel_history_backfill':False,'special_events_candidates_credited':False,
                 'special_events_sidecar':'.s14special','special_events_history_capacity':4096,
                 'special_events_officer_columns':['wins','losses','captures','captured']})
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
