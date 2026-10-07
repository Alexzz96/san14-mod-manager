#include <windows.h>
#include "package.h"
__declspec(dllexport) int S14TestInstall(const wchar_t *root,const wchar_t *source,wchar_t *error) { return s14_package_install(root,source,error); }
__declspec(dllexport) int S14TestRemove(const wchar_t *root,wchar_t *error) { return s14_package_remove(root,error); }
__declspec(dllexport) int S14TestOwned(const wchar_t *root) { return s14_owned_install(root); }
__declspec(dllexport) int S14TestRunning(const wchar_t *root) { return s14_game_running(root); }
__declspec(dllexport) unsigned int S14TestDetect(const wchar_t *root,const wchar_t *source) { return s14_package_detect(root,source); }
__declspec(dllexport) int S14TestClean(const wchar_t *root,const wchar_t *source,S14CleanupReport *report,wchar_t *error) { return s14_package_clean(root,source,report,error); }
