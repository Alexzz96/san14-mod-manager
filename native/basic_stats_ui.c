#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "MinHook.h"
#include "basic_stats_ui.h"
typedef struct {void *page,*extra;uintptr_t person;int active;ULONGLONG next;} Pane;
typedef struct {uintptr_t page,person,world;S14BattleTotals totals;int connected,incomplete,bound;uint64_t merit;ULONGLONG time;} Snapshot;
static Pane panes[8];
static SRWLOCK lock=SRWLOCK_INIT;
static Snapshot snapshot;
static uintptr_t visible_page,visible_person,image;
static void *(*allocate)(size_t),*(*widget_ctor)(void*);
static void (*resource_ctor)(void*);
static void (*widget_visible)(void*,int);
static void *(*native_context)(void);
static int (*resource_init)(void*,int,int,void*,const char*,int,void*);
static void (*layout_visible)(void*,int,int),(*layout_text)(void*,int,const wchar_t*,int),(*layout_style)(void*,int,int);
static void (*native_update)(void*),(*native_show)(void*),(*native_hide)(void*),(*native_cleanup)(void*,void*);
static uintptr_t (*native_refresh)(void*,void*);
static void *(*native_destroy)(void*,unsigned);
static uintptr_t read_pointer(uintptr_t p){uintptr_t v=0;SIZE_T got;ReadProcessMemory(GetCurrentProcess(),(void*)p,&v,8,&got);return v;}
static int read_int(uintptr_t p){int v=0;SIZE_T got;ReadProcessMemory(GetCurrentProcess(),(void*)p,&v,4,&got);return v;}
static void call(void *p,int slot){((void(*)(void*))(*(uintptr_t**)p)[slot])(p);}
static Pane *find(void *page){for(int i=0;i<8;i++)if(panes[i].page==page)return &panes[i];return NULL;}
void s14_basic_stats_publish(uintptr_t page,uintptr_t person,uintptr_t world,const S14BattleTotals *totals,int connected,int incomplete,int bound,uint64_t merit){
 AcquireSRWLockExclusive(&lock);snapshot=(Snapshot){.page=page,.person=person,.world=world,.connected=connected,.incomplete=incomplete,.bound=bound,.merit=merit,.time=GetTickCount64()};
 if(totals)snapshot.totals=*totals;ReleaseSRWLockExclusive(&lock);
}
int s14_basic_stats_embedded(uintptr_t page,uintptr_t person){
 AcquireSRWLockShared(&lock);int yes=page&&person&&visible_page==page&&visible_person==person;ReleaseSRWLockShared(&lock);return yes;
}
static void announce(Pane *p,int show){
 AcquireSRWLockExclusive(&lock);
 if(show){visible_page=(uintptr_t)p->page;visible_person=p->person;}
 else if(visible_page==(uintptr_t)p->page){visible_page=visible_person=0;}
 ReleaseSRWLockExclusive(&lock);
}
static void release(Pane *p,void *resources){
 announce(p,0);
 if(p->extra){widget_visible(p->extra,0);if(p->active)call(p->extra,8);uintptr_t *v=*(uintptr_t**)p->extra;
  ((void(*)(void*,void*))v[5])(p->extra,resources);((void*(*)(void*,unsigned))v[0])(p->extra,1);}
 memset(p,0,sizeof(*p));
}
static void *resources(void){void *ctx=native_context();return (void*)read_pointer((uintptr_t)ctx+0x210);}
/* Reuse the actual basic-page row resource, with its own native ownership.
   Only the task-row subtree and its ancestors remain visible; no shared
   graphic pointer or game resource is modified. Moving it down by 120 puts
   its three rows below the original three at the native 40-unit pitch. */
static int keep(int i){return i==0||i==55||i==87||(i>=88&&i<=102);}
static Pane *create(void *page){
 if(read_int((uintptr_t)page+0x138)!=105)return NULL;
 Pane *p=NULL;for(int i=0;i<8;i++)if(!panes[i].page){p=&panes[i];break;}if(!p)return NULL;
 p->page=page;p->extra=allocate(0x168);if(!p->extra){memset(p,0,sizeof(*p));return NULL;}
 memset(p->extra,0,0x168);widget_ctor(p->extra);*(uintptr_t*)p->extra=image+0x123e360;resource_ctor((char*)p->extra+0x128);
 int ok=resource_init(p->extra,0,120,page,"11_01_02_00_info_busho0",0,NULL);
 if(!ok||read_int((uintptr_t)p->extra+0x138)!=105){release(p,resources());return NULL;}
 for(int i=0;i<105;i++)layout_visible((char*)p->extra+0x128,i,keep(i));
 for(int i=91;i<=101;i+=5)layout_style((char*)p->extra+0x128,i,13);
 *(unsigned*)((char*)p->extra+0x40)|=8;
 widget_visible(p->extra,0);
 return p;
}
static void render(Pane *p){
 Snapshot s;AcquireSRWLockShared(&lock);s=snapshot;ReleaseSRWLockShared(&lock);
 int fresh=s.page==(uintptr_t)p->page&&s.person==p->person&&s.world==read_pointer(image+0x1fc91d0)&&GetTickCount64()-s.time<1500;
 const wchar_t *names[]={L"杀敌总数",L"击溃部队",L"自身覆灭",L"兵力损失",L"击伤武将",L"军功"};
 unsigned masks[]={S14_STATS_DAMAGE,S14_STATS_ROUTS,S14_STATS_DEFEATS,S14_STATS_LOSSES,S14_STATS_INJURIES};
 uint64_t values[]={s.totals.enemy_loss,s.totals.units_routed,s.totals.units_defeated,s.totals.own_loss,s.totals.officers_injured};
 for(int i=0;i<6;i++){
  void *layout=(char*)(i<3?p->page:p->extra)+0x128;int label=91+5*(i%3);wchar_t value[32];
  if(i==5){if(fresh&&s.connected)swprintf(value,32,L"%llu",(unsigned long long)s.merit);else wcscpy(value,L"未采集");}
  else if(fresh&&s.connected&&(s.totals.valid_mask&masks[i]))swprintf(value,32,L"%llu",(unsigned long long)values[i]);
  else wcscpy(value,L"未采集");
  layout_text(layout,label,names[i],1);layout_text(layout,label+1,value,1);
 }
 if(p->active)announce(p,1);
}
static uintptr_t refresh_hook(void *page,void *person){
 uintptr_t result=native_refresh(page,person);
 if(read_pointer((uintptr_t)page)!=image+0x1381700||!person)return result;
 Pane *p=find(page);if(!p)p=create(page);if(!p)return result;
 p->person=(uintptr_t)person;p->next=0;render(p);
 if(!p->active&&(read_int((uintptr_t)page+0x40)&3)==3){call(p->extra,7);widget_visible(p->extra,1);p->active=1;announce(p,1);}
 return result;
}
static void update_hook(void *page){
 native_update(page);Pane *p=find(page);if(!p||!p->active)return;
 ULONGLONG now=GetTickCount64();if(now>=p->next){p->next=now+250;render(p);}
}
static void show_hook(void *page){
 /* Some stock entries refresh an already-visible basic page before its
    modal show. Re-register after the parent, even when we were active:
    otherwise its opaque native resource is drawn over our extension. */
 Pane *p=find(page);
 if(p&&p->active){announce(p,0);widget_visible(p->extra,0);call(p->extra,8);p->active=0;}
 native_show(page);p=find(page);
 if(p){call(p->extra,7);widget_visible(p->extra,1);p->active=1;render(p);}
}
static void hide_hook(void *page){Pane *p=find(page);if(p&&p->active){announce(p,0);widget_visible(p->extra,0);call(p->extra,8);p->active=0;}native_hide(page);}
static void cleanup_hook(void *page,void *res){Pane *p=find(page);if(p)release(p,res);native_cleanup(page,res);}
static void *destroy_hook(void *page,unsigned flags){Pane *p=find(page);if(p)release(p,resources());return native_destroy(page,flags);}
int s14_basic_stats_install(uintptr_t base,uintptr_t end){
 if(!base||end-base<0x1fc91d8)return 0;
 uintptr_t v[50];SIZE_T got;if(!ReadProcessMemory(GetCurrentProcess(),(void*)(base+0x1381700),v,sizeof(v),&got)||got!=sizeof(v))return 0;
 const int slots[]={0,2,5,7,8,48};const uintptr_t rvas[]={0x7cf780,0x78de90,0x800f50,0x776ca0,0x765df0,0x80a220};
 for(int i=0;i<6;i++)if(v[slots[i]]!=base+rvas[i])return 0;
 image=base;allocate=(void*(*)(size_t))(base+0x3a54d0);widget_ctor=(void*(*)(void*))(base+0x77a0b0);resource_ctor=(void(*)(void*))(base+0x82beb0);
 native_context=(void*(*)(void))(base+0x13ee90);
 widget_visible=(void(*)(void*,int))(base+0x789820);
 resource_init=(int(*)(void*,int,int,void*,const char*,int,void*))(base+0x7834a0);
 layout_visible=(void(*)(void*,int,int))(base+0x8309d0);layout_text=(void(*)(void*,int,const wchar_t*,int))(base+0x831e70);layout_style=(void(*)(void*,int,int))(base+0x8300d0);
 void *hooks[]={destroy_hook,update_hook,cleanup_hook,show_hook,hide_hook,refresh_hook};
 void **originals[]={(void**)&native_destroy,(void**)&native_update,(void**)&native_cleanup,(void**)&native_show,(void**)&native_hide,(void**)&native_refresh};
 for(int i=0;i<6;i++)if(MH_CreateHook((void*)(base+rvas[i]),hooks[i],originals[i])!=MH_OK){for(int j=0;j<i;j++)MH_RemoveHook((void*)(base+rvas[j]));return 0;}
 return 1;
}
