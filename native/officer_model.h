#ifndef S14_OFFICER_MODEL_H
#define S14_OFFICER_MODEL_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include "battle_stats.h"
#include "special_stats.h"
#define S14_OFFICER_MAX 6001
#define S14_PERSONALITY_MAX 356
#define S14_TACTICS_MAX 201
#define S14_CAREER_KILLS 1u
#define S14_CAREER_ROUTS 2u
#define S14_CAREER_SOLDIERS 4u
#define S14_CAREER_WOUNDED 8u
typedef struct {
    unsigned int valid_mask;
    uint64_t officer_kills,units_routed,soldiers_killed,soldiers_wounded;
} S14OfficerCareer;
/* Versioned extension: attach only statistics bound to this campaign/branch.
   A missing value stays unknown; it must not silently become zero. */
typedef struct {
    unsigned int version;
    const wchar_t *campaign_id,*branch_id;
    void *context;
    int (*get)(void*,const wchar_t*,const wchar_t*,int,S14OfficerCareer*);
} S14OfficerCareerProvider;
typedef struct {
    int id,force,raw_force,status,health,home,current,army,tile,troops,place_kind;
    int ability[5],total;
    int ambition,bond,loyalty,ambition_raw,bond_raw,loyalty_raw;
    unsigned short personalities[9];
    unsigned char tactics[10];
    wchar_t name[24],courtesy[12],force_name[32],status_name[24],health_name[24];
    wchar_t home_name[40],location[96],personality_text[240],tactics_text[240];
    S14OfficerCareer career;
    S14BattleTotals battle;
    S14SpecialTotals special;
} S14Officer;
typedef struct { wchar_t name[12],description[64]; } S14OfficerDefinition;
typedef struct {
    unsigned int schema_version;
    int count,player_force,read_errors,unstable;
    uint64_t bytes_read;
    uintptr_t world,settings;
    unsigned char clock[6];
    int battle_connected,battle_incomplete,battle_start_day,battle_bound;
    S14OfficerDefinition personalities[S14_PERSONALITY_MAX],tactics[S14_TACTICS_MAX];
    S14Officer rows[S14_OFFICER_MAX];
} S14OfficerSnapshot;
typedef int (*S14OfficerRead)(void*,uintptr_t,void*,size_t);
enum { S14_SORT_NAME=0,S14_SORT_FORCE,S14_SORT_STATUS,S14_SORT_LOCATION,
       S14_SORT_LEADERSHIP,S14_SORT_WAR,S14_SORT_INTELLIGENCE,S14_SORT_POLITICS,S14_SORT_CHARM,
       S14_SORT_TOTAL,S14_SORT_PERSONALITY,S14_SORT_TROOPS,S14_SORT_KILLS,S14_SORT_ROUTS,S14_SORT_AMBITION,S14_SORT_BOND,S14_SORT_LOYALTY,
       S14_SORT_ENEMY_LOSS,S14_SORT_DEFEATS,S14_SORT_OWN_LOSS,S14_SORT_INJURIES,
       S14_SORT_DUELS,S14_SORT_DUEL_WINS,S14_SORT_DUEL_LOSSES,S14_SORT_CAPTURES,S14_SORT_CAPTURED,S14_SORT_UNIQUE_CAPTIVES,S14_SORT_KDA,S14_SORT_COUNT };
typedef struct {
    wchar_t query[128];
    int own_force,place,include_history,sort,descending;
} S14OfficerFilter;
int s14_officer_inner_grade(int raw);
void s14_officer_decode_character(const unsigned char raw[512],S14Officer *out);
int s14_officers_capture(S14OfficerRead,void*,uintptr_t,S14OfficerSnapshot*);
int s14_officer_matches(const S14Officer*,int,const S14OfficerFilter*);
int s14_officer_count_key(int);
uint64_t s14_officer_count_value(const S14Officer*,int);
void s14_officer_kda_text(const S14Officer*,wchar_t*,size_t);
int s14_officer_compare(const S14Officer*,const S14Officer*,int,int);
void s14_officers_attach_career(S14OfficerSnapshot*,const S14OfficerCareerProvider*);
void s14_officers_attach_battle(S14OfficerSnapshot*,const S14BattleStatsSnapshot*);
uint64_t s14_officer_special_value(const S14Officer*,int);
unsigned int s14_officer_special_flag(int);
#endif
