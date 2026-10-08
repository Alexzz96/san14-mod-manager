#ifndef S14_BATTLE_PROBE_H
#define S14_BATTLE_PROBE_H
#include <windows.h>
#include <stdint.h>
int s14_battle_install(uintptr_t base,uintptr_t end,unsigned char **manager);
void s14_battle_configure(unsigned int flags,int enabled);
int s14_battle_enabled(void);
void s14_battle_planning(uintptr_t world,int day,int force);
void s14_battle_progress(void);
void s14_battle_log(uintptr_t caller,const uintptr_t args[10],const wchar_t *short_text,const wchar_t *long_text);
void s14_battle_worker(const wchar_t *root);
#endif
