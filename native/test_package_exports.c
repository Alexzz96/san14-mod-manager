#include <windows.h>
#include "package.h"
__declspec(dllexport) int S14TestInstall(const wchar_t *root,const wchar_t *source,wchar_t *error) { return s14_package_install(root,source,error); }
__declspec(dllexport) int S14TestRemove(const wchar_t *root,wchar_t *error) { return s14_package_remove(root,error); }
__declspec(dllexport) int S14TestOwned(const wchar_t *root) { return s14_owned_install(root); }
__declspec(dllexport) int S14TestRunning(const wchar_t *root) { return s14_game_running(root); }
