#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <stdio.h>
#include <wchar.h>
#include "features.h"
#include "manager_ui.h"
#include "package.h"

static wchar_t executable[MAX_PATH];
static void refresh(S14ManagerUI *ui,int check_directory) {
    if (check_directory) ui->game_found=s14_game_available(ui->root);
    ui->installed=s14_owned_install(ui->root); ui->running=s14_game_running(ui->root);
    ui->requested=s14_config_read(ui->ini);
    unsigned int applied=0; int fault=0; ui->attached=s14_read_runtime(ui->root,&applied,&fault); ui->fault=fault;
    ui->effective=s14_effective_flags(ui->requested);
    if (ui->attached && fault) wcscpy(ui->status,s14_fault_message(fault));
    else if (ui->attached) wcscpy(ui->status,applied==ui->effective?L"游戏已连接 · 当前设置已生效":L"设置已保存 · 等待游戏应用");
    else if (ui->running) wcscpy(ui->status,L"游戏正在运行 · 尚未连接此版本管理器，更新后需要重启");
    else wcscpy(ui->status,L"游戏未运行 · 设置将在下次启动时载入");
    s14_manager_refresh(ui);
}
static void set_root(S14ManagerUI *ui,const wchar_t *root) {
    wchar_t full[MAX_PATH]; DWORD length=GetFullPathNameW(root,MAX_PATH,full,NULL); if (!length || length>=MAX_PATH) return;
    size_t n=wcslen(full); while (n>3 && (full[n-1]==L'\\' || full[n-1]==L'/')) full[--n]=0;
    if (!s14_join(ui->ini,full,L"SAN14BuildLimit.ini")) return;
    wcscpy(ui->root,full); ui->notice[0]=0; ui->notice_error=0;
    if (ui->directory_edit) SetWindowTextW(ui->directory_edit,full);
    refresh(ui,1);
}
static void choose(S14ManagerUI *ui) {
    IFileOpenDialog *dialog=NULL;
    HRESULT hr=CoCreateInstance(&CLSID_FileOpenDialog,NULL,CLSCTX_INPROC_SERVER,&IID_IFileOpenDialog,(void**)&dialog);
    if (FAILED(hr)) { wcscpy(ui->notice,L"无法打开目录选择器。"); return; }
    FILEOPENDIALOGOPTIONS options=0; IFileOpenDialog_GetOptions(dialog,&options);
    IFileOpenDialog_SetOptions(dialog,options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
    IFileOpenDialog_SetTitle(dialog,L"选择含 SAN14PK_SC.exe 的游戏目录");
    if (SUCCEEDED(IFileOpenDialog_Show(dialog,ui->window))) {
        IShellItem *item=NULL; if (SUCCEEDED(IFileOpenDialog_GetResult(dialog,&item))) {
            wchar_t *path=NULL; if (SUCCEEDED(IShellItem_GetDisplayName(item,SIGDN_FILESYSPATH,&path))) { set_root(ui,path); CoTaskMemFree(path); }
            IShellItem_Release(item);
        }
    }
    IFileOpenDialog_Release(dialog);
}
static void action(S14ManagerUI *ui,int command,void *context) {
    (void)context; wchar_t error[192]={0};
    if (command==S14_ACTION_CONFIG) refresh(ui,0);
    else if (command==S14_ACTION_CHOOSE) choose(ui);
    else if (command==S14_ACTION_INSTALL) {
        int installed=s14_package_install(ui->root,executable,error);
        ui->notice_error=!installed;
        if (installed) wcscpy(ui->notice,L"安装完成。现在可启动游戏，用 F10 打开管理器。");
        else wcscpy(ui->notice,error);
        refresh(ui,1);
        MessageBoxW(ui->window,ui->notice,installed?L"安装完成":L"安装失败",MB_OK|(installed?MB_ICONINFORMATION:MB_ICONERROR));
    } else if (command==S14_ACTION_REMOVE) {
        int removed=s14_package_remove(ui->root,error); ui->notice_error=!removed;
        if (removed) wcscpy(ui->notice,L"插件已移除。设置、管理器和备份已保留。"); else wcscpy(ui->notice,error);
        refresh(ui,1);
    } else if (command==S14_ACTION_LOGS) {
        wchar_t logs[MAX_PATH]; if (s14_join(logs,ui->root,L"SAN14ModManager\\logs")) ShellExecuteW(ui->window,L"open",logs,NULL,NULL,SW_SHOWNORMAL);
    }
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR command,int show) {
    (void)previous; (void)command; (void)show;
    GetModuleFileNameW(NULL,executable,MAX_PATH);
    int count=0; wchar_t **args=CommandLineToArgvW(GetCommandLineW(),&count);
    if (count==3 && (!wcscmp(args[1],L"--install") || !wcscmp(args[1],L"--remove"))) {
        wchar_t target[MAX_PATH],error[192]={0}; DWORD length=GetFullPathNameW(args[2],MAX_PATH,target,NULL);
        if (!length || length>=MAX_PATH) { LocalFree(args); return 2; }
        size_t n=wcslen(target); while (n>3 && (target[n-1]==L'\\' || target[n-1]==L'/')) target[--n]=0;
        int ok=!wcscmp(args[1],L"--install")?s14_package_install(target,executable,error):s14_package_remove(target,error);
        if (!ok) { char utf8[768]; if (WideCharToMultiByte(CP_UTF8,0,error,-1,utf8,sizeof(utf8),NULL,NULL)) fprintf(stderr,"%s\n",utf8); }
        LocalFree(args); return ok?0:2;
    }
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    S14ManagerUI ui={0}; ui.action=action;
    wchar_t root[MAX_PATH]; wcscpy(root,executable); wchar_t *slash=wcsrchr(root,L'\\'); if (slash) *slash=0;
    if (count==3 && !wcscmp(args[1],L"--game-dir") && wcslen(args[2])<MAX_PATH) wcscpy(root,args[2]);
    else { wchar_t initial[MAX_PATH]; wcscpy(initial,root); int found=0;
        for (int i=0;i<4;i++) { wchar_t candidate[MAX_PATH];
            if (s14_join(candidate,root,L"SAN14PK_SC.exe") && GetFileAttributesW(candidate)!=INVALID_FILE_ATTRIBUTES) { found=1; break; }
            wchar_t *parent=wcsrchr(root,L'\\'); if (!parent || parent-root<3) break; *parent=0;
        }
        if (!found) wcscpy(root,initial);
    }
    LocalFree(args); set_root(&ui,root);
    if (!s14_manager_create(&ui,instance,NULL,0)) { CoUninitialize(); return 1; }
    refresh(&ui,1); s14_manager_toggle(&ui);
    ULONGLONG last_refresh=0; MSG message;
    while (IsWindow(ui.window)) {
        while (PeekMessageW(&message,NULL,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        ULONGLONG now=GetTickCount64(); if (now-last_refresh>=1000) { last_refresh=now; refresh(&ui,0); }
        MsgWaitForMultipleObjects(0,NULL,FALSE,50,QS_ALLINPUT);
    }
    s14_manager_destroy(&ui); CoUninitialize(); return 0;
}
