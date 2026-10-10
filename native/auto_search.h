#ifndef S14_AUTO_SEARCH_H
#define S14_AUTO_SEARCH_H
#include <windows.h>
#include <stdint.h>
#include "manager_ui.h"
#include "toast.h"
int s14_search_install(uintptr_t base,uintptr_t end,unsigned char **manager);
void s14_search_configure(unsigned int flags,unsigned int settings);
int s14_search_is_ready(void);
int s14_search_state(int *force,int *group);
void s14_search_worker(S14ManagerUI *ui,S14Toast *toast,HINSTANCE instance,HWND owner,HANDLE log);
#ifdef S14_SELFTEST
__declspec(dllexport) int S14TestSearchPrologue(int index,unsigned char *bytes,int *length);
__declspec(dllexport) int S14TestSearchEntryCount(void);
#endif
#endif
