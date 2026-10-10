#include "career_affix.h"
#include <wchar.h>
#include <stdio.h>
#include <string.h>
static SRWLOCK affix_lock=SRWLOCK_INIT;
static S14AffixState affix;
static volatile LONG configured;
void s14_affix_configure(int enabled){InterlockedExchange(&configured,!!enabled);}
unsigned int s14_affix_suspend(void){
    AcquireSRWLockExclusive(&affix_lock);unsigned int epoch=affix.epoch+1;if(!epoch)epoch=1;
    memset(&affix,0,sizeof(affix));affix.epoch=epoch;affix.suspended=1;ReleaseSRWLockExclusive(&affix_lock);return epoch;
}
unsigned int s14_affix_epoch(void){AcquireSRWLockShared(&affix_lock);unsigned int epoch=affix.epoch;ReleaseSRWLockShared(&affix_lock);return epoch;}
int s14_affix_publish(uintptr_t world,uint64_t loss,int valid,int bound,int incomplete,int day,unsigned int epoch,int resume){
    AcquireSRWLockExclusive(&affix_lock);int ok=epoch==affix.epoch && (!affix.suspended || resume);
    if(ok){affix.world=world;affix.enemy_loss=loss;affix.valid=!!valid && world!=0;affix.bound=!!bound;affix.incomplete=!!incomplete;affix.day=day;affix.suspended=0;}
    ReleaseSRWLockExclusive(&affix_lock);return ok;
}
void s14_affix_snapshot(S14AffixState *out){
    if(!out)return;AcquireSRWLockShared(&affix_lock);*out=affix;ReleaseSRWLockShared(&affix_lock);
    out->enabled=InterlockedCompareExchange(&configured,0,0)!=0;out->active=out->enabled && out->valid && !out->suspended && out->enemy_loss>=S14_ELITE_THRESHOLD;
}
int s14_affix_active(uintptr_t world,int officer){S14AffixState s;s14_affix_snapshot(&s);return officer==S14_ELITE_OFFICER && s.world==world && s.active;}
int s14_affix_display(uintptr_t world,int officer,const wchar_t *name,wchar_t *out,size_t cap){
    if(!out || !cap || !name)return 0;int active=s14_affix_active(world,officer),prefix=active && wcsncmp(name,L"神 ",2)!=0;
    size_t n=wcslen(name);if(n+(prefix?2:0)>=cap){out[0]=0;return 0;}
    if(prefix){out[0]=L'神';out[1]=L' ';wcscpy(out+2,name);}else wcscpy(out,name);return active;
}
void s14_affix_status(uintptr_t world,wchar_t *out,size_t cap){
    if(!out || !cap)return;S14AffixState s;s14_affix_snapshot(&s);
    if(!s.valid || s.suspended || (world && s.world!=world))swprintf(out,cap,L"百战精锐 · 等待载入存档战绩");
    else swprintf(out,cap,L"曹仁已记录斩敌 %llu / 5000（含伤兵） · %ls%ls",(unsigned long long)s.enemy_loss,s.active?L"已生效 · 神 曹仁":s.enemy_loss>=S14_ELITE_THRESHOLD?L"已达标 · 功能关闭":L"尚未达标",s.incomplete?L" · 记录存在缺口":L"");
    out[cap-1]=0;
}
