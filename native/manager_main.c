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
#include "search_model.h"
#include "update.h"

static wchar_t executable[MAX_PATH];
#define UPDATE_DONE (WM_APP+140)
// Worker owns network state; the window thread reads it only after worker exit.
static struct {HANDLE thread;HWND window;int download,ok;S14Release release;wchar_t path[MAX_PATH],root[MAX_PATH],error[192];} updater;
static DWORD WINAPI update_worker(void *unused){
    (void)unused;updater.ok=updater.download?s14_update_download(&updater.release,updater.path,updater.error):s14_update_check(&updater.release,updater.error);
    PostMessageW(updater.window,UPDATE_DONE,0,0);return 0;
}
static void start_update(S14ManagerUI *ui,int download){
    if(ui->update_busy)return;
    if(updater.thread){if(WaitForSingleObject(updater.thread,0)!=WAIT_OBJECT_0)return;CloseHandle(updater.thread);updater.thread=NULL;}
    ui->update_busy=1;updater.window=ui->window;updater.download=download;updater.error[0]=0;updater.path[0]=0;
    wcscpy(updater.root,ui->root);
    if(!download)ui->update_ready=0;
    wcscpy(ui->update_status,download?L"正在下载并校验安装器…":L"正在检查 GitHub 发布版本…");
    updater.thread=CreateThread(NULL,0,update_worker,NULL,0,NULL);
    if(!updater.thread){ui->update_busy=0;wcscpy(ui->update_status,L"无法开始更新检查，请重试。");}
}
static void finish_update(S14ManagerUI *ui){
    if(!updater.thread || WaitForSingleObject(updater.thread,0)!=WAIT_OBJECT_0)return;
    CloseHandle(updater.thread);updater.thread=NULL;ui->update_busy=0;ui->notice_error=!updater.ok;
    if(!updater.ok){wcscpy(ui->update_status,L"操作失败，详情见下方。");wcscpy(ui->notice,updater.error);}
    else if(!updater.download){
        ui->update_ready=updater.release.newer;wchar_t version[32];MultiByteToWideChar(CP_UTF8,0,updater.release.version,-1,version,32);
        swprintf(ui->update_status,192,updater.release.newer?L"发现 v%ls，可下载更新。":L"当前已是最新版本（v%ls）。",version);
        wcscpy(ui->notice,L"发布源：Alexzz96/san14-mod-manager，包含已发布的预发布版本。");
    }else if(_wcsicmp(updater.root,ui->root)){
        wcscpy(ui->update_status,L"目录已改变，未安装。");wcscpy(ui->notice,L"请选择要更新的游戏目录后重新更新。");
    }else if(s14_game_running(ui->root)){
        wcscpy(ui->update_status,L"已下载；游戏仍在运行。");wcscpy(ui->notice,L"请保存并退出游戏后再次更新，暂未替换任何游戏文件。");
    }else{
        wchar_t args[MAX_PATH+64];swprintf(args,MAX_PATH+64,L"--apply-update \"%ls\"",ui->root);
        if((INT_PTR)ShellExecuteW(ui->window,L"open",updater.path,args,NULL,SW_SHOWNORMAL)>32){DestroyWindow(ui->window);return;}
        wcscpy(ui->notice,L"无法打开下载的安装器，请重新尝试。");ui->notice_error=1;
    }
    s14_manager_refresh(ui);
}
static void refresh(S14ManagerUI *ui,int check_directory) {
    if (check_directory) ui->game_found=s14_game_available(ui->root);
    ui->installed=s14_owned_install(ui->root); ui->running=s14_game_running(ui->root);
    ui->detected=s14_package_detect(ui->root,executable);
    ui->requested=s14_config_read(ui->ini);
    ui->search_settings=s14_search_settings_read(ui->ini);
    ui->officers_enabled=GetPrivateProfileIntW(L"Views",L"Officers",1,ui->ini)!=0;
    ui->views_enabled=s14_views_setting_read(ui->ini);
    ui->native_stats_enabled=GetPrivateProfileIntW(L"Views",L"NativeOfficerStats",1,ui->ini)!=0;
    ui->native_army_enabled=GetPrivateProfileIntW(L"Views",L"NativeArmyValues",1,ui->ini)!=0;
    ui->visual_settings=s14_visual_settings_read(ui->ini);
    ui->battle_enabled=s14_battle_setting_read(ui->ini);
    s14_search_report_read(ui->root,ui->search_summary,ui->search_details);
    unsigned int applied=0; int fault=0; ui->attached=s14_read_runtime(ui->root,&applied,&fault); ui->fault=fault;
    ui->effective=s14_effective_flags(ui->requested);
    if(ui->attached){
        ui->effective=applied;
        s14_read_search_runtime(ui->root,&ui->search_state,&ui->search_force,&ui->search_group,ui->search_status);
    }else{ui->effective=0;ui->search_state=S14_SEARCH_WAITING;ui->search_force=ui->search_group=0;ui->search_status[0]=0;}
    if (ui->attached && fault) wcscpy(ui->status,s14_fault_message(fault));
    else if (ui->attached) wcscpy(ui->status,(ui->requested&S14_AUTO_SEARCH) && ui->search_state==S14_SEARCH_STOPPED?L"游戏已连接 · 自动搜索异常停用，请展开查看原因":L"游戏已连接 · 已显示实际生效状态");
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
    else if(command==S14_ACTION_CHECK_UPDATE)start_update(ui,0);
    else if(command==S14_ACTION_DOWNLOAD_UPDATE)start_update(ui,1);
    else if (command==S14_ACTION_CHOOSE) choose(ui);
    else if (command==S14_ACTION_INSTALL) {
        int installed=s14_package_install(ui->root,executable,error);
        ui->notice_error=!installed;
        if (installed) wcscpy(ui->notice,L"安装完成。启动游戏后，F 查看武将，F10 打开管理器。");
        else wcscpy(ui->notice,error);
        refresh(ui,1);
        MessageBoxW(ui->window,ui->notice,installed?L"安装完成":L"安装失败",MB_OK|(installed?MB_ICONINFORMATION:MB_ICONERROR));
    } else if (command==S14_ACTION_CLEAN) {
        if (MessageBoxW(ui->window,L"彻底卸载将删除本项目已识别的插件、管理器、设置、日志及备份。\n游戏文件、存档和未知文件会保留。\n\n请从游戏目录外的安装包运行，并关闭目标目录中的管理器。\n确认彻底卸载？",L"彻底卸载",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES) return;
        S14CleanupReport report={0}; int cleaned=s14_package_clean(ui->root,executable,&report,error); ui->notice_error=!cleaned;
        if (cleaned) swprintf(ui->notice,192,L"卸载完成：清理 %d 个文件、%d 个空目录。%ls",report.files,report.directories,report.preserved?L"未知文件已保留，未删除其所在目录。":L"游戏与存档已保留。");
        else swprintf(ui->notice,192,L"未完成（已清理 %d 个文件）：%.150ls",report.files,error);
        refresh(ui,1);
        MessageBoxW(ui->window,ui->notice,cleaned?L"卸载完成":L"卸载未完成",MB_OK|(cleaned?MB_ICONINFORMATION:MB_ICONERROR));
    } else if (command==S14_ACTION_REMOVE) {
        int removed=s14_package_remove(ui->root,error); ui->notice_error=!removed;
        if (removed) wcscpy(ui->notice,L"插件已移除。设置、管理器和备份已保留。"); else wcscpy(ui->notice,error);
        refresh(ui,1);
        MessageBoxW(ui->window,ui->notice,removed?L"插件已移除":L"移除失败",MB_OK|(removed?MB_ICONINFORMATION:MB_ICONERROR));
    } else if (command==S14_ACTION_LOGS) {
        wchar_t logs[MAX_PATH]; if (s14_join(logs,ui->root,L"SAN14ModManager\\logs")) ShellExecuteW(ui->window,L"open",logs,NULL,NULL,SW_SHOWNORMAL);
    }
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR command,int show) {
    (void)previous; (void)command; (void)show;
    GetModuleFileNameW(NULL,executable,MAX_PATH);
    int count=0; wchar_t **args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(count==2 && !wcscmp(args[1],L"--check-updates")){
        S14Release release;wchar_t error[192]={0};int ok=s14_update_check(&release,error);
        if(ok)printf("{\"version\":\"%s\",\"newer\":%s,\"sha256\":\"%s\"}\n",release.version,release.newer?"true":"false",release.sha256);
        else{char text[768];WideCharToMultiByte(CP_UTF8,0,error,-1,text,sizeof(text),NULL,NULL);fprintf(stderr,"%s\n",text);}LocalFree(args);return ok?0:2;
    }
    if (count==3 && (!wcscmp(args[1],L"--install") || !wcscmp(args[1],L"--remove") || !wcscmp(args[1],L"--uninstall"))) {
        wchar_t target[MAX_PATH],error[192]={0}; DWORD length=GetFullPathNameW(args[2],MAX_PATH,target,NULL);
        if (!length || length>=MAX_PATH) { LocalFree(args); return 2; }
        size_t n=wcslen(target); while (n>3 && (target[n-1]==L'\\' || target[n-1]==L'/')) target[--n]=0;
        S14CleanupReport report={0};
        int ok=!wcscmp(args[1],L"--install")?s14_package_install(target,executable,error):
            !wcscmp(args[1],L"--remove")?s14_package_remove(target,error):s14_package_clean(target,executable,&report,error);
        if (ok && !wcscmp(args[1],L"--uninstall")) printf("{\"files\":%d,\"directories\":%d,\"preserved\":%d}\n",report.files,report.directories,report.preserved);
        if (!ok) { char utf8[768]; if (WideCharToMultiByte(CP_UTF8,0,error,-1,utf8,sizeof(utf8),NULL,NULL)) fprintf(stderr,"%s\n",utf8); }
        LocalFree(args); return ok?0:2;
    }
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    S14ManagerUI ui={0}; ui.action=action;
    int updates=count==3 && !wcscmp(args[1],L"--updates"),apply_update=count==3 && !wcscmp(args[1],L"--apply-update");
    wchar_t root[MAX_PATH]; wcscpy(root,executable); wchar_t *slash=wcsrchr(root,L'\\'); if (slash) *slash=0;
    if (count==3 && (!wcscmp(args[1],L"--game-dir") || updates || apply_update) && wcslen(args[2])<MAX_PATH) wcscpy(root,args[2]);
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
    if(updates || apply_update){ui.tab=1;s14_manager_refresh(&ui);}
    if(updates)start_update(&ui,0);
    if(apply_update){Sleep(600);action(&ui,S14_ACTION_INSTALL,NULL);}
    ULONGLONG last_refresh=0; MSG message;
    while (IsWindow(ui.window)) {
        while (PeekMessageW(&message,NULL,0,0,PM_REMOVE)) { if(message.message==UPDATE_DONE)continue;TranslateMessage(&message); DispatchMessageW(&message); }
        if(ui.update_busy && updater.thread && WaitForSingleObject(updater.thread,0)==WAIT_OBJECT_0)finish_update(&ui);
        ULONGLONG now=GetTickCount64(); if (now-last_refresh>=1000) { last_refresh=now; refresh(&ui,0); }
        MsgWaitForMultipleObjects(0,NULL,FALSE,50,QS_ALLINPUT);
    }
    s14_manager_destroy(&ui); CoUninitialize(); return 0;
}
