#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "package.h"
#include "battle_stats.h"
#include "special_stats.h"
#include "battle_timeline.h"
#include "troop_store.h"
#include "ai_affix.h"

// Only the fixed application paths are visited; no recursive directory delete.
#define CLEAN_MAX 2048
typedef struct { HANDLE handle; wchar_t path[MAX_PATH]; } CleanFile;
typedef struct {
    wchar_t root[MAX_PATH]; const wchar_t *source;
    CleanFile *files; int count; S14CleanupReport *report; wchar_t *error;
} CleanPlan;
static int failure(CleanPlan *p,const wchar_t *message) { wcsncpy(p->error,message,191); p->error[191]=0; return 0; }
static int plain_path(const wchar_t *path) {
    wchar_t full[MAX_PATH]; DWORD n=GetFullPathNameW(path,MAX_PATH,full,NULL);
    if (!n || n>=MAX_PATH) return 0;
    for (wchar_t *q=full+3; ; q++) {
        if (*q && *q!=L'\\') continue;
        wchar_t saved=*q; *q=0; DWORD a=GetFileAttributesW(full); *q=saved;
        if (a==INVALID_FILE_ATTRIBUTES || (a&FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
        if (!saved) break;
    }
    return 1;
}
static int metadata(const wchar_t *path,const wchar_t *section,const wchar_t *key) {
    wchar_t value[80]; GetPrivateProfileStringW(section,key,L"",value,80,path); return value[0]!=0;
}
static int own_log(const wchar_t *path,const wchar_t *name) {
    size_t length=wcslen(name);
    int battle=!wcsncmp(name,L"battle-",7);
    if ((!battle && wcsncmp(name,L"plugin-",7)) || length<30 || wcscmp(name+length-6,L".jsonl")) return 0;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if (file==INVALID_HANDLE_VALUE) return 0;
    char text[2049]; DWORD got=0; int ok=ReadFile(file,text,2048,&got,NULL); CloseHandle(file); text[got]=0;
    if(battle) return ok && !strncmp(text,"{\"event\":\"battle_probe_startup\",",sizeof("{\"event\":\"battle_probe_startup\",")-1) &&
        strstr(text,"\"gameplay_modified_by_probe\":false") && (strstr(text,"\"career_statistics\":false") || strstr(text,"\"career_statistics\":true"));
    return ok && (strstr(text,"\"event\":\"startup\"") || strstr(text,"\"event\":\"startup_failed\"")) &&
        (strstr(text,"\"wall_owner_source\"") || strstr(text,"\"hook_entry_unavailable\""));
}
static int add(CleanPlan *p,const wchar_t *name,int kind) {
    wchar_t path[MAX_PATH]; if (!s14_join(path,p->root,name)) return failure(p,L"清理路径过长，未删除文件。");
    DWORD a=GetFileAttributesW(path); if (a==INVALID_FILE_ATTRIBUTES) {
        DWORD e=GetLastError(); return e==ERROR_FILE_NOT_FOUND || e==ERROR_PATH_NOT_FOUND?1:failure(p,L"无法检查清理路径，未删除文件。");
    }
    if (a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) { p->report->preserved++; return 1; }
    int own=0;
    if (kind==1 || kind==2) own=s14_owned_artifact(p->root,path,p->source,kind==2);
    else if (kind==3) own=metadata(path,L"Manager",L"Enabled") || metadata(path,L"Features",L"WallClusterLimit") || metadata(path,L"Rule",L"Mode");
    else if (kind==4) own=metadata(path,L"Install",L"DllSHA256") && metadata(path,L"Install",L"Version");
    else if (kind==5) own=metadata(path,L"Runtime",L"Pid") && metadata(path,L"Runtime",L"Version");
    else if (kind==6) { const wchar_t *leaf=wcsrchr(path,L'\\'); own=own_log(path,leaf?leaf+1:path); }
    else if (kind==7) own=metadata(path,L"SearchReport",L"Version") && metadata(path,L"SearchReport",L"Summary");
    else if (kind==8) own=metadata(path,L"BattleObservation",L"Version") && metadata(path,L"BattleObservation",L"Enabled");
    else if (kind==9) own=s14_stats_checkpoint_owned(path) || s14_special_checkpoint_owned(path) || s14_timeline_checkpoint_owned(path) || s14_troop_checkpoint_owned(path) || s14_ai_checkpoint_owned(path);
    if (!own) { p->report->preserved++; return 1; }
    if (p->count>=CLEAN_MAX) return failure(p,L"待清理文件过多，请先整理日志；未删除文件。");
    if (!plain_path(path) || (a&FILE_ATTRIBUTE_READONLY)) return failure(p,L"待清理文件是只读文件或链接路径，未删除文件。");
    HANDLE file=CreateFileW(path,GENERIC_READ|DELETE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if (file==INVALID_HANDLE_VALUE) return failure(p,L"清理文件被占用或无权限，未删除文件。请关闭游戏和目标目录中的管理器，从游戏目录外的安装包运行彻底卸载。");
    BY_HANDLE_FILE_INFORMATION info; wchar_t actual[MAX_PATH+4],expected[MAX_PATH+4];
    DWORD n=GetFinalPathNameByHandleW(file,actual,MAX_PATH+4,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    swprintf(expected,MAX_PATH+4,L"\\\\?\\%ls",path);
    if (!n || n>=MAX_PATH+4 || _wcsicmp(actual,expected) || !GetFileInformationByHandle(file,&info) ||
        (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)) ||
        info.nNumberOfLinks!=1 || ((kind==1 || kind==2) && !s14_owned_artifact(p->root,path,p->source,kind==2))) {
        CloseHandle(file); return failure(p,L"清理文件路径或内容发生变化，未删除文件。");
    }
    p->files[p->count].handle=file; wcscpy(p->files[p->count++].path,path); return 1;
}
static int scan(CleanPlan *p,const wchar_t *folder,int logs) {
    wchar_t dir[MAX_PATH],pattern[MAX_PATH]; if (!s14_join(dir,p->root,folder) || !s14_join(pattern,dir,L"*")) return failure(p,L"清理路径过长。");
    DWORD a=GetFileAttributesW(dir); if (a==INVALID_FILE_ATTRIBUTES) return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND;
    if (!(a&FILE_ATTRIBUTE_DIRECTORY) || (a&FILE_ATTRIBUTE_REPARSE_POINT)) { p->report->preserved++; return 1; }
    if (!plain_path(dir)) return failure(p,L"管理器目录包含链接，未删除文件。");
    WIN32_FIND_DATAW data; HANDLE search=FindFirstFileW(pattern,&data); if (search==INVALID_HANDLE_VALUE) return failure(p,L"无法列出管理器文件，未删除文件。");
    int ok=1;
    do {
        if (!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
        if (data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) { p->report->preserved++; continue; }
        int kind=0;
        if (logs==2) kind=9;
        else if (logs) kind=6;
        else if (!_wcsicmp(data.cFileName,L"dinput8.previous.dll")) kind=1;
        else if (!_wcsicmp(data.cFileName,L"manager.previous.exe")) kind=2;
        if (!kind) { p->report->preserved++; continue; }
        wchar_t relative[MAX_PATH]; if (!s14_join(relative,folder,data.cFileName) || !add(p,relative,kind)) { ok=0; break; }
    } while (FindNextFileW(search,&data));
    if (ok && GetLastError()!=ERROR_NO_MORE_FILES) ok=failure(p,L"列出管理器文件时发生错误，未删除文件。");
    FindClose(search); return ok;
}
int s14_package_clean(const wchar_t *root,const wchar_t *source,S14CleanupReport *report,wchar_t error[192]) {
    *report=(S14CleanupReport){0}; error[0]=0;
    CleanPlan p={0}; p.source=source; p.report=report; p.error=error;
    DWORD n=GetFullPathNameW(root,MAX_PATH,p.root,NULL); if (!n || n>=MAX_PATH) return failure(&p,L"游戏目录无效或路径过长。");
    size_t len=wcslen(p.root); while (len>3 && p.root[len-1]==L'\\') p.root[--len]=0;
    if (len<=3 || !plain_path(p.root)) return failure(&p,L"请指定普通游戏目录，不能清理盘符根目录或链接目录。");
    if (s14_game_running(p.root)) return failure(&p,L"请保存存档并完全退出游戏后再彻底卸载。");
    report->detected=s14_package_detect(p.root,source);
    if (!(report->detected&(S14_FOUND_DLL|S14_FOUND_MANAGER|S14_FOUND_DATA))) return failure(&p,L"未找到可确认归属的本项目文件，同名未知文件已保留。");
    wchar_t current[MAX_PATH],target[MAX_PATH]; GetModuleFileNameW(NULL,current,MAX_PATH);
    if (s14_join(target,p.root,L"SAN14ModManager.exe") && !_wcsicmp(current,target)) return failure(&p,L"当前管理器位于游戏目录，无法删除自身。请关闭它，从解压到其他目录的安装包运行，再选择游戏目录执行彻底卸载。");
    p.files=calloc(CLEAN_MAX,sizeof(CleanFile)); if (!p.files) return failure(&p,L"无法准备清理清单。");
    int ok=add(&p,L"dinput8.dll",1) && add(&p,L"SAN14ModManager.exe",2) && add(&p,L"SAN14BuildLimit.ini",3);
    wchar_t dir[MAX_PATH]; s14_join(dir,p.root,L"SAN14ModManager"); DWORD a=GetFileAttributesW(dir);
    if (ok && a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT)) {
        if (!plain_path(dir)) ok=failure(&p,L"管理器目录包含链接，未删除文件。");
        else ok=add(&p,L"SAN14ModManager\\runtime.ini",5) && add(&p,L"SAN14ModManager\\search-results.ini",7) && add(&p,L"SAN14ModManager\\battle-observation-status.ini",8) &&
            add(&p,L"SAN14ModManager\\install.dll.tmp",1) && add(&p,L"SAN14ModManager\\manager.exe.tmp",2) && add(&p,L"SAN14ModManager\\receipt.ini.tmp",4) &&
            scan(&p,L"SAN14ModManager\\logs",1) && scan(&p,L"SAN14ModManager\\backups",0) && scan(&p,L"SAN14ModManager\\career\\checkpoints",2) && scan(&p,L"SAN14ModManager\\troops",2) && scan(&p,L"SAN14ModManager\\ai-affixes",2) && add(&p,L"SAN14ModManager\\installation.ini",4);
        // Unknown entries in the application directory are preserved, too.
        wchar_t pattern[MAX_PATH]; s14_join(pattern,dir,L"*"); WIN32_FIND_DATAW data; HANDLE search=FindFirstFileW(pattern,&data);
        if (search!=INVALID_HANDLE_VALUE) { do {
            if (!wcscmp(data.cFileName,L".") || !wcscmp(data.cFileName,L"..")) continue;
            const wchar_t *known[]={L"runtime.ini",L"installation.ini",L"install.dll.tmp",L"manager.exe.tmp",L"receipt.ini.tmp",L"logs",L"backups",L"search-results.ini",L"battle-observation-status.ini",L"career",L"troops",L"ai-affixes"}; int recognized=0;
            for (int i=0;i<12;i++) if (!_wcsicmp(data.cFileName,known[i])) recognized=1;
            if (!recognized) report->preserved++;
        } while (FindNextFileW(search,&data)); FindClose(search); }
        else if (ok) ok=failure(&p,L"无法检查管理器目录，未删除文件。");
    } else if (a!=INVALID_FILE_ATTRIBUTES) report->preserved++;
    // Open all files for deletion before changing any file. A locked EXE or
    // read-only file aborts the entire preflight without removing the DLL.
    if (ok && !p.count) ok=failure(&p,L"没有可清理的本项目普通文件。");
    int marked=0;
    if (ok) for (int i=0;i<p.count;i++) {
        FILE_DISPOSITION_INFO disposition={TRUE};
        if (!SetFileInformationByHandle(p.files[i].handle,FileDispositionInfo,&disposition,sizeof(disposition))) {
            ok=failure(&p,L"部分文件无法清理。已删除数量见结果，请关闭占用程序后重试。"); break;
        }
        marked++;
    }
    if (!ok && marked) for (int i=0;i<marked;i++) {
        FILE_DISPOSITION_INFO disposition={FALSE};
        if (!SetFileInformationByHandle(p.files[i].handle,FileDispositionInfo,&disposition,sizeof(disposition))) report->files++;
    }
    for (int i=0;i<p.count;i++) if (p.files[i].handle) CloseHandle(p.files[i].handle);
    if (ok) report->files=p.count;
    free(p.files);
    if (ok) {
        const wchar_t *folders[]={L"SAN14ModManager\\ai-affixes",L"SAN14ModManager\\troops",L"SAN14ModManager\\career\\checkpoints",L"SAN14ModManager\\career",L"SAN14ModManager\\logs",L"SAN14ModManager\\backups",L"SAN14ModManager"};
        for (int i=0;i<7;i++) if (s14_join(dir,p.root,folders[i]) && plain_path(dir)) {
            if (RemoveDirectoryW(dir)) report->directories++;
            else if (GetLastError()!=ERROR_DIR_NOT_EMPTY) ok=failure(&p,L"本项目文件已清理，但空目录无法删除，请检查权限后重试。");
        }
    }
    return ok;
}
