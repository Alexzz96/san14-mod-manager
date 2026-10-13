#ifndef S14_ARCHER_PROBE_H
#define S14_ARCHER_PROBE_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
/* Ammunition is a per-deployment sidecar; the game's army/save layout is untouched. */
int s14_archer_probe_install(uintptr_t base,uintptr_t end);
void s14_archer_probe_configure(int enabled);
int s14_archer_ammo_enabled(void);
int s14_archer_probe_next_log(char *out,size_t cap);
void s14_archer_ammo_settings(const wchar_t *root,int capacity,int recovery);
void s14_archer_ammo_parameters(int officer,int *capacity,int *recovery_hundredths);
int s14_archer_ammo_snapshot(uintptr_t army,int *remaining,int *maximum);
void s14_archer_ammo_forget(void *army);
int s14_archer_ammo_save(uintptr_t world,const unsigned char *hash);
void s14_archer_ammo_load_begin(void);
void s14_archer_ammo_load_end(uintptr_t world,int success,const unsigned char *hash,int valid);
void s14_archer_ammo_new_game(uintptr_t world);
#endif
