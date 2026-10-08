#ifndef S14_BATTLE_REPORT_H
#define S14_BATTLE_REPORT_H
#include <windows.h>
#include <stdint.h>
enum { S14_ROUND_BEGIN=1,S14_ROUND_END,S14_ROUND_DAMAGE,S14_ROUND_SKILL,S14_ROUND_FIRE,S14_ROUND_ABNORMAL,S14_ROUND_REMOVE,S14_ROUND_INJURY };
typedef struct {int kind,id,leader,force,troops,wounded,health,active;wchar_t name[32];} S14RoundObject;
typedef struct {
    uint64_t id,action_id;uintptr_t world;
    int kind,day,player_force,stable,source_stable,reason,mode,before,after,source_verified,fault;
    unsigned char clock[6];
    wchar_t tactic[12];S14RoundObject source,target,source_after,target_after;
} S14RoundEvent;
void s14_battle_round_consume(const S14RoundEvent *e);
void s14_battle_round_reset(void);
void s14_battle_round_capture_gap(void);
#endif
