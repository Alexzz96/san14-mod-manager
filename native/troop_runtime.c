#define WIN32_LEAN_AND_MEAN
#include "troop_runtime.h"
#include "MinHook.h"
#include "troop_store.h"
#include "ai_affix.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>

/* Only the native game thread calls setters and game functions. The worker
   publishes a click request; it never dereferences or mutates a game packet. */
typedef uintptr_t (*OneCall)(void*);
typedef uintptr_t (*TwoCall)(void*,void*);
typedef uintptr_t (*GoldCall)(void*,int);
typedef void *(*CityCall)(void*,int);
typedef int (*FundsCall)(void*);
typedef int (*CapacityCall)(void*,int);
typedef void (*ControlCall)(void*,int);
typedef void (*ButtonCall)(void*,void*);
typedef void (*TextCall)(void*,int,const wchar_t*,int);
typedef float (*AttributeCall)(float,void*,void*,void*,int,int);
typedef float (*MobilityCall)(void*,void*,int,int,void*);
typedef float (*BreakCall)(void*,void*,int,int);
typedef int (*QueryCall)(void*,int);
typedef uintptr_t (*AbnormalCall)(void*,void*,int,int,void*,const wchar_t*,int);
static OneCall original_tick,original_preview,original_create;
static TwoCall original_bind,original_confirm,original_exit,original_init;
static AttributeCall original_siege;
static BreakCall original_break;
static QueryCall original_query;
static AbnormalCall native_abnormal;
static MobilityCall original_mobility;
static TwoCall original_formation,native_copy;
static OneCall native_refresh;
static OneCall original_refresh;
static FundsCall original_funds;
static CapacityCall original_capacity;
static TwoCall original_detail;
static ButtonCall native_button;
static TextCall native_text;
static ControlCall native_visible,native_selected,native_enabled;
static CityCall native_city;
static GoldCall native_gold;
static uintptr_t image,image_end;
static volatile LONG ready,enabled,loading;
static SRWLOCK guard=SRWLOCK_INIT;
typedef S14TroopBinding Binding;
typedef S14TroopIntent Intent;
typedef S14TroopState State;
static State live,backup;
static uintptr_t selection_state,selection_tab,selected_packet;
static int selection[6001];
/* Reuse only an unused native land button. No new native formation ID, GUI
   allocation or resource-table extension is needed. Game callbacks own it. */
static struct {uintptr_t state,tab,wrapper,inner;int wrapper_tag,inner_tag;} native_slot;
static S14TroopFrame frame,request;
static int request_pending;
static wchar_t storage[MAX_PATH],notice[160];
static _Thread_local const S14TroopDefinition *preview_definition;
static _Thread_local uintptr_t preview_army;
static _Thread_local int creating_leader;
static _Thread_local const S14TroopDefinition *creating_definition;
static _Thread_local uintptr_t creating_world,clearing_army;
static volatile LONG clear_pending_scan;
typedef struct {volatile LONG full;char text[512];} Event;
static Event events[64];static volatile LONG event_number,dropped;

static int read_at(uintptr_t at,void *out,size_t n){SIZE_T got;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static uintptr_t ptr(uintptr_t at){uintptr_t x=0;read_at(at,&x,8);return x;}
static int integer(uintptr_t at){int x=0;read_at(at,&x,4);return x;}
static uintptr_t world_now(void){return image?ptr(image+0x1fc91d0):0;}
static uintptr_t top_state(void){unsigned char b[40];if(!image || !read_at(image+0x19e6330,b,40))return 0;uint64_t n=0,items=0;memcpy(&n,b+16,8);memcpy(&items,b+32,8);return n && n<=64?ptr(items+(n-1)*8):0;}
static int deployment(uintptr_t state){return state && top_state()==state && ptr(state)==image+0x133f438;}
static const S14TroopDefinition *definition(void){return s14_troop_registry_find(s14_troop_builtin_registry(),"san14.xianzhen");}
static void event(const char *kind,int leader,int unit,int cost){
    LONG n=InterlockedIncrement(&event_number);Event *e=&events[(unsigned)n%64];
    if(InterlockedCompareExchange(&e->full,1,0)){InterlockedIncrement(&dropped);return;}
    snprintf(e->text,sizeof(e->text),"{\"event\":\"plugin_troop_%s\",\"troop_id\":\"san14.xianzhen\",\"officer_id\":%d,\"army_id\":%d,\"extra_gold\":%d,\"tick_ms\":%llu,\"dropped\":%ld}\n",kind,leader,unit,cost,(unsigned long long)GetTickCount64(),dropped);InterlockedExchange(&e->full,2);
}
static void say(const wchar_t *text){AcquireSRWLockExclusive(&guard);wcsncpy(notice,text,159);notice[159]=0;ReleaseSRWLockExclusive(&guard);}
static void clear_pending(int leader){AcquireSRWLockExclusive(&guard);for(int i=0;i<64;i++)if(live.pending[i].active && live.pending[i].leader==leader)memset(&live.pending[i],0,sizeof(Intent));ReleaseSRWLockExclusive(&guard);}
static int native_permission(uintptr_t world,int leader,int carrier){uintptr_t officer=ptr(world+0x148+(uintptr_t)leader*8);unsigned int mask=0;return officer && ptr(officer)==image+0x12a00d0 && read_at(officer+0x164,&mask,4) && carrier>=1 && carrier<=20 && (mask&(1u<<carrier));}
static uintptr_t city_for(uintptr_t world,int leader,int *home){
    uintptr_t officer=ptr(world+0x148+(uintptr_t)leader*8);unsigned short h=0;
    if(!officer || ptr(officer)!=image+0x12a00d0 || !read_at(officer+0x11a,&h,2) || h>200)return 0;
    /* 2f05a0 is a bounded native city lookup, invoked only in game callbacks. */
    uintptr_t city=(uintptr_t)native_city((void*)world,h);
    if(home)*home=h;
    return city;
}
static int unit_identity(void *army,int *id,Binding *key){
    unsigned char b[48];uintptr_t world=world_now(),pool=ptr(world+0x7df60);
    if(!world || !pool || (uintptr_t)army<=pool || ((uintptr_t)army-pool)%512 || ((uintptr_t)army-pool)/512>500 || !read_at((uintptr_t)army,b,48))return 0;
    int n=(int)(((uintptr_t)army-pool)/512);unsigned short leader,serial,home;memcpy(&leader,b+0x12,2);memcpy(&serial,b+0x28,2);memcpy(&home,b+0x14,2);
    if(ptr(world+0x7df60+(uintptr_t)n*8)!=(uintptr_t)army || ptr((uintptr_t)army)!=image+0x123e288 || b[0x10]!=1 || leader<1 || leader>6000)return 0;
    if(id)*id=n;if(key){memset(key,0,sizeof(*key));key->active=1;key->leader=leader;key->serial=serial;key->home=home;key->carrier=b[0x1b];}return 1;
}
/* +14 is mutable city affiliation, filled AFTER the native creation returns.
   Keep the field in v1 sidecars, but never use it as a generation identity.
   Canonical pool/world checks, removal and every create clear reused slots. */
static int same_binding(const Binding *a,const Binding *b){return a->active && b->active && a->leader==b->leader && a->carrier==b->carrier && a->serial==b->serial;}
const S14TroopDefinition *s14_troop_bound(void *army){
    if(!enabled || loading)return NULL;
    uintptr_t world=world_now(),pool=ptr(world+0x7df60),at=(uintptr_t)army;
    if(!world || !pool || at<=pool || (at-pool)%512 || (at-pool)/512>500)return NULL;
    int candidate=(int)((at-pool)/512);Binding copy,key;
    /* Most attribute calls belong to ordinary armies. Reject an empty slot
       before expensive reads; a live slot still gets the full native check. */
    AcquireSRWLockShared(&guard);int valid=live.world==world && live.units[candidate].active;copy=live.units[candidate];ReleaseSRWLockShared(&guard);
    if(!valid)return NULL;
    int id;if(!unit_identity(army,&id,&key) || id!=candidate || key.leader==creating_leader || !same_binding(&copy,&key))return NULL;
    const S14TroopDefinition *d=valid?s14_troop_registry_find(s14_troop_builtin_registry(),copy.id):NULL;
    return d && d->revision==copy.revision && d->native_carrier==key.carrier && s14_troop_commander_allowed(d,key.leader)?d:NULL;
}
static const S14TroopDefinition *effect_definition(void *army){
    if(!enabled || loading)return NULL;
    if(preview_definition && preview_army==(uintptr_t)army)return preview_definition;
    if(creating_definition && creating_world==world_now()){
        Binding key;int id;if(unit_identity(army,&id,&key) && key.leader==creating_leader && key.carrier==creating_definition->native_carrier)return creating_definition;
    }
    return s14_troop_bound(army);
}
static int has_effect(const S14TroopDefinition *d,int kind,int status){
    if(d)for(int i=0;i<d->effect_count;i++)if(d->effects[i].kind==kind && d->effects[i].status==status)return 1;return 0;
}
static void attribute_event(void *army,int attr,int actual,float before,float after,int mode,uintptr_t caller){
    int stage=preview_army==(uintptr_t)army?1:creating_definition?2:3;
    typedef struct {uintptr_t army;float before,after;int actual,mode;} Last;
    static _Thread_local Last last[30];Last *p=&last[(stage-1)*10+actual*5+attr];
    if(p->army==(uintptr_t)army && p->actual==actual && p->mode==mode && p->before==before && p->after==after)return;
    *p=(Last){(uintptr_t)army,before,after,actual,mode};
    Binding key={0};int id=0;unit_identity(army,&id,&key);
    LONG n=InterlockedIncrement(&event_number);Event *e=&events[(unsigned)n%64];
    if(InterlockedCompareExchange(&e->full,1,0)){InterlockedIncrement(&dropped);return;}
    snprintf(e->text,sizeof(e->text),"{\"event\":\"plugin_troop_attribute\",\"army_id\":%d,\"officer_id\":254,\"serial\":%u,\"home\":%u,\"carrier\":1,\"stage\":%d,\"attribute\":%d,\"actual\":%d,\"mode\":%d,\"caller_rva\":\"0x%llx\",\"native_float\":%.9g,\"plugin_float\":%.9g,\"tick_ms\":%llu}\n",id,key.serial,key.home,stage,attr,actual,mode,(unsigned long long)(caller>=image && caller<image_end?caller-image:0),(double)before,(double)after,(unsigned long long)GetTickCount64());InterlockedExchange(&e->full,2);
}
float s14_troop_attribute_at(void *army,int attr,int actual,float value,int mode,uintptr_t caller){
    if(!enabled || loading || (actual!=0 && actual!=1) || attr<0 || attr>=5 || !isfinite(value) || value<0)return value;
    s14_troop_status_service(army);
    const S14TroopDefinition *d=effect_definition(army);
    if(!d)return value;float result=actual?value*(1.f+(float)d->bonus_bp[attr]/10000.f):value;
    if(!isfinite(result))return value;attribute_event(army,attr,actual,value,result,mode,caller);return result;
}
float s14_troop_attribute(void *army,int attr,int actual,float value){return s14_troop_attribute_at(army,attr,actual,value,-1,0);}
/* Local native evidence: mode 0 writes +23 (confusion); refusal category 22.
   Returning native refusal does not touch +24/+25 or fire/supply state. */
int s14_troop_confusion_reject(void *army,int mode,int duration){
    if(mode!=0 || duration<=0 || !has_effect(effect_definition(army),S14_TROOP_STATUS_IMMUNITY,S14_TROOP_CONFUSION))return 0;
    s14_troop_status_service(army);int id=0;Binding key;unit_identity(army,&id,&key);event("confusion_refused",254,id,0);return 22;
}
void s14_troop_clear_modifiers(void *army,int *out){
    /* Only suppress duration resistance during OUR negative native clear.
       All attribute/status categories in ordinary calculations stay native. */
    if(clearing_army==(uintptr_t)army && out){out[9]=0;out[10]=0;}
}
void s14_troop_status_service(void *army){
    if(!enabled || loading || clearing_army || !native_abnormal)return;
    if(InterlockedExchange(&clear_pending_scan,0)){
        uintptr_t world=world_now();for(int i=1;i<=500;i++)s14_troop_status_service((void*)ptr(world+0x7df60+(uintptr_t)i*8));
    }
    unsigned char before[3],after[3];const S14TroopDefinition *d=s14_troop_bound(army);
    if(!has_effect(d,S14_TROOP_STATUS_IMMUNITY,S14_TROOP_CONFUSION) || !read_at((uintptr_t)army+0x23,before,3) || !before[0])return;
    DWORD error=GetLastError();clearing_army=(uintptr_t)army;
    /* Negative duration is supported by 2bbfb0's native clamp. Empty native
       message identity suppresses its notification path. No raw timer write. */
    native_abnormal(army,NULL,0,-255,NULL,(const wchar_t*)ptr(image+0x18ea8d8),0);
    clearing_army=0;int id=0;Binding key;unit_identity(army,&id,&key);
    int ok=read_at((uintptr_t)army+0x23,after,3) && !after[0] && before[1]==after[1] && before[2]==after[2];
    event(ok?"confusion_cleared":"confusion_clear_fault",254,id,0);SetLastError(error);
}
static int query_hook(void *army,int category){
    if(clearing_army==(uintptr_t)army && (category==21 || category==22))return 0;
    if(category==22 && has_effect(effect_definition(army),S14_TROOP_STATUS_IMMUNITY,S14_TROOP_CONFUSION))return 1;
    return original_query(army,category);
}
int s14_troop_surround_mode(void *army,int mode,int actual,uintptr_t caller){
    /* Native 15c05d queries personality category 55, replaces surround count
       with 1, then calls 27b180 at 15c07f. Reproduce ONLY that penalty input. */
    if(caller==image+0x15c084 && actual==1 && mode>1 && has_effect(s14_troop_bound(army),S14_TROOP_SURROUND_IMMUNITY,0)){
        int id=0;Binding key;unit_identity(army,&id,&key);event("surround_ignored",254,id,mode);return 1;
    }return mode;
}
int s14_troop_damage(void *army,int amount){
    /* Verified combat setter receives positive damage, not a negative delta. */
    const S14TroopDefinition *d=amount>0?s14_troop_bound(army):NULL;if(!d)return amount;
    int bp=0;for(int i=0;i<d->effect_count;i++)if(d->effects[i].kind==S14_TROOP_DAMAGE_REDUCTION)bp=d->effects[i].value_bp;
    /* Round loss upward: mitigation can never turn a nonzero hit into healing. */
    int64_t loss=amount;loss=(loss*(10000-bp)+9999)/10000;return loss>INT_MAX?amount:(int)loss;
}
void s14_troop_forget(void *army){
    uintptr_t world=world_now(),pool=ptr(world+0x7df60),at=(uintptr_t)army;unsigned char active=1;
    if(!pool || at<=pool || (at-pool)%512 || (at-pool)/512>500 || ptr(at)!=image+0x123e288 || !read_at(at+0x10,&active,1) || active==1)return;
    int id=(int)((at-pool)/512);if(ptr(world+0x7df60+(uintptr_t)id*8)!=at)return;
    AcquireSRWLockExclusive(&guard);memset(&live.units[id],0,sizeof(Binding));ReleaseSRWLockExclusive(&guard);
}
static int packet_read(uintptr_t at,int p[5]){return read_at(at,p,20) && p[0]>0 && p[0]<=6000 && p[2]>0 && p[2]<=65535 && p[3]>0 && p[3]<=20 && p[4]>=0 && p[4]<=20;}
static int native_fee(uintptr_t world,int formation){uintptr_t f=ptr(world+0x76b58+(uintptr_t)formation*8);unsigned short cost=0;if(formation<0 || formation>20 || ptr(f)!=image+0x12a0300 || !read_at(f+0x9a,&cost,2))return -1;return cost;}
static int quote_packet(uintptr_t world,const int p[5],uint64_t gold,S14TroopQuote *q){const S14TroopDefinition *d=definition();S14TroopRequest r={p[0],p[3],native_permission(world,p[0],p[3]),1,(uint32_t)p[2],gold};return d && s14_troop_quote(s14_troop_builtin_registry(),d->id,&r,q)==S14_TROOP_OK;}
static int capacity_hook(void *state,int row){
    int maximum=original_capacity(state,row);DWORD error=GetLastError();uintptr_t s=(uintptr_t)state;
    /* The argument is the selected ROW INDEX, not the commander ID. */
    if(enabled && !loading && deployment(s) && s==selection_state && row>=0 && row<64){
        uintptr_t begin=ptr(s+0x4c0),end=ptr(s+0x4c8);int p[5];const S14TroopDefinition *d=definition();
        if(end>=begin && (end-begin)%8==0 && (end-begin)/8<=64 && (uintptr_t)row<(end-begin)/8 &&
           packet_read(ptr(begin+(uintptr_t)row*8),p) && d && selection[p[0]] &&
           s14_troop_commander_allowed(d,p[0]) && d->max_soldiers && maximum>(int)d->max_soldiers)maximum=(int)d->max_soldiers;
    }
    SetLastError(error);return maximum;
}
static void cap_current_packet(uintptr_t state,uintptr_t tab){
    int p[5];const S14TroopDefinition *d=definition();
    if(!enabled || loading || !deployment(state) || ptr(state+0x490)!=tab || ptr(tab)!=image+0x133fd10 ||
       !packet_read(tab+0x168,p) || !d || !selection[p[0]] || p[3]!=d->native_carrier || !s14_troop_commander_allowed(d,p[0]) || !d->max_soldiers)return;
    int maximum=(int)d->max_soldiers,row=integer(state+0x4e4);uintptr_t begin=ptr(state+0x4c0),end=ptr(state+0x4c8);
    if(end>=begin && (end-begin)%8==0 && (end-begin)/8<=64 && row>=0 && (uintptr_t)row<(end-begin)/8){int available=capacity_hook((void*)state,row);if(available>=0 && available<maximum)maximum=available;}
    if(p[2]>maximum)*(int*)(tab+0x170)=maximum;
}
static int control_pair(uintptr_t wrapper,uintptr_t *inner){
    uintptr_t child=ptr(wrapper+0x168);
    if(ptr(wrapper)!=image+0x133ff30 || ptr(child)!=image+0x12fc420)return 0;
    if(inner)*inner=child;return 1;
}
static int owned_slot(uintptr_t state,uintptr_t tab){
    uintptr_t inner=0;
    return native_slot.state==state && native_slot.tab==tab &&
        ptr(tab+0x268)==native_slot.wrapper && control_pair(native_slot.wrapper,&inner) && inner==native_slot.inner;
}
static void restore_slot(uintptr_t state,uintptr_t tab){
    /* Called before binder/exit frees or repopulates the native controls. If the
       context has already changed, discard the cache without touching it. */
    if(owned_slot(state,tab)){
        native_selected((void*)native_slot.inner,0);
        native_visible((void*)native_slot.wrapper,0);
        *(int*)(native_slot.wrapper+0x80)=native_slot.wrapper_tag;
        *(int*)(native_slot.inner+0x80)=native_slot.inner_tag;
    }
    memset(&native_slot,0,sizeof(native_slot));
}
static void apply_native_ui(uintptr_t state,uintptr_t tab,int sync_selection){
    int p[5];const S14TroopDefinition *d=definition();
    int valid=enabled && !loading && deployment(state) && ptr(state+0x490)==tab &&
        ptr(tab)==image+0x133fd10 && (integer(tab+0x40)&3)==3 && packet_read(tab+0x168,p) && d &&
        s14_troop_commander_allowed(d,p[0]) && native_permission(world_now(),p[0],d->native_carrier);
    if(!valid){restore_slot(state,tab);return;}
    if(!owned_slot(state,tab)){
        restore_slot(state,tab);uintptr_t wrapper=ptr(tab+0x268),inner=0;
        /* Slot 3 must be hidden. Never replace an allowed native third troop. */
        if(!control_pair(wrapper,&inner) || (integer(wrapper+0x40)&1))return;
        uintptr_t carrier=ptr(world_now()+0x76b58+(uintptr_t)d->native_carrier*8);
        if(ptr(carrier)!=image+0x12a0300 || integer(wrapper+0x138)<5)return;
        native_slot.state=state;native_slot.tab=tab;native_slot.wrapper=wrapper;native_slot.inner=inner;
        native_slot.wrapper_tag=integer(wrapper+0x80);native_slot.inner_tag=integer(inner+0x80);
        wchar_t name[64];if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,d->name,-1,name,64)){memset(&native_slot,0,sizeof(native_slot));return;}
        native_button((void*)wrapper,(void*)carrier);
        native_text((void*)(wrapper+0x128),4,name,1);
        /* 0 is ignored by the original fixed-range callback. Our exact-control
           branch handles it; a virtual ID never reaches native lookup tables. */
        *(int*)(wrapper+0x80)=0;*(int*)(inner+0x80)=0;
        native_enabled((void*)inner,1);native_visible((void*)inner,1);native_visible((void*)wrapper,1);
        sync_selection=1;
    }
    /* The native 0x100 flag also holds an in-progress mouse press while logical
       selected (+188) is still zero. Never normalize it from the frame tick:
       doing so cancels release before the native click callback is dispatched. */
    if(!sync_selection)return;
    int special=selection[p[0]] && p[3]==d->native_carrier;
    for(int i=0;i<5;i++){
        uintptr_t wrapper=ptr(tab+0x258+(uintptr_t)i*8),inner=0;
        if(!control_pair(wrapper,&inner) || !(integer(wrapper+0x40)&1))continue;
        int selected=wrapper==native_slot.wrapper?special:(!special && integer(wrapper+0x80)==p[3]);
        if(integer(inner+0x188)!=selected || !!(integer(inner+0x40)&0x100)!=selected)native_selected((void*)inner,selected);
    }
}
static uintptr_t refresh_hook(void *tab){
    DWORD before=GetLastError();uintptr_t state=top_state();cap_current_packet(state,(uintptr_t)tab);SetLastError(before);
    uintptr_t result=original_refresh(tab);DWORD error=GetLastError();state=top_state();
    if(deployment(state) && ptr(state+0x490)==(uintptr_t)tab)apply_native_ui(state,(uintptr_t)tab,1);
    SetLastError(error);return result;
}
static int funds_hook(void *state){
    int result=original_funds(state);DWORD error=GetLastError();uintptr_t s=(uintptr_t)state;
    if(enabled && !loading && deployment(s) && s==selection_state){
        uintptr_t begin=ptr(s+0x4c0),end=ptr(s+0x4c8);int64_t extra=0;
        if(end>=begin && (end-begin)%8==0 && (end-begin)/8<=64){
            for(uintptr_t it=begin;it<end;it+=8){int p[5];S14TroopQuote q;
                if(packet_read(ptr(it),p) && selection[p[0]] && quote_packet(world_now(),p,UINT64_MAX,&q))extra+=q.extra_gold;
            }
            int64_t remaining=(int64_t)result-extra;result=remaining<INT_MIN?INT_MIN:(int)remaining;
        }
    }
    SetLastError(error);return result;
}
static void publish(uintptr_t state){
    S14TroopFrame f={0};uintptr_t tab=ptr(state+0x490);int p[5];const S14TroopDefinition *d=definition();
    if(enabled && !loading && deployment(state) && ptr(tab)==image+0x133fd10 && (integer(tab+0x40)&3)==3 && packet_read(tab+0x168,p) && d && s14_troop_commander_allowed(d,p[0])){
        uintptr_t button=native_slot.wrapper;int b[4],t[4];uintptr_t city=ptr(state+0x4b8),world=world_now();
        if(owned_slot(state,tab) && (integer(button+0x40)&1) && read_at(button+0x20,b,16) && read_at(tab+0x20,t,16) && b[2]>0 && b[2]<=200 && b[3]>0 && b[3]<=200){
            f.visible=1;f.state=state;f.tab=tab;f.world=world;f.commander=p[0];f.soldiers=p[2];f.carrier=p[3];f.selected=selection[p[0]] && p[3]==d->native_carrier;f.max_soldiers=(int)d->max_soldiers;
            f.x=t[0]+b[0];f.y=t[1]+b[1];f.w=b[2];f.h=b[3];f.gold=integer(city+0x34);
            int fee1=native_fee(world,p[3]),fee2=native_fee(world,p[4]);f.native_cost=fee1>=0 && fee2>=0?fee1+fee2:INT_MAX;
            S14TroopQuote q={0};int copy[5];memcpy(copy,p,20);copy[3]=d->native_carrier;if(d->max_soldiers && copy[2]>(int)d->max_soldiers)copy[2]=(int)d->max_soldiers;quote_packet(world,copy,UINT64_MAX,&q);f.extra_cost=(int)q.extra_gold;
        }
    }
    AcquireSRWLockExclusive(&guard);wcscpy(f.message,notice);frame=f;ReleaseSRWLockExclusive(&guard);
}
static uintptr_t tick_hook(void *state){
    DWORD before=GetLastError();uintptr_t result=original_tick(state);DWORD error=GetLastError();uintptr_t s=(uintptr_t)state;
    if(deployment(s)){
        uintptr_t tab=ptr(s+0x490);if(selection_state!=s){selection_state=s;selection_tab=tab;memset(selection,0,sizeof(selection));say(L"");}
        if(!enabled || loading){
            int had=selection[254];restore_slot(s,tab);memset(selection,0,sizeof(selection));
            if(had && !loading){native_refresh((void*)tab);native_copy(state,NULL);}
            publish(s);SetLastError(error);return result;
        }
        S14TroopFrame click={0};int clicked=0;AcquireSRWLockExclusive(&guard);if(request_pending){click=request;request_pending=0;clicked=1;}ReleaseSRWLockExclusive(&guard);
        if(clicked && click.state==s && click.tab==tab && click.world==world_now() && ptr(tab)==image+0x133fd10){
            int p[5];const S14TroopDefinition *d=definition();if(packet_read(tab+0x168,p) && d && p[0]==click.commander && s14_troop_commander_allowed(d,p[0]) && native_permission(world_now(),p[0],d->native_carrier)){
                selection[p[0]]=1;
                if(selection[p[0]])*(int*)(tab+0x174)=d->native_carrier;
                native_refresh((void*)tab);
                native_copy(state,NULL);
                say(selection[p[0]]?L"已选择陷阵营；确认并成功建队后扣除额外费用。":L"已取消陷阵营，使用普通兵种。");event("selection",p[0],0,0);
            }
        }
        apply_native_ui(s,tab,0);
        publish(s);
    }else {AcquireSRWLockExclusive(&guard);frame.visible=0;ReleaseSRWLockExclusive(&guard);}
    (void)before;SetLastError(error);return result;
}
static uintptr_t bind_hook(void *tab,void *packet){
    DWORD error=GetLastError();int p[5];uintptr_t state=top_state();
    if(deployment(state) && ptr(state+0x490)==(uintptr_t)tab)restore_slot(state,(uintptr_t)tab);
    if(deployment(state) && ptr(state+0x490)==(uintptr_t)tab){int fresh=selection_state!=state;if(fresh){selection_state=state;selection_tab=(uintptr_t)tab;memset(selection,0,sizeof(selection));}if(packet && packet_read((uintptr_t)packet,p)){selected_packet=(uintptr_t)packet;if(fresh)clear_pending(p[0]);}}
    SetLastError(error);uintptr_t result=original_bind(tab,packet);error=GetLastError();if(deployment(state)){apply_native_ui(state,(uintptr_t)tab,1);publish(state);}SetLastError(error);return result;
}
static uintptr_t formation_hook(void *event_data,void *widget_ref){
    DWORD error=GetLastError();uintptr_t tab=ptr((uintptr_t)event_data+8),widget=ptr((uintptr_t)widget_ref);int id=integer(widget+0x80),p[5];
    int special=enabled && !loading && tab==selection_tab && deployment(selection_state) && owned_slot(selection_state,tab) && widget==native_slot.inner && id==0 && packet_read(tab+0x168,p);
    if(tab==selection_tab && deployment(selection_state) && id>=1 && id<=20 && packet_read(tab+0x168,p))selection[p[0]]=0;
    SetLastError(error);uintptr_t result=original_formation(event_data,widget_ref);error=GetLastError();
    if(special){selection[p[0]]=1;*(int*)(tab+0x174)=definition()->native_carrier;native_refresh((void*)tab);native_copy((void*)selection_state,NULL);say(L"已选择陷阵营；确认并成功建队后扣除额外费用。");event("selection",p[0],0,0);}
    if(deployment(selection_state)){apply_native_ui(selection_state,tab,1);publish(selection_state);}SetLastError(error);return result;
}
static const S14TroopDefinition *selected_definition(uintptr_t packet){
    int p[5];if(!enabled || loading || !packet_read(packet,p) || !selection[p[0]] || !deployment(selection_state))return NULL;
    const S14TroopDefinition *d=definition();return d && (!d->max_soldiers || p[2]<=(int)d->max_soldiers) && p[3]==d->native_carrier && s14_troop_commander_allowed(d,p[0])?d:NULL;
}
static uintptr_t detail_hook(void *page,void *army){
    uintptr_t result=original_detail(page,army);DWORD error=GetLastError();uintptr_t at=(uintptr_t)page;
    const S14TroopDefinition *d=s14_troop_bound(army);
    /* Exact army identity and basic-page layout. The native call has already
       formatted the carrier; only its land troop caption is replaced. */
    if(d && ptr(at)==image+0x137ebf0 && ptr(at+0x168)==(uintptr_t)army && integer(at+0x170)==0 && integer(at+0x138)>0x29){
        wchar_t name[64];if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,d->name,-1,name,64))native_text((void*)(at+0x128),0x29,name,1);
    }
    SetLastError(error);return result;
}
static uintptr_t preview_hook(void *packet){
    DWORD error=GetLastError();const S14TroopDefinition *old=preview_definition;uintptr_t old_army=preview_army;
    preview_definition=selected_definition((uintptr_t)packet);preview_army=0;SetLastError(error);uintptr_t result=original_preview(packet);error=GetLastError();preview_definition=old;preview_army=old_army;SetLastError(error);return result;
}
static uintptr_t init_hook(void *army,void *packet){DWORD error=GetLastError();if(preview_definition){int p[5];if(packet_read((uintptr_t)packet,p) && s14_troop_commander_allowed(preview_definition,p[0]) && p[3]==preview_definition->native_carrier)preview_army=(uintptr_t)army;}SetLastError(error);uintptr_t result=original_init(army,packet);error=GetLastError();SetLastError(error);return result;}
static uintptr_t exit_hook(void *state,void *arg){DWORD error=GetLastError();if((uintptr_t)state==native_slot.state)restore_slot((uintptr_t)state,native_slot.tab);SetLastError(error);uintptr_t result=original_exit(state,arg);error=GetLastError();if((uintptr_t)state==selection_state){selection_state=selection_tab=selected_packet=0;memset(selection,0,sizeof(selection));AcquireSRWLockExclusive(&guard);frame.visible=request_pending=0;ReleaseSRWLockExclusive(&guard);}SetLastError(error);return result;}
static uintptr_t confirm_hook(void *state,void *button){
    DWORD error=GetLastError();Intent intents[64]={0};int count=0;uintptr_t s=(uintptr_t)state,world=world_now();
    if(enabled && !loading && deployment(s) && integer((uintptr_t)button+0x80)==5){
        uintptr_t begin=ptr(s+0x4c0),end=ptr(s+0x4c8),city=ptr(s+0x4b8);uint64_t total=0;int gold=integer(city+0x34);
        if(end<begin || (end-begin)%8 || (end-begin)/8>64){SetLastError(error);return 0;}
        for(uintptr_t it=begin;it<end;it+=8){uintptr_t packet=ptr(it);int p[5];if(!packet_read(packet,p)){SetLastError(error);return 0;}int a=native_fee(world,p[3]),b=native_fee(world,p[4]);if(a<0||b<0){SetLastError(error);return 0;}total+=(unsigned)(a+b);
            if(selection[p[0]]){const S14TroopDefinition *d=definition();S14TroopQuote q;if(d && d->max_soldiers && p[2]>(int)d->max_soldiers){say(L"陷阵营最多编成 1000 人，请调整兵力。");event("confirmation_cap_rejected",p[0],0,0);SetLastError(error);return 0;}if(!quote_packet(world,p,UINT64_MAX,&q) || !d){say(L"陷阵营编成已改变，请重新选择兵种。");SetLastError(error);return 0;}total+=q.extra_gold;
                Intent *intent=&intents[count++];intent->active=1;intent->leader=p[0];intent->number=p[1];intent->soldiers=p[2];intent->carrier=p[3];intent->water=p[4];intent->home=integer(city+0x10)&65535;intent->revision=d->revision;strcpy(intent->id,d->id);
            }
        }
        if(count && (gold<0 || total>(uint64_t)gold)){say(L"资金不足：请减少兵力、取消陷阵营或补充城市资金。");event("funds_rejected",254,0,0);SetLastError(error);return 0;}
        /* Accepted native confirmation moves these exact packets to its own
           command queue. Pending identities survive UI teardown and saving. */
        if(count){AcquireSRWLockExclusive(&guard);int free_slots=0;for(int j=0;j<64;j++)if(!live.pending[j].active)free_slots++;for(int i=0;i<count;i++)for(int j=0;j<64;j++)if(live.pending[j].active && live.pending[j].leader==intents[i].leader){free_slots++;break;}ReleaseSRWLockExclusive(&guard);if(free_slots<count){say(L"特殊兵种出征队列已满，请完成当前出征。");SetLastError(error);return 0;}}
        /* An ordinary re-confirmation supersedes any previous special order. */
        for(uintptr_t it=begin;it<end;it+=8){int p[5];if(packet_read(ptr(it),p))clear_pending(p[0]);}
        if(count){AcquireSRWLockExclusive(&guard);if(live.world && live.world!=world)memset(&live,0,sizeof(live));live.world=world;
            for(int i=0;i<count;i++){int slot=-1;for(int j=0;j<64;j++)if(live.pending[j].active && live.pending[j].leader==intents[i].leader){slot=j;break;}if(slot<0)for(int j=0;j<64;j++)if(!live.pending[j].active){slot=j;break;}if(slot<0){ReleaseSRWLockExclusive(&guard);say(L"特殊兵种出征队列已满，请完成当前出征。");SetLastError(error);return 0;}live.pending[slot]=intents[i];}
            ReleaseSRWLockExclusive(&guard);
        }
    }
    SetLastError(error);return original_confirm(state,button);
}
static uintptr_t create_hook(void *packet){
    DWORD error=GetLastError();int p[5],slot=-1;Intent intent={0};uintptr_t world=world_now(),city=0;S14TroopQuote q={0};
    if(packet_read((uintptr_t)packet,p)){
        AcquireSRWLockExclusive(&guard);if(live.world==world)for(int i=0;i<64;i++)if(live.pending[i].active && live.pending[i].leader==p[0]){intent=live.pending[i];slot=i;memset(&live.pending[i],0,sizeof(Intent));break;}ReleaseSRWLockExclusive(&guard);
    }
    const S14TroopDefinition *spec=definition();
    if(slot>=0 && spec && !strcmp(intent.id,spec->id) && intent.revision==spec->revision && spec->max_soldiers && p[2]>(int)spec->max_soldiers){say(L"陷阵营出征被取消：编成兵力超过 1000 人。");event("creation_cap_rejected",p[0],0,0);SetLastError(error);return 0;}
    int special=enabled && !loading && slot>=0 && spec && intent.revision==spec->revision && !strcmp(intent.id,spec->id) && intent.carrier==p[3] && intent.water==p[4] && intent.soldiers==p[2] && intent.number==p[1];
    if(slot>=0 && !special)event("intent_mismatch",p[0],0,0);
    if(special){int home;city=city_for(world,p[0],&home);int gold=integer(city+0x34),a=native_fee(world,p[3]),b=native_fee(world,p[4]);
        if(!city || home!=intent.home || a<0 || b<0 || gold<a+b || !quote_packet(world,p,(uint64_t)(gold-a-b),&q)){say(L"陷阵营出征未完成：城市资金或编成已改变。请重新下达出征。");event("creation_rejected",p[0],0,0);SetLastError(error);return 0;}
    }
    int previous_leader=creating_leader;const S14TroopDefinition *previous_definition=creating_definition;uintptr_t previous_world=creating_world;
    creating_leader=packet_read((uintptr_t)packet,p)?p[0]:0;creating_definition=special?spec:NULL;creating_world=world;
    S14AiDraw ai;s14_ai_prepare(creating_leader,(uintptr_t)__builtin_return_address(0),&ai);s14_ai_creation_begin(&ai);
    SetLastError(error);uintptr_t result=original_create(packet);error=GetLastError();s14_ai_creation_end();s14_ai_created((void*)result,&ai);creating_leader=previous_leader;creating_definition=previous_definition;creating_world=previous_world;int id;Binding key;
    if(unit_identity((void*)result,&id,&key)){
        AcquireSRWLockExclusive(&guard);if(live.world && live.world!=world)memset(&live,0,sizeof(live));live.world=world;memset(&live.units[id],0,sizeof(Binding));ReleaseSRWLockExclusive(&guard);
        if(special && key.leader==p[0] && key.carrier==p[3]){
            int before=integer(city+0x34);native_gold((void*)city,-(int)q.extra_gold);
            if(integer(city+0x34)==before-(int)q.extra_gold){strcpy(key.id,intent.id);key.revision=intent.revision;AcquireSRWLockExclusive(&guard);live.units[id]=key;ReleaseSRWLockExclusive(&guard);event("deployed",key.leader,id,(int)q.extra_gold);}
            else {say(L"陷阵营扣费结果异常，特殊效果未启用；请查看日志。");event("gold_fault",key.leader,id,(int)q.extra_gold);}
        }
    }else if(special)event("creation_failed",p[0],0,0);
    SetLastError(error);return result;
}
static float siege_hook(float base,void *a,void *t,void *f,int actual,int mode){uintptr_t caller=(uintptr_t)__builtin_return_address(0);DWORD before=GetLastError();s14_troop_status_service(a);SetLastError(before);float v=original_siege(base,a,t,f,actual,mode);DWORD e=GetLastError();v=s14_troop_attribute_at(a,S14_TROOP_SIEGE_ATTACK,actual,v,mode,caller);SetLastError(e);return v;}
static float break_hook(void *a,void *f,int actual,int mode){uintptr_t caller=(uintptr_t)__builtin_return_address(0);DWORD before=GetLastError();s14_troop_status_service(a);SetLastError(before);float v=original_break(a,f,actual,mode);DWORD e=GetLastError();v=s14_troop_attribute_at(a,S14_TROOP_SIEGE_BREAK,actual,v,mode,caller);SetLastError(e);return v;}
static float mobility_hook(void *a,void *f,int actual,int mode,void *hex){uintptr_t caller=(uintptr_t)__builtin_return_address(0);DWORD before=GetLastError();s14_troop_status_service(a);SetLastError(before);float v=original_mobility(a,f,actual,mode,hex);DWORD e=GetLastError();v=s14_troop_attribute_at(a,S14_TROOP_MOBILITY,actual,v,mode,caller);SetLastError(e);return v;}

/* Content-addressed sidecars: never add fields to the game's save format. */
typedef S14TroopCheckpoint Checkpoint;
static int folder(const wchar_t *p){DWORD a=GetFileAttributesW(p);if(a==INVALID_FILE_ATTRIBUTES){if(!CreateDirectoryW(p,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;a=GetFileAttributesW(p);}return (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT);}
static int path_for(const unsigned char *hash,wchar_t out[MAX_PATH]){if(!storage[0] || wcslen(storage)+115>=MAX_PATH)return 0;wchar_t dir[MAX_PATH];swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",storage);if(!folder(dir))return 0;swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\troops",storage);if(!folder(dir))return 0;wchar_t hex[65];for(int i=0;i<32;i++)swprintf(hex+i*2,3,L"%02x",hash[i]);return swprintf(out,MAX_PATH,L"%ls\\%ls.s14troops",dir,hex)>0;}
#define read_checkpoint s14_troop_checkpoint_read
void s14_troop_root(const wchar_t *root){if(root && wcslen(root)<MAX_PATH)wcscpy(storage,root);}
int s14_troop_save(uintptr_t world,const unsigned char *hash){
    if(!ready || loading || !world || !hash){event("save_unbound",0,0,0);return 0;}Checkpoint *c=calloc(1,sizeof(*c)),*existing=malloc(sizeof(*c));wchar_t path[MAX_PATH],temp[MAX_PATH];if(!c || !existing || !path_for(hash,path)){free(c);free(existing);return 0;}
    AcquireSRWLockShared(&guard);int matches=live.world==world;c->state=live;ReleaseSRWLockShared(&guard);if(!matches){memset(&c->state,0,sizeof(State));c->state.world=world;}
    c->state.world=0;memcpy(c->magic,"S14TROOPS.v1",12);c->version=1;c->bytes=sizeof(*c);memcpy(c->hash,hash,32);c->crc=s14_troop_crc(c->hash,sizeof(*c)-offsetof(Checkpoint,hash));
    int ok=(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES || read_checkpoint(path,existing)) && swprintf(temp,MAX_PATH,L"%ls.tmp",path)>0;
    HANDLE f=ok?CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL):INVALID_HANDLE_VALUE;DWORD n=0;
    ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,c,sizeof(*c),&n,NULL) && n==sizeof(*c) && FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok && f!=INVALID_HANDLE_VALUE)DeleteFileW(temp);
    free(c);free(existing);event(ok?"saved":"save_failed",0,0,0);return ok;
}
void s14_troop_load_begin(void){InterlockedExchange(&loading,1);AcquireSRWLockExclusive(&guard);backup=live;frame.visible=request_pending=0;ReleaseSRWLockExclusive(&guard);}
void s14_troop_load_end(uintptr_t world,int success,const unsigned char *hash,int valid){
    Checkpoint *c=malloc(sizeof(*c));wchar_t path[MAX_PATH];int found=success && valid && hash && c && path_for(hash,path) && read_checkpoint(path,c) && !memcmp(hash,c->hash,32);
    AcquireSRWLockExclusive(&guard);if(!success)live=backup;else {memset(&live,0,sizeof(live));if(found)live=c->state;live.world=world;}ReleaseSRWLockExclusive(&guard);
    if(success){selection_state=selection_tab=selected_packet=0;memset(selection,0,sizeof(selection));AcquireSRWLockExclusive(&guard);for(int i=1;i<=500;i++){Binding key;int id;void *army=(void*)ptr(world+0x7df60+(uintptr_t)i*8);if(!unit_identity(army,&id,&key) || !same_binding(&live.units[i],&key))memset(&live.units[i],0,sizeof(Binding));}ReleaseSRWLockExclusive(&guard);}
    free(c);InterlockedExchange(&loading,0);if(success)InterlockedExchange(&clear_pending_scan,1);event(found?"restored":success?"no_checkpoint":"load_failed",0,0,0);
}
void s14_troop_new_game(uintptr_t world){AcquireSRWLockExclusive(&guard);memset(&live,0,sizeof(live));live.world=world;frame.visible=request_pending=0;ReleaseSRWLockExclusive(&guard);selection_state=selection_tab=selected_packet=0;memset(selection,0,sizeof(selection));InterlockedExchange(&loading,0);}
void s14_troop_frame(S14TroopFrame *out){if(out){AcquireSRWLockShared(&guard);*out=frame;ReleaseSRWLockShared(&guard);if(!enabled || loading || !deployment(out->state) || ptr(out->state+0x490)!=out->tab || out->world!=world_now())out->visible=0;}}
/* Read-only, fail-closed coordinates for the private frame renderer. The
   original native control keeps its hit test, press flags and radio callback. */
static int detail_icon(uintptr_t dialog,S14TroopIconFrame *out){
    if(ptr(dialog)!=image+0x137e970 || (integer(dialog+0x40)&3)!=3)return 0;
    uintptr_t body=ptr(dialog+0x2e0),page=ptr(ptr(body+0x130)),army=ptr(body+0x140);
    if(ptr(body)!=image+0x1351680 || (integer(body+0x40)&3)!=3 || integer(body+0x138)!=2 ||
       ptr(page)!=image+0x137ebf0 || (integer(page+0x40)&3)!=3 || ptr(page+0x168)!=army ||
       integer(page+0x170)!=0 || !s14_troop_bound((void*)army) || integer(page+0x138)<=40)return 0;
    uintptr_t sprite=ptr(ptr(page+0x130)+40*8);float local[2];int pos[4];unsigned short wh[2];
    if(ptr(sprite)!=image+0x15602f8 || !read_at(sprite+0x130,local,8) ||
       !read_at(sprite+0xf0,wh,4) || !read_at(page+0x20,pos,16))return 0;
    float size=wh[0]>wh[1]?wh[0]:wh[1];
    if(!isfinite(local[0]) || !isfinite(local[1]) || size<16 || size>64)return 0;
    out->x=pos[0]+local[0];out->y=pos[1]-local[1];out->size=size;out->detail=1;
    return out->x>=size/2 && out->y>=size/2 && out->x+size/2<=1920 && out->y+size/2<=1080;
}
int s14_troop_icon_frame(S14TroopIconFrame *out){
    if(!out)return 0;memset(out,0,sizeof(*out));if(!enabled || loading || !image)return 0;
    S14TroopFrame f;s14_troop_frame(&f);
    if(f.visible){int p[5],b[4],t[4];
        if(!owned_slot(f.state,f.tab) || !packet_read(f.tab+0x168,p) || p[0]!=f.commander ||
           !(integer(native_slot.wrapper+0x40)&1) || !read_at(native_slot.wrapper+0x20,b,16) || !read_at(f.tab+0x20,t,16))return 0;
        out->x=t[0]+b[0]+b[2]*.5f;out->y=t[1]+b[1]+29;out->size=48;out->selected=f.selected;return 1;}
    /* The active native UI registry, rather than a retained page pointer,
       excludes closed dialogs, other tabs, water previews and reused units. */
    uintptr_t table=ptr(image+0x201c320+0x40),handle=ptr(image+0x19e6390+56);unsigned index=0;
    int count=integer(image+0x201c320+0x70);
    if(!table || !handle || count<1 || count>0x40000 || !read_at(handle,&index,4) || index>=(unsigned)count)return 0;
    static uintptr_t cached_table,cached_handle,cached_world,cached_top,cached_dialog;static unsigned cached_index;static ULONGLONG scan_due;
    uintptr_t world=world_now(),top=top_state();ULONGLONG now=GetTickCount64();
    if(cached_table==table && cached_handle==handle && cached_index==index && cached_world==world && cached_top==top){
        if(cached_dialog){if(detail_icon(cached_dialog,out))return 1;cached_dialog=0;scan_due=now;return 0;}
        if(now<scan_due)return 0;
    }
    cached_table=table;cached_handle=handle;cached_index=index;cached_world=world;cached_top=top;scan_due=now+200;
    cached_dialog=0;uintptr_t node=ptr(table+index*8),found_dialog=0;S14TroopIconFrame found={0};int matches=0;
    for(int i=0;node && i<2048;i++){
        uintptr_t object=ptr(node),next=ptr(node+8);if(next==node)return 0;
        S14TroopIconFrame candidate={0};if(detail_icon(object,&candidate)){found=candidate;found_dialog=object;matches++;}
        node=next;
    }
    if(node || matches!=1 || table!=ptr(image+0x201c320+0x40) || world!=world_now() || top!=top_state() ||
       handle!=ptr(image+0x19e6390+56) || index!=(unsigned)integer(handle))return 0;
    cached_table=table;cached_handle=handle;cached_index=index;cached_world=world;cached_top=top;cached_dialog=found_dialog;*out=found;return 1;
}
void s14_troop_request(const S14TroopFrame *f){if(!f || !f->visible)return;AcquireSRWLockExclusive(&guard);if(!request_pending){request=*f;request_pending=1;}ReleaseSRWLockExclusive(&guard);}
void s14_troop_configure(int on){LONG old=InterlockedExchange(&enabled,on && ready);if(on && ready && !old)InterlockedExchange(&clear_pending_scan,1);if(!on){AcquireSRWLockExclusive(&guard);frame.visible=request_pending=0;ReleaseSRWLockExclusive(&guard);}}
int s14_troop_ready(void){return ready!=0;}
unsigned s14_troop_capabilities(void){return ready?127:0;}
int s14_troop_next_log(char *out,size_t cap){if(!out || cap<512)return 0;for(int i=0;i<64;i++)if(InterlockedCompareExchange(&events[i].full,3,2)==2){strcpy(out,events[i].text);InterlockedExchange(&events[i].full,0);return 1;}return 0;}
int s14_troop_install(uintptr_t base,uintptr_t end){
    if(!base || end<=base || ready)return 0;
    static const uintptr_t rva[]={0x7078b0,0x71e330,0x7140c0,0x702120,0x6ed2d0,0x30f0a0,0x2a20c0,0x283d10,0x27c220,0x281e50,0x726280,0x71ea00,0x6ed740,0x6f5110,0x806ec0,0x293bf0,0x710350,0x2f05a0,0x15ba00,0x70d5a0,0x831e70,0x789820,0x7718f0,0x125f0,0x2bbfb0};
    static const unsigned char signatures[][12]={
        {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0xe8,0x12,0x23},
        {0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57},
        {0x48,0x8b,0xc4,0x55,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48},
        {0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89},
        {0x48,0x8b,0xc4,0x57,0x48,0x81,0xec,0x40,0x02,0x00,0x00,0x48},
        {0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x48,0xc7},
        {0x40,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41},
        {0x40,0x53,0x55,0x56,0x41,0x57,0x48,0x81,0xec,0x68,0x02,0x00},
        {0x40,0x55,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x58,0x02},
        {0x40,0x53,0x57,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xec},
        {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x02,0x48,0x8b,0x59},
        {0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57},
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b},
        {0x48,0x83,0xec,0x28,0x4c,0x63,0xd2,0x4c,0x8b,0xc9,0x85,0xd2},
        {0x48,0x85,0xd2,0x0f,0x84,0xe2,0x11,0x00,0x00,0x55,0x56,0x57},
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0xfa},
        {0x40,0x53,0x48,0x83,0xec,0x20,0x4c,0x8b,0x89,0x90,0x04,0x00},
        {0x44,0x8b,0xca,0x4c,0x8b,0xd1,0x85,0xd2,0x75,0x08,0x48,0x8b},
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48},
        {0x48,0x89,0x6c,0x24,0x18,0x56,0x48,0x83,0xec,0x30,0x48,0x8b},
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89},
        {0x40,0x53,0x48,0x83,0xec,0x20,0x44,0x8b,0x41,0x40,0x33,0xdb},
        {0x81,0x61,0x40,0xff,0xfe,0xff,0xff,0x8b,0xc2,0xf7,0xd8,0x45},
        {0x83,0x61,0x40,0xfb,0xf7,0xda,0x1b,0xc0,0x83,0xe0,0x04,0x09},
        {0x40,0x53,0x55,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x81}};
    for(int i=0;i<25;i++){unsigned char b[12];if(rva[i]+12>end-base || !read_at(base+rva[i],b,12) || memcmp(b,signatures[i],12))return 0;}
    image=base;image_end=end;
    void *hooks[]={tick_hook,bind_hook,confirm_hook,exit_hook,preview_hook,init_hook,create_hook,siege_hook,break_hook,mobility_hook,formation_hook,refresh_hook,funds_hook,capacity_hook,detail_hook,query_hook};
    void **originals[]={(void**)&original_tick,(void**)&original_bind,(void**)&original_confirm,(void**)&original_exit,(void**)&original_preview,(void**)&original_init,(void**)&original_create,(void**)&original_siege,(void**)&original_break,(void**)&original_mobility,(void**)&original_formation,(void**)&original_refresh,(void**)&original_funds,(void**)&original_capacity,(void**)&original_detail,(void**)&original_query};
    for(int i=0;i<16;i++)if(MH_CreateHook((void*)(base+rva[i]),hooks[i],originals[i])!=MH_OK){for(int j=0;j<i;j++)MH_RemoveHook((void*)(base+rva[j]));return 0;}
    native_abnormal=(AbnormalCall)(base+0x2bbfb0);
    native_refresh=refresh_hook;native_copy=(TwoCall)(base+0x710350);native_city=(CityCall)(base+0x2f05a0);native_gold=(GoldCall)(base+0x15ba00);
    native_button=(ButtonCall)(base+0x70d5a0);native_text=(TextCall)(base+0x831e70);native_visible=(ControlCall)(base+0x789820);native_selected=(ControlCall)(base+0x7718f0);native_enabled=(ControlCall)(base+0x125f0);
    InterlockedExchange(&ready,1);return 1;
}
