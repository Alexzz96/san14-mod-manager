#ifndef S14_CAREER_AFFIX_H
#define S14_CAREER_AFFIX_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#define S14_ELITE_OFFICER 518
#define S14_ELITE_THRESHOLD 5000ull
typedef struct {uintptr_t world;uint64_t enemy_loss;unsigned int epoch;int valid,bound,incomplete,day,enabled,active,suspended;} S14AffixState;
void s14_affix_configure(int);
unsigned int s14_affix_suspend(void);
unsigned int s14_affix_epoch(void);
int s14_affix_publish(uintptr_t,uint64_t,int,int,int,int,unsigned int,int);
void s14_affix_snapshot(S14AffixState*);
int s14_affix_active(uintptr_t,int);
int s14_affix_display(uintptr_t,int,const wchar_t*,wchar_t*,size_t);
void s14_affix_status(uintptr_t,wchar_t*,size_t);
#endif
