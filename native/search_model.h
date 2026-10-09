#ifndef S14_SEARCH_MODEL_H
#define S14_SEARCH_MODEL_H
#include <stdint.h>
#include <wchar.h>
#define S14_SEARCH_ITEMS 6001
#define S14_SEARCH_LINE 512
enum { S14_SEARCH_BEGIN=1,S14_SEARCH_RESULT,S14_SEARCH_END,S14_SEARCH_RESET,S14_SEARCH_FAULT,S14_SEARCH_TRACE };
enum { S14_SEARCH_NOTHING=0,S14_SEARCH_PERSON,S14_SEARCH_ITEM,S14_SEARCH_BOOK,S14_SEARCH_MONEY };
typedef struct {
    int kind,day,force,type,amount,dispatched,pending,outcome;
    unsigned int elapsed_ms;
    wchar_t detail[256];
    wchar_t actor[32],location[32];
} S14SearchEvent;
typedef struct {
    uintptr_t world;
    int initialized,latest_day,days[52];
    unsigned char used[52];
} S14SearchGuard;
typedef struct {
    int active,day,force,dispatched,completed,people,items,books,money,empty,pending,lines,truncated;
    int capacity;
    wchar_t (*details)[S14_SEARCH_LINE];
    unsigned char types[S14_SEARCH_ITEMS];
} S14SearchReport;
int s14_search_claim(S14SearchGuard *guard,uintptr_t world,int day,int force,int enabled,int confirmed);
int s14_search_budget(int orders,int cost,int candidates);
void s14_search_reduce(S14SearchReport *report,const S14SearchEvent *event);
void s14_search_format(const S14SearchReport *report,wchar_t summary[256],wchar_t details[4096]);
wchar_t *s14_search_popup_text(const S14SearchReport *report);
int s14_search_report_save(const wchar_t *root,const S14SearchReport *report);
void s14_search_report_read(const wchar_t *root,wchar_t summary[256],wchar_t details[4096]);
#endif
