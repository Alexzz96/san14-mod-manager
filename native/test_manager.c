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
    int width=MulDiv(760,ui->scale,96),height=MulDiv(670,ui->scale,96);
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
    REQUIRE(s14_config_read(ini)==(S14_MASTER|S14_LIMIT_HINT|S14_AUTO_SEARCH));
    REQUIRE(s14_effective_flags(s14_config_read(ini))==(S14_MASTER|S14_AUTO_SEARCH));
    REQUIRE(s14_search_settings_read(ini)==1 && s14_battle_setting_read(ini));
    REQUIRE(WritePrivateProfileStringW(L"AutoSearch",L"Executors",L"99",ini));
    REQUIRE(s14_search_settings_read(ini)==1);
    REQUIRE(WritePrivateProfileStringW(L"AutoSearch",L"Executors",NULL,ini));
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
    for (unsigned int flags=0;flags<32;flags++) {
        unsigned int effective=s14_effective_flags(flags);
        REQUIRE(!(effective&S14_LIMIT_HINT) || (effective&S14_WALL_LIMIT));
        REQUIRE(!(flags&S14_MASTER)?effective==0:1);
        for (int i=0;i<1000;i++) REQUIRE(s14_runtime_mode(flags)==s14_runtime_mode(effective));
    }
    HWND foreground=GetForegroundWindow(); S14ManagerUI ui={0}; ui.action=changed;
    wcscpy(ui.ini,ini); wcscpy(ui.root,root); wcscpy(ui.status,L"游戏已接入 · 开关从下一次检查起生效");
    ui.game_found=ui.installed=ui.attached=1;
    REQUIRE(s14_manager_create(&ui,GetModuleHandleW(NULL),NULL,1));
    REQUIRE(GetForegroundWindow()==foreground && !IsWindowVisible(ui.window));
    REQUIRE(s14_manager_hit(&ui,(POINT){668,242})==10);
    REQUIRE(s14_manager_activate(&ui,10)); REQUIRE(!(ui.requested&S14_WALL_LIMIT));
    REQUIRE(s14_manager_activate(&ui,10)); REQUIRE(ui.effective&S14_WALL_LIMIT);
    REQUIRE(callback_count==2);
    REQUIRE(s14_manager_activate(&ui,1)); REQUIRE(ui.effective==0);
    REQUIRE(s14_manager_activate(&ui,1)); REQUIRE(ui.effective&S14_LIMIT_HINT);
    ui.tab=1; REQUIRE(!s14_manager_activate(&ui,31)); REQUIRE(!s14_manager_activate(&ui,32)); REQUIRE(!s14_manager_activate(&ui,34));
    REQUIRE(s14_manager_hit(&ui,(POINT){100,535})==35 && s14_manager_activate(&ui,35) && check_count==1);
    REQUIRE(!s14_manager_activate(&ui,36));ui.update_busy=1;REQUIRE(!s14_manager_activate(&ui,35));ui.update_busy=0;
    ui.in_game=0;ui.update_ready=1;ui.running=1;REQUIRE(!s14_manager_activate(&ui,36));
    ui.running=0;REQUIRE(s14_manager_activate(&ui,36)&&download_count==1);ui.in_game=1;
    ui.tab=3;REQUIRE(s14_manager_hit(&ui,(POINT){100,570})==42 && s14_manager_activate(&ui,42) && ui.report_requested);
    ui.report_requested=0;ui.in_game=0;REQUIRE(s14_manager_hit(&ui,(POINT){100,570})==0 && !s14_manager_activate(&ui,42) && !ui.report_requested);ui.in_game=1;
    ui.tab=0; preview(&ui,"manager-preview.bmp");
    ui.tab=1; preview(&ui,"manager-game-status.bmp");
    ui.in_game=0; ui.detected=S14_FOUND_MANAGER|S14_FOUND_DATA;
    wcscpy(ui.root,L"D:\\Games\\Romance_of_the_Three_Kingdoms_14");
    REQUIRE(s14_manager_hit(&ui,(POINT){100,455})==34);
    ui.running=1; REQUIRE(!s14_manager_activate(&ui,34)); ui.running=0;
    REQUIRE(s14_manager_activate(&ui,34) && clean_count==1);
    ui.detected=S14_FOUND_UNKNOWN; REQUIRE(!s14_manager_activate(&ui,34));
    ui.detected=S14_FOUND_MANAGER|S14_FOUND_DATA; preview(&ui,"manager-install.bmp");
    ui.tab=2; REQUIRE(s14_manager_hit(&ui,(POINT){650,200})==40);REQUIRE(ui.officers_enabled);
    REQUIRE(s14_manager_activate(&ui,40) && !ui.officers_enabled);REQUIRE(GetPrivateProfileIntW(L"Views",L"Officers",1,ini)==0);
    REQUIRE(s14_manager_activate(&ui,40) && ui.officers_enabled);REQUIRE(GetPrivateProfileIntW(L"Views",L"Officers",0,ini)==1);
    REQUIRE(s14_manager_hit(&ui,(POINT){650,320})==41 && ui.battle_enabled);
    REQUIRE(s14_manager_activate(&ui,41) && !ui.battle_enabled && !s14_battle_setting_read(ini));
    REQUIRE(s14_manager_activate(&ui,41) && ui.battle_enabled && s14_battle_setting_read(ini));
    REQUIRE(s14_manager_hit(&ui,(POINT){650,440})==43 && ui.native_stats_enabled);
    REQUIRE(s14_manager_activate(&ui,43) && !ui.native_stats_enabled && GetPrivateProfileIntW(L"Views",L"NativeOfficerStats",1,ini)==0);
    REQUIRE(s14_manager_activate(&ui,43) && ui.native_stats_enabled && GetPrivateProfileIntW(L"Views",L"NativeOfficerStats",0,ini)==1);
    preview(&ui,"manager-roadmap.bmp");
    ui.tab=3; ui.in_game=1; REQUIRE(s14_manager_hit(&ui,(POINT){690,170})==13);
    REQUIRE(s14_manager_activate(&ui,13) && (ui.requested&S14_AUTO_SEARCH));
    REQUIRE(s14_manager_hit(&ui,(POINT){110,250})==200);
    REQUIRE(s14_manager_activate(&ui,200) && (ui.search_settings&15)==0);
    wcscpy(ui.search_status,L"已按原生探索规则派遣 14 名武将。");
    wcscpy(ui.search_summary,L"派遣 14 人 · 完成 12 次 · 武将 1 · 名品 2 · 战法书 1 · 金钱 300\n未发现 8 次 · 在途 / 返回中 2 人");
    wcscpy(ui.search_details,L"曹操发现武将张辽，登用成功。\n荀彧发现名品青釭剑。\n发现战法书。\n夏侯惇在城市找到 300 金钱。");
    preview(&ui,"manager-auto-search.bmp");
    ui.in_game=0;
    ui.scale=192; HFONT *fonts[]={&ui.title_font,&ui.body_font,&ui.small_font};
    for (int i=0;i<3;i++) { LOGFONTW font; REQUIRE(GetObjectW(*fonts[i],sizeof(font),&font)); font.lfHeight*=2; DeleteObject(*fonts[i]); *fonts[i]=CreateFontIndirectW(&font); REQUIRE(*fonts[i]); }
    ui.tab=1; REQUIRE(s14_manager_hit(&ui,(POINT){200,910})==34); preview(&ui,"manager-install-2x.bmp");
    s14_manager_destroy(&ui); REQUIRE(GetForegroundWindow()==foreground);
    REQUIRE(DeleteFileW(ini)); REQUIRE(RemoveDirectoryW(root));
    printf("{\"config_roundtrip\":true,\"search_settings\":true,\"search_controls\":true,\"unknown_keys_preserved\":true,\"master_off_on\":true,\"dependency_preserves_preference\":true,\"all_flag_combinations\":32,\"ui_live_callbacks\":%d,\"cleanup_callback\":%d,\"rendered_views\":6,\"dpi_scales\":[96,192],\"game_process_touched\":false}\n",callback_count,clean_count);
    return 0;
}
