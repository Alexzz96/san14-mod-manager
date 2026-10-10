#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "features.h"
#include "manager_ui.h"
#include "package.h"
#define REQUIRE(expr) do { if (!(expr)) { fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#expr); exit(1); } } while (0)
static int callback_count;
static int clean_count;
static int check_count,download_count;
static void changed(S14ManagerUI *ui,int action,void *context) { (void)ui; (void)context; if (action==S14_ACTION_CONFIG) callback_count++; if (action==S14_ACTION_CLEAN) clean_count++;if(action==S14_ACTION_CHECK_UPDATE)check_count++;if(action==S14_ACTION_DOWNLOAD_UPDATE)download_count++; }
static void preview(S14ManagerUI *ui,const char *name) {
    int width=MulDiv(S14_MANAGER_WIDTH,ui->scale,96),height=MulDiv(S14_MANAGER_HEIGHT,ui->scale,96);
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(info.bmiHeader); info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL); REQUIRE(dc!=NULL); void *pixels=NULL;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0); REQUIRE(bitmap && pixels);
    HGDIOBJ old=SelectObject(dc,bitmap); s14_manager_paint(ui,dc,width,height); GdiFlush();
    FILE *file=fopen(name,"wb"); REQUIRE(file!=NULL);
    BITMAPFILEHEADER header={0}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader); header.bfSize=header.bfOffBits+width*height*4;
    REQUIRE(fwrite(&header,sizeof(header),1,file)==1); REQUIRE(fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1); REQUIRE(fwrite(pixels,width*height*4,1,file)==1);
    fclose(file); SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc);
}
int main(void) {
    wchar_t temp[MAX_PATH],root[MAX_PATH],ini[MAX_PATH]; REQUIRE(GetTempPathW(MAX_PATH,temp));
    swprintf(root,MAX_PATH,L"%lsS14-manager-test-%lu",temp,(unsigned long)GetCurrentProcessId()); REQUIRE(CreateDirectoryW(root,NULL));
    REQUIRE(s14_join(ini,root,L"SAN14BuildLimit.ini"));
    REQUIRE(s14_config_read(ini)==(S14_MASTER|S14_LIMIT_HINT|S14_AUTO_SEARCH|S14_PLUGIN_TROOPS));
    REQUIRE(s14_effective_flags(s14_config_read(ini))==(S14_MASTER|S14_AUTO_SEARCH|S14_PLUGIN_TROOPS));
    REQUIRE(s14_search_settings_read(ini)==1 && s14_battle_setting_read(ini));
    REQUIRE(WritePrivateProfileStringW(L"AutoSearch",L"Executors",L"99",ini));
    REQUIRE(s14_search_settings_read(ini)==1);
    REQUIRE(WritePrivateProfileStringW(L"AutoSearch",L"Executors",NULL,ini));
    REQUIRE(!(s14_config_read(ini)&S14_CAO_REN_BUFF));
    REQUIRE(s14_config_read(ini)&S14_PLUGIN_TROOPS);
    REQUIRE(s14_config_set(ini,S14_PLUGIN_TROOPS,0) && !(s14_config_read(ini)&S14_PLUGIN_TROOPS));
    REQUIRE(s14_config_set(ini,S14_PLUGIN_TROOPS,1));
    REQUIRE(s14_config_set(ini,S14_CAO_REN_BUFF,1) && (s14_effective_flags(s14_config_read(ini))&S14_CAO_REN_BUFF));
    REQUIRE(s14_config_set(ini,S14_MASTER,0) && !(s14_effective_flags(s14_config_read(ini))&S14_CAO_REN_BUFF));
    REQUIRE(s14_config_set(ini,S14_MASTER,1));REQUIRE(s14_config_set(ini,S14_CAO_REN_BUFF,0));
    REQUIRE(s14_config_set(ini,S14_AUTO_SEARCH,0));
    REQUIRE(!(s14_config_read(ini)&S14_AUTO_SEARCH));
    REQUIRE(s14_search_setting_set(ini,0,0) && s14_search_settings_read(ini)==0);
    REQUIRE(WritePrivateProfileStringW(L"Observation",L"BattleEvents",L"0",ini));
    REQUIRE(!s14_battle_setting_read(ini));
    REQUIRE(s14_config_set(ini,S14_AUTO_SEARCH,1));
    REQUIRE(s14_config_read(ini)&S14_AUTO_SEARCH);
    REQUIRE(s14_config_set(ini,S14_AUTO_SEARCH,0));
    REQUIRE(WritePrivateProfileStringW(L"Observation",L"BattleEvents",NULL,ini));
    REQUIRE(WritePrivateProfileStringW(L"Manager",L"Enabled",L"1",ini));
    REQUIRE(!(s14_config_read(ini)&S14_WALL_LIMIT));
    REQUIRE(WritePrivateProfileStringW(L"Rule",L"Mode",L"2",ini));
    REQUIRE(s14_config_read(ini)&S14_WALL_LIMIT);
    REQUIRE(s14_config_set(ini,S14_WALL_LIMIT,0));
    REQUIRE(!(s14_config_read(ini)&S14_WALL_LIMIT));
    REQUIRE(s14_config_set(ini,S14_WALL_LIMIT,1)); REQUIRE(s14_config_set(ini,S14_MASTER,1));
    REQUIRE(WritePrivateProfileStringW(L"Future",L"UnknownFeature",L"keep-me",ini));
    REQUIRE(s14_config_set(ini,S14_DIAGNOSTICS,1));
    REQUIRE(s14_config_set(ini,S14_MASTER,0)); REQUIRE(!s14_effective_flags(s14_config_read(ini)));
    REQUIRE(s14_config_set(ini,S14_MASTER,1)); REQUIRE(s14_runtime_mode(s14_config_read(ini))==2);
    REQUIRE(s14_config_set(ini,S14_WALL_LIMIT,0)); REQUIRE(!(s14_effective_flags(s14_config_read(ini))&S14_LIMIT_HINT));
    REQUIRE(s14_config_read(ini)&S14_LIMIT_HINT); REQUIRE(s14_runtime_mode(s14_config_read(ini))==1);
    REQUIRE(s14_config_set(ini,S14_DIAGNOSTICS,0)); REQUIRE(s14_runtime_mode(s14_config_read(ini))==0);
    REQUIRE(s14_config_set(ini,S14_WALL_LIMIT,1)); REQUIRE(s14_runtime_mode(s14_config_read(ini))==2);
    wchar_t value[32]; GetPrivateProfileStringW(L"Future",L"UnknownFeature",L"",value,32,ini); REQUIRE(!wcscmp(value,L"keep-me"));
    REQUIRE(!s14_config_set(ini,0x10000,1));
    REQUIRE(!s14_search_settings_read(ini));
    REQUIRE(s14_search_setting_set(ini,0,3) && s14_search_setting_set(ini,1,2) && s14_search_setting_set(ini,2,1));
    REQUIRE(s14_search_settings_read(ini)==0x123);
    REQUIRE(!s14_search_setting_set(ini,0,4) && !s14_search_setting_set(ini,2,2));
    for (unsigned int flags=0;flags<=S14_KNOWN_FLAGS;flags++) {
        unsigned int effective=s14_effective_flags(flags);
        REQUIRE(!(effective&S14_LIMIT_HINT) || (effective&S14_WALL_LIMIT));
        REQUIRE(!(flags&S14_MASTER)?effective==0:1);
        for (int i=0;i<1000;i++) REQUIRE(s14_runtime_mode(flags)==s14_runtime_mode(effective));
    }
    HWND foreground=GetForegroundWindow(); S14ManagerUI ui={0}; ui.action=changed;
    wcscpy(ui.ini,ini); wcscpy(ui.root,root); wcscpy(ui.status,L"游戏已接入 · 开关从下一次检查起生效");
    ui.game_found=ui.installed=ui.attached=1;
    REQUIRE(s14_manager_create(&ui,GetModuleHandleW(NULL),NULL,1));
    unsigned int previous_requested=ui.requested,previous_effective=ui.effective;
    ui.requested=S14_MASTER|S14_AUTO_SEARCH;ui.effective=S14_MASTER;
    ui.search_state=S14_SEARCH_WAITING;REQUIRE(!wcscmp(s14_manager_mod_status(&ui,S14_MOD_SEARCH),L"等待载入"));
    ui.search_state=S14_SEARCH_CONTEXT_PAUSED;REQUIRE(!wcscmp(s14_manager_mod_status(&ui,S14_MOD_SEARCH),L"等待匹配"));
    ui.search_state=S14_SEARCH_STOPPED;REQUIRE(!wcscmp(s14_manager_mod_status(&ui,S14_MOD_SEARCH),L"异常停用"));
    ui.search_state=S14_SEARCH_UNAVAILABLE;REQUIRE(!wcscmp(s14_manager_mod_status(&ui,S14_MOD_SEARCH),L"入口不兼容"));
    ui.search_state=S14_SEARCH_MATCHED;ui.effective|=S14_AUTO_SEARCH;REQUIRE(!wcscmp(s14_manager_mod_status(&ui,S14_MOD_SEARCH),L"已开启"));
    ui.requested=S14_AUTO_SEARCH;REQUIRE(!wcscmp(s14_manager_mod_status(&ui,S14_MOD_SEARCH),L"总开关关闭"));
    ui.requested=S14_MASTER;REQUIRE(!wcscmp(s14_manager_mod_status(&ui,S14_MOD_SEARCH),L"已关闭"));
    ui.requested=previous_requested;ui.effective=previous_effective;
    wchar_t runtime_folder[MAX_PATH],runtime_file[MAX_PATH];
    REQUIRE(s14_join(runtime_folder,root,L"SAN14ModManager") && CreateDirectoryW(runtime_folder,NULL));
    s14_publish_search_runtime(root,S14_SEARCH_CONTEXT_PAUSED,3,4,L"等待玩家势力重新匹配");
    int read_state=-1,read_force=-1,read_group=-1;wchar_t read_detail[192];
    s14_read_search_runtime(root,&read_state,&read_force,&read_group,read_detail);
    REQUIRE(read_state==S14_SEARCH_CONTEXT_PAUSED && read_force==3 && read_group==4 && !wcscmp(read_detail,L"等待玩家势力重新匹配"));
    REQUIRE(s14_join(runtime_file,runtime_folder,L"runtime.ini") && DeleteFileW(runtime_file) && RemoveDirectoryW(runtime_folder));
    REQUIRE(GetForegroundWindow()==foreground && !IsWindowVisible(ui.window));
    REQUIRE(s14_views_setting_read(ini) && ui.views_enabled);
    REQUIRE(s14_manager_hit(&ui,(POINT){776,250})==10);
    REQUIRE(s14_manager_hit(&ui,(POINT){60,250})==80);
    REQUIRE(s14_manager_hit(&ui,(POINT){320,120})==0); // Only Mod and settings tabs.
    REQUIRE(!s14_manager_activate(&ui,102));
    REQUIRE(s14_manager_activate(&ui,10));REQUIRE(!(ui.requested&S14_WALL_LIMIT));
    REQUIRE(s14_manager_activate(&ui,10));REQUIRE(ui.effective&S14_WALL_LIMIT);
    REQUIRE(callback_count==2);
    REQUIRE(s14_manager_activate(&ui,80));REQUIRE(ui.expanded==1);
    REQUIRE(s14_manager_hit(&ui,(POINT){776,315})==11);
    REQUIRE(s14_manager_activate(&ui,11));REQUIRE(!(ui.requested&S14_LIMIT_HINT));
    REQUIRE(s14_manager_activate(&ui,11));REQUIRE(ui.requested&S14_LIMIT_HINT);
    REQUIRE(s14_manager_activate(&ui,10));REQUIRE(!(ui.effective&S14_LIMIT_HINT) && (ui.requested&S14_LIMIT_HINT));
    REQUIRE(s14_manager_activate(&ui,10));REQUIRE(ui.effective&S14_LIMIT_HINT);
    REQUIRE(s14_manager_activate(&ui,80));REQUIRE(!ui.expanded);
    REQUIRE(s14_manager_activate(&ui,1));REQUIRE(ui.effective==0);
    REQUIRE(s14_manager_activate(&ui,1));REQUIRE(ui.effective&S14_LIMIT_HINT);
    REQUIRE(ui.scroll==0);preview(&ui,"manager-preview.bmp");
    REQUIRE(s14_manager_activate(&ui,83));int y=s14_manager_mod_top(&ui,S14_MOD_VIEWS);
    REQUIRE(s14_manager_hit(&ui,(POINT){776,y+100})==40);
    REQUIRE(s14_manager_activate(&ui,40) && !ui.officers_enabled);REQUIRE(GetPrivateProfileIntW(L"Views",L"Officers",1,ini)==0);
    REQUIRE(s14_manager_activate(&ui,40) && ui.officers_enabled);REQUIRE(s14_manager_view_enabled(&ui,0));
    REQUIRE(s14_manager_activate(&ui,43) && !ui.native_stats_enabled);REQUIRE(!s14_manager_view_enabled(&ui,1));
    REQUIRE(ui.native_army_enabled && s14_manager_view_enabled(&ui,2));
    REQUIRE(s14_manager_activate(&ui,46) && !ui.native_army_enabled && !s14_manager_view_enabled(&ui,2));
    REQUIRE(GetPrivateProfileIntW(L"Views",L"NativeArmyValues",1,ini)==0);
    REQUIRE(s14_manager_activate(&ui,44) && !ui.views_enabled);REQUIRE(!s14_views_setting_read(ini));
    REQUIRE(!s14_manager_view_enabled(&ui,0) && !s14_manager_view_enabled(&ui,1));
    REQUIRE(ui.officers_enabled && !ui.native_stats_enabled); // Parent off preserves individual preferences.
    REQUIRE(s14_manager_activate(&ui,43) && ui.native_stats_enabled);REQUIRE(!s14_manager_view_enabled(&ui,1));
    REQUIRE(s14_manager_activate(&ui,44) && ui.views_enabled);REQUIRE(s14_manager_view_enabled(&ui,0) && s14_manager_view_enabled(&ui,1));
    REQUIRE(!ui.native_army_enabled && !s14_manager_view_enabled(&ui,2));
    REQUIRE(s14_manager_activate(&ui,46) && ui.native_army_enabled && s14_manager_view_enabled(&ui,2));
    REQUIRE(GetPrivateProfileIntW(L"Views",L"Officers",0,ini)==1 && GetPrivateProfileIntW(L"Views",L"NativeOfficerStats",0,ini)==1);
    REQUIRE(GetPrivateProfileIntW(L"Views",L"Enabled",0,ini)==1);
    REQUIRE(s14_manager_activate(&ui,14) && s14_manager_mod_enabled(&ui,S14_MOD_CAO_REN_BUFF));
    REQUIRE(ui.visual_settings==15 && s14_visual_settings_read(ini)==15);
    REQUIRE(s14_manager_activate(&ui,47) && ui.visual_settings==14);
    REQUIRE(s14_manager_activate(&ui,48) && ui.visual_settings==12);
    REQUIRE(s14_manager_activate(&ui,14) && !(ui.effective&S14_CAO_REN_BUFF) && ui.visual_settings==12);
    REQUIRE(s14_manager_activate(&ui,47) && ui.visual_settings==13 && !(ui.effective&S14_CAO_REN_BUFF));
    REQUIRE(s14_manager_activate(&ui,14) && (ui.effective&S14_CAO_REN_BUFF) && ui.visual_settings==13);
    REQUIRE(s14_manager_activate(&ui,48) && ui.visual_settings==15);
    REQUIRE(s14_manager_activate(&ui,84));ui.scroll=9999;s14_manager_refresh(&ui);
    y=s14_manager_mod_top(&ui,S14_MOD_CAO_REN_BUFF);
    REQUIRE(s14_manager_hit(&ui,(POINT){776,y+100})==47 && s14_manager_hit(&ui,(POINT){776,y+170})==48);
    preview(&ui,"manager-map-effects.bmp");
    ui.focus=14;SendMessageW(ui.window,WM_KEYDOWN,VK_TAB,0);REQUIRE(ui.focus==47);
    SendMessageW(ui.window,WM_KEYDOWN,VK_TAB,0);REQUIRE(ui.focus==48);
    REQUIRE(s14_manager_activate(&ui,84));ui.scroll=0;s14_manager_refresh(&ui);
    REQUIRE(s14_manager_activate(&ui,14) && !s14_manager_mod_enabled(&ui,S14_MOD_CAO_REN_BUFF));
    REQUIRE(s14_manager_activate(&ui,41) && !ui.battle_enabled && !s14_battle_setting_read(ini));
    REQUIRE(s14_manager_activate(&ui,41) && ui.battle_enabled && s14_battle_setting_read(ini));
    preview(&ui,"manager-mod-views.bmp");
    REQUIRE(s14_manager_activate(&ui,61) && ui.mod_sort==1);
    for(int i=1;i<S14_MOD_COUNT;i++)REQUIRE(wcscmp(s14_mods[s14_manager_mod_index(&ui,i-1)].name,s14_mods[s14_manager_mod_index(&ui,i)].name)<=0);
    REQUIRE(ui.expanded&(1u<<S14_MOD_VIEWS));
    REQUIRE(s14_manager_activate(&ui,61) && ui.mod_sort==2);
    for(int i=1;i<S14_MOD_COUNT;i++)REQUIRE(s14_manager_mod_enabled(&ui,s14_manager_mod_index(&ui,i-1))>=s14_manager_mod_enabled(&ui,s14_manager_mod_index(&ui,i)));
    REQUIRE(s14_manager_activate(&ui,61) && ui.mod_sort==0);
    REQUIRE(s14_manager_activate(&ui,83)); // collapse views
    REQUIRE(s14_manager_activate(&ui,81)); // expand search
    REQUIRE(s14_manager_activate(&ui,13) && (ui.requested&S14_AUTO_SEARCH));
    y=s14_manager_mod_top(&ui,S14_MOD_SEARCH);
    REQUIRE(s14_manager_hit(&ui,(POINT){110,y+115})==200);
    REQUIRE(s14_manager_activate(&ui,200) && (ui.search_settings&15)==0);
    int clipped=s14_manager_mod_top(&ui,S14_MOD_DIAGNOSTICS);
    REQUIRE(clipped>=S14_MOD_LIST_BOTTOM && s14_manager_hit(&ui,(POINT){776,clipped+30})==0);
    REQUIRE(GetWindowLongPtrW(ui.mod_scrollbar,GWL_STYLE)&WS_VISIBLE);
    SendMessageW(ui.window,WM_VSCROLL,SB_BOTTOM,(LPARAM)ui.mod_scrollbar);REQUIRE(ui.scroll>0);
    ui.focus=221;SendMessageW(ui.window,WM_KEYDOWN,VK_TAB,0);REQUIRE(ui.focus==42);
    y=s14_manager_mod_top(&ui,S14_MOD_SEARCH);int report_y=y+76+277;
    REQUIRE(report_y>=S14_MOD_LIST_TOP && report_y<S14_MOD_LIST_BOTTOM);
    REQUIRE(s14_manager_hit(&ui,(POINT){100,report_y})==42 && s14_manager_activate(&ui,42) && ui.report_requested);
    ui.report_requested=0;ui.in_game=0;REQUIRE(s14_manager_hit(&ui,(POINT){100,report_y})==0 && !s14_manager_activate(&ui,42));ui.in_game=1;
    wcscpy(ui.search_status,L"已按原生探索规则派遣 14 名武将。");
    wcscpy(ui.search_summary,L"派遣 14 人 · 完成 12 次 · 武将 1 · 名品 2 · 战法书 1 · 金钱 300\n未发现 8 次 · 在途 / 返回中 2 人");
    preview(&ui,"manager-auto-search.bmp");
    ui.focus=10;SendMessageW(ui.window,WM_KEYDOWN,VK_TAB,0);REQUIRE(ui.focus==81);
    REQUIRE(s14_manager_mod_top(&ui,S14_MOD_SEARCH)>=S14_MOD_LIST_TOP); // Keyboard focus reveals scrolled row.
    REQUIRE(s14_manager_activate(&ui,81));ui.scroll=9999;s14_manager_refresh(&ui);
    REQUIRE(ui.scroll>0 && (GetWindowLongPtrW(ui.mod_scrollbar,GWL_STYLE)&WS_VISIBLE));
    REQUIRE(s14_manager_mod_top(&ui,S14_MOD_TROOPS)+76<=S14_MOD_LIST_BOTTOM);
    REQUIRE(s14_manager_activate(&ui,15) && !s14_manager_mod_enabled(&ui,S14_MOD_TROOPS));
    REQUIRE(s14_manager_activate(&ui,15) && s14_manager_mod_enabled(&ui,S14_MOD_TROOPS));
    REQUIRE(!s14_manager_mod_enabled(&ui,S14_MOD_AI_AFFIX) && (ui.visual_settings&12)==12);
    REQUIRE(s14_manager_activate(&ui,87));REQUIRE(s14_manager_activate(&ui,16) && s14_manager_mod_enabled(&ui,S14_MOD_AI_AFFIX));
    ui.focus=16;SendMessageW(ui.window,WM_KEYDOWN,VK_TAB,0);REQUIRE(ui.focus==49);
    SendMessageW(ui.window,WM_KEYDOWN,VK_TAB,0);REQUIRE(ui.focus==50);
    REQUIRE(s14_manager_activate(&ui,49) && !(ui.visual_settings&4) && (ui.visual_settings&8));
    REQUIRE(s14_manager_activate(&ui,16) && !s14_manager_mod_enabled(&ui,S14_MOD_AI_AFFIX));
    REQUIRE(s14_manager_activate(&ui,16) && !(ui.visual_settings&4));
    REQUIRE(s14_manager_activate(&ui,49) && (ui.visual_settings&12)==12);
    preview(&ui,"manager-ai-affix.bmp");
    REQUIRE(s14_manager_activate(&ui,16) && !s14_manager_mod_enabled(&ui,S14_MOD_AI_AFFIX));
    REQUIRE(s14_manager_activate(&ui,101) && ui.tab==1);
    REQUIRE(!s14_manager_activate(&ui,31) && !s14_manager_activate(&ui,32) && !s14_manager_activate(&ui,34));
    REQUIRE(s14_manager_hit(&ui,(POINT){100,512})==35 && s14_manager_activate(&ui,35) && check_count==1);
    REQUIRE(!s14_manager_activate(&ui,36));ui.update_busy=1;REQUIRE(!s14_manager_activate(&ui,35));ui.update_busy=0;
    preview(&ui,"manager-game-status.bmp");
    ui.in_game=0;ui.update_ready=1;ui.running=1;REQUIRE(!s14_manager_activate(&ui,36));
    ui.running=0;REQUIRE(s14_manager_activate(&ui,36)&&download_count==1);
    ui.detected=S14_FOUND_MANAGER|S14_FOUND_DATA;
    wcscpy(ui.root,L"D:\\Games\\Romance_of_the_Three_Kingdoms_14");
    REQUIRE(s14_manager_hit(&ui,(POINT){100,455})==34);
    ui.running=1;REQUIRE(!s14_manager_activate(&ui,34));ui.running=0;
    REQUIRE(s14_manager_activate(&ui,34)&&clean_count==1);
    ui.detected=S14_FOUND_UNKNOWN;REQUIRE(!s14_manager_activate(&ui,34));
    ui.detected=S14_FOUND_MANAGER|S14_FOUND_DATA;preview(&ui,"manager-install.bmp");
    REQUIRE(s14_manager_hit(&ui,(POINT){776,625})==1);
    ui.scale=192;HFONT *fonts[]={&ui.title_font,&ui.body_font,&ui.small_font};
    for(int i=0;i<3;i++){LOGFONTW font;REQUIRE(GetObjectW(*fonts[i],sizeof(font),&font));font.lfHeight*=2;DeleteObject(*fonts[i]);*fonts[i]=CreateFontIndirectW(&font);REQUIRE(*fonts[i]);}
    REQUIRE(s14_manager_hit(&ui,(POINT){200,910})==34);preview(&ui,"manager-install-2x.bmp");
    ui.tab=0;ui.expanded=1;ui.scroll=0;REQUIRE(s14_manager_hit(&ui,(POINT){1552,630})==11);preview(&ui,"manager-mods-2x.bmp");
    GetPrivateProfileStringW(L"Future",L"UnknownFeature",L"",value,32,ini);REQUIRE(!wcscmp(value,L"keep-me"));
    s14_manager_destroy(&ui); REQUIRE(GetForegroundWindow()==foreground);
    REQUIRE(DeleteFileW(ini)); REQUIRE(RemoveDirectoryW(root));
    printf("{\"config_roundtrip\":true,\"search_settings\":true,\"search_controls\":true,\"unknown_keys_preserved\":true,\"master_off_on\":true,\"dependency_preserves_preference\":true,\"all_flag_combinations\":256,\"ui_live_callbacks\":%d,\"cleanup_callback\":%d,\"rendered_views\":7,\"manager_tabs\":[\"mods\",\"settings\"],\"expandable_mod_list\":true,\"stable_mod_sort\":true,\"child_preferences_preserved\":true,\"views_parent_runtime_gate\":true,\"scroll_and_keyboard_reveal\":true,\"dpi_scales\":[96,192],\"game_process_touched\":false}\n",callback_count,clean_count);
    return 0;
}
