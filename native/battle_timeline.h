#ifndef S14_BATTLE_TIMELINE_H
#define S14_BATTLE_TIMELINE_H
#include "battle_report.h"
#include "battle_place.h"
#define S14_TIMELINE_MAX 16384
enum {S14_TIMELINE_ROUT=1,S14_TIMELINE_DUEL,S14_TIMELINE_CAPTURE,S14_TIMELINE_INJURY};
typedef struct {
    uint64_t id;int kind,day,actor,target,actor_force,target_force,outcome,verified,actor_role,legacy;
    unsigned char clock[6];wchar_t actor_name[32],target_name[32];S14BattlePlace place;
} S14TimelineEvent;
typedef struct {unsigned int version,count,first,truncated;uintptr_t world;int start_day,last_day,incomplete,bound;S14TimelineEvent events[S14_TIMELINE_MAX];} S14TimelineSnapshot;
void s14_timeline_root(const wchar_t*);
void s14_timeline_begin(uintptr_t,int);
void s14_timeline_gap(void);
void s14_timeline_load(uintptr_t,int,int,const unsigned char[32],int,int);
int s14_timeline_save(uintptr_t,const unsigned char[32],int);
void s14_timeline_battle(const S14RoundEvent*,int);
void s14_timeline_special(uintptr_t,const S14SpecialEvent*,const S14BattlePlace*);
void s14_timeline_import_special(const S14SpecialSnapshot*);
int s14_timeline_snapshot(uintptr_t,S14TimelineSnapshot*);
int s14_timeline_checkpoint_owned(const wchar_t*);
int s14_timeline_matches(const S14TimelineEvent*,int);
void s14_timeline_describe(const S14TimelineEvent*,int,wchar_t*,size_t);
void s14_timeline_date(const S14TimelineEvent*,wchar_t*,size_t);
#endif
