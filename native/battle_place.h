#ifndef S14_BATTLE_PLACE_H
#define S14_BATTLE_PLACE_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
typedef struct {int tile,city_id,area_id,basis;wchar_t city[40],area[40];} S14BattlePlace;
typedef int (*S14PlaceRead)(void*,uintptr_t,void*,size_t);
int s14_place_capture(S14PlaceRead,void*,uintptr_t base,uintptr_t world,int tile,S14BattlePlace*);
void s14_place_text(const S14BattlePlace*,wchar_t*,size_t);
#endif
