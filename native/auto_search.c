// Native calls execute only inside the player's confirmed strategy transition.
// Hooks enqueue bounded copies; the manager worker owns all UI and file I/O.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "MinHook.h"
#include "auto_search.h"
#include "battle_probe.h"
#include "turn_report.h"
#include "search_model.h"
#include "search_offsets.h"

#define QSIZE 512
static uintptr_t base_address,end_address;
static unsigned char **manager_address;
static int ready;
static volatile LONG search_flags,search_settings,search_fault,queue_sequence,queue_dropped;
static S14SearchGuard guard;
typedef struct { volatile LONG state; LONG sequence; S14SearchEvent event; } QueueSlot;
static QueueSlot queue[QSIZE];
typedef uintptr_t (*UpdateFunction)(void*,void*);
typedef void (*ProgressFunction)(int);
typedef int (*ExecuteFunction)(void*);
typedef void* (*PersonFunction)(void*,void*,int*,int);
typedef int (*TreasureFunction)(void*,void*,void**,void**,int*);
typedef int (*MoneyFunction)(void*,void*,int*);
typedef int (*OutcomeFunction)(void*,int,int,int);
// The native logger returns a log-record pointer. Keep every integer argument
// slot and its return value at Win64 width, including calls outside exploration.
typedef uintptr_t (*LogFunction)(void*,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,const wchar_t*,const wchar_t*,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static UpdateFunction original_update;
static ProgressFunction original_progress;
static ExecuteFunction original_execute;
static PersonFunction original_person;
static TreasureFunction original_treasure;
static MoneyFunction original_money;
static OutcomeFunction original_outcome;
static LogFunction original_log;
static _Thread_local void *current_user_state;
static _Thread_local S14SearchEvent *current_result;
static int last_planning_day=-1,last_force=-1;
static uintptr_t last_world;

static int readable(const void *pointer,size_t length) {
    uintptr_t at=(uintptr_t)pointer,limit=at+length;
    if (!pointer || limit<at) return 0;
    while (at<limit) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((void*)at,&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return 0;
        uintptr_t next=(uintptr_t)info.BaseAddress+info.RegionSize;
        if (next<=at) return 0; at=next<limit?next:limit;
    }
    return 1;
}
static int code_address(uintptr_t address) { return address>=base_address+0x1000 && address<end_address; }
static int writable(void *pointer,size_t length) {
    uintptr_t at=(uintptr_t)pointer,limit=at+length;
    if (!pointer || limit<at) return 0;
    while (at<limit) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((void*)at,&info,sizeof(info)) || info.State!=MEM_COMMIT ||
            (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
            !(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return 0;
        uintptr_t next=(uintptr_t)info.BaseAddress+info.RegionSize;
        if (next<=at) return 0; at=next<limit?next:limit;
    }
    return 1;
}
static void push(S14SearchEvent event) {
    LONG sequence=InterlockedIncrement(&queue_sequence);
    QueueSlot *slot=&queue[(unsigned int)sequence%QSIZE];
    if (InterlockedCompareExchange(&slot->state,1,0)!=0) { InterlockedIncrement(&queue_dropped); InterlockedExchange(&search_fault,1); return; }
    slot->sequence=sequence; slot->event=event; InterlockedExchange(&slot->state,2);
}
static void fail(const wchar_t *text) {
    if (InterlockedCompareExchange(&search_fault,1,0)) return;
    S14SearchEvent e={0}; e.kind=S14_SEARCH_FAULT; wcsncpy(e.detail,text,255); push(e);
}
static int world_clock(unsigned char **world,unsigned char **settings,int *day) {
    if (!manager_address || !readable(manager_address,sizeof(void*)) || !*manager_address) return 0;
    unsigned char *g=*manager_address;
    if (!readable(g+0x85130,sizeof(void*))) return 0;
    unsigned char *s=*(unsigned char**)(g+0x85130);
    if (!readable(s,0x3b)) return 0;
    int serial=((int(*)(void*))(base_address+0x2eff90))(s+0x34);
    if (serial<0) return 0;
    *world=g; *settings=s; *day=serial; return 1;
}
static int context(unsigned char **world,unsigned char **settings,int *force,int *day) {
    if (!world_clock(world,settings,day)) return 0;
    int id=(*settings)[0x3a]; if (id<1 || id>51) return 0; *force=id; return 1;
}
static void* list_head(void *handle,int *count) {
    if (!readable(handle,4)) return NULL;
    unsigned int id=*(unsigned int*)handle,capacity=*(unsigned int*)(base_address+0x201c390);
    void **heads=*(void***)(base_address+0x201c360);
    uintptr_t *counts=*(uintptr_t**)(base_address+0x201c378);
    if (id>=capacity || !readable(heads+id,sizeof(void*)) || !readable(counts+id,sizeof(uintptr_t))) return NULL;
    uintptr_t total=counts[id]; if (total>6001) return NULL;
    if (count) *count=(int)total; return heads[id];
}
static int pending_searches(unsigned char *g,int force) {
    if (!readable(g+0x85128,sizeof(void*))) return -1;
    unsigned char *commands=*(unsigned char**)(g+0x85128);
    if (!readable(commands,0x20)) return -1;
    void *node=list_head(*(void**)(commands+0x18),NULL); int total=0;
    for (int i=0;node && i<6001;i++) {
        if (!readable(node,16)) return -1;
        unsigned char *command=*(unsigned char**)node;
        if (readable(command,0x41) && *(uintptr_t*)command==base_address+0x129bf20 && command[0x28]==force) total++;
        node=*((void**)node+1);
    }
    return node?-1:total;
}
#define SEARCH_PEOPLE 6001
typedef struct { unsigned char *person; int days; unsigned short marked; } PersonScratch;
typedef struct {
    void ***slot;
    void **previous,**targets;
    PersonScratch *people;
    int count,bound;
} SearchWorkspace;
static void workspace_end(SearchWorkspace *workspace) {
    if (workspace->bound) {
        if (*workspace->slot==workspace->targets) *workspace->slot=workspace->previous;
        else fail(L"探索临时表被其他模块替换，自动搜索已停止。");
        MemoryBarrier();
    }
    for (int i=0;i<workspace->count;i++) {
        PersonScratch *saved=&workspace->people[i];
        if (!writable(saved->person+0x196,0x62)) { fail(L"探索临时状态无法恢复，自动搜索已停止。"); continue; }
        unsigned short *flags=(unsigned short*)(saved->person+0x196);
        *flags=(*flags&~0x40u)|saved->marked;
        *(int*)(saved->person+0x1f4)=saved->days;
    }
    if (workspace->people) HeapFree(GetProcessHeap(),0,workspace->people);
    if (workspace->targets) HeapFree(GetProcessHeap(),0,workspace->targets);
    memset(workspace,0,sizeof(*workspace));
}
static int workspace_begin(SearchWorkspace *workspace,unsigned char *g) {
    // Native CStrategySearchState allocates exactly 0xBB88 = 6001 * 8 bytes
    // at 0x66C2F9 and frees that global table at 0x668612. Auto-search is called
    // without that UI, so provide a private, scoped table and restore its owner.
    workspace->slot=(void***)(base_address+0x1fc9520);
    if (!writable(workspace->slot,sizeof(void*)) || !readable(g+0x100,sizeof(void*))) return 0;
    int count=-1; void *node=list_head(*(void**)(g+0x100),&count);
    if (count<0 || (count && !node)) return 0;
    workspace->targets=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,SEARCH_PEOPLE*sizeof(void*));
    if (!workspace->targets) return 0;
    if (count) {
        workspace->people=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(size_t)count*sizeof(PersonScratch));
        if (!workspace->people) return 0;
    }
    for (int i=0;i<count;i++) {
        if (!readable(node,16)) return 0;
        unsigned char *person=*(unsigned char**)node;
        if (!readable(person,0x1f8) || !writable(person+0x196,0x62) || *(unsigned short*)(person+0x10)>=SEARCH_PEOPLE) return 0;
        workspace->people[workspace->count++]=(PersonScratch){person,*(int*)(person+0x1f4),*(unsigned short*)(person+0x196)&0x40};
        node=*((void**)node+1);
    }
    if (node) return 0;
    workspace->previous=*workspace->slot; *workspace->slot=workspace->targets;
    workspace->bound=1; MemoryBarrier(); return 1;
}
static void trace_dispatch(int phase,int day,int force,ULONGLONG started) {
    static const wchar_t *labels[]={L"正在根据设置选择空闲武将。",L"探索路线已准备。",L"原生探索候选表已生成。",L"探索派遣已完成。",L"探索临时数据已恢复。"};
    S14SearchEvent event={0}; event.kind=S14_SEARCH_TRACE; event.type=phase; event.day=day; event.force=force;
    event.elapsed_ms=(unsigned int)(GetTickCount64()-started); wcscpy(event.detail,labels[phase]); push(event);
}
typedef struct { uintptr_t *vt; void **current,**end; int force; int padding; } CityIterator;
// Native iterator predicate: RCX is the iterator, RDX is the current slot.
// 0x2034E0 and native loop 0x67AA1E both pass this second argument explicitly.
typedef int (*CityPredicate)(CityIterator*,void**);
_Static_assert(sizeof(CityIterator)==32,"Native city iterator size");
_Static_assert(offsetof(CityIterator,current)==8,"Native city cursor offset");
_Static_assert(offsetof(CityIterator,force)==24,"Native city force offset");
static int cities(void *data,void *out[52]) {
    CityIterator it={0};
    ((void*(*)(void*,void*))(base_address+0x2034e0))(data,&it);
    uintptr_t begin=(uintptr_t)it.current,end=(uintptr_t)it.end;
    if (!it.vt || !readable(it.vt,16) || !code_address(it.vt[1]) || end<begin || (end-begin)%8 || (end-begin)/8>52) return -1;
    int count=0;
    while (it.current!=it.end) {
        if (!readable(it.current,8) || !readable(*it.current,0x20)) return -1;
        if (count==52) return -1; out[count++]=*it.current;
        do {
            it.current++;
            if (it.current==it.end) break;
            if (!readable(it.current,8) || !readable(*it.current,0x20)) return -1;
        } while (!((CityPredicate)it.vt[1])(&it,it.current));
    }
    return count;
}
typedef struct { void *head; size_t size; } RouteMap;
static int insert_route(RouteMap *map,void *city,int days) {
    unsigned char *head=map->head,*hint=head,*node=*(unsigned char**)(head+8);
    int visits=0;
    while (!node[0x19]) {
        if (++visits>64) return 0;
        if (*(uintptr_t*)(node+0x20)>=(uintptr_t)city) { hint=node; node=*(unsigned char**)node; }
        else node=*(unsigned char**)(node+0x10);
    }
    if (hint==head || (uintptr_t)city<*(uintptr_t*)(hint+0x20)) {
        void *key=city,*key_pointer=&key,*unused=NULL;
        node=((void*(*)(void*,void*,void*,void*))(base_address+0x1575b0))(map,NULL,&key_pointer,&unused);
        if (!node) return 0;
        void *result=NULL;
        ((void*(*)(void*,void*,void*,void*,void*))(base_address+0x158330))(map,&result,hint,node+0x20,node);
        if (!result) return 0; hint=result;
    }
    *(int*)(hint+0x28)=days; return 1;
}
static int build_routes(void *force_data,RouteMap maps[52],int *initialized) {
    void *owned[52]; int count=cities(force_data,owned); if (count<1) return count;
    for (int i=0;i<count;i++) {
        ((void*(*)(void*))(base_address+0x159880))(&maps[i]);
        if (!maps[i].head) return -1; (*initialized)++;
        for (int j=0;j<count;j++) {
            if (i==j) continue;
            uintptr_t *a=*(uintptr_t**)owned[i],*b=*(uintptr_t**)owned[j];
            if (!readable(a,0x88) || !readable(b,0x88) || !code_address(a[16]) || !code_address(b[16])) return -1;
            unsigned char *left=((void*(*)(void*))a[16])(owned[i]);
            unsigned char *right=((void*(*)(void*))b[16])(owned[j]);
            if (!readable(left,0x12) || !readable(right,0x12)) return -1;
            if (left[0x11]!=1 && right[0x11]!=1) continue;
            int distance=((int(*)(void*,void*))(base_address+0x27e6d0))(owned[i],owned[j]);
            int days=((int(*)(int,void*,void*))(base_address+0x20e710))(distance,NULL,force_data);
            if (!insert_route(&maps[i],owned[j],days)) return -1;
        }
    }
    return count;
}
static void dispatch_search(void) {
    unsigned int flags=(unsigned int)InterlockedCompareExchange(&search_flags,0,0);
    if (!(flags&S14_AUTO_SEARCH) || !(flags&S14_MASTER) || !ready || InterlockedCompareExchange(&search_fault,0,0)) return;
    unsigned char *g,*s; int id,day;
    if (!context(&g,&s,&id,&day)) { fail(L"无法读取当前势力或日期，自动搜索已停止；原生探索仍可用。"); return; }
    if (!s14_search_claim(&guard,(uintptr_t)s,day,id,1,1)) return;
    if (!readable(g+0xde40,52*8) || !readable(g+0xdca0,52*8)) { fail(L"势力数据不可用，自动搜索已停止。"); return; }
    unsigned char *force=((unsigned char**)(g+0xde40))[id];
    void *data=((void**)(g+0xdca0))[id];
    if (!readable(force,0x15) || force[0x10]!=id || !readable(data,0x60)) { fail(L"势力记录不符合派遣条件，自动搜索已停止。"); return; }
    int cost=((int(*)(void))(base_address+0x62ab00))();
    int orders=force[0x14]; if (cost<=0 || cost>200 || orders>200) { fail(L"探索政令费用异常，自动搜索已停止。"); return; }
    S14SearchEvent event={0}; event.kind=S14_SEARCH_BEGIN; event.force=id; event.day=day;
    ULONGLONG started=GetTickCount64();
    RouteMap maps[52]={{0}}; int initialized=0;
    SearchWorkspace workspace={0};
    unsigned char state[0x4d0]={0};
    void *list[2]={(void*)(base_address+0x123f448),NULL};
    if (orders<cost) { event.pending=pending_searches(g,id); wcscpy(event.detail,L"剩余政令不足，未派遣新的探索。"); push(event); return; }
    trace_dispatch(0,day,id,started);
    int count=build_routes(data,maps,&initialized);
    if (count<0) { fail(L"探索路线数据异常，自动搜索已停止。"); goto cleanup; }
    if (!count) { wcscpy(event.detail,L"没有可探索的城市，未派遣。"); goto completed; }
    if (!workspace_begin(&workspace,g)) { fail(L"无法准备原生探索临时表，自动搜索已停止；未提交探索命令。"); goto cleanup; }
    trace_dispatch(1,day,id,started);
    *(void**)(state+0x470)=force; *(void**)(state+0x478)=maps;
    ((void(*)(void*))(base_address+0x17fb60))(list);
    if (!list[1]) { fail(L"无法创建原生探索候选表。"); goto cleanup; }
    unsigned int settings=(unsigned int)InterlockedCompareExchange(&search_settings,0,0);
    ((void(*)(void*,void*,int,int,int))(base_address+0x65cf80))(state,list,settings&15,(settings>>4)&15,(settings>>8)&15);
    trace_dispatch(2,day,id,started);
    int candidates=0; void *node=list_head(list[1],&candidates);
    int budget=s14_search_budget(orders,cost,candidates);
    for (int i=0;node && i<budget;i++) {
        if (!readable(node,16)) { fail(L"探索候选列表异常，后续派遣已停止。"); break; }
        unsigned char *person=*(unsigned char**)node; node=*((void**)node+1);
        if (!readable(person,0x198) || force[0x14]<cost || (*(unsigned short*)(person+0x196)&1)) continue;
        int person_id=*(unsigned short*)(person+0x10); if (person_id>6000) { fail(L"探索武将编号异常。"); break; }
        // The PE global contains a table pointer, not an inline array.
        void *city=workspace.targets[person_id];
        void *target=((void*(*)(void*,void*,void*))(base_address+0x631aa0))(force,person,city);
        if (!target || !((int(*)(void*))(base_address+0x2f29d0))(target)) continue;
        void *arguments[]={target,person};
        int modifier=((int(*)(void*))(base_address+0x210cd0))(force);
        ((void(*)(void*,int))(base_address+0x1d6c30))(arguments,modifier);
        if (*(unsigned short*)(person+0x196)&1) event.dispatched++;
    }
    if (event.dispatched) swprintf(event.detail,256,L"已按原生探索规则派遣 %d 名武将。",event.dispatched);
    else wcscpy(event.detail,L"没有满足当前设置的空闲武将，未派遣新的探索。");
    trace_dispatch(3,day,id,started);
completed:
    event.pending=pending_searches(g,id); event.elapsed_ms=(unsigned int)(GetTickCount64()-started); push(event);
cleanup:
    if (list[1]) ((void(*)(void*))(base_address+0x5d30f0))(list);
    for (int i=0;i<initialized;i++) ((void(*)(void*))(base_address+0x159fa0))(&maps[i]);
    workspace_end(&workspace);
    trace_dispatch(4,day,id,started);
}
static void planning(void *state) {
    if (!readable(state,0x474) || *(int*)((unsigned char*)state+0x470)>2) return;
    if (!(InterlockedCompareExchange(&search_flags,0,0)&S14_AUTO_SEARCH)) return;
    unsigned char *g,*s; int force,day; if (!context(&g,&s,&force,&day)) return;
    if (last_planning_day==day && last_force==force && last_world==(uintptr_t)s) return;
    if (last_world && (last_world!=(uintptr_t)s || day<last_planning_day || force!=last_force)) push((S14SearchEvent){.kind=S14_SEARCH_RESET});
    else if (last_planning_day>=0) push((S14SearchEvent){.kind=S14_SEARCH_END,.day=day,.force=force,.pending=pending_searches(g,force)});
    push((S14SearchEvent){.kind=S14_SEARCH_BEGIN,.day=day,.force=force,.pending=pending_searches(g,force)});
    last_planning_day=day; last_force=force; last_world=(uintptr_t)s;
}
static uintptr_t hooked_update(void *state,void *arg) {
    void *previous=current_user_state; current_user_state=state;
    if (s14_battle_enabled() && readable(state,0x474) && *(int*)((unsigned char*)state+0x470)<=2) {
        unsigned char *g,*s;int day;
        if (world_clock(&g,&s,&day)) s14_battle_planning((uintptr_t)g,day,s[0x3a]);
    }
    planning(state); uintptr_t result=original_update(state,arg); current_user_state=previous; return result;
}
static void hooked_progress(int value) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-base_address;
    // This call exists only in the accepted progress branch, after the native
    // confirmation flag has been consumed and before strategy phase cleanup.
    if (!value && caller==0x3f9446 && current_user_state) { s14_battle_progress();dispatch_search(); }
    original_progress(value);
}
static int hooked_execute(void *command) {
    S14SearchEvent event={0}; S14SearchEvent *previous=current_result;
    unsigned char *g,*s; int day;
    unsigned int flags=(unsigned int)InterlockedCompareExchange(&search_flags,0,0);
    int capture=(flags&(S14_MASTER|S14_AUTO_SEARCH))==(S14_MASTER|S14_AUTO_SEARCH) && last_force>0 && world_clock(&g,&s,&day) &&
        (uintptr_t)s==last_world && readable(command,0x29) && ((unsigned char*)command)[0x28]==last_force;
    if (capture) { event.kind=S14_SEARCH_RESULT; event.force=last_force; event.day=day; current_result=&event; }
    int result=original_execute(command); current_result=previous; return result;
}
static size_t name_part(wchar_t out[32],size_t at,const unsigned char *pointer,size_t characters) {
    if (!readable(pointer,characters*sizeof(wchar_t))) return at;
    const wchar_t *text=(const wchar_t*)pointer;
    for (size_t i=0;i<characters && text[i] && at<31;i++) {
        if (text[i]<L' ') break; out[at++]=text[i];
    }
    out[at]=0; return at;
}
static void result_identity(void *person,void *target) {
    if (!current_result) return;
    unsigned char *p=person,*area=target;
    // CPersonData's native formatter 0x20C270 concatenates the two fixed
    // nine-WCHAR strings at +0x12 and +0x24 (%s%s). The area-name lookup at
    // 0x20F3D2 returns CAreaData +0x10. Copy names, never call a new game API.
    if (readable(p,0x36) && *(uintptr_t*)p==base_address+0x12a00d0) {
        size_t at=name_part(current_result->actor,0,p+0x12,9); name_part(current_result->actor,at,p+0x24,9);
    }
    if (readable(area,0x34) && *(uintptr_t*)area==base_address+0x129ff98) name_part(current_result->location,0,area+0x10,18);
}
static void* hooked_person(void *person,void *target,int *roll,int mode) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-base_address;
    if (current_result && caller==0x1c9378) result_identity(person,target);
    void *found=original_person(person,target,roll,mode);
    if (current_result && caller==0x1c9378 && found && ((int(*)(void*))(base_address+0x2f29d0))(found)) current_result->type=S14_SEARCH_PERSON;
    return found;
}
static int hooked_treasure(void *person,void *target,void **item,void **book,int *roll) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-base_address;
    if (current_result && caller==0x1c9a0d) result_identity(person,target);
    int result=original_treasure(person,target,item,book,roll);
    if (current_result && caller==0x1c9a0d && result) {
        if (item && *item) current_result->type=S14_SEARCH_ITEM;
        else if (book && *book) current_result->type=S14_SEARCH_BOOK;
    }
    return result;
}
static int hooked_money(void *person,void *target,int *amount) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-base_address;
    if (current_result && caller==0x1c9e14) result_identity(person,target);
    int result=original_money(person,target,amount);
    if (current_result && caller==0x1c9e14 && result && amount && *amount>0) { current_result->type=S14_SEARCH_MONEY; current_result->amount=*amount; }
    return result;
}
static void copy_text(wchar_t out[256],const wchar_t *text) {
    if (!text) return;
    for (int i=0;i<255;i++) { if (!readable(text+i,2)) break; wchar_t c=text[i]; if (!c) { out[i]=0; return; } out[i]=c<L' '?L' ':c; out[i+1]=0; }
}
static uintptr_t hooked_log(void *log,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,const wchar_t *short_text,const wchar_t *long_text,uintptr_t f,uintptr_t h,uintptr_t i,uintptr_t j) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-base_address;
    uintptr_t arguments[10]={(uintptr_t)log,a,b,c,d,e,f,h,i,j};
    s14_battle_log(caller,arguments,short_text,long_text);
    if (current_result && (caller==0x1c9718 || caller==0x1c99d0 || caller==0x1c9df1 || caller==0x1c9ff4))
        copy_text(current_result->detail,long_text?long_text:short_text);
    return original_log(log,a,b,c,d,e,short_text,long_text,f,h,i,j);
}
static int hooked_outcome(void *person,int group,int outcome,int option) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0)-base_address;
    if (current_result && caller==0x1ca290 && group==3) { current_result->outcome=outcome; push(*current_result); }
    return original_outcome(person,group,outcome,option);
}
static void *detours[]={hooked_update,hooked_progress,hooked_execute,hooked_person,hooked_treasure,hooked_money,hooked_outcome,hooked_log};
static void **originals[]={(void**)&original_update,(void**)&original_progress,(void**)&original_execute,(void**)&original_person,
    (void**)&original_treasure,(void**)&original_money,(void**)&original_outcome,(void**)&original_log};
int s14_search_install(uintptr_t base,uintptr_t end,unsigned char **manager) {
    base_address=base; end_address=end; manager_address=manager;
    for (size_t i=0;i<sizeof(search_entries)/sizeof(search_entries[0]);i++) {
        const S14SearchEntry *entry=&search_entries[i];
        if (entry->rva>end-base || entry->length>end-base-entry->rva || !readable((void*)(base+entry->rva),entry->length) ||
            memcmp((void*)(base+entry->rva),entry->signature,entry->length)) return 0;
    }
    const uintptr_t tables[]={0x1fc9520,0x201c360,0x201c378,0x201c390};
    for (int i=0;i<4;i++) if (base+tables[i]>=end || !readable((void*)(base+tables[i]),8)) return 0;
    // Validate the confirmation call's destination, independently of version strings.
    unsigned char *call=(unsigned char*)(base+0x3f9441); int32_t displacement; memcpy(&displacement,call+1,4);
    if (*call!=0xe8 || (uintptr_t)(call+5)+displacement!=base+0x3fcf20) return 0;
    int created=0;
    for (int i=0;i<8;i++) {
        if (MH_CreateHook((void*)(base+search_entries[i].rva),detours[i],originals[i])!=MH_OK) {
            for (int j=0;j<created;j++) MH_RemoveHook((void*)(base+search_entries[j].rva)); return 0;
        }
        created++;
    }
    ready=1; return 1;
}
void s14_search_configure(unsigned int flags,unsigned int settings) {
    if ((settings&15)>3 || ((settings>>4)&15)>2 || ((settings>>8)&15)>1) settings=0;
    InterlockedExchange(&search_settings,(LONG)settings); InterlockedExchange(&search_flags,(LONG)flags);
}
int s14_search_is_ready(void) { return ready && !InterlockedCompareExchange(&search_fault,0,0); }
static int compare_events(const void *a,const void *b) { LONG x=((const QueueSlot*)a)->sequence,y=((const QueueSlot*)b)->sequence; return (x>y)-(x<y); }
static void log_event(HANDLE log,const S14SearchEvent *e) {
    char text[4096],json[4608]; int at=0; char utf8[1024]={0};
    WideCharToMultiByte(CP_UTF8,0,e->detail,-1,utf8,sizeof(utf8),NULL,NULL);
    for (int i=0;utf8[i] && at<(int)sizeof(text)-8;i++) {
        unsigned char c=(unsigned char)utf8[i]; if (c=='"' || c=='\\') text[at++]='\\';
        if (c>=32) text[at++]=(char)c;
    }
    text[at]=0;
    // Names come from fixed in-game records; JSON-escape them independently.
    char names[2][512]={{0}};
    const wchar_t *sources[]={e->actor,e->location};
    for (int index=0;index<2;index++) {
        char raw[128]={0}; int length=WideCharToMultiByte(CP_UTF8,0,sources[index],-1,raw,sizeof(raw),NULL,NULL); int used=0;
        if (length) for (int i=0;raw[i] && used<500;i++) { unsigned char c=(unsigned char)raw[i]; if (c=='"' || c=='\\') names[index][used++]='\\'; if (c>=32) names[index][used++]=c; }
    }
    int length=snprintf(json,sizeof(json),"{\"event\":\"auto_search\",\"kind\":%d,\"day\":%d,\"force\":%d,\"type\":%d,\"money\":%d,\"dispatched\":%d,\"pending\":%d,\"outcome\":%d,\"elapsed_ms\":%u,\"actor\":\"%s\",\"location\":\"%s\",\"detail\":\"%s\"}\n",
        e->kind,e->day,e->force,e->type,e->amount,e->dispatched,e->pending,e->outcome,e->elapsed_ms,names[0],names[1],text);
    LARGE_INTEGER size;
    if (log!=INVALID_HANDLE_VALUE && length>0 && length<(int)sizeof(json) && GetFileSizeEx(log,&size) && size.QuadPart+length<=16*1024*1024) { DWORD written; WriteFile(log,json,(DWORD)length,&written,NULL); }
}
void s14_search_worker(S14ManagerUI *ui,S14Toast *toast,HINSTANCE instance,HWND owner,HANDLE log) {
    static S14SearchReport report;
    QueueSlot events[QSIZE]; int count=0,changed=0;
    for (int i=0;i<QSIZE;i++) if (InterlockedCompareExchange(&queue[i].state,3,2)==2) {
        events[count].sequence=queue[i].sequence; events[count++].event=queue[i].event; InterlockedExchange(&queue[i].state,0);
    }
    qsort(events,count,sizeof(events[0]),compare_events);
    for (int i=0;i<count;i++) {
        S14SearchEvent *e=&events[i].event; log_event(log,e);
        if (e->kind==S14_SEARCH_FAULT) { wcsncpy(ui->search_status,e->detail,191); ui->search_status[191]=0; changed=1; continue; }
        if (e->kind==S14_SEARCH_TRACE) {
            if (e->type==0) {
                RECT bounds; POINT point={0,0}; if (GetWindowRect(owner,&bounds)) point=(POINT){bounds.left+(bounds.right-bounds.left)/2-230,bounds.top+80};
                s14_toast_text(toast,instance,owner,point,GetTickCount64(),L"开始自动搜索",e->detail,2000);
                wcsncpy(ui->search_status,e->detail,191); ui->search_status[191]=0; changed=1;
            }
            continue;
        }
        if (e->kind==S14_SEARCH_BEGIN) {
            if (e->detail[0]) {
                wcsncpy(ui->search_status,e->detail,191); ui->search_status[191]=0; changed=1;
            }
            s14_search_reduce(&report,e);
        } else if (e->kind==S14_SEARCH_END && report.active && e->force==report.force) {
            report.pending=e->pending; s14_search_format(&report,ui->search_summary,ui->search_details);
            s14_search_report_save(ui->root,&report); ui->search_scroll=0; changed=1;
            wchar_t *message=s14_search_popup_text(&report);
            if (message) {
                s14_turn_report_search(report.day,e->day,message);HeapFree(GetProcessHeap(),0,message);
                S14ReportSearch view={.start=report.day,.end=e->day,.force=report.force,.available=1,.completed=report.completed,
                    .empty=report.empty,.people=report.people,.items=report.items,.books=report.books,.money=report.money,
                    .pending=report.pending,.truncated=report.truncated,.line_count=report.lines};
                if(!report.lines || (view.lines=calloc((size_t)report.lines,sizeof(*view.lines)))){
                    for(int n=0;n<report.lines;n++){view.lines[n].type=report.types[n];memcpy(view.lines[n].text,report.details[n],sizeof(view.lines[n].text));}
                    s14_turn_report_search_data(&view);s14_report_search_free(&view);
                }
            }
        } else {
            s14_search_reduce(&report,e);
            if (e->kind==S14_SEARCH_RESET) { s14_turn_report_reset();ui->search_summary[0]=ui->search_details[0]=0; ui->search_scroll=0; changed=1; }
        }
    }
    if (!ready) { wcscpy(ui->search_status,L"探索入口未通过指令校验，自动搜索不可用；原有墙体功能独立运行。"); changed=1; }
    if (queue_dropped) { wcscpy(ui->search_status,L"探索记录队列已满，统计可能不完整；自动派遣已停止，重启后重试。"); changed=1; }
    if (changed) s14_manager_refresh(ui);
    s14_toast_tick(toast,GetTickCount64(),owner && (GetForegroundWindow()==owner || GetForegroundWindow()==ui->window));
}
#ifdef S14_SELFTEST
__declspec(dllexport) int S14TestSearchPrologue(int index,unsigned char *bytes,int *length) {
    if (index<0 || index>=8) return 0;
    *length=search_entries[index].length; memcpy(bytes,search_entries[index].signature,*length); return (int)search_entries[index].rva;
}
#endif
