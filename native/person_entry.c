#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "person_entry.h"
#include "tech_tree.h"
#include "map_effects_model.h"
typedef void *(*Getter)(void);
typedef void *(*Primary)(void*);
typedef void (*ListInit)(void*);
typedef void (*ListCopy)(void*,void**,void**);
typedef void (*ListDelete)(void*);
typedef void (*OpenDetail)(void*,void*,void*);
static uintptr_t entry_base;
static Getter native_info;
static Primary native_primary;
static ListInit native_list_init;
static ListCopy native_list_copy;
static ListDelete native_list_delete;
static OpenDetail native_open;
static uintptr_t ptr(uintptr_t);
static volatile LONG event_code,event_count,event_force,event_selected,ui_requests,ui_returns,entry_default_tab;
static int game_memory(void *unused,uintptr_t at,void *out,size_t n){(void)unused;SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got)&&got==n;}
static void record_event(LONG code,LONG count){InterlockedExchange(&event_count,count);InterlockedExchange(&event_code,code);}
enum {ENTRY_NONE,ENTRY_CONTEXT_CHANGED,ENTRY_CONTEXT_FAILED,ENTRY_FILTER_FAILED,ENTRY_OPEN_REQUESTED,ENTRY_LIST_FAILED,ENTRY_MAP_RETURN_READY,ENTRY_RETURN_CANCELLED};
static volatile LONG ui_opening;
static struct {uintptr_t world,settings,manager,map_state;DWORD thread;} returning;
typedef struct Node {void *person;struct Node *next;} Node;
static int get(uintptr_t p,void *out,size_t n){SIZE_T got=0;return p&&ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got)&&got==n;}
static uintptr_t ptr(uintptr_t p){uintptr_t v=0;get(p,&v,8);return v;}
static uintptr_t current_state(void){
    uintptr_t depth=ptr(entry_base+0x19e6330+0x10),array=ptr(entry_base+0x19e6330+0x20);
    return depth&&depth<=64&&array?ptr(array+(depth-1)*8):0;
}
void s14_person_entry_cancel_return(void){memset(&returning,0,sizeof(returning));}
int s14_person_entry_return_ready(void){
    DWORD error=GetLastError();int ready=0;
    if(returning.manager&&GetCurrentThreadId()==returning.thread){
        if(ptr(entry_base+0x1fc91d0)!=returning.world || ptr(returning.world+0x85130)!=returning.settings ||
           (uintptr_t)native_info()!=returning.manager){
            s14_person_entry_cancel_return();record_event(ENTRY_RETURN_CANCELLED,0);ready=-1;
        }else if(ptr(returning.manager)==ptr(returning.manager+8) && current_state()==returning.map_state &&
                 s14_map_main_scene(game_memory,NULL,entry_base)){
            /* 0x7f0a00 pops the information page, then queues the state return
               through 0x010950. Empty information data alone is too early. */
            s14_person_entry_cancel_return();InterlockedIncrement(&ui_returns);
            record_event(ENTRY_MAP_RETURN_READY,InterlockedCompareExchange(&event_count,0,0));ready=1;
        }
    }
    SetLastError(error);return ready;
}
static int own_people(uintptr_t world,uintptr_t settings,Node *rows,int *leader,int *force_out){
    unsigned char force=0;if(!get(settings+0x3a,&force,1)||force<1||force>51)return -1;
    uintptr_t primary=(uintptr_t)native_primary((void*)settings);unsigned char header[18];
    if(!get(primary,header,sizeof(header))||*(uintptr_t*)header!=entry_base+0x129fec8||header[0x10]!=force||header[0x11]!=1)return -1;
    uintptr_t force_data=ptr(world+0xdca0+force*8);unsigned char data[18];
    if(!get(force_data,data,sizeof(data))||*(uintptr_t*)data!=entry_base+0x129fe58)return -1;
    unsigned short ruler;memcpy(&ruler,data+0x10,2);*leader=ruler;*force_out=force;
    unsigned char group_force[52]={0};int found_primary=0;
    for(int i=1;i<=51;i++){
        uintptr_t group=ptr(world+0xde40+i*8);if(!group)continue;
        if(!get(group,header,sizeof(header))||*(uintptr_t*)header!=entry_base+0x129fec8||header[0x10]>51)return -1;
        group_force[i]=header[0x10];if(group==primary)found_primary=1;
    }
    if(!found_primary)return -1;
    int count=0;
    for(int id=1;id<=6000;id++){
        uintptr_t person=ptr(world+0x148+id*8);if(!person)continue;
        unsigned char p[0x120];if(!get(person,p,sizeof(p)))return -1;
        unsigned short actual,name;memcpy(&actual,p+0x10,2);memcpy(&name,p+0x12,2);
        if(*(uintptr_t*)p!=entry_base+0x12a00d0||actual!=id||!name)continue;
        /* Person +0x118 stores a district index, not a force ID. */
        unsigned district=p[0x118],status=p[0x11e];
        if(district<1||district>51||group_force[district]!=force||status<2||status>4)continue;
        rows[count++].person=(void*)person;
    }
    return count;
}
static int native_count(void *handle){
    unsigned id=0,capacity=0;uintptr_t total=0,counts=ptr(entry_base+0x201c378);
    if(!get((uintptr_t)handle,&id,4)||!get(entry_base+0x201c390,&capacity,4)||id>=capacity||!counts||!get(counts+id*8,&total,8)||total>6000)return -1;
    return (int)total;
}
int s14_person_entry_open(void){
    DWORD error=GetLastError();int ok=0,count=0,ruler=0,force=0;
    if(returning.manager || InterlockedCompareExchange(&ui_opening,0,0) || !s14_map_main_scene(game_memory,NULL,entry_base)){SetLastError(error);return 0;}uintptr_t world=ptr(entry_base+0x1fc91d0),settings=ptr(world+0x85130);
    if(!world||!settings){record_event(ENTRY_CONTEXT_FAILED,0);SetLastError(error);return 0;}
    Node *rows=calloc(6000,sizeof(Node));if(!rows){SetLastError(error);return 0;}
    count=own_people(world,settings,rows,&ruler,&force);
    if(count<=0||ptr(entry_base+0x1fc91d0)!=world||ptr(world+0x85130)!=settings){record_event(ENTRY_FILTER_FAILED,count);free(rows);SetLastError(error);return 0;}
    void *selected=rows[0].person;
    for(int i=0;i<count;i++){
        if(i+1<count)rows[i].next=&rows[i+1];unsigned short id=0;get((uintptr_t)rows[i].person+0x10,&id,2);if(id==ruler)selected=rows[i].person;
    }
    void *manager=native_info();void *list[2]={(void*)(entry_base+0x123f448),NULL};
    uintptr_t map_state=current_state();
    /* This entry owns a map-to-information round trip only. Never capture or
       overwrite the return path of a pre-existing information window. */
    if(manager && map_state && ptr((uintptr_t)manager)==ptr((uintptr_t)manager+8)){
        native_list_init(list);
        if(list[1]){
            void *begin=rows,*end=NULL;native_list_copy(list,&begin,&end);
            if(native_count(list[1])==count){
                if(ptr(entry_base+0x1fc91d0)==world && ptr(world+0x85130)==settings &&
                   s14_map_main_scene(game_memory,NULL,entry_base) && InterlockedCompareExchange(&ui_opening,1,0)==0){
                    /* Same nonblocking entry as the stock officer-list click
                       (0x7f9183 -> 0x3c5980). The native information UI copies
                       the list into CPersonInfoLayout and consumes this handle;
                       no script pause event or worker thread is involved. */
                    InterlockedExchange(&event_force,force);unsigned short id=0;get((uintptr_t)selected+0x10,&id,2);InterlockedExchange(&event_selected,id);
                    native_open(manager,list,selected);
                    if(ptr((uintptr_t)manager+8)>ptr((uintptr_t)manager)){
                        /* Only the window just pushed by this map entry gets
                           the fourth-tab default. Never alter global tab init. */
                        void *dialog=(void*)ptr(ptr((uintptr_t)manager+8)-0x10);
                        InterlockedExchange(&entry_default_tab,s14_tech_tree_select_fourth(dialog)?4:1);
                        returning.world=world;returning.settings=settings;returning.manager=(uintptr_t)manager;
                        returning.map_state=map_state;returning.thread=GetCurrentThreadId();ok=1;InterlockedIncrement(&ui_requests);
                    }
                    InterlockedExchange(&ui_opening,0);
                }else record_event(ENTRY_CONTEXT_CHANGED,0);
            }
            native_list_delete(list); /* no-op if the opener consumed it */
        }
    }
    record_event(ok?ENTRY_OPEN_REQUESTED:ENTRY_LIST_FAILED,count);
    free(rows);SetLastError(error);return ok;
}
int s14_person_entry_next_log(char *out,size_t capacity){
    static LONG last=-1,last_count=-1,last_requests=-1;LONG code=InterlockedCompareExchange(&event_code,0,0),count=InterlockedCompareExchange(&event_count,0,0),requests=InterlockedCompareExchange(&ui_requests,0,0);
    if(code==last&&count==last_count&&requests==last_requests)return 0;
    static const char *names[]={"none","context_changed","context_failed","officer_filter_failed","own_force_ui_submitted","list_failed","map_return_ready","return_cancelled"};
    int n=snprintf(out,capacity,"{\"event\":\"native_officer_entry\",\"state\":\"%s\",\"count\":%ld,\"force\":%ld,\"selected_id\":%ld,\"requests\":%ld,\"returns\":%ld,\"default_tab\":%ld,\"entry\":\"information_ui\"}\n",names[code],count,InterlockedCompareExchange(&event_force,0,0),InterlockedCompareExchange(&event_selected,0,0),requests,InterlockedCompareExchange(&ui_returns,0,0),InterlockedCompareExchange(&entry_default_tab,0,0));
    if(n<=0||(size_t)n>=capacity)return 0;last=code;last_count=count;last_requests=requests;return code!=ENTRY_NONE;
}
int s14_person_entry_init(uintptr_t base,uintptr_t end){
    static const struct {uintptr_t r;unsigned char b[8];unsigned n;} checks[]={
        {0x1459e0,{0x40,0x53,0x48,0x83,0xec,0x30,0x48,0xc7},8},
        {0x2f1fc0,{0x0f,0xb6,0x41,0x3a,0x83,0xf8,0x33,0x77},8},
        {0x17fb60,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x83},8},
        {0x5ff390,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c},8},
        {0x5d30f0,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x3c5980,{0x48,0x8b,0xc4,0x48,0x89,0x50,0x10},7}
    };
    entry_base=base;if(!base||end-base<0x201c398)return 0;unsigned char bytes[8];
    for(unsigned i=0;i<sizeof(checks)/sizeof(checks[0]);i++)if(!get(base+checks[i].r,bytes,checks[i].n)||memcmp(bytes,checks[i].b,checks[i].n))return 0;
    native_info=(Getter)(base+0x1459e0);native_primary=(Primary)(base+0x2f1fc0);native_list_init=(ListInit)(base+0x17fb60);native_list_copy=(ListCopy)(base+0x5ff390);native_list_delete=(ListDelete)(base+0x5d30f0);native_open=(OpenDetail)(base+0x3c5980);return 1;
}
