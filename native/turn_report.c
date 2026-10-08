#include "turn_report.h"
#include <wchar.h>
#include <string.h>
typedef struct { int start,end;wchar_t *text; } Component;
static Component search_report={-1,-1,NULL},battle_report={-1,-1,NULL};
static int pending_start=-1,pending_end=-1,shown_start=-1,shown_end=-1;
static ULONGLONG changed_at;
static unsigned int scope_epoch;
unsigned int s14_turn_report_epoch(void) {return scope_epoch;}
void s14_turn_report_reset(void) {
    scope_epoch++;
    if(search_report.text) HeapFree(GetProcessHeap(),0,search_report.text);
    if(battle_report.text) HeapFree(GetProcessHeap(),0,battle_report.text);
    search_report=(Component){-1,-1,NULL};battle_report=(Component){-1,-1,NULL};
    pending_start=pending_end=shown_start=shown_end=-1;changed_at=0;
}
static void publish(Component *c,int start,int end,const wchar_t *text) {
    if(!text || start<0 || end<=start) return;
    size_t n=wcslen(text);if(n>6001ull*512+4096) return;
    wchar_t *copy=HeapAlloc(GetProcessHeap(),0,(n+1)*sizeof(wchar_t));if(!copy) return;
    memcpy(copy,text,(n+1)*sizeof(wchar_t));if(c->text) HeapFree(GetProcessHeap(),0,c->text);
    *c=(Component){start,end,copy};pending_start=start;pending_end=end;changed_at=GetTickCount64();
}
void s14_turn_report_search(int start,int end,const wchar_t *text) {publish(&search_report,start,end,text);}
void s14_turn_report_battle(int start,int end,const wchar_t *text) {publish(&battle_report,start,end,text);}
int s14_turn_report_take(ULONGLONG now,const wchar_t **search,const wchar_t **battle) {
    if(pending_start<0 || now<changed_at || now-changed_at<250 || (shown_start==pending_start && shown_end==pending_end)) return 0;
    *search=search_report.start==pending_start && search_report.end==pending_end?search_report.text:L"本回合没有已采集的探索结果。\n自动探索未开启或本回合未采集到完整结果。";
    *battle=battle_report.start==pending_start && battle_report.end==pending_end?battle_report.text:L"本回合没有已采集的战斗报告。\n请在 F10 → 更多拓展开启战斗数据记录；开启后的完整回合才会统计。";
    shown_start=pending_start;shown_end=pending_end;return 1;
}
