#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "package.h"
#include "battle_stats.h"
#include "features.h"
#ifdef S14_INSTALLER
#include "payload.h"
#endif
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
int s14_game_available(const wchar_t *root) {
    wchar_t path[MAX_PATH]; return s14_join(path,root,L"SAN14PK_SC.exe") && ordinary_file(path);
}
const wchar_t *s14_fault_message(int fault) {
    switch (fault) {
    case S14_FAULT_ENTRY: return L"建造接入点未匹配 · 扩展功能已停用，管理器仍可使用";
    case S14_FAULT_HOOK: return L"建造接入失败 · 扩展功能已停用，请查看日志";
    case S14_FAULT_MAP: return L"地图数据读取异常 · 扩展功能已停用，请重启后检查";
    default: return L"扩展功能已保护性停用 · 请查看日志";
    }
}
int s14_game_running(const wchar_t *root) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if (snapshot==INVALID_HANDLE_VALUE) return 1;
    PROCESSENTRY32W process={0}; process.dwSize=sizeof(process); int running=0;
    if (Process32FirstW(snapshot,&process)) do {
        if (_wcsicmp(process.szExeFile,L"SAN14PK_SC.exe")) continue;
        // Crash-report clones and terminated processes may remain enumerable
        // with no threads; they cannot execute. File replacement still checks
        // sharing and rolls back if an existing file remains locked.
        if (!process.cntThreads) continue;
        HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,process.th32ProcessID);
        if (!handle) {
            // A process may disappear between the snapshot and this lookup.
            if (GetLastError()==ERROR_INVALID_PARAMETER) continue;
            running=1; break;
        }
        DWORD exit_code;
        if (GetExitCodeProcess(handle,&exit_code) && exit_code!=STILL_ACTIVE) { CloseHandle(handle); continue; }
        if (!root) { CloseHandle(handle); running=1; break; }
        wchar_t path[MAX_PATH],expected[MAX_PATH]; DWORD size=MAX_PATH;
        int known=QueryFullProcessImageNameW(handle,0,path,&size); CloseHandle(handle);
        if (!known || !s14_join(expected,root,L"SAN14PK_SC.exe") || !_wcsicmp(path,expected)) { running=1; break; }
    } while (Process32NextW(snapshot,&process));
    CloseHandle(snapshot); return running;
}
static int receipt_path(wchar_t *path,const wchar_t *root) { return s14_join(path,root,L"SAN14ModManager\\installation.ini"); }
static int directory(const wchar_t *root,const wchar_t *name) {
    wchar_t path[MAX_PATH]; if (!s14_join(path,root,name)) return 0;
    return CreateDirectoryW(path,NULL) || (GetLastError()==ERROR_ALREADY_EXISTS && ordinary_directory(path));
}
static int fail(wchar_t error[192],const wchar_t *text) { wcsncpy(error,text,191); error[191]=0; return 0; }

static int known_previous_manager(const char *hash) {
    static const char *known[]={
        "a12bde5634abcb4f3841d76852da523968f48fef73beab782c744a382c67764a", // local 0.5.2
        "4063432b33717b6d9ce4cfecd1d23e096926ab607e1187a7af43baa3e7bdf0ba", // local 0.5.1
        "deab45bf5bbb0538f5a4ed9492e50ad0676347189b7bbb674dcd8f6cb472f8f2", // local 0.5.0
        "1d32fff1c27f155b46494cd1f762474ed32eec6a613a1b88614691b7ff749030", // public 0.2.0
        "7be3eb8f46556e4603702dbf9aedf2443ff84f56c5cfc794aedc793e8824c692", // source 0.2.0
        "570c30fd02bc3ddc647889770c6bece79280fd6ed65a145b84e4c920721a0829", // public 0.2.1
        "2b565bb0ac8b2fbd42b4c3c0e29a06b48ba654704efaed7dad1549bedfef4cab", // public 0.2.2
        "ec2dbdaac23d42578a229877146787259afc7a72c1248bc22aa14f9f46097dad", // local 0.3.0
        "1bacedb4ec1632d6ded1cc2ec2016fd416d53435290043d004f79de4108bf26d", // local 0.4.0
        "4d94a1e3c7a18e4a08ff139d2bba4975630214a23e6a7c4574cbb3d29a2eb6f0", // local 0.4.1
        "1c3af86e9bcc940642f3f322ae865581355c936709bc8b6d17e41f8354df787f", // local 0.4.2
        "23f4186fad6c651640e9fbc208a387e8d00051ad24813fd62659fa81cd218de4", // local 0.5.3
        "4c962ddce7353a51f23e5d207763b163ec78105d5c631f489dfd00d7ff5f0df0", // local 0.6.0
        "958518645460caa9735a60c96c9de8508c6bed8f005f7dd577cb6f2647712a88", // local 0.6.1
        "c1d2dae333edfbe222908519085c43573756cb16f412db857bd74e5920bc98f9", // local 0.6.2
        "522230d684952cdbfb2db0864796363d94c690b8d042d417bcc0e42bae3f0f1f", // local 0.6.3
        "d1cdb01676e600797701917daca041817c1ac0fc16bf3ef2afb92859cd80c4ad", // local 0.7.0
        "0c9b5bddfec9f59e32ee04e8f6bec14bc3092d4a58f16a3c96ac7c1fd9371451" // accepted local 0.7.1
    };
    for (size_t i=0;i<sizeof(known)/sizeof(known[0]);i++) if (!strcmp(hash,known[i])) return 1;
    return 0;
}
static int known_previous_dll(const char *hash) {
    static const char *known[]={
        "6d6964e52255be65aa3b86859dd10221f9a6ec4d1e8ecdd6dbe4cfe16e14df30", // accepted local 0.7.1
        "3383059ff8e8a90550bf958ff48b5015d05613cb06e5b7152fb120f6072b848c", // local 0.5.2
        "8763d16be17adfbc06a8815fe8d0f9e4c1890dd92dddaf712abd0c9c0b221033", // local 0.5.1
        "31c5d9e20a069de6b29ce6e1a967d7a2577d544a9b0d2143424d2e72eef1b1cf", // local 0.5.0
        "343b59054103ca7a7024afb47a9b13f38905140f4183dce0b9ce1e6609ad5208", // local 0.4.2
        "6edc9c72178d0bd0e8b95d092fb0f843a392158b3f00319c51ad7935d76cde44", // local 0.4.1
        "25aa90932a53dafe93ede7f7c470a4ae276d4f782e76983405cf170ecbba1624", // local 0.4.0
        "22b1afec1a454f0a323bd84242f54b9034d294fc14dc2bf0ec1c27afdb09f899", // local 0.3.0
        "d22049371c7fb8eaa036101b526e0a6135ec61a017556e2de32926d9bb47e2fe",
        "aa9145a8590e1bd8d94fc852ccefaf14d1ecf7075f250ac462910d8396cd2571",
        "2b6996215274d3d465bc1cc08478c9df16ab80717c556b5822ef807e9b0a028a",
        "f97fe520092b64976480b858cfe5a97a8368a51b7768130d4ca7a00f915dfd84",
        "25043bbaef4cda54eaaf47fe9a63ef47ca81ca224db1c6e8fd01be1915620a77",
        "5cfceeb8457829240c9e06740784046b5d4d47170868cd0a6469f58c0260634d"
    };
    for (size_t i=0;i<sizeof(known)/sizeof(known[0]);i++) if (!strcmp(hash,known[i])) return 1;
    return 0;
}
int s14_owned_artifact(const wchar_t *root,const wchar_t *path,const wchar_t *source,int manager) {
    char hash[65],expected_ascii[80]={0}; wchar_t receipt[MAX_PATH],expected[80];
    if (!s14_hash_file(path,hash)) return 0;
    if (manager?known_previous_manager(hash):known_previous_dll(hash)) return 1;
    if (manager && source) { char current[65]; if (s14_hash_file(source,current) && !strcmp(hash,current)) return 1; }
#ifdef S14_INSTALLER
    if (!manager && !strcmp(hash,s14_payload_hash)) return 1;
#endif
    if (!receipt_path(receipt,root) || !ordinary_file(receipt)) return 0;
    GetPrivateProfileStringW(L"Install",manager?L"ManagerSHA256":L"DllSHA256",L"",expected,80,receipt);
    WideCharToMultiByte(CP_UTF8,0,expected,-1,expected_ascii,80,NULL,NULL);
    return !strcmp(hash,expected_ascii);
}
int s14_owned_install(const wchar_t *root) {
    wchar_t dll[MAX_PATH]; return s14_join(dll,root,L"dinput8.dll") && s14_owned_artifact(root,dll,NULL,0);
}
unsigned int s14_package_detect(const wchar_t *root,const wchar_t *source) {
    wchar_t path[MAX_PATH]; unsigned int found=0;
    if (!ordinary_directory(root)) return 0;
    if (s14_join(path,root,L"dinput8.dll") && GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)
        found|=s14_owned_artifact(root,path,source,0)?S14_FOUND_DLL:S14_FOUND_UNKNOWN;
    if (s14_join(path,root,L"SAN14ModManager.exe") && GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)
        found|=s14_owned_artifact(root,path,source,1)?S14_FOUND_MANAGER:S14_FOUND_UNKNOWN;
    if (s14_join(path,root,L"SAN14ModManager\\installation.ini") && ordinary_file(path)) {
        wchar_t dll[80],app[80],version[32];
        GetPrivateProfileStringW(L"Install",L"DllSHA256",L"",dll,80,path);
        GetPrivateProfileStringW(L"Install",L"ManagerSHA256",L"",app,80,path);
        GetPrivateProfileStringW(L"Install",L"Version",L"",version,32,path);
        if (version[0] && wcslen(dll)==64 && wcslen(app)==64 && wcsspn(dll,L"0123456789abcdef")==64 && wcsspn(app,L"0123456789abcdef")==64) found|=S14_FOUND_DATA;
    }
    if (found&(S14_FOUND_DLL|S14_FOUND_MANAGER)) found|=S14_FOUND_DATA;
    if (s14_join(path,root,L"SAN14BuildLimit.ini") && ordinary_file(path)) {
        wchar_t enabled[16],mode[16];
        GetPrivateProfileStringW(L"Manager",L"Enabled",L"",enabled,16,path);
        GetPrivateProfileStringW(L"Rule",L"Mode",L"",mode,16,path);
        if ((!wcscmp(enabled,L"0") || !wcscmp(enabled,L"1")) && (!wcscmp(mode,L"0") || !wcscmp(mode,L"1") || !wcscmp(mode,L"2"))) found|=S14_FOUND_DATA;
    }
    if (s14_join(path,root,L"SAN14ModManager\\search-results.ini") && ordinary_file(path)) {
        wchar_t version[16],summary[32];
        GetPrivateProfileStringW(L"SearchReport",L"Version",L"",version,16,path);
        GetPrivateProfileStringW(L"SearchReport",L"Summary",L"",summary,32,path);
        if (!wcscmp(version,L"1") && summary[0]) found|=S14_FOUND_DATA;
    }
    if (s14_join(path,root,L"SAN14ModManager\\battle-observation-status.ini") && ordinary_file(path)) {
        wchar_t version[32],enabled[16];
        GetPrivateProfileStringW(L"BattleObservation",L"Version",L"",version,32,path);
        GetPrivateProfileStringW(L"BattleObservation",L"Enabled",L"",enabled,16,path);
        if(version[0] && (!wcscmp(enabled,L"0") || !wcscmp(enabled,L"1"))) found|=S14_FOUND_DATA;
    }
    wchar_t folder[MAX_PATH],pattern[MAX_PATH];
    if(s14_join(folder,root,L"SAN14ModManager\\career\\checkpoints") && ordinary_directory(folder) && s14_join(pattern,folder,L"*.s14career")) {
        WIN32_FIND_DATAW data;HANDLE find=FindFirstFileW(pattern,&data);int examined=0;
        if(find!=INVALID_HANDLE_VALUE) {do {if(++examined>2048) break;if(s14_join(path,folder,data.cFileName) && s14_stats_checkpoint_owned(path)) {found|=S14_FOUND_DATA;break;}} while(FindNextFileW(find,&data));FindClose(find);}
    }
    return found;
}
#ifdef S14_INSTALLER
int s14_package_install(const wchar_t *root,const wchar_t *manager_source,wchar_t error[192]) {
    wchar_t dll[MAX_PATH],config[MAX_PATH],receipt[MAX_PATH],stage[MAX_PATH],backup[MAX_PATH],app[MAX_PATH],app_stage[MAX_PATH],receipt_stage[MAX_PATH],app_backup[MAX_PATH];
    if (!ordinary_directory(root) || !s14_join(dll,root,L"dinput8.dll") || !s14_join(config,root,L"SAN14BuildLimit.ini") ||
        !receipt_path(receipt,root) || !s14_join(stage,root,L"SAN14ModManager\\install.dll.tmp") ||
        !s14_join(backup,root,L"SAN14ModManager\\backups\\dinput8.previous.dll") ||
        !s14_join(app,root,L"SAN14ModManager.exe") || !s14_join(app_stage,root,L"SAN14ModManager\\manager.exe.tmp") ||
        !s14_join(receipt_stage,root,L"SAN14ModManager\\receipt.ini.tmp") || !s14_join(app_backup,root,L"SAN14ModManager\\backups\\manager.previous.exe")) return fail(error,L"目录不可用或路径过长，请选择普通游戏目录。");
    if (s14_game_running(root)) return fail(error,L"请先保存存档并完全退出游戏，再安装或更新。");
    if (!s14_game_available(root)) return fail(error,L"当前目录未找到 SAN14PK_SC.exe，请选择游戏文件所在目录。");
    int exists=GetFileAttributesW(dll)!=INVALID_FILE_ATTRIBUTES;
    if (exists && !s14_owned_install(root)) return fail(error,L"已有其他或来源未知的 dinput8.dll，未覆盖。需要先确认 MOD 兼容方式。");
    if (GetFileAttributesW(receipt)!=INVALID_FILE_ATTRIBUTES && !ordinary_file(receipt)) return fail(error,L"安装凭据路径不是普通文件，未更改。");
    if (GetFileAttributesW(config)!=INVALID_FILE_ATTRIBUTES && !ordinary_file(config)) return fail(error,L"配置路径不是普通文件，未更改。");
    char source_hash[65],existing_hash[65];
    if (!s14_hash_file(manager_source,source_hash)) return fail(error,L"无法读取当前安装器文件，请重新解压安装包。");
    int existing_manager=GetFileAttributesW(app)!=INVALID_FILE_ATTRIBUTES;
    int copy_manager=1;
    if (existing_manager) {
        if (!s14_hash_file(app,existing_hash)) return fail(error,L"同名管理器文件无法读取或不是普通文件，未覆盖。");
        // A manually copied installer needs no receipt yet. Identical bytes
        // also cover alternate path spellings and a running destination EXE.
        copy_manager=strcmp(existing_hash,source_hash)!=0;
        if (copy_manager) {
            wchar_t expected[80]; char expected_ascii[80]={0};
            GetPrivateProfileStringW(L"Install",L"ManagerSHA256",L"",expected,80,receipt);
            WideCharToMultiByte(CP_UTF8,0,expected,-1,expected_ascii,80,NULL,NULL);
            if (strcmp(existing_hash,expected_ascii) && !known_previous_manager(existing_hash))
                return fail(error,L"同名管理器不是本项目已识别文件，未覆盖。请检查或移动该文件后重试。");
        }
    }
    if (!directory(root,L"SAN14ModManager") || !directory(root,L"SAN14ModManager\\backups") || !directory(root,L"SAN14ModManager\\logs")) return fail(error,L"无法创建管理器目录，请检查写入权限。");
    if (GetFileAttributesW(stage)!=INVALID_FILE_ATTRIBUTES || GetFileAttributesW(app_stage)!=INVALID_FILE_ATTRIBUTES || GetFileAttributesW(receipt_stage)!=INVALID_FILE_ATTRIBUTES) return fail(error,L"上次安装的暂存文件仍存在，请先检查后再重试。");
    if (GetFileAttributesW(backup)!=INVALID_FILE_ATTRIBUTES && !ordinary_file(backup)) return fail(error,L"备份路径不是普通文件，未更改。");
    if (exists && !CopyFileW(dll,backup,FALSE)) return fail(error,L"旧插件备份失败，未替换插件。");
    HANDLE file=CreateFileW(stage,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) return fail(error,L"无法写入插件暂存文件。");
    DWORD written=0; int valid=WriteFile(file,s14_payload,(DWORD)sizeof(s14_payload),&written,NULL) && written==sizeof(s14_payload) && FlushFileBuffers(file);
    CloseHandle(file); char hash[65]; valid=valid && s14_hash_file(stage,hash) && !strcmp(hash,s14_payload_hash);
    if (!valid) { DeleteFileW(stage); return fail(error,L"插件写入或哈希校验失败，未替换现有插件。"); }
    if (copy_manager && existing_manager) {
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
    if (!s14_hash_file(copy_manager?app_stage:app,manager_hash) || strcmp(manager_hash,source_hash)) receipt_ok=0;
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
