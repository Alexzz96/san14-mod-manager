#ifndef S14_BLINK_BACKSTAB_H
#define S14_BLINK_BACKSTAB_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#define S14_BLINK_OFFICER 271 /* Read-only current game table: 黄忠, 2026-10-10. */
typedef struct {uint16_t id,leader,serial;uint8_t carrier,force;} S14BlinkKey;
typedef struct {uintptr_t world,army;S14BlinkKey key;int tile,troops;} S14BlinkArmy;
typedef struct {S14BlinkArmy source,target;unsigned epoch,generation;int day,active,boosted;} S14BlinkHit;
int s14_blink_install(uintptr_t,uintptr_t,const wchar_t*);
int s14_blink_ready(void);
int s14_blink_faulted(void);
void s14_blink_configure(int);
int s14_blink_hit_begin(int,int,int,int,int,uintptr_t,S14BlinkHit*);
void s14_blink_hit_end(const S14BlinkHit*);
void s14_blink_forget(void*);
void s14_blink_created(void*);
void s14_blink_load_begin(void);
void s14_blink_load_end(uintptr_t,int,const unsigned char*,int);
void s14_blink_new_game(uintptr_t);
int s14_blink_save(uintptr_t,const unsigned char*);
int s14_blink_checkpoint_owned(const wchar_t*);
int s14_blink_name(void*,const wchar_t*,wchar_t*,size_t);
void s14_blink_status(wchar_t*,size_t);
int s14_blink_next_log(char*,size_t);
#endif
