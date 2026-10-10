#define WIN32_LEAN_AND_MEAN
#include "army_observer.h"
#include "troop_runtime.h"
#include "MinHook.h"
#include <string.h>
typedef uintptr_t (*PageCall)(void*,void*);
typedef int (*PacketCall)(int*,void*,void*,int);
typedef uintptr_t (*AggregateCall)(void*,int,int*,int,int);
typedef int (*ExtraCall)(void*,int);
typedef float (*FactorCall)(void*,void*,int,int);
typedef int (*AbilityCall)(void*,int,int);
typedef int (*PolicyCall)(void*,void*);
typedef int (*StrengthCall)(void*,int);
typedef float (*AreaCall)(void*,int,int*);
typedef int (*LinkCall)(void*,void*);
static PageCall original_page;static PacketCall original_actual,original_baseline;
static AggregateCall original_aggregate;static ExtraCall original_extra;static FactorCall original_factor;
static AbilityCall original_ability;static PolicyCall original_policy;static StrengthCall original_strength;
static AreaCall original_area;static LinkCall original_link;
static uintptr_t image;static volatile LONG ready,active,serial;
static SRWLOCK trace_lock=SRWLOCK_INIT;static S14ArmyTrace latest;
static _Thread_local S14ArmyTrace *context;static _Thread_local int actual_scope;
static _Thread_local int packet_scope=-1,factor_scope=-1,policy_scope=-1;
#ifdef S14_ARMY_TEST
static _Thread_local uintptr_t test_caller;
#define ARMY_CALLER (test_caller?test_caller:(uintptr_t)__builtin_return_address(0))
#else
#define ARMY_CALLER ((uintptr_t)__builtin_return_address(0))
#endif
static int read_at(uintptr_t p,void *out,size_t n){SIZE_T got;return p && ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got) && got==n;}
static unsigned short half(const unsigned char *p){unsigned short v;memcpy(&v,p,2);return v;}
static int local_read(void *c,uintptr_t p,void *out,size_t n){(void)c;return read_at(p,out,n);}
static int attribute(uintptr_t caller){
    if(caller>=image+0x283ad0 && caller<image+0x283d10)return S14_ARMY_ATTACK;
    if(caller>=image+0x283d10 && caller<image+0x283ff0)return S14_ARMY_CITY;
    if(caller>=image+0x27c220 && caller<image+0x27c500)return S14_ARMY_BREAK;
    if(caller>=image+0x281e50 && caller<image+0x282220)return S14_ARMY_MOVE;
    if(caller>=image+0x27bd90 && caller<image+0x27c090)return S14_ARMY_DEFENSE;
    return -1;
}
static int identity(void *army,S14ArmyTrace *t){
    unsigned char b[80];uintptr_t vt=0,pool=0,canonical=0;
    if(!read_at(image+0x1fc91d0,&t->world,8) || !t->world || !read_at((uintptr_t)army,b,sizeof(b)))return 0;
    memcpy(&vt,b,8);if(vt!=image+0x123e288 || b[0x10]!=1 || half(b+0x12)!=518)return 0;
    if(!read_at(t->world+0x7df60,&pool,8) || (uintptr_t)army<=pool || ((uintptr_t)army-pool)%512)return 0;
    uintptr_t index=((uintptr_t)army-pool)/512;
    if(index>500 || !read_at(t->world+0x7df60+index*8,&canonical,8) || canonical!=(uintptr_t)army)return 0;
    t->army=(uintptr_t)army;t->army_id=(int)index;t->officer_id=518;memcpy(t->identity,b+0x10,16);
    const int offsets[]={0x4b,0x4c,0x4e,0x4a,0x4d};
    const uintptr_t steps[]={0x18ec204,0x18ec20c,0x18ec214,0x18ec1fc,0x18ec21c};
    const uintptr_t limits[]={0x18ebe30,0x18ebe44,0x18ebe70,0x18ebe14,0x18ebe78};
    for(int i=0;i<5;i++){
        t->temporary[i]=(signed char)b[offsets[i]];
        if(!read_at(image+steps[i],&t->steps[i],4) || !read_at(image+limits[i],&t->limits[i],4) ||
           t->steps[i]<0 || t->steps[i]>100 || t->limits[i]<0 || t->limits[i]>100)return 0;
    }return 1;
}
static uintptr_t page_hook(void *page,void *army){
    DWORD before=GetLastError();S14ArmyTrace trace={0},*previous=context;int old_scope=actual_scope,old_packet=packet_scope;
    uintptr_t vt=0;int observe=InterlockedCompareExchange(&active,0,0) && !previous &&
        read_at((uintptr_t)page,&vt,8) && vt==image+0x137ebf0 && identity(army,&trace);
    if(observe){trace.page=(uintptr_t)page;context=&trace;actual_scope=0;packet_scope=-1;}
    else if(previous){context=NULL;actual_scope=0;}
    SetLastError(before);uintptr_t result=original_page(page,army);DWORD after=GetLastError();
    context=previous;actual_scope=old_scope;packet_scope=old_packet;
    if(observe && trace.packets==3){
        unsigned char identity_after[16],temporary_after[5];uintptr_t world=0;
        const int order[]={1,2,4,0,3};int temporary_stable=read_at((uintptr_t)army+0x4a,temporary_after,5);
        for(int i=0;i<5;i++)if(temporary_stable && (signed char)temporary_after[order[i]]!=trace.temporary[i])temporary_stable=0;
        if(read_at((uintptr_t)army+0x10,identity_after,16) && !memcmp(trace.identity,identity_after,16) &&
           temporary_stable && read_at((uintptr_t)page+0x170,&trace.preview_mode,4) &&
           read_at(image+0x1fc91d0,&world,8) && world==trace.world && TryAcquireSRWLockExclusive(&trace_lock)){
            trace.tick=GetTickCount64();trace.serial=(unsigned int)InterlockedIncrement(&serial);latest=trace;ReleaseSRWLockExclusive(&trace_lock);
        }
    }SetLastError(after);return result;
}
static int packet(int actual,int *out,void *army,void *formation,int mode){
    DWORD before=GetLastError();int old_scope=actual_scope,old_packet=packet_scope;S14ArmyTrace *t=context;int observe=t && t->army==(uintptr_t)army;
    if(observe){actual_scope=actual?1:0;packet_scope=actual;
        int values[5];if(s14_army_formation(local_read,NULL,image,(uintptr_t)formation,values)){memcpy(t->formation,values,20);for(int i=0;i<5;i++)t->formation_seen[i]=1;}
        const uintptr_t weights[]={0x18ec1c8,0x18ec1c8,0,0,0x18ec1d0};
        for(int i=0;i<5;i++)if(weights[i])t->global_seen[i]=read_at(image+weights[i],&t->global_percent[i],4);
    }SetLastError(before);
    int result=(actual?original_actual:original_baseline)(out,army,formation,mode);DWORD after=GetLastError();
    if(observe && read_at((uintptr_t)out,actual?t->actual:t->baseline,20)){
        t->packets|=actual?2:1;if(actual)t->actual_mode=mode;else t->baseline_mode=mode;
    }actual_scope=old_scope;packet_scope=old_packet;SetLastError(after);return result;
}
static int actual_hook(int *out,void *army,void *formation,int mode){return packet(1,out,army,formation,mode);}
static int baseline_hook(int *out,void *army,void *formation,int mode){return packet(0,out,army,formation,mode);}
static uintptr_t aggregate_hook(void *army,int zero,int *out,int mode,int flag){
    uintptr_t caller=ARMY_CALLER;uintptr_t result=original_aggregate(army,zero,out,mode,flag);DWORD after=GetLastError();
    s14_troop_clear_modifiers(army,out);
    int i=attribute(caller);S14ArmyTrace *t=context;
    if(t && packet_scope>=0 && t->army==(uintptr_t)army && i>=0){
        const int categories[]={5,6,8,4,7};int values[119];
        if(read_at((uintptr_t)out,values,sizeof(values))){
            int *a=actual_scope?t->aggregate:t->base_aggregate,*all=actual_scope?t->all:t->base_all,*seen=actual_scope?t->observed:t->base_observed,*conditional=actual_scope?t->conditional:t->base_conditional;
            a[i]=values[categories[i]];all[i]=values[19];seen[i]=1;
            if(i==S14_ARMY_MOVE){unsigned char special=0;if(!read_at((uintptr_t)army+0x140,&special,1))seen[i]=0;else conditional[i]=special?values[116]:0;}
            if(actual_scope && i==0){uintptr_t person=0;t->sources_seen=read_at((uintptr_t)army+0x64,t->source_ids,72) &&
                read_at(t->world+0x148+518*8,&person,8) && read_at(person+0x150,t->source_ids+36,18) && read_at(person+0x1bc,&t->source_mask,2);t->source_mode=flag;}
        }
    }SetLastError(after);return result;
}
static int extra_hook(void *army,int mode){
    uintptr_t caller=ARMY_CALLER;int result=original_extra(army,mode);DWORD after=GetLastError();int i=attribute(caller);S14ArmyTrace *t=context;
    if(t && packet_scope>=0 && t->army==(uintptr_t)army && i>=0){(actual_scope?t->extra:t->base_extra)[i]=result;(actual_scope?t->extra_observed:t->base_extra_observed)[i]=1;}
    SetLastError(after);return result;
}
static float factor_hook(void *army,void *city,int mode,int actual){
    DWORD before=GetLastError();mode=s14_troop_surround_mode(army,mode,actual,ARMY_CALLER);SetLastError(before);
    S14ArmyTrace *t=context;int previous=factor_scope;int observe=t && packet_scope>=0 && t->army==(uintptr_t)army && !city && mode==1 && (actual==0 || actual==1);
    factor_scope=observe?actual:-1;
    float result=original_factor(army,city,mode,actual);DWORD after=GetLastError();factor_scope=previous;
    if(t && t->army==(uintptr_t)army && !city && mode==1 && (actual==0 || actual==1)){t->factor[actual]=result;t->factor_seen[actual]=1;}
    SetLastError(after);return result;
}
static int ability_hook(void *person,int attribute_id,int flags){
    uintptr_t caller=ARMY_CALLER;int result=original_ability(person,attribute_id,flags);DWORD after=GetLastError();S14ArmyTrace *t=context;
    if(t && factor_scope>=0 && attribute_id==0 && flags==0xfff && caller>=image+0x27b180 && caller<image+0x27b6c5){
        unsigned short id=0;if(read_at((uintptr_t)person+0x10,&id,2) && id==518){t->leadership[factor_scope]=result;t->leadership_seen[factor_scope]=1;}
        t->foundation_seen=read_at(image+0x12a7870,&t->leadership_exponent,8) && read_at(image+0x18ebd58,&t->sqrt_divisor,4) &&
            read_at(image+0x18ebd60,&t->leadership_scale,4) && read_at(image+0x18ebd68,&t->foundation_flat,4);
    }SetLastError(after);return result;
}
static int policy_hook(void *army,void *formation){
    uintptr_t caller=ARMY_CALLER;S14ArmyTrace *t=context;int i=attribute(caller),previous=policy_scope;
    int observe=t && packet_scope>=0 && t->army==(uintptr_t)army && i>=0;policy_scope=observe?i:-1;
    int result=original_policy(army,formation);DWORD after=GetLastError();policy_scope=previous;
    if(observe){t->policy[packet_scope][i]=result;t->policy_seen[packet_scope][i]=1;}
    SetLastError(after);return result;
}
static int strength_hook(void *force,int id){
    int result=original_strength(force,id);DWORD after=GetLastError();S14ArmyTrace *t=context;
    if(t && packet_scope>=0 && policy_scope>=0){t->policy_id[packet_scope][policy_scope]=id;t->policy_strength[packet_scope][policy_scope]=result;}
    SetLastError(after);return result;
}
static float area_hook(void *tile,int force,int *out){
    float result=original_area(tile,force,out);DWORD after=GetLastError();S14ArmyTrace *t=context;
    if(t && factor_scope==1){unsigned short id=0;int units=0;if(read_at((uintptr_t)tile+0x12,&id,2) && read_at((uintptr_t)out,&units,4)){
        t->area_factor=result;t->area_seen=1;t->area_units=units;t->area_id=id;t->area_force=force;}}
    SetLastError(after);return result;
}
static int link_hook(void *army,void *list){
    int result=original_link(army,list);DWORD after=GetLastError();S14ArmyTrace *t=context;
    if(t && factor_scope==1 && t->army==(uintptr_t)army){int setting=0;t->link_count=result;t->link_seen=1;
        if(read_at(image+0x18ea620+0xd8,&setting,4))t->link_step_seen=read_at(image+(setting==0?0x18ebba0:setting==1?0x18ebd88:0x18ebd88),&t->link_step,4);}
    SetLastError(after);return result;
}
int s14_army_observer_install(uintptr_t base,uintptr_t end){
    const uintptr_t rv[]={0x8062d0,0x2ffa60,0x2ffb70,0x243e70,0x2788c0,0x27b180,0x20ce20,0x3ca600,0x3ca6c0,0x282230,0x2815d0};
    const unsigned char signatures[11][12]={
        {0x48,0x85,0xd2,0x0f,0x84,0xe5,0x0b,0,0,0x55,0x56,0x57},
        {0x48,0x89,0x5c,0x24,8,0x48,0x89,0x74,0x24,0x10,0x48,0x89},
        {0x48,0x89,0x5c,0x24,8,0x48,0x89,0x6c,0x24,0x10,0x48,0x89},
        {0x48,0x8b,0xc4,0x44,0x89,0x48,0x20,0x89,0x50,0x10,0x55,0x56},
        {0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x81,0xec,0x20,2,0},
        {0x48,0x8b,0xc4,0x44,0x89,0x48,0x20,0x44,0x89,0x40,0x18,0x55},
        {0x48,0x8b,0xc4,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57},
        {0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b},
        {0x41,0x57,0x48,0x83,0xec,0x50,0x48,0xc7,0x44,0x24,0x20,0xfe},
        {0x48,0x8b,0xc4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56},
        {0x48,0x8b,0xc4,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57}
    };
    void *detours[]={page_hook,actual_hook,baseline_hook,aggregate_hook,extra_hook,factor_hook,ability_hook,policy_hook,strength_hook,area_hook,link_hook};
    void **originals[]={(void**)&original_page,(void**)&original_actual,(void**)&original_baseline,(void**)&original_aggregate,(void**)&original_extra,(void**)&original_factor,(void**)&original_ability,(void**)&original_policy,(void**)&original_strength,(void**)&original_area,(void**)&original_link};
    if(!base || end<=base || InterlockedCompareExchange(&ready,0,0))return 0;image=base;
    for(int i=0;i<11;i++){unsigned char b[12];if(base+rv[i]+12>end || !read_at(base+rv[i],b,12) || memcmp(b,signatures[i],12))return 0;}
    int made=0;for(;made<11;made++)if(MH_CreateHook((void*)(base+rv[made]),detours[made],originals[made])!=MH_OK)break;
    if(made!=11){for(int i=0;i<made;i++)MH_RemoveHook((void*)(base+rv[i]));return 0;}
    InterlockedExchange(&ready,1);return 1;
}
void s14_army_observer_configure(int enabled){InterlockedExchange(&active,enabled && InterlockedCompareExchange(&ready,0,0));}
int s14_army_observer_ready(void){return InterlockedCompareExchange(&ready,0,0)!=0;}
int s14_army_observer_snapshot(S14ArmyTrace *out){
    if(!out || !TryAcquireSRWLockShared(&trace_lock))return 0;*out=latest;ReleaseSRWLockShared(&trace_lock);return out->packets==3;
}
void s14_army_observer_attribute(int i,void *army,int actual,float native,float final,int percent){
    S14ArmyTrace *t=context;
    if(t && packet_scope==1 && actual_scope && actual==1 && t->army==(uintptr_t)army && (i==S14_ARMY_ATTACK || i==S14_ARMY_DEFENSE)){
        t->native_attribute[i]=native;t->buffed_attribute[i]=final;t->buff_percent[i]=percent;t->buff_seen[i]=1;
    }
}
