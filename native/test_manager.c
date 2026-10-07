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
static void changed(S14ManagerUI *ui,int action,void *context) { (void)ui; (void)context; if (action==S14_ACTION_CONFIG) callback_count++; }
static void preview(S14ManagerUI *ui) {
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(info.bmiHeader); info.bmiHeader.biWidth=760; info.bmiHeader.biHeight=-670;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL); REQUIRE(dc!=NULL); void *pixels=NULL;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0); REQUIRE(bitmap && pixels);
    HGDIOBJ old=SelectObject(dc,bitmap); s14_manager_paint(ui,dc,760,670); GdiFlush();
    FILE *file=fopen("manager-preview.bmp","wb"); REQUIRE(file!=NULL);
    BITMAPFILEHEADER header={0}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader); header.bfSize=header.bfOffBits+760*670*4;
    REQUIRE(fwrite(&header,sizeof(header),1,file)==1); REQUIRE(fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1); REQUIRE(fwrite(pixels,760*670*4,1,file)==1);
    fclose(file); SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc);
}
int main(void) {
    wchar_t temp[MAX_PATH],root[MAX_PATH],ini[MAX_PATH]; REQUIRE(GetTempPathW(MAX_PATH,temp));
    swprintf(root,MAX_PATH,L"%lsS14-manager-test-%lu",temp,(unsigned long)GetCurrentProcessId()); REQUIRE(CreateDirectoryW(root,NULL));
    REQUIRE(s14_join(ini,root,L"SAN14BuildLimit.ini"));
    REQUIRE(s14_config_read(ini)==(S14_MASTER|S14_WALL_LIMIT|S14_LIMIT_HINT));
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
    for (unsigned int flags=0;flags<16;flags++) {
        unsigned int effective=s14_effective_flags(flags);
        REQUIRE(!(effective&S14_LIMIT_HINT) || (effective&S14_WALL_LIMIT));
        REQUIRE(!(flags&S14_MASTER)?effective==0:1);
        for (int i=0;i<1000;i++) REQUIRE(s14_runtime_mode(flags)==s14_runtime_mode(effective));
    }
    HWND foreground=GetForegroundWindow(); S14ManagerUI ui={0}; ui.action=changed;
    wcscpy(ui.ini,ini); wcscpy(ui.root,root); wcscpy(ui.status,L"游戏已接入 · 开关从下一次检查起生效");
    ui.compatible=ui.installed=ui.attached=1;
    REQUIRE(s14_manager_create(&ui,GetModuleHandleW(NULL),NULL,1));
    REQUIRE(GetForegroundWindow()==foreground && !IsWindowVisible(ui.window));
    REQUIRE(s14_manager_hit(&ui,(POINT){668,242})==10);
    REQUIRE(s14_manager_activate(&ui,10)); REQUIRE(!(ui.requested&S14_WALL_LIMIT));
    REQUIRE(s14_manager_activate(&ui,10)); REQUIRE(ui.effective&S14_WALL_LIMIT);
    REQUIRE(callback_count==2);
    REQUIRE(s14_manager_activate(&ui,1)); REQUIRE(ui.effective==0);
    REQUIRE(s14_manager_activate(&ui,1)); REQUIRE(ui.effective&S14_LIMIT_HINT);
    ui.tab=1; REQUIRE(!s14_manager_activate(&ui,31)); REQUIRE(!s14_manager_activate(&ui,32));
    ui.tab=0; preview(&ui);
    s14_manager_destroy(&ui); REQUIRE(GetForegroundWindow()==foreground);
    REQUIRE(DeleteFileW(ini)); REQUIRE(RemoveDirectoryW(root));
    printf("{\"config_roundtrip\":true,\"unknown_keys_preserved\":true,\"master_off_on\":true,\"dependency_preserves_preference\":true,\"all_flag_combinations\":16,\"ui_live_callbacks\":%d,\"game_process_touched\":false}\n",callback_count);
    return 0;
}
