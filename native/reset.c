#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include "package.h"
#include "features.h"

static int fail(wchar_t *error,const wchar_t *message){wcsncpy(error,message,191);error[191]=0;return 0;}
static int plain(const wchar_t *path,int directory){DWORD a=GetFileAttributesW(path);return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_REPARSE_POINT) && !!(a&FILE_ATTRIBUTE_DIRECTORY)==directory;}
static int plain_ancestors(const wchar_t *path){wchar_t full[MAX_PATH];DWORD n=GetFullPathNameW(path,MAX_PATH,full,NULL);if(!n || n>=MAX_PATH || full[1]!=L':')return 0;
    for(wchar_t *p=full+3;;p++){if(*p && *p!=L'\\')continue;wchar_t save=*p;*p=0;int ok=plain(full,1);*p=save;if(!ok)return 0;if(!save)break;}return 1;}
// Validate the complete fixed plugin subtree before moving it. Never follow links.
static int tree(const wchar_t *path,int depth){if(depth>32 || !plain(path,1))return 0;wchar_t pattern[MAX_PATH];if(!s14_join(pattern,path,L"*"))return 0;
    WIN32_FIND_DATAW d;HANDLE f=FindFirstFileW(pattern,&d);if(f==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;int ok=1;
    do{if(!wcscmp(d.cFileName,L".") || !wcscmp(d.cFileName,L".."))continue;wchar_t child[MAX_PATH];if((d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) || !s14_join(child,path,d.cFileName)){ok=0;break;}if((d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !tree(child,depth+1)){ok=0;break;}}while(FindNextFileW(f,&d));
    DWORD e=GetLastError();FindClose(f);return ok && e==ERROR_NO_MORE_FILES;}
static int defaults(const wchar_t *path){
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return 0;
    const char text[]="[Rule]\r\nMode=0\r\n[Manager]\r\nEnabled=1\r\n[Features]\r\nWallClusterLimit=0\r\nLimitHint=1\r\nDiagnostics=0\r\nAutoSearch=1\r\nAutoDomesticFill=1\r\nCaoRenAttackDefense=0\r\nPluginTroops=0\r\nAiRandomAffix=0\r\nHuangZhongBlink=0\r\n[Observation]\r\nBattleEvents=1\r\n[Views]\r\nEnabled=1\r\nOfficers=1\r\nNativeOfficerStats=1\r\nNativeArmyValues=1\r\n[AutoSearch]\r\nExecutors=1\r\nReturnDays=0\r\nPriority=0\r\n";
    DWORD n=0;int ok=WriteFile(file,text,sizeof(text)-1,&n,NULL) && n==sizeof(text)-1 && FlushFileBuffers(file);CloseHandle(file);return ok;
}
int s14_package_reset(const wchar_t *root,wchar_t backup[MAX_PATH],wchar_t error[192]){
    backup[0]=0;error[0]=0;wchar_t full[MAX_PATH];DWORD n=GetFullPathNameW(root,MAX_PATH,full,NULL);if(!n || n>=MAX_PATH)return fail(error,L"初始化路径无效。");
    size_t len=wcslen(full);while(len>3 && full[len-1]==L'\\')full[--len]=0;
    if(!plain_ancestors(full) || !s14_game_available(full) || !s14_owned_install(full))return fail(error,L"未找到本项目已识别的安装，或目录包含链接；未初始化。");
    if(s14_game_running(full))return fail(error,L"请保存并完全退出游戏后初始化。");
    wchar_t state[MAX_PATH],config[MAX_PATH],receipt[MAX_PATH],backups[MAX_PATH],old_state[MAX_PATH],old_config[MAX_PATH],fresh[MAX_PATH],fresh_config[MAX_PATH],fresh_receipt[MAX_PATH],saved_receipt[MAX_PATH];
    if(!s14_join(state,full,L"SAN14ModManager") || !s14_join(config,full,L"SAN14BuildLimit.ini") || !s14_join(receipt,state,L"installation.ini") || !s14_join(backups,full,L"SAN14ModManager-ResetBackups") || !tree(state,0) || !plain(receipt,0))return fail(error,L"插件数据路径异常；未初始化。");
    int had_config=GetFileAttributesW(config)!=INVALID_FILE_ATTRIBUTES;if(had_config && !plain(config,0))return fail(error,L"配置不是普通文件；未初始化。");
    if(!CreateDirectoryW(backups,NULL) && !(GetLastError()==ERROR_ALREADY_EXISTS && plain(backups,1)))return fail(error,L"无法建立初始化备份目录。");
    SYSTEMTIME t;GetLocalTime(&t);wchar_t name[80];swprintf(name,80,L"%04u%02u%02u-%02u%02u%02u-%03u-%lu",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,(unsigned long)GetCurrentProcessId());
    if(!s14_join(backup,backups,name) || !CreateDirectoryW(backup,NULL))return fail(error,L"无法建立独立备份；未初始化。");
    if(!s14_join(old_state,backup,L"SAN14ModManager") || !s14_join(old_config,backup,L"SAN14BuildLimit.ini") || !s14_join(fresh,backup,L"new-state") || !s14_join(fresh_config,backup,L"new-config.ini") || !s14_join(fresh_receipt,fresh,L"installation.ini") || !s14_join(saved_receipt,backup,L"installation.ini") || !CreateDirectoryW(fresh,NULL) || !CopyFileW(receipt,saved_receipt,TRUE) || !CopyFileW(receipt,fresh_receipt,TRUE) || !defaults(fresh_config))return fail(error,L"准备默认配置失败；原数据未清空，备份目录已保留。");
    const wchar_t *binary[]={L"dinput8.dll",L"SAN14ModManager.exe"};for(int i=0;i<2;i++){wchar_t from[MAX_PATH],to[MAX_PATH];if(!s14_join(from,full,binary[i]) || !s14_join(to,backup,binary[i]) || !plain(from,0) || !CopyFileW(from,to,TRUE))return fail(error,L"插件备份失败；原数据未清空。");}
    // Move rather than delete. A failed transaction restores the old active paths.
    WritePrivateProfileStringW(NULL,NULL,NULL,config);WritePrivateProfileStringW(NULL,NULL,NULL,receipt);
    int moved_state=0,moved_config=0,placed_state=0,placed_config=0;
    if(!(moved_state=MoveFileExW(state,old_state,MOVEFILE_WRITE_THROUGH)!=0))goto rollback;
    if(had_config && !(moved_config=MoveFileExW(config,old_config,MOVEFILE_WRITE_THROUGH)!=0))goto rollback;
    if(!(placed_state=MoveFileExW(fresh,state,MOVEFILE_WRITE_THROUGH)!=0))goto rollback;
    if(!(placed_config=MoveFileExW(fresh_config,config,MOVEFILE_WRITE_THROUGH)!=0))goto rollback;
    WritePrivateProfileStringW(NULL,NULL,NULL,config);WritePrivateProfileStringW(NULL,NULL,NULL,receipt);return 1;
rollback:;
    int restored=1;if(placed_config)restored=MoveFileExW(config,fresh_config,MOVEFILE_WRITE_THROUGH)!=0;
    if(placed_state)restored=(MoveFileExW(state,fresh,MOVEFILE_WRITE_THROUGH)!=0)&&restored;
    if(moved_config)restored=(MoveFileExW(old_config,config,MOVEFILE_WRITE_THROUGH)!=0)&&restored;
    if(moved_state)restored=(MoveFileExW(old_state,state,MOVEFILE_WRITE_THROUGH)!=0)&&restored;
    WritePrivateProfileStringW(NULL,NULL,NULL,config);WritePrivateProfileStringW(NULL,NULL,NULL,receipt);
    return fail(error,restored?L"文件被占用或无法替换；原数据已恢复，请关闭其他管理器后重试。":L"初始化未完成，自动恢复不完整；请保留初始化备份目录恢复数据。");
}
