#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "package.h"
#include "features.h"
#ifdef S14_INSTALLER
#include "payload.h"
#endif
static const char expected_game_hash[]="e6ae68925c266a19b05641913e60bf7d97d5eb4754901c3e82a5362d05ff7372";
int s14_join(wchar_t *out,const wchar_t *root,const wchar_t *name) {
    if (!root[0] || wcslen(root)+wcslen(name)+2>=MAX_PATH) return 0;
    swprintf(out,MAX_PATH,L"%ls\\%ls",root,name); return 1;
}
static int ordinary_file(const wchar_t *path) {
    DWORD a=GetFileAttributesW(path); return a!=INVALID_FILE_ATTRIBUTES && !(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
static int ordinary_directory(const wchar_t *path) {
    DWORD a=GetFileAttributesW(path); return a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT);
}
int s14_hash_file(const wchar_t *path,char out[65]) {
    if (!ordinary_file(path)) return 0;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) return 0;
    BCRYPT_ALG_HANDLE a=NULL; BCRYPT_HASH_HANDLE hash=NULL; unsigned char digest[32],buffer[65536]; DWORD got; int ok=0;
    if (BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)<0 || BCryptCreateHash(a,&hash,NULL,0,NULL,0,0)<0) goto done;
    for (;;) { if (!ReadFile(file,buffer,sizeof(buffer),&got,NULL)) goto done; if (!got) break; if (BCryptHashData(hash,buffer,got,0)<0) goto done; }
    if (BCryptFinishHash(hash,digest,32,0)<0) goto done;
    for (int i=0;i<32;i++) snprintf(out+i*2,3,"%02x",digest[i]); ok=1;
done:
    if (hash) BCryptDestroyHash(hash); if (a) BCryptCloseAlgorithmProvider(a,0); CloseHandle(file); return ok;
}
int s14_game_compatible(const wchar_t *root) {
    wchar_t path[MAX_PATH]; char hash[65]; return s14_join(path,root,L"SAN14PK_SC.exe") && s14_hash_file(path,hash) && !strcmp(hash,expected_game_hash);
}
int s14_game_running(const wchar_t *root) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if (snapshot==INVALID_HANDLE_VALUE) return 1;
    PROCESSENTRY32W process={0}; process.dwSize=sizeof(process); int running=0;
    if (Process32FirstW(snapshot,&process)) do {
        if (_wcsicmp(process.szExeFile,L"SAN14PK_SC.exe")) continue;
        if (!root) { running=1; break; }
        HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,process.th32ProcessID);
        if (!handle) { running=1; break; }
        wchar_t path[MAX_PATH],expected[MAX_PATH]; DWORD size=MAX_PATH;
        int known=QueryFullProcessImageNameW(handle,0,path,&size); CloseHandle(handle);
        if (!known || !s14_join(expected,root,L"SAN14PK_SC.exe") || !_wcsicmp(path,expected)) { running=1; break; }
    } while (Process32NextW(snapshot,&process));
    CloseHandle(snapshot); return running;
}
static int receipt_path(wchar_t *path,const wchar_t *root) { return s14_join(path,root,L"SAN14ModManager\\installation.ini"); }
int s14_owned_install(const wchar_t *root) {
    wchar_t dll[MAX_PATH],receipt[MAX_PATH],expected[80]; char actual[65];
    if (!s14_join(dll,root,L"dinput8.dll") || !receipt_path(receipt,root) || !s14_hash_file(dll,actual)) return 0;
    // Migrate only exact, previously verified builds of this mod.
    if (!ordinary_file(receipt)) return !strcmp(actual,"d22049371c7fb8eaa036101b526e0a6135ec61a017556e2de32926d9bb47e2fe") ||
        !strcmp(actual,"aa9145a8590e1bd8d94fc852ccefaf14d1ecf7075f250ac462910d8396cd2571");
    GetPrivateProfileStringW(L"Install",L"DllSHA256",L"",expected,80,receipt);
    char expected_utf8[80]; if (!WideCharToMultiByte(CP_UTF8,0,expected,-1,expected_utf8,80,NULL,NULL)) return 0;
    return !strcmp(actual,expected_utf8);
}
static int directory(const wchar_t *root,const wchar_t *name) {
    wchar_t path[MAX_PATH]; if (!s14_join(path,root,name)) return 0;
    return CreateDirectoryW(path,NULL) || (GetLastError()==ERROR_ALREADY_EXISTS && ordinary_directory(path));
}
static int fail(wchar_t error[192],const wchar_t *text) { wcsncpy(error,text,191); error[191]=0; return 0; }

#ifdef S14_INSTALLER
int s14_package_install(const wchar_t *root,const wchar_t *manager_source,wchar_t error[192]) {
    wchar_t dll[MAX_PATH],config[MAX_PATH],receipt[MAX_PATH],stage[MAX_PATH],backup[MAX_PATH],app[MAX_PATH],app_stage[MAX_PATH],receipt_stage[MAX_PATH],app_backup[MAX_PATH];
    if (!ordinary_directory(root) || !s14_join(dll,root,L"dinput8.dll") || !s14_join(config,root,L"SAN14BuildLimit.ini") ||
        !receipt_path(receipt,root) || !s14_join(stage,root,L"SAN14ModManager\\install.dll.tmp") ||
        !s14_join(backup,root,L"SAN14ModManager\\backups\\dinput8.previous.dll") ||
        !s14_join(app,root,L"SAN14ModManager.exe") || !s14_join(app_stage,root,L"SAN14ModManager\\manager.exe.tmp") ||
        !s14_join(receipt_stage,root,L"SAN14ModManager\\receipt.ini.tmp") || !s14_join(app_backup,root,L"SAN14ModManager\\backups\\manager.previous.exe")) return fail(error,L"目录不可用或路径过长，请选择普通游戏目录。");
    if (s14_game_running(root)) return fail(error,L"请先保存存档并完全退出游戏，再安装或更新。");
    if (!s14_game_compatible(root)) return fail(error,L"游戏版本不匹配，本版本仅支持已经核对的 SAN14PK_SC.exe。");
    int exists=GetFileAttributesW(dll)!=INVALID_FILE_ATTRIBUTES;
    if (exists && !s14_owned_install(root)) return fail(error,L"已有其他或来源未知的 dinput8.dll，未覆盖。需要先确认 MOD 兼容方式。");
    if (GetFileAttributesW(receipt)!=INVALID_FILE_ATTRIBUTES && !ordinary_file(receipt)) return fail(error,L"安装凭据路径不是普通文件，未更改。");
    if (GetFileAttributesW(config)!=INVALID_FILE_ATTRIBUTES && !ordinary_file(config)) return fail(error,L"配置路径不是普通文件，未更改。");
    if (!directory(root,L"SAN14ModManager") || !directory(root,L"SAN14ModManager\\backups") || !directory(root,L"SAN14ModManager\\logs")) return fail(error,L"无法创建管理器目录，请检查写入权限。");
    if (GetFileAttributesW(stage)!=INVALID_FILE_ATTRIBUTES || GetFileAttributesW(app_stage)!=INVALID_FILE_ATTRIBUTES || GetFileAttributesW(receipt_stage)!=INVALID_FILE_ATTRIBUTES) return fail(error,L"上次安装的暂存文件仍存在，请先检查后再重试。");
    if (GetFileAttributesW(backup)!=INVALID_FILE_ATTRIBUTES && !ordinary_file(backup)) return fail(error,L"备份路径不是普通文件，未更改。");
    if (exists && !CopyFileW(dll,backup,FALSE)) return fail(error,L"旧插件备份失败，未替换插件。");
    HANDLE file=CreateFileW(stage,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) return fail(error,L"无法写入插件暂存文件。");
    DWORD written=0; int valid=WriteFile(file,s14_payload,(DWORD)sizeof(s14_payload),&written,NULL) && written==sizeof(s14_payload) && FlushFileBuffers(file);
    CloseHandle(file); char hash[65]; valid=valid && s14_hash_file(stage,hash) && !strcmp(hash,s14_payload_hash);
    if (!valid) { DeleteFileW(stage); return fail(error,L"插件写入或哈希校验失败，未替换现有插件。"); }
    // The manager may run from the destination on subsequent reinstalls.
    int copy_manager=_wcsicmp(manager_source,app)!=0;
    int existing_manager=GetFileAttributesW(app)!=INVALID_FILE_ATTRIBUTES;
    if (copy_manager && existing_manager) {
        wchar_t app_expected[80]; char app_hash[65],expected_ascii[80];
        GetPrivateProfileStringW(L"Install",L"ManagerSHA256",L"",app_expected,80,receipt);
        WideCharToMultiByte(CP_UTF8,0,app_expected,-1,expected_ascii,80,NULL,NULL);
        if (!s14_hash_file(app,app_hash) || strcmp(app_hash,expected_ascii)) { DeleteFileW(stage); return fail(error,L"同名管理器文件来源未知，未覆盖。"); }
        if ((GetFileAttributesW(app_backup)!=INVALID_FILE_ATTRIBUTES && !ordinary_file(app_backup)) || !CopyFileW(app,app_backup,FALSE)) { DeleteFileW(stage); return fail(error,L"旧管理器备份失败，未替换。"); }
    }
    if (copy_manager && !CopyFileW(manager_source,app_stage,TRUE)) {
        DeleteFileW(stage); return fail(error,L"管理器复制失败，请检查目录权限。");
    }
    unsigned int flags=s14_config_read(config);
    if (!s14_config_set(config,S14_MASTER,!!(flags&S14_MASTER))) { DeleteFileW(stage); if (copy_manager) DeleteFileW(app_stage); return fail(error,L"配置写入失败，未替换插件。"); }
    HANDLE receipt_file=CreateFileW(receipt_stage,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (receipt_file==INVALID_HANDLE_VALUE) { DeleteFileW(stage); if (copy_manager) DeleteFileW(app_stage); return fail(error,L"无法创建安装凭据暂存文件，未替换插件。"); }
    CloseHandle(receipt_file);
    wchar_t hash_wide[65]; MultiByteToWideChar(CP_UTF8,0,s14_payload_hash,-1,hash_wide,65);
    int receipt_ok=WritePrivateProfileStringW(L"Install",L"DllSHA256",hash_wide,receipt_stage) && WritePrivateProfileStringW(L"Install",L"Version",S14_MANAGER_VERSION,receipt_stage);
    char manager_hash[65];
    if (!s14_hash_file(copy_manager?app_stage:app,manager_hash)) receipt_ok=0;
    else { MultiByteToWideChar(CP_UTF8,0,manager_hash,-1,hash_wide,65); receipt_ok=receipt_ok && WritePrivateProfileStringW(L"Install",L"ManagerSHA256",hash_wide,receipt_stage); }
    WritePrivateProfileStringW(NULL,NULL,NULL,receipt_stage);
    if (!receipt_ok) { DeleteFileW(stage); DeleteFileW(receipt_stage); if (copy_manager) DeleteFileW(app_stage); return fail(error,L"安装凭据保存失败，未替换插件。"); }
    if (!MoveFileExW(stage,dll,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { DeleteFileW(stage); DeleteFileW(receipt_stage); if (copy_manager) DeleteFileW(app_stage); return fail(error,L"插件文件仍被占用或无法替换，请完全退出游戏。"); }
    if (copy_manager && !MoveFileExW(app_stage,app,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        int restored=exists?CopyFileW(backup,dll,FALSE):DeleteFileW(dll);
        DeleteFileW(app_stage); DeleteFileW(receipt_stage);
        return fail(error,restored?L"管理器仍被占用，插件已恢复。请关闭其他管理器后重试。":L"管理器替换失败，插件回退也失败，请保留 backups 目录检查。");
    }
    if (!MoveFileExW(receipt_stage,receipt,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        int restored=exists?CopyFileW(backup,dll,FALSE):DeleteFileW(dll);
        if (copy_manager) restored=(existing_manager?CopyFileW(app_backup,app,FALSE):DeleteFileW(app)) && restored;
        DeleteFileW(receipt_stage);
        return fail(error,restored?L"安装凭据替换失败，插件和管理器已回退。":L"安装凭据替换失败且回退不完整，请保留 backups 目录检查。");
    }
    WritePrivateProfileStringW(NULL,NULL,NULL,receipt);
    return 1;
}
#else
int s14_package_install(const wchar_t *root,const wchar_t *source,wchar_t error[192]) { (void)root; (void)source; return fail(error,L"请通过独立管理器安装。"); }
#endif

int s14_package_remove(const wchar_t *root,wchar_t error[192]) {
    if (s14_game_running(root)) return fail(error,L"请完全退出游戏后再移除插件。");
    if (!s14_owned_install(root)) return fail(error,L"插件文件与安装凭据不一致，未删除未知文件。");
    wchar_t path[MAX_PATH]; if (!s14_join(path,root,L"dinput8.dll") || !DeleteFileW(path)) return fail(error,L"插件移除失败，请检查文件占用及目录权限。");
    // Retain manager, configuration, ownership receipt and backups for reinstall.
    return 1;
}
void s14_publish_runtime(const wchar_t *root,unsigned int desired,unsigned int effective,int ready,int fault) {
    wchar_t path[MAX_PATH],text[64]; if (!s14_join(path,root,L"SAN14ModManager\\runtime.ini")) return;
    swprintf(text,64,L"%lu",(unsigned long)GetCurrentProcessId()); WritePrivateProfileStringW(L"Runtime",L"Pid",text,path);
    swprintf(text,64,L"%u",desired); WritePrivateProfileStringW(L"Runtime",L"Requested",text,path);
    swprintf(text,64,L"%u",effective); WritePrivateProfileStringW(L"Runtime",L"Effective",text,path);
    swprintf(text,64,L"%d",ready); WritePrivateProfileStringW(L"Runtime",L"Ready",text,path);
    swprintf(text,64,L"%d",fault); WritePrivateProfileStringW(L"Runtime",L"Fault",text,path);
    WritePrivateProfileStringW(L"Runtime",L"Version",S14_MANAGER_VERSION,path);
    // Heartbeat is written last so readers see a completed state update.
    swprintf(text,64,L"%llu",(unsigned long long)GetTickCount64()); WritePrivateProfileStringW(L"Runtime",L"Heartbeat",text,path);
}
int s14_read_runtime(const wchar_t *root,unsigned int *effective,int *fault) {
    wchar_t path[MAX_PATH],text[64],actual[MAX_PATH],expected[MAX_PATH]; if (!s14_join(path,root,L"SAN14ModManager\\runtime.ini")) return 0;
    GetPrivateProfileStringW(L"Runtime",L"Heartbeat",L"0",text,64,path); ULONGLONG heartbeat=_wcstoui64(text,NULL,10),now=GetTickCount64();
    if (!heartbeat || heartbeat>now || now-heartbeat>4000) return 0;
    DWORD pid=GetPrivateProfileIntW(L"Runtime",L"Pid",0,path); HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
    if (!process) return 0; DWORD size=MAX_PATH;
    int valid=WaitForSingleObject(process,0)==WAIT_TIMEOUT && QueryFullProcessImageNameW(process,0,actual,&size) && s14_join(expected,root,L"SAN14PK_SC.exe") && !_wcsicmp(actual,expected);
    CloseHandle(process); if (!valid) return 0;
    *effective=GetPrivateProfileIntW(L"Runtime",L"Effective",0,path); *fault=GetPrivateProfileIntW(L"Runtime",L"Fault",0,path);
    return 1;
}
