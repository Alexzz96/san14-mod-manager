#define WIN32_LEAN_AND_MEAN
#include "personality_edit.h"
#include "battle_probe.h"
#include <string.h>
#include <stdio.h>
typedef void (*SetSlot)(void*,int,int);
typedef void (*ClearSlot)(void*,int);
static uintptr_t image;
static SetSlot native_set;static ClearSlot native_clear;
static SRWLOCK lock=SRWLOCK_INIT;static S14PersonalityEdit request;
static int ready,enabled,result,selected_slot,busy;static ULONGLONG submitted;
static char log_line[512];static int log_pending;
static int get(uintptr_t at,void *out,size_t n){SIZE_T got=0;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static uintptr_t pointer(uintptr_t at){uintptr_t v=0;get(at,&v,8);return v;}
static uintptr_t person(uintptr_t world,int id){
    if(!ready || !world || id<1 || id>6000 || pointer(image+0x1fc91d0)!=world)return 0;
    uintptr_t p=pointer(world+0x148+(uintptr_t)id*8);unsigned short identity=0;
    return pointer(p)==image+0x12a00d0 && get(p+0x10,&identity,2) && identity==id?p:0;
}
int s14_personality_capture(uintptr_t world,int id,unsigned short slots[9]){
    uintptr_t p=person(world,id);if(!p || !slots || !get(p+0x150,slots,18))return 0;
    for(int i=0;i<9;i++)if(slots[i]>355)return 0;return pointer(image+0x1fc91d0)==world && person(world,id)==p;
}
int s14_personality_init(uintptr_t base,uintptr_t end){
    const unsigned char cleanup[]={0x40,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x30};unsigned char b[16];
    /* Entry bytes are checked below against this build's actual Win64 setter;
       no executable version/hash lock. No new detours are installed. */
    ready=0;native_set=NULL;native_clear=NULL;image=base;
    if(!base || end<=base+0x274ab0 || !get(base+0x21d5f0,b,8) ||
       memcmp(b,(unsigned char[]){0x83,0xfa,0x08,0x77,0x4f,0x4c,0x63,0xca},8) ||
       !get(base+0x2748b0,b,sizeof(cleanup)) || memcmp(b,cleanup,sizeof(cleanup)))return 0;
    native_set=(SetSlot)(base+0x21d5f0);native_clear=(ClearSlot)(base+0x2748b0);ready=1;return 1;
}
void s14_personality_enabled(int on){AcquireSRWLockExclusive(&lock);enabled=on && ready;if(!enabled && result==S14_PE_PENDING)result=S14_PE_UNAVAILABLE;ReleaseSRWLockExclusive(&lock);}
int s14_personality_submit(uintptr_t world,int id,const unsigned short before[9],int add,int slot){
    int target=0,status=s14_personality_plan(before,add,slot,&target);if(status!=S14_PE_PENDING)return status;
    unsigned short current[9];if(!s14_personality_capture(world,id,current) || memcmp(current,before,18) || s14_battle_is_loading())return S14_PE_STALE;
    AcquireSRWLockExclusive(&lock);
    if(!enabled || busy || result==S14_PE_PENDING){ReleaseSRWLockExclusive(&lock);return S14_PE_UNAVAILABLE;}
    request=(S14PersonalityEdit){.world=world,.officer=id,.slot=target,.add=add,.epoch=s14_battle_session_epoch()};memcpy(request.before,before,18);
    submitted=GetTickCount64();selected_slot=target;result=S14_PE_PENDING;ReleaseSRWLockExclusive(&lock);return S14_PE_PENDING;
}
int s14_personality_result(int *slot){AcquireSRWLockExclusive(&lock);if(result==S14_PE_PENDING && !busy && GetTickCount64()-submitted>10000)result=S14_PE_UNAVAILABLE;int r=result;if(slot)*slot=selected_slot;ReleaseSRWLockExclusive(&lock);return r;}
static int execute(const S14PersonalityEdit *r){
    unsigned short slots[9];uintptr_t p=person(r->world,r->officer);int target=-1;
    if(!p || s14_battle_is_loading() || r->epoch!=s14_battle_session_epoch() || !s14_personality_capture(r->world,r->officer,slots) || memcmp(slots,r->before,18))return S14_PE_STALE;
    if(s14_personality_plan(slots,r->add,r->slot,&target)!=S14_PE_PENDING || target!=r->slot)return S14_PE_STALE;
    uintptr_t def=pointer(r->world+0x7d440+6*8);unsigned char raw[224];
    if(!get(def,raw,sizeof(raw)) || pointer(def)!=image+0x12a0658 ||
       memcmp(raw+0x10,L"远矢",6) || raw[0xb6]!=105 || raw[0xb8]!=1 || raw[0xb9]!=0)return S14_PE_UNAVAILABLE;
    /* Resolve ONLY the current canonical army of this officer. The native
       cleanup runs BEFORE changing the ID, so its aura-owner lookup sees the
       old personality. It clears owned neighbor entries, activation/timers and
       stale native effect objects. Never mutate somebody else's slots. */
    uintptr_t pool=pointer(r->world+0x7df60);void *army=NULL;
    if(!pool)return S14_PE_FAILED;
    for(int i=1;i<=500;i++){
        uintptr_t a=pointer(r->world+0x7df60+(uintptr_t)i*8);unsigned char b[48];
        if(a!=pool+(uintptr_t)i*512 || !get(a,b,48) || pointer(a)!=image+0x123e288 || b[0x10]!=1)continue;
        unsigned short leader;memcpy(&leader,b+0x12,2);if(leader==r->officer){if(army)return S14_PE_FAILED;army=(void*)a;}
    }
    if(army && slots[target])native_clear(army,target);
    native_set((void*)p,target,6);
    unsigned short expected[9];memcpy(expected,slots,18);expected[target]=6;
    return s14_personality_capture(r->world,r->officer,slots) && !memcmp(expected,slots,18)?S14_PE_APPLIED:S14_PE_FAILED;
}
void s14_personality_service(void *state){
    DWORD error=GetLastError();AcquireSRWLockExclusive(&lock);
    if(result!=S14_PE_PENDING || busy){ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
    if(!enabled){result=S14_PE_UNAVAILABLE;ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
    /* Called only by the already validated native planning callback. Do not
       apply while loading or during turn execution. */
    int phase=-1;
    if(!state || !get((uintptr_t)state+0x470,&phase,4) || phase<0 || phase>2 || s14_battle_is_loading()){
        if(GetTickCount64()-submitted>10000)result=S14_PE_UNAVAILABLE;ReleaseSRWLockExclusive(&lock);SetLastError(error);return;
    }
    S14PersonalityEdit edit=request;busy=1;ReleaseSRWLockExclusive(&lock);int completed=execute(&edit);
    AcquireSRWLockExclusive(&lock);busy=0;result=completed;
    snprintf(log_line,sizeof(log_line),"{\"event\":\"personality_edit\",\"officer_id\":%d,\"slot\":%d,\"add\":%d,\"before\":%u,\"after\":6,\"result\":%d,\"game_thread\":%lu}\n",edit.officer,edit.slot+1,edit.add,edit.before[edit.slot],result,(unsigned long)GetCurrentThreadId());log_pending=1;
    ReleaseSRWLockExclusive(&lock);SetLastError(error);
}
int s14_personality_next_log(char *text,size_t size){AcquireSRWLockExclusive(&lock);int ok=log_pending && text && size>=sizeof(log_line);if(ok){strcpy(text,log_line);log_pending=0;}ReleaseSRWLockExclusive(&lock);return ok;}
