#include "battle_report.h"
#include "turn_report.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#define MAX_LINES 4096
#define LINE_SIZE 256
#define SEEN_SIZE 16384
typedef struct {wchar_t name[32];uint64_t inflicted,lost,absorbed;int effects,routs,injuries;} Actor;
static struct {
    int active,day,force,end,incomplete,lines,skipped,participants,unknown,routs,injuries,effects;
    uintptr_t world;uint64_t inflicted,lost,absorbed,seen[SEEN_SIZE];
    unsigned char clock[6],end_clock[6];
    Actor actors[6001];wchar_t (*details)[LINE_SIZE];
} round_data;
void s14_battle_round_reset(void) {
    if(round_data.details) free(round_data.details);memset(&round_data,0,sizeof(round_data));
}
void s14_battle_round_capture_gap(void) {if(round_data.active) round_data.incomplete=1;}
static int known(const S14RoundObject *o) {return (o->kind==1 || o->kind==2) && o->active && o->leader>0 && o->leader<=6000 && o->force>0;}
static int own(const S14RoundObject *o) {return round_data.force>0 && known(o) && o->force==round_data.force;}
static Actor *actor(const S14RoundObject *o) {
    if(!own(o)) return NULL;Actor *a=&round_data.actors[o->leader];
    if(!a->name[0]) {wcsncpy(a->name,o->name[0]?o->name:L"未识别武将",31);round_data.participants++;}return a;
}
static const wchar_t *name(const S14RoundObject *o) {return o->name[0]?o->name:L"来源未知";}
static void line(const wchar_t *format,...) {
    if(round_data.lines>=MAX_LINES) {round_data.skipped++;return;}
    if(!round_data.details) round_data.details=calloc(MAX_LINES,sizeof(*round_data.details));
    if(!round_data.details) {round_data.incomplete=1;round_data.skipped++;return;}
    va_list args;va_start(args,format);int n=vswprintf(round_data.details[round_data.lines],LINE_SIZE,format,args);va_end(args);
    if(n<0) {round_data.skipped++;return;}round_data.lines++;
}
static void publish(void) {
    size_t capacity=2048+(size_t)round_data.lines*(LINE_SIZE+24)+(size_t)round_data.participants*256;
    wchar_t *text=calloc(capacity,sizeof(wchar_t));if(!text) return;size_t at=0;
#define ROUND_TEXT(...) do {int n=swprintf(text+at,capacity-at,__VA_ARGS__);if(n<0) {free(text);return;}at+=(size_t)n;} while(0)
    ROUND_TEXT(L"自势力 · 本回合战斗报告\n参与武将 %d · 造成敌方兵力减少 %llu · 己方兵力损失 %llu\n击溃事件 %d · 击伤事件 %d · 吸收伤兵 %llu\n\n",round_data.participants,(unsigned long long)round_data.inflicted,(unsigned long long)round_data.lost,round_data.routs,round_data.injuries,(unsigned long long)round_data.absorbed);
    const unsigned char *c=round_data.clock,*d=round_data.end_clock;int year=c[0]+256*c[1],end_year=d[0]+256*d[1];
    if(year>0 && c[2]>=1 && c[2]<=12 && c[3]>=1 && c[3]<=30 && end_year>0 && d[2]>=1 && d[2]<=12 && d[3]>=1 && d[3]<=30)
        ROUND_TEXT(L"%d 年 %d 月%ls → %d 年 %d 月%ls\n\n",year,c[2],c[3]<=10?L"上旬":c[3]<=20?L"中旬":L"下旬",end_year,d[2],d[3]<=10?L"上旬":d[3]<=20?L"中旬":L"下旬");
    if(!round_data.force) ROUND_TEXT(L"未能确认当前玩家势力，不能筛选自势力战斗；本页不代表零战绩。\n\n");
    if(round_data.incomplete) ROUND_TEXT(L"本回合采集不完整，以下是已观察到的部分数据。\n\n");
    ROUND_TEXT(L"士兵阵亡、新增伤兵与武将击杀：尚未核实，暂不显示计数。\n兵力减少包含受伤等变化；吸收伤兵单独统计。\n\n");
    if(round_data.unknown) ROUND_TEXT(L"涉及己方但来源或对方势力未知的事件 %d 条，明细保留未知。\n\n",round_data.unknown);
    if(round_data.participants) {
        ROUND_TEXT(L"武将汇总\n");
        for(int i=1;i<=6000;i++) {Actor *a=&round_data.actors[i];if(!a->name[0]) continue;
            ROUND_TEXT(L"%ls  ·  造成兵力减少 %llu  ·  自身损失 %llu  ·  击溃 %d  ·  击伤 %d  ·  吸收伤兵 %llu  ·  战法效果 %d\n",a->name,(unsigned long long)a->inflicted,(unsigned long long)a->lost,a->routs,a->injuries,(unsigned long long)a->absorbed,a->effects);
        }ROUND_TEXT(L"\n");
    }
    ROUND_TEXT(L"战斗明细\n");if(!round_data.lines) ROUND_TEXT(L"本回合未观察到自势力相关战斗事件。\n");
    for(int i=0;i<round_data.lines;i++) ROUND_TEXT(L"%d. %ls\n",i+1,round_data.details[i]);
    if(round_data.skipped) ROUND_TEXT(L"另有 %d 条明细未展示；请查阅原始战斗日志。\n",round_data.skipped);
    s14_turn_report_battle(round_data.day,round_data.end,text);free(text);
#undef ROUND_TEXT
}
void s14_battle_round_consume(const S14RoundEvent *e) {
    if(e->kind==S14_ROUND_BEGIN) {
        if(round_data.world && (e->world!=round_data.world || e->day<round_data.day || e->day<round_data.end)) s14_turn_report_reset();
        s14_battle_round_reset();round_data.active=1;round_data.world=e->world;round_data.day=e->day;round_data.force=e->player_force;round_data.incomplete=e->fault;memcpy(round_data.clock,e->clock,6);return;
    }
    if(e->kind==S14_ROUND_END) {
        if(round_data.active && e->world==round_data.world && e->day>round_data.day && e->player_force==round_data.force) {
            round_data.end=e->day;round_data.incomplete|=e->fault;memcpy(round_data.end_clock,e->clock,6);publish();
        } else if(round_data.world && (e->world!=round_data.world || e->day<round_data.day || e->day<round_data.end || e->player_force!=round_data.force)) s14_turn_report_reset();
        round_data.active=0;return;
    }
    if(!round_data.active || e->world!=round_data.world || e->day!=round_data.day) return;
    round_data.incomplete|=e->fault;
    // IDs may arrive out of order because a parent is enqueued on return.
    if(!e->id) return;unsigned int slot=(unsigned int)e->id&(SEEN_SIZE-1),n;
    for(n=0;n<SEEN_SIZE;n++,slot=(slot+1)&(SEEN_SIZE-1)) {
        if(round_data.seen[slot]==e->id) return;
        if(!round_data.seen[slot]) {round_data.seen[slot]=e->id;break;}
    }
    if(n==SEEN_SIZE) {round_data.incomplete=1;return;}
    if(!own(&e->source) && !own(&e->target)) return;
    if(!e->stable && e->kind!=S14_ROUND_SKILL) {round_data.incomplete=1;return;}
    Actor *a=actor(&e->source),*b=actor(&e->target);
    if(e->kind==S14_ROUND_DAMAGE) {
        int loss=e->target.troops-e->target_after.troops;if(loss<=0) return;
        if(b) {b->lost+=loss;round_data.lost+=loss;}
        if(a && known(&e->target) && !own(&e->target)) {a->inflicted+=loss;round_data.inflicted+=loss;}
        if(!known(&e->source) || !known(&e->target)) round_data.unknown++;
        line(L"%ls → %ls：兵力 %d → %d，减少 %d%ls",name(&e->source),name(&e->target),e->target.troops,e->target_after.troops,loss,!known(&e->source)?L"（来源未确认）":L"");
    } else if(e->kind==S14_ROUND_SKILL && a) {
        a->effects++;round_data.effects++;line(L"%ls：%ls 战法效果已触发",name(&e->source),e->tactic[0]?e->tactic:L"未知战法");
    } else if(e->kind==S14_ROUND_FIRE && e->before==0 && e->after>0) {
        line(L"%ls → %ls：%ls，起火%ls",name(&e->source),name(&e->target),e->tactic,e->source_verified?L"":L"（施放归属待核对）");
    } else if(e->kind==S14_ROUND_ABNORMAL) {
        const wchar_t *state=e->mode==1?L"止步":e->mode==0?L"混乱（待核对）":e->mode==2?L"挑衅（待核对）":L"异常状态";
        line(L"%ls → %ls：%ls · %ls，%ls%ls",name(&e->source),name(&e->target),e->tactic,state,e->before>=0 && e->after>e->before?L"状态已施加／延长":L"未观察到状态增加",e->source_verified?L"":L"（归属待核对）");
    } else if(e->kind==S14_ROUND_REMOVE) {
        int rout=e->reason==1 && e->target.kind==2 && e->target.troops==0 && e->source.kind==2 && known(&e->source) && known(&e->target) && e->source.force!=e->target.force;
        int received=e->source_stable?e->source_after.wounded-e->source.wounded:0;if(received<0) received=0;
        if(rout && a) {a->routs++;round_data.routs++;a->absorbed+=received;round_data.absorbed+=received;}
        if(rout) line(L"%ls 击溃 %ls 部队；吸收伤兵 %d",name(&e->source),name(&e->target),received);
        else line(L"%ls 部队移除（原因未确认为击溃）",name(&e->target));
    } else if(e->kind==S14_ROUND_INJURY && e->target_after.health>e->target.health && e->target_after.health>=1 && e->target_after.health<=3) {
        if(a && known(&e->target) && !own(&e->target)) {a->injuries++;round_data.injuries++;}
        line(L"%ls → %ls：%ls",name(&e->source),name(&e->target),e->target_after.health==1?L"轻伤":e->target_after.health==2?L"重伤（待核对）":L"伤势等级 3（待核对）");
    }
}
