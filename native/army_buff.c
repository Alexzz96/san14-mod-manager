#define WIN32_LEAN_AND_MEAN
#include "army_buff.h"
#include "army_observer.h"
#include "troop_runtime.h"
#include "career_affix.h"
#include "ai_affix.h"
#include "MinHook.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
// Windows x64: float is XMM0; pointers are RDX/R8/R9; both ints are stack args.
typedef float (*AttributeCall)(float,void*,void*,void*,int,int);
static AttributeCall buff_original_attack,buff_original_defense;
static uintptr_t buff_image;static volatile LONG buff_ready,buff_enabled;
static int buff_install_code;static uintptr_t buff_failed_entry;
typedef struct {uintptr_t world,army;int army_id;unsigned char identity[16];} BuffIdentity;
typedef struct {volatile LONG state;int attribute,army_id,leader,mode;uintptr_t caller;float before,after;ULONGLONG tick;} BuffSample;
static BuffSample samples[64];static volatile LONG sample_sequence,sample_dropped;
#ifdef S14_ARMY_BUFF_TEST
static uintptr_t buff_test_caller;
#define BUFF_CALLER (buff_test_caller?buff_test_caller:(uintptr_t)__builtin_return_address(0))
#else
#define BUFF_CALLER ((uintptr_t)__builtin_return_address(0))
#endif
static int buff_read(uintptr_t at,void *out,size_t n){SIZE_T got;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static int buff_identity(void *army,BuffIdentity *out){
    unsigned char raw[32];uintptr_t world=0,pool=0,canonical=0,vt=0;unsigned short leader=0;
    if(!buff_image || !buff_read((uintptr_t)army,raw,sizeof(raw)))return 0;
    memcpy(&vt,raw,8);memcpy(&leader,raw+0x12,2);
    if(vt!=buff_image+0x123e288 || raw[0x10]!=1 || (leader<1 || leader>6000) ||
       !buff_read(buff_image+0x1fc91d0,&world,8) || !world || !buff_read(world+0x7df60,&pool,8) || !pool ||
       (uintptr_t)army<=pool || ((uintptr_t)army-pool)%512 || ((uintptr_t)army-pool)/512>500)return 0;
    int id=(int)(((uintptr_t)army-pool)/512);
    if(!buff_read(world+0x7df60+(uintptr_t)id*8,&canonical,8) || canonical!=(uintptr_t)army)return 0;
    out->world=world;out->army=(uintptr_t)army;out->army_id=id;memcpy(out->identity,raw+0x10,16);return 1;
}
static int buff_same(const BuffIdentity *before,void *army){
    BuffIdentity after;return buff_identity(army,&after) && before->world==after.world && before->army_id==after.army_id &&
        !memcmp(before->identity,after.identity,16);
}
static int combat_caller(uintptr_t caller){return (caller>=buff_image+0x15ca40 && caller<buff_image+0x15cc4c) || (caller>=buff_image+0x15c880 && caller<buff_image+0x15ca3a);}
static void buff_sample(int attribute,const BuffIdentity *unit,int mode,uintptr_t caller,float before,float after){
    if(!combat_caller(caller)){
        if(!s14_ai_active((void*)unit->army))return;
        typedef struct {uintptr_t world,army;float before,after;int mode;uintptr_t caller;} Last;
        static _Thread_local Last seen[501][2];Last *last=&seen[unit->army_id][attribute==S14_ARMY_DEFENSE];
        if(last->world==unit->world && last->army==unit->army && last->before==before && last->after==after && last->mode==mode && last->caller==caller)return;
        *last=(Last){unit->world,unit->army,before,after,mode,caller};
    }
    LONG n=InterlockedIncrement(&sample_sequence);BuffSample *s=&samples[(unsigned)n%64];
    if(InterlockedCompareExchange(&s->state,1,0)!=0){InterlockedIncrement(&sample_dropped);return;}
    s->attribute=attribute;s->army_id=unit->army_id;memcpy(&s->leader,unit->identity+2,2);s->mode=mode;s->caller=caller-buff_image;s->before=before;s->after=after;s->tick=GetTickCount64();InterlockedExchange(&s->state,2);
}
static float buff_attribute(int attribute,AttributeCall original,float base,void *army,void *target,void *formation,int actual,int mode,uintptr_t caller){
    DWORD previous=GetLastError();BuffIdentity unit={0};int eligible=actual==1 && buff_identity(army,&unit);
    unsigned short leader=0;memcpy(&leader,unit.identity+2,2);
    int bonus=s14_ai_percent_max(eligible && buff_enabled && s14_affix_active(unit.world,leader)?10:0,eligible && s14_ai_active(army)?10:0);
    s14_troop_status_service(army);SetLastError(previous);float native=original(base,army,target,formation,actual,mode);DWORD after=GetLastError();float result=native;int percent=0;
    if(eligible && bonus && isfinite(native) && native>=0 && buff_same(&unit,army)){
        float raised=native*(1.0f+(float)bonus/100.0f);
        if(isfinite(raised)){result=raised;percent=bonus;buff_sample(attribute,&unit,mode,caller,native,result);}
    }
    result=s14_troop_attribute_at(army,attribute,actual,result,mode,caller);
    s14_army_observer_attribute(attribute,army,actual,native,result,percent);
    SetLastError(after);return result;
}
static float buff_attack_hook(float base,void *army,void *target,void *formation,int actual,int mode){
    return buff_attribute(S14_ARMY_ATTACK,buff_original_attack,base,army,target,formation,actual,mode,BUFF_CALLER);
}
static float buff_defense_hook(float base,void *army,void *target,void *formation,int actual,int mode){
    return buff_attribute(S14_ARMY_DEFENSE,buff_original_defense,base,army,target,formation,actual,mode,BUFF_CALLER);
}
int s14_army_buff_install(uintptr_t base,uintptr_t end){
    const uintptr_t rv[]={0x283ad0,0x27bd90};const unsigned char signature[2][12]={
        {0x48,0x89,0x5c,0x24,0x08,0x56,0x57,0x41,0x56,0x48,0x81,0xec},
        {0x40,0x55,0x56,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x81,0xec}
    };
    buff_install_code=0;buff_failed_entry=0;
    if(!base || end<=base || InterlockedCompareExchange(&buff_ready,0,0)){buff_install_code=-1;return 0;}
    for(int i=0;i<2;i++){unsigned char bytes[12];if(rv[i]>end-base || 12>end-base-rv[i] || !buff_read(base+rv[i],bytes,12) || memcmp(bytes,signature[i],12)){buff_install_code=-2;buff_failed_entry=rv[i];return 0;}}
    buff_image=base;
    MH_STATUS status=MH_CreateHook((void*)(base+rv[0]),buff_attack_hook,(void**)&buff_original_attack);
    if(status!=MH_OK){buff_install_code=(int)status;buff_failed_entry=rv[0];return 0;}
    status=MH_CreateHook((void*)(base+rv[1]),buff_defense_hook,(void**)&buff_original_defense);
    if(status!=MH_OK){MH_RemoveHook((void*)(base+rv[0]));buff_install_code=(int)status;buff_failed_entry=rv[1];return 0;}
    InterlockedExchange(&buff_ready,1);return 1;
}
int s14_army_buff_install_code(void){return buff_install_code;}
uintptr_t s14_army_buff_failed_entry(void){return buff_failed_entry;}
void s14_army_buff_configure(int enabled){int active=enabled && InterlockedCompareExchange(&buff_ready,0,0);InterlockedExchange(&buff_enabled,active);s14_affix_configure(active);}
int s14_army_buff_ready(void){return InterlockedCompareExchange(&buff_ready,0,0)!=0;}
int s14_army_buff_enabled(void){return InterlockedCompareExchange(&buff_enabled,0,0)!=0;}
int s14_army_buff_next_log(char *out,size_t cap){
    if(!out || cap<768)return 0;
    for(int i=0;i<64;i++)if(InterlockedCompareExchange(&samples[i].state,3,2)==2){
        BuffSample *s=&samples[i];int n=snprintf(out,cap,"{\"event\":\"army_buff_applied\",\"officer_id\":%d,\"army_id\":%d,\"attribute\":%d,\"percent\":10,\"native_float\":%.9g,\"buffed_float\":%.9g,\"mode\":%d,\"caller_rva\":\"0x%llx\",\"combat_path\":%s,\"tick_ms\":%llu,\"queue_dropped\":%ld,\"gameplay_modified\":true}\n",s->leader,s->army_id,s->attribute,(double)s->before,(double)s->after,s->mode,(unsigned long long)s->caller,combat_caller(buff_image+s->caller)?"true":"false",(unsigned long long)s->tick,InterlockedCompareExchange(&sample_dropped,0,0));
        InterlockedExchange(&s->state,0);return n>0 && (size_t)n<cap;
    }return 0;
}
