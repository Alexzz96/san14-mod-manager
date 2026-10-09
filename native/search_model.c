#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "search_model.h"

int s14_search_claim(S14SearchGuard *g,uintptr_t world,int day,int force,int enabled,int confirmed) {
    if (!enabled || !confirmed || !world || day<0 || force<1 || force>51) return 0;
    if (!g->initialized || g->world!=world || day<g->latest_day) {
        memset(g,0,sizeof(*g)); g->initialized=1; g->world=world;
    }
    g->latest_day=day;
    if (g->used[force] && g->days[force]==day) return 0;
    g->used[force]=1; g->days[force]=day; return 1;
}
int s14_search_budget(int orders,int cost,int candidates) {
    if (orders<0 || orders>200 || cost<=0 || cost>200 || candidates<0 || candidates>6001) return 0;
    int budget=orders/cost; return budget<candidates?budget:candidates;
}
static void reset_report(S14SearchReport *r) {
    if (r->details) HeapFree(GetProcessHeap(),0,r->details);
    memset(r,0,sizeof(*r));
}
static void clean_text(wchar_t *out,size_t size,const wchar_t *text) {
    size_t at=0;
    for (size_t i=0;text[i] && at+1<size;i++) {
        // Native reports contain color commands such as ^05 and ^00.
        if (text[i]==L'^' && text[i+1]>=L'0' && text[i+1]<=L'9' && text[i+2]>=L'0' && text[i+2]<=L'9') { i+=2; continue; }
        wchar_t c=text[i]; out[at++]=c<L' '?L' ':c;
    }
    out[at]=0;
}
static int add_line(S14SearchReport *r,const S14SearchEvent *e) {
    if (r->lines>=S14_SEARCH_ITEMS) return 0;
    if (r->lines==r->capacity) {
        int capacity=r->capacity?r->capacity*2:32; if (capacity>S14_SEARCH_ITEMS) capacity=S14_SEARCH_ITEMS;
        size_t bytes=(size_t)capacity*sizeof(*r->details);
        void *memory=r->details?HeapReAlloc(GetProcessHeap(),0,r->details,bytes):HeapAlloc(GetProcessHeap(),0,bytes);
        if (!memory) return 0; r->details=memory; r->capacity=capacity;
    }
    wchar_t actor[32],location[32],result[300]; clean_text(actor,32,e->actor); clean_text(location,32,e->location);
    if (e->type==S14_SEARCH_NOTHING) wcscpy(result,L"探索失败（未发现任何收获）");
    else if (e->detail[0]) clean_text(result,300,e->detail);
    else if (e->type==S14_SEARCH_MONEY) swprintf(result,300,L"找到金钱 %d",e->amount);
    else wcscpy(result,e->type==S14_SEARCH_PERSON?L"发现武将":e->type==S14_SEARCH_ITEM?L"发现名品":L"发现战法书");
    r->types[r->lines]=(unsigned char)e->type;
    swprintf(r->details[r->lines++],S14_SEARCH_LINE,L"%ls → %ls：%ls",actor[0]?actor:L"未识别武将",location[0]?location:L"未识别地点",result);
    return 1;
}
void s14_search_reduce(S14SearchReport *r,const S14SearchEvent *e) {
    if (e->kind==S14_SEARCH_RESET) { reset_report(r); return; }
    if (e->kind==S14_SEARCH_BEGIN) {
        reset_report(r); r->active=1; r->day=e->day; r->force=e->force; r->dispatched=e->dispatched; r->pending=e->pending; return;
    }
    if (e->kind!=S14_SEARCH_RESULT || e->force!=r->force || !r->active) return;
    r->completed++;
    if (e->type==S14_SEARCH_PERSON) r->people++;
    else if (e->type==S14_SEARCH_ITEM) r->items++;
    else if (e->type==S14_SEARCH_BOOK) r->books++;
    else if (e->type==S14_SEARCH_MONEY && e->amount>0 && e->amount<=1000000 && r->money<=INT_MAX-e->amount) r->money+=e->amount;
    else r->empty++;
    if (!add_line(r,e)) r->truncated++;
}
void s14_search_format(const S14SearchReport *r,wchar_t summary[256],wchar_t details[4096]) {
    wchar_t pending[24]; if (r->pending<0) wcscpy(pending,L"未知"); else swprintf(pending,24,L"%d 人",r->pending);
    swprintf(summary,256,L"派遣 %d 人 · 完成 %d 次 · 武将 %d · 名品 %d · 战法书 %d · 金钱 %d\n探索失败 %d 次 · 在途 / 返回中 %ls",r->dispatched,r->completed,r->people,r->items,r->books,r->money,r->empty,pending);
    details[0]=0; size_t at=0;
    for (int i=0;i<r->lines;i++) {
        size_t length=wcslen(r->details[i]);
        if (at+length+40>=4096) { wcscat(details,L"\n更多明细见完整结果弹窗及搜索报告。"); break; }
        if (at) details[at++]=L'\n'; memcpy(details+at,r->details[i],length*sizeof(wchar_t)); at+=length; details[at]=0;
    }
    if (!r->lines) wcscpy(details,L"本回合没有完成的探索。长途任务在实际结束的回合计入。");
}
wchar_t *s14_search_popup_text(const S14SearchReport *r) {
    wchar_t summary[256],unused[4096]; s14_search_format(r,summary,unused);
    size_t size=wcslen(summary)+128;
    for (int i=0;i<r->lines;i++) size+=wcslen(r->details[i])+16;
    wchar_t *text=HeapAlloc(GetProcessHeap(),0,size*sizeof(wchar_t)); if (!text) return NULL;
    wcscpy(text,summary); wcscat(text,L"\n\n"); size_t at=wcslen(text);
    if (!r->lines) wcscat(text,L"本回合没有完成的探索。");
    for (int i=0;i<r->lines;i++) {
        int written=swprintf(text+at,size-at,L"%d. %ls\n",i+1,r->details[i]);
        if (written<0) { HeapFree(GetProcessHeap(),0,text); return NULL; } at+=(size_t)written;
    }
    if (r->truncated) wcscat(text,L"部分明细未能保存，请查看诊断日志。");
    return text;
}
static int report_path(const wchar_t *root,wchar_t path[MAX_PATH]) {
    if (wcslen(root)>MAX_PATH-44) return 0;
    swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager\\search-results.ini",root); return 1;
}
int s14_search_report_save(const wchar_t *root,const S14SearchReport *r) {
    wchar_t path[MAX_PATH],summary[256],details[4096],value[16]; if (!report_path(root,path)) return 0;
    if (GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES) {
        HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if (file==INVALID_HANDLE_VALUE) return 0;
        wchar_t bom=0xfeff; DWORD written; int ok=WriteFile(file,&bom,2,&written,NULL) && written==2; CloseHandle(file); if (!ok) return 0;
    }
    s14_search_format(r,summary,details);
    // INI values are single lines; the UI reconstructs the summary break.
    wchar_t *newline=wcschr(summary,L'\n'); if (newline) *newline=L'|';
    int old_count=(int)GetPrivateProfileIntW(L"SearchReport",L"Count",0,path);
    int ok=WritePrivateProfileStringW(L"SearchReport",L"Version",L"1",path) && WritePrivateProfileStringW(L"SearchReport",L"Summary",summary,path);
    swprintf(value,16,L"%d",r->lines); ok=WritePrivateProfileStringW(L"SearchReport",L"Count",value,path) && ok;
    for (int i=0;i<r->lines;i++) { wchar_t key[16]; swprintf(key,16,L"Item%02d",i); ok=WritePrivateProfileStringW(L"SearchReport",key,r->details[i],path) && ok; }
    if (old_count>S14_SEARCH_ITEMS) old_count=S14_SEARCH_ITEMS;
    for (int i=r->lines;i<old_count;i++) { wchar_t key[16]; swprintf(key,16,L"Item%02d",i); ok=WritePrivateProfileStringW(L"SearchReport",key,NULL,path) && ok; }
    return ok;
}
void s14_search_report_read(const wchar_t *root,wchar_t summary[256],wchar_t details[4096]) {
    wchar_t path[MAX_PATH]; if (!report_path(root,path)) return;
    GetPrivateProfileStringW(L"SearchReport",L"Summary",L"",summary,256,path);
    wchar_t *newline=wcschr(summary,L'|'); if (newline) *newline=L'\n'; details[0]=0;
    int count=(int)GetPrivateProfileIntW(L"SearchReport",L"Count",0,path); if (count<0 || count>S14_SEARCH_ITEMS) return;
    for (int i=0;i<count;i++) { wchar_t key[16],value[S14_SEARCH_LINE]; swprintf(key,16,L"Item%02d",i);
        GetPrivateProfileStringW(L"SearchReport",key,L"",value,S14_SEARCH_LINE,path);
        if (wcslen(details)+wcslen(value)+2>=4096) break;
        if (details[0]) wcscat(details,L"\n"); wcscat(details,value);
    }
}
