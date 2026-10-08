#ifndef S14_BATTLE_STATS_H
#define S14_BATTLE_STATS_H
#include "battle_report.h"
#define S14_STATS_MAX 6001
#define S14_STATS_DAMAGE 1u
#define S14_STATS_ROUTS 2u
#define S14_STATS_DEFEATS 4u
#define S14_STATS_LOSSES 8u
#define S14_STATS_INJURIES 16u
typedef struct {unsigned int valid_mask;uint64_t enemy_loss,units_routed,units_defeated,own_loss,officers_injured;} S14BattleTotals;
typedef struct {
    unsigned int version;uintptr_t world;int start_day,last_day,incomplete,bound_to_save;
    unsigned char campaign[16],branch[16];
    S14BattleTotals rows[S14_STATS_MAX];
} S14BattleStatsSnapshot;
typedef struct {unsigned int version;void *context;int (*read)(void*,uintptr_t,S14BattleStatsSnapshot*);} S14BattleStatsProvider;
void s14_stats_root(const wchar_t *root);
void s14_stats_consume(const S14RoundEvent *e);
void s14_stats_gap(void);
void s14_stats_load_begin(void);
void s14_stats_load_end(uintptr_t world,int day,int success,const unsigned char hash[32],int hash_valid,int new_game);
int s14_stats_save(uintptr_t world,const unsigned char hash[32],int hash_valid);
int s14_stats_snapshot(void *unused,uintptr_t world,S14BattleStatsSnapshot *out);
// Worker-thread-only, small row access for native detail overlays.
int s14_stats_row(uintptr_t world,int officer,S14BattleTotals *out,int *incomplete,int *bound);
// Only validates plugin checkpoint files; never recognizes ordinary game saves.
int s14_stats_checkpoint_owned(const wchar_t *path);
#endif
