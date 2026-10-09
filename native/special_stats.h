#ifndef S14_SPECIAL_STATS_H
#define S14_SPECIAL_STATS_H
#include <windows.h>
#include <stdint.h>
#define S14_SPECIAL_OFFICERS 6001
#define S14_SPECIAL_HISTORY 4096
#define S14_SPECIAL_DUEL 1u
#define S14_SPECIAL_CAPTURE 2u
enum { S14_SPECIAL_ACTOR_UNKNOWN=0,S14_SPECIAL_ACTOR_PERSON=1,S14_SPECIAL_ACTOR_COMMANDER=2 };
typedef struct {
    uint64_t id;int kind,day,actor,target,actor_force,target_force,outcome,actor_role,verified;
    unsigned char clock[6];
    wchar_t actor_name[32],target_name[32];
} S14SpecialEvent;
typedef struct {unsigned int valid_mask;uint64_t duels,wins,losses,captures,captured,unique_captives;} S14SpecialTotals;
typedef struct {
    unsigned int version,count,truncated;int start_day,last_day,incomplete,bound;
    uintptr_t world;S14SpecialTotals rows[S14_SPECIAL_OFFICERS];
    S14SpecialEvent events[S14_SPECIAL_HISTORY];
} S14SpecialSnapshot;
void s14_special_root(const wchar_t *root);
void s14_special_begin(uintptr_t world,int day);
void s14_special_load(uintptr_t world,int day,int success,const unsigned char hash[32],int valid,int new_game);
int s14_special_save(uintptr_t world,const unsigned char hash[32],int valid);
void s14_special_consume(uintptr_t world,const S14SpecialEvent *event);
void s14_special_gap(void);
int s14_special_snapshot(uintptr_t world,S14SpecialSnapshot *out);
int s14_special_checkpoint_owned(const wchar_t *path);
#endif
