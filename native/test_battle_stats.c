#include <assert.h>
#ifdef NDEBUG
#error "Assertions are required"
#endif
#include "battle_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
static S14RoundObject unit(int id,int leader,int force,int troops) {S14RoundObject o={.kind=2,.id=id,.leader=leader,.force=force,.troops=troops,.active=1};return o;}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);wchar_t root[MAX_PATH],dir[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));
    swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);CreateDirectoryW(dir,NULL);s14_stats_root(root);
    unsigned char old[32]={1},later[32]={2},copy[32]={3},unknown[32]={4};
    S14BattleStatsSnapshot *out=malloc(sizeof(*out));assert(out);
    if(argc==3) {s14_stats_load_begin();s14_stats_load_end(99,110,1,later,1,0);assert(s14_stats_snapshot(NULL,99,out));
        assert(out->rows[10].enemy_loss==100 && out->rows[10].units_routed==2 && out->rows[20].units_defeated==2 && out->bound_to_save);
        printf("{\"cross_process_restore\":true}\n");free(out);return 0;}
    assert(!s14_stats_snapshot(NULL,1,out));s14_stats_load_end(1,100,1,old,1,0);assert(s14_stats_snapshot(NULL,1,out));
    assert(out->rows[10].valid_mask && !out->rows[10].enemy_loss && !out->bound_to_save);assert(s14_stats_save(1,old,1));
    S14BattleTotals one;int gap,bound;assert(s14_stats_row(1,10,&one,&gap,&bound) && !one.enemy_loss && !gap && bound);
    assert(!s14_stats_row(2,10,&one,&gap,&bound) && !s14_stats_row(1,0,&one,&gap,&bound));
    S14RoundEvent e={.id=10,.world=1,.day=100,.kind=S14_ROUND_DAMAGE,.stable=1,.source=unit(1,10,1,5000),.target=unit(2,20,2,5000),.target_after=unit(2,20,2,4900)};
    s14_stats_consume(&e);s14_stats_consume(&e);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].enemy_loss==100 && out->rows[20].own_loss==100);
    e.id=11;e.source.force=2;s14_stats_consume(&e);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].enemy_loss==100 && out->rows[20].own_loss==200); // Friendly damage never adds enemy losses.
    e.id=12;e.source=(S14RoundObject){0};s14_stats_consume(&e);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].enemy_loss==100 && out->rows[20].own_loss==300); // Unknown fire actor stays unknown.
    e.id=13;e.kind=S14_ROUND_REMOVE;e.source=unit(1,10,1,5000);e.target=unit(2,20,2,0);e.reason=1;s14_stats_consume(&e);
    e.id=14;s14_stats_consume(&e);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].units_routed==1 && out->rows[20].units_defeated==1); // Distinct callbacks, same unit lifetime.
    e.id=15;e.reason=0;s14_stats_consume(&e);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].units_routed==1);
    e.id=16;e.kind=S14_ROUND_DAMAGE;e.target=unit(2,20,2,200);e.target_after=unit(2,20,2,200);s14_stats_consume(&e); // Same slot/leader, a new live unit.
    e.id=17;e.kind=S14_ROUND_REMOVE;e.reason=1;e.target.troops=0;s14_stats_consume(&e);
    assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].units_routed==2 && out->rows[20].units_defeated==2);
    e.id=19;e.target=unit(3,30,3,0);e.source=(S14RoundObject){0};e.stable=0;e.target_after=unit(3,0,0,0);e.target_after.active=0;s14_stats_consume(&e);
    assert(s14_stats_snapshot(NULL,1,out) && out->rows[30].units_defeated==1 && out->rows[10].units_routed==2);e.stable=1;e.source=unit(1,10,1,5000);e.target=unit(2,20,2,0);
    assert(s14_stats_save(1,later,1));unsigned char branch[16];memcpy(branch,out->branch,16);
    s14_stats_load_begin();assert(!s14_stats_snapshot(NULL,1,out));s14_stats_load_end(1,100,1,old,1,0);
    assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].enemy_loss==0 && out->rows[10].units_routed==0 && memcmp(branch,out->branch,16)); // Old save restores old counters.
    s14_stats_load_end(1,100,1,later,1,0);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].units_routed==2);
    s14_stats_load_end(1,100,1,old,1,0);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].units_routed==0); // Same date, different save content.
    s14_stats_load_end(1,110,1,later,1,0);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].units_routed==2);
    e.id=18;e.kind=S14_ROUND_REMOVE;e.target.troops=0;s14_stats_consume(&e);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].units_routed==2); // Checkpoint carries lifecycle dedupe.
    s14_stats_load_begin();s14_stats_load_end(1,110,0,old,1,0);assert(s14_stats_snapshot(NULL,1,out) && out->rows[10].enemy_loss==100); // Failed load preserves current branch.
    assert(s14_stats_save(1,copy,1));s14_stats_load_end(2,110,1,copy,1,0);assert(s14_stats_snapshot(NULL,2,out) && out->rows[10].enemy_loss==100 && !s14_stats_snapshot(NULL,1,out));
    s14_stats_gap();assert(s14_stats_snapshot(NULL,2,out) && out->incomplete);assert(!s14_stats_save(2,unknown,0));
    s14_stats_load_end(3,100,1,unknown,0,1);assert(s14_stats_snapshot(NULL,3,out) && out->rows[10].enemy_loss==0); // New campaign cannot inherit.
    s14_stats_load_end(3,110,1,later,1,0);assert(s14_stats_snapshot(NULL,3,out) && out->rows[10].enemy_loss==100);
    assert(s14_stats_save(3,copy,1));wchar_t bad[MAX_PATH],hex[65];for(int i=0;i<32;i++) swprintf(hex+i*2,3,L"%02x",copy[i]);
    swprintf(bad,MAX_PATH,L"%ls\\SAN14ModManager\\career\\checkpoints\\%ls.s14career",root,hex);
    HANDLE f=CreateFileW(bad,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);assert(f!=INVALID_HANDLE_VALUE);DWORD wrote;assert(SetFilePointer(f,256,NULL,FILE_BEGIN)==256);
    unsigned char corrupt=0xab;assert(WriteFile(f,&corrupt,1,&wrote,NULL) && wrote==1);CloseHandle(f);
    assert(!s14_stats_checkpoint_owned(bad));s14_stats_load_end(3,110,1,copy,1,0);assert(s14_stats_snapshot(NULL,3,out) && !out->rows[10].enemy_loss && !out->bound_to_save);
    assert(!s14_stats_save(3,copy,1));assert(GetFileAttributesW(bad)!=INVALID_FILE_ATTRIBUTES); // Unknown or corrupt checkpoint is preserved, never overwritten.
    s14_stats_load_end(3,110,1,later,1,0);assert(s14_stats_snapshot(NULL,3,out) && out->rows[10].enemy_loss==100);free(out);
    printf("{\"status\":\"passed\",\"save_checkpoints\":true,\"old_save_rollback\":true,\"same_day_branch_isolation\":true,\"duplicate_event_and_unit_lifetime\":true,\"friendly_damage_excluded\":true,\"unknown_fire_not_credited\":true,\"failed_load_preserves_counters\":true,\"new_campaign_isolation\":true,\"unknown_distinct_from_zero\":true}\n");return 0;
}
