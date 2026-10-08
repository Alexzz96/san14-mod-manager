#include <windows.h>
#include "package.h"
#include "battle_stats.h"
__declspec(dllexport) int S14TestStatsSave(const wchar_t *root) {unsigned char hash[32]={1};s14_stats_root(root);s14_stats_load_end(1,100,1,hash,0,1);return s14_stats_save(1,hash,1);}
__declspec(dllexport) int S14TestInstall(const wchar_t *root,const wchar_t *source,wchar_t *error) { return s14_package_install(root,source,error); }
__declspec(dllexport) int S14TestRemove(const wchar_t *root,wchar_t *error) { return s14_package_remove(root,error); }
__declspec(dllexport) int S14TestOwned(const wchar_t *root) { return s14_owned_install(root); }
__declspec(dllexport) int S14TestRunning(const wchar_t *root) { return s14_game_running(root); }
__declspec(dllexport) unsigned int S14TestDetect(const wchar_t *root,const wchar_t *source) { return s14_package_detect(root,source); }
__declspec(dllexport) int S14TestClean(const wchar_t *root,const wchar_t *source,S14CleanupReport *report,wchar_t *error) { return s14_package_clean(root,source,report,error); }
