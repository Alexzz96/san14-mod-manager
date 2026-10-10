#define WIN32_LEAN_AND_MEAN
#include "affix_names.h"
#include "career_affix.h"
#include "ai_affix.h"
#include "troop_runtime.h"
#include "MinHook.h"
#include <string.h>
#include <wchar.h>
typedef uintptr_t (*PersonNameCall)(void*,wchar_t*);
typedef const wchar_t *(*ArmyNameCall)(void*);
static PersonNameCall original_person_name;
static ArmyNameCall original_army_name;
static uintptr_t names_base;
static volatile LONG names_ready;
static _Thread_local wchar_t titled_army[24];
static int names_read(uintptr_t at,void *out,size_t n){SIZE_T got;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static int person_identity(void *person,uintptr_t *world,unsigned char raw[72]){
    uintptr_t canonical=0,vt=0;unsigned short id=0;
    if(!names_read((uintptr_t)person,raw,72))return 0;memcpy(&vt,raw,8);memcpy(&id,raw+0x10,2);
    return vt==names_base+0x12a00d0 && id>=1 && id<=6000 && names_read(names_base+0x1fc91d0,world,8) && *world &&
        names_read(*world+0x148+(uintptr_t)id*8,&canonical,8) && canonical==(uintptr_t)person && (s14_affix_active(*world,id) || s14_ai_person_active(*world,id));
}
static uintptr_t person_name_hook(void *person,wchar_t *out){
    DWORD error=GetLastError();unsigned char raw[72];uintptr_t world=0;int eligible=out && person_identity(person,&world,raw);
    // The native formatter accesses only the two fixed nine-character fields.
    // Format a local copy with the same field limits; never prepend into an
    // unknown caller buffer or write the live person / game save.
    if(eligible){wchar_t family[9];memcpy(family,raw+0x12,18);int n=0;while(n<9 && family[n])n++;
        if(n<=6 && (n<2 || family[0]!=L'神' || family[1]!=L' ')){
            unsigned short id;memcpy(&id,raw+0x10,2);wchar_t titled[9]={s14_ai_person_active(world,id)?L'禁':L'神',L' '};memcpy(titled+2,family,(size_t)n*2);memcpy(raw+0x12,titled,18);
            SetLastError(error);return original_person_name(raw,out);
        }
    }
    SetLastError(error);return original_person_name(person,out);
}
static const wchar_t *army_name_hook(void *army){
    DWORD error=GetLastError();SetLastError(error);const wchar_t *native=original_army_name(army);DWORD after=GetLastError();
    unsigned char raw[32],person[72];uintptr_t world=0,pool=0,canonical=0,officer=0,vt=0;unsigned short leader=0;
    if(names_read((uintptr_t)army,raw,32)){memcpy(&vt,raw,8);memcpy(&leader,raw+0x12,2);
        if(vt==names_base+0x123e288 && raw[0x10]==1 && leader==S14_ELITE_OFFICER && names_read(names_base+0x1fc91d0,&world,8) && world &&
           names_read(world+0x7df60,&pool,8) && (uintptr_t)army>pool && !(((uintptr_t)army-pool)%512) && ((uintptr_t)army-pool)/512<=500 &&
           names_read(world+0x7df60+((uintptr_t)army-pool)/512*8,&canonical,8) && canonical==(uintptr_t)army &&
           names_read(world+0x148+(uintptr_t)leader*8,&officer,8) && person_identity((void*)officer,&world,person)){
            wchar_t name[20]={0};size_t n=0;for(int part=0;part<2;part++)for(int i=0;i<9;i++){wchar_t ch;memcpy(&ch,person+0x12+part*18+i*2,2);if(!ch)break;name[n++]=ch;}
            if(n && s14_affix_display(world,leader,name,titled_army,24))native=titled_army;
        }
    }
    const S14TroopDefinition *troop=s14_troop_bound(army);
    if(troop && !strcmp(troop->id,"san14.xianzhen")){wcscpy(titled_army,L"高顺 · 陷阵营");native=titled_army;}
    wchar_t random_name[24];if(s14_ai_name(army,native,random_name,24)){wcscpy(titled_army,random_name);native=titled_army;}
    SetLastError(after);return native;
}
int s14_affix_names_install(uintptr_t base,uintptr_t end){
    static const uintptr_t rvas[]={0x20c270,0x20bec0};
    static const unsigned char signatures[2][12]={{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x65,0x48},{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x01,0x48,0x8b,0xd9}};
    if(!base || end<=base || names_ready)return 0;
    for(int i=0;i<2;i++){unsigned char b[12];if(rvas[i]+12>end-base || !names_read(base+rvas[i],b,12) || memcmp(b,signatures[i],12))return 0;}
    names_base=base;
    if(MH_CreateHook((void*)(base+rvas[0]),person_name_hook,(void**)&original_person_name)!=MH_OK)return 0;
    if(MH_CreateHook((void*)(base+rvas[1]),army_name_hook,(void**)&original_army_name)!=MH_OK){MH_RemoveHook((void*)(base+rvas[0]));return 0;}
    InterlockedExchange(&names_ready,1);return 1;
}
int s14_affix_names_ready(void){return InterlockedCompareExchange(&names_ready,0,0)!=0;}
