#define main legacy_battle_fixture
#include "test_battle_probe.c"
#undef main
static BattleObject army(int id,int leader,int force,int troops){return (BattleObject){.address=(uintptr_t)id*512,.kind=2,.id=id,.leader=leader,.force_id=force,.troops=troops,.active=1};}
int main(void){
    s14_affix_configure(1);unsigned epoch=s14_affix_suspend();
    BattleEvent e={.id=1,.world=99,.kind=B_LOAD_END,.planning_day=100,.native_return=1,.affix_epoch=epoch};battle_report_event(&e);
    S14AffixState state;s14_affix_snapshot(&state);assert(state.valid && !state.active && state.world==99);
    e.id=2;e.kind=B_PROGRESS;battle_report_event(&e);
    e.id=3;e.kind=B_TROOPS;e.source=army(29,518,1,5500);e.source_after=e.source;e.target=army(30,613,2,10000);e.target_after=army(30,613,2,5001);e.post_identity_matches=1;battle_report_event(&e);
    e.id=4;e.kind=B_PLANNING;battle_report_event(&e);s14_affix_snapshot(&state);assert(state.enemy_loss==4999 && !state.active);
    e.id=5;e.kind=B_PROGRESS;battle_report_event(&e);
    e.id=6;e.kind=B_TROOPS;e.target=army(30,613,2,5001);e.target_after=army(30,613,2,5000);battle_report_event(&e);battle_report_event(&e);assert(!s14_affix_active(99,518));
    e.id=7;e.kind=B_PLANNING;battle_report_event(&e);s14_affix_snapshot(&state);assert(state.enemy_loss==5000 && state.active);
    unsigned next=s14_affix_suspend();assert(!s14_affix_active(99,518));e.id=8;battle_report_event(&e);assert(!s14_affix_active(99,518));
    e.id=9;e.kind=B_NEW_GAME;e.native_return=1;e.affix_epoch=next;battle_report_event(&e);s14_affix_snapshot(&state);assert(state.valid && state.enemy_loss==0 && !state.active);
    puts("{\"status\":\"passed\",\"production_event_bridge\":true,\"round_end_only_unlock\":true,\"duplicate_event_not_counted\":true,\"stale_planning_cannot_unlock_during_load\":true,\"new_campaign_resets\":true}");return 0;
}
