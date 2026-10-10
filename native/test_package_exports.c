#include <windows.h>
#include "package.h"
#include "battle_stats.h"
#include "special_stats.h"
#include "battle_timeline.h"
#include "troop_store.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
__declspec(dllexport) int S14TestTroopSave(const wchar_t *root){
    wchar_t dir[MAX_PATH],path[MAX_PATH];swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);CreateDirectoryW(dir,NULL);swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\troops",root);CreateDirectoryW(dir,NULL);swprintf(path,MAX_PATH,L"%ls\\test.s14troops",dir);
    S14TroopCheckpoint *c=calloc(1,sizeof(*c));if(!c)return 0;memcpy(c->magic,"S14TROOPS.v1",12);c->version=1;c->bytes=sizeof(*c);c->hash[0]=8;
    c->state.units[1]=(S14TroopBinding){.active=1,.leader=254,.carrier=1,.serial=1,.revision=1};strcpy(c->state.units[1].id,"san14.xianzhen");c->crc=s14_troop_crc(c->hash,sizeof(*c)-offsetof(S14TroopCheckpoint,hash));
    HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);DWORD n=0;int ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,c,sizeof(*c),&n,NULL) && n==sizeof(*c);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);free(c);return ok;
}
__declspec(dllexport) int S14TestTimelineSave(const wchar_t *root){unsigned char hash[32]={5};s14_timeline_root(root);s14_timeline_load(1,100,1,NULL,0,1);return s14_timeline_save(1,hash,1);}
__declspec(dllexport) int S14TestSpecialSave(const wchar_t *root){unsigned char hash[32]={4};s14_special_root(root);s14_special_load(1,100,1,hash,0,1);return s14_special_save(1,hash,1);}
__declspec(dllexport) int S14TestStatsSave(const wchar_t *root) {unsigned char hash[32]={1};s14_stats_root(root);s14_stats_load_end(1,100,1,hash,0,1);return s14_stats_save(1,hash,1);}
__declspec(dllexport) int S14TestInstall(const wchar_t *root,const wchar_t *source,wchar_t *error) { return s14_package_install(root,source,error); }
__declspec(dllexport) int S14TestRemove(const wchar_t *root,wchar_t *error) { return s14_package_remove(root,error); }
__declspec(dllexport) int S14TestOwned(const wchar_t *root) { return s14_owned_install(root); }
__declspec(dllexport) int S14TestRunning(const wchar_t *root) { return s14_game_running(root); }
__declspec(dllexport) unsigned int S14TestDetect(const wchar_t *root,const wchar_t *source) { return s14_package_detect(root,source); }
__declspec(dllexport) int S14TestClean(const wchar_t *root,const wchar_t *source,S14CleanupReport *report,wchar_t *error) { return s14_package_clean(root,source,report,error); }
