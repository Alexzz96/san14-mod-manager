#ifndef S14_BASIC_STATS_UI_H
#define S14_BASIC_STATS_UI_H
#include "battle_stats.h"
int s14_basic_stats_install(uintptr_t base,uintptr_t end);
/* Worker publishes a value copy; only the native UI thread touches widgets. */
void s14_basic_stats_publish(uintptr_t page,uintptr_t person,uintptr_t world,const S14BattleTotals*,int connected,int incomplete,int bound,uint64_t merit);
int s14_basic_stats_embedded(uintptr_t page,uintptr_t person);
#endif
