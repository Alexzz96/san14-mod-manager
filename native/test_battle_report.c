#include <assert.h>
#ifdef NDEBUG
#error "Assertions are required"
#endif
#include "battle_report.h"
#include "turn_report.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
static S14RoundObject unit(int id,int force,const wchar_t *name,int troops) {
    S14RoundObject o={.kind=2,.id=id,.leader=id,.force=force,.troops=troops,.active=1};wcscpy(o.name,name);return o;
}
static void begin(uintptr_t world,int day,int force) {S14RoundEvent e={.kind=S14_ROUND_BEGIN,.world=world,.day=day,.player_force=force,.clock={7,1,5,21}};s14_battle_round_consume(&e);}
static void end(uintptr_t world,int day,int force,int fault) {S14RoundEvent e={.kind=S14_ROUND_END,.world=world,.day=day,.player_force=force,.fault=fault,.clock={7,1,6,1}};s14_battle_round_consume(&e);}
static const wchar_t *report(void) {const wchar_t *search,*battle;assert(s14_turn_report_take(GetTickCount64()+300,&search,&battle));return battle;}
int main(void) {
    const wchar_t *s,*b;s14_turn_report_reset();s14_battle_round_reset();
    end(1,100,1,0);assert(!s14_turn_report_take(GetTickCount64()+300,&s,&b)); // Initial load is not a completed turn.
    begin(1,100,1);
    S14RoundEvent e={.id=3,.world=1,.day=100,.kind=S14_ROUND_DAMAGE,.stable=1,.source_stable=1,
        .source=unit(1,1,L"张嶷",5000),.target=unit(2,2,L"曹仁",5500),.target_after=unit(2,2,L"曹仁",5499)};
    s14_battle_round_consume(&e);s14_battle_round_consume(&e); // Duplicate ID must not double count.
    e.id=2;e.source=unit(3,2,L"曹洪",5000);e.target=unit(4,2,L"曹仁",5000);e.target_after.troops=4500;s14_battle_round_consume(&e); // Foreign versus foreign excluded.
    e.id=4;e.source=unit(2,2,L"曹仁",5499);e.target=unit(1,1,L"张嶷",5000);e.target_after=unit(1,1,L"张嶷",0);s14_battle_round_consume(&e);
    e.id=5;e.source=(S14RoundObject){0};e.target=unit(5,1,L"罗宪",5000);e.target_after=unit(5,1,L"罗宪",4912);s14_battle_round_consume(&e); // Own fire loss with unknown caster.
    e.id=6;e.kind=S14_ROUND_REMOVE;e.reason=1;e.source=unit(6,1,L"张辽",1000);e.target=unit(7,2,L"敌将",0);e.source_after=e.source;e.source_after.wounded=263;s14_battle_round_consume(&e);
    e.id=7;e.reason=0;e.target.troops=1000;s14_battle_round_consume(&e); // Ordinary removal is not a rout.
    e.id=8;e.kind=S14_ROUND_INJURY;e.source=unit(6,1,L"张辽",1000);e.target=unit(7,2,L"敌将",0);e.target.kind=1;e.target.health=0;e.target_after=e.target;e.target_after.health=1;s14_battle_round_consume(&e);
    e.id=9;e.target_after.health=0;s14_battle_round_consume(&e); // Injury setter with no change is not a wound.
    e.id=10;e.kind=S14_ROUND_SKILL;e.source=unit(6,1,L"张辽",1000);wcscpy(e.tactic,L"止步");s14_battle_round_consume(&e);
    e.id=11;e.kind=S14_ROUND_ABNORMAL;e.mode=1;e.before=0;e.after=15;e.source_verified=1;e.target=unit(7,2,L"敌将",1000);e.target_after=e.target;s14_battle_round_consume(&e);
    s14_turn_report_search(100,110,L"张辽 → 襄阳：探索失败");end(1,110,1,0);
    assert(!s14_turn_report_take(GetTickCount64(),&s,&b));assert(s14_turn_report_take(GetTickCount64()+300,&s,&b));
    assert(wcsstr(s,L"张辽 → 襄阳") && wcsstr(b,L"造成敌方兵力减少 1") && wcsstr(b,L"己方兵力损失 5088"));
    assert(wcsstr(b,L"击溃事件 1") && wcsstr(b,L"击伤事件 1") && wcsstr(b,L"吸收伤兵 263"));
    assert(wcsstr(b,L"263 年 5 月下旬") && !wcsstr(b,L"曹洪 → 曹仁") && wcsstr(b,L"来源未确认") && wcsstr(b,L"尚未核实"));
    assert(!s14_turn_report_take(GetTickCount64()+600,&s,&b));end(1,110,1,0);assert(!s14_turn_report_take(GetTickCount64()+600,&s,&b));
    s14_turn_report_search(100,110,L"迟到的同回合搜索结果");assert(!s14_turn_report_take(GetTickCount64()+600,&s,&b)); // Cannot reopen a dismissed turn.
    begin(1,110,1);end(1,120,1,0);b=report();assert(wcsstr(b,L"造成敌方兵力减少 0") && wcsstr(b,L"未观察到自势力相关战斗"));
    begin(1,120,1);end(1,130,1,1);b=report();assert(wcsstr(b,L"采集不完整"));
    unsigned int epoch=s14_turn_report_epoch();end(1,120,1,0);assert(s14_turn_report_epoch()!=epoch && !s14_turn_report_take(GetTickCount64()+600,&s,&b));
    begin(1,120,1);s14_battle_round_capture_gap();end(1,130,1,0);b=report();assert(wcsstr(b,L"采集不完整"));
    begin(1,130,1);end(2,140,1,0);assert(!s14_turn_report_take(GetTickCount64()+600,&s,&b)); // Load another world cancels scope.
    begin(2,140,1);end(2,130,1,0);assert(!s14_turn_report_take(GetTickCount64()+600,&s,&b)); // Rollback cancels.
    begin(2,140,0);end(2,150,0,0);b=report();assert(wcsstr(b,L"未能确认当前玩家势力"));
    s14_turn_report_reset();s14_turn_report_battle(100,110,L"旧回合战斗");s14_turn_report_search(110,120,L"新回合搜索");
    assert(s14_turn_report_take(GetTickCount64()+300,&s,&b));assert(wcsstr(s,L"新回合搜索") && !wcsstr(b,L"旧回合战斗"));
    s14_battle_round_reset();s14_turn_report_reset();
    printf("{\"status\":\"passed\",\"own_force_filter\":true,\"duplicate_and_out_of_order_ids\":true,\"initial_load_no_popup\":true,\"round_reset\":true,\"exploration_battle_same_round\":true,\"foreign_battle_excluded\":true,\"unknown_fire_source_preserved\":true,\"wounded_absorption_separate\":true,\"rout_and_injury_outcomes\":true,\"load_and_rollback_cancel\":true,\"partial_capture_warning\":true}\n");return 0;
}
