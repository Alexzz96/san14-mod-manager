#define WIN32_LEAN_AND_MEAN
#include "personality_edit.h"
#include "officer_edit.h"
#include "battle_probe.h"
#include <string.h>
#include <stdio.h>
#include <wchar.h>
typedef void (*SetSlot)(void*,int,int);
typedef void (*ClearSlot)(void*,int);
static uintptr_t image;
static SetSlot native_set;static ClearSlot native_clear;
static SRWLOCK lock=SRWLOCK_INIT;static S14PersonalityEdit request;
static int ready,enabled,result,selected_slot,busy;static ULONGLONG submitted;
static char log_line[512];static int log_pending;
static S14EditRequest generic_request;
static S14EditResult generic_results[64];
static uint64_t next_id,generic_id;
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
int s14_edit_capability(int operation){return operation==S14_EDIT_PERSONALITIES && ready;}
int s14_edit_capability_info(int operation,S14EditCapability *out){
    if(!out || operation<0 || operation>=S14_EDIT_OPERATION_COUNT)return 0;
    static const wchar_t *missing[]={L"",L"原生五维设置会更新成长数据；当前 EXE 的计算函数使用改写后的数值公式，表项和参数约定尚未核实。",
        L"已学十槽已定位；增删设置及本次出征装备读取尚未接入。",L"官职关联清理、任命和调换路径尚未核实。",
        L"亲爱容量及原生增删路径尚未核实。",L"厌恶容量及原生增删路径尚未核实。",
        L"结义成员容量、解除和重组路径尚未核实。",L"战法书库存模型及原生增减路径尚未核实。",
        L"名品持有者字段已定位；关联更新和转移路径尚未核实。"};
    *out=(S14EditCapability){.operation=operation,.available=s14_edit_capability(operation)};
    const wchar_t *reason=operation==S14_EDIT_PERSONALITIES?(ready?L"个性编辑已接入并通过独立测试；实机验收待执行。":L"原生个性设置或旧效果清理函数的兼容检查未通过。"):missing[operation];
    wcsncpy(out->reason,reason,191);return 1;
}
int s14_edit_capture(uintptr_t world,int id,S14EditSnapshot *out){
    if(!out || s14_battle_is_loading())return 0;
    unsigned epoch=s14_battle_session_epoch();uintptr_t p=person(world,id);unsigned char second[512];
    S14EditSnapshot captured={.version=1,.epoch=epoch,.world=world,.person=p,.officer=id};
    if(!p || !get(p,captured.raw,512) || !get(p,second,512) || memcmp(captured.raw,second,512) ||
       epoch!=s14_battle_session_epoch() || s14_battle_is_loading() || person(world,id)!=p)return 0;
    *out=captured;return 1;
}
static int current_snapshot(const S14EditSnapshot *before){S14EditSnapshot now;return before->version==1 && s14_edit_capture(before->world,before->officer,&now) &&
    before->person==now.person && before->epoch==now.epoch && !memcmp(before->raw,now.raw,512);}
static void generic_complete(int status,const wchar_t *detail,int changed,int verified){
    if(!generic_id)return;
    S14EditResult *r=&generic_results[generic_id%64];r->status=status;r->changed=changed;r->verified=verified;
    wcsncpy(r->detail,detail,191);r->detail[191]=0;generic_id=0;
}
/* All requests, including rejected submissions, have an inspectable result.
   Keep the in-flight result out of the ring eviction path. */
static uint64_t record_result(const S14EditRequest *edit,int status,const wchar_t *detail){
    uint64_t id;
    do{id=++next_id;}while(!id || (generic_id && id%64==generic_id%64));
    S14EditResult *r=&generic_results[id%64];
    *r=(S14EditResult){.id=id,.status=status,.operation=edit?edit->operation:-1,.officer=edit?edit->before.officer:0};
    wcsncpy(r->detail,detail,191);return id;
}
static int reject_request(const S14EditRequest *edit,uint64_t *id,int status,const wchar_t *detail){
    AcquireSRWLockExclusive(&lock);*id=record_result(edit,status,detail);ReleaseSRWLockExclusive(&lock);return status;
}
int s14_edit_submit(const S14EditRequest *edit,uint64_t *id){
    if(!id)return S14_EDIT_FAILED;*id=0;
    if(!edit || edit->version!=1)return reject_request(edit,id,S14_EDIT_INCOMPATIBLE,L"请求格式或版本不兼容，未执行。");
    if(!s14_edit_capability(edit->operation)){S14EditCapability capability;
        return reject_request(edit,id,S14_EDIT_INCOMPATIBLE,s14_edit_capability_info(edit->operation,&capability)?capability.reason:L"操作类型不兼容，未执行。");}
    unsigned short before[9];memcpy(before,edit->before.raw+0x150,18);
    if(s14_edit_personality_validate(before,edit->personalities)!=S14_PE_PENDING)return reject_request(edit,id,S14_EDIT_FAILED,L"个性编号或重复项校验失败，未执行。");
    if(!current_snapshot(&edit->before))return reject_request(edit,id,S14_EDIT_EXPIRED,L"身份、会话或修改前快照已改变，未执行；请刷新。");
    AcquireSRWLockExclusive(&lock);
    if(!enabled || busy || result==S14_PE_PENDING){*id=record_result(edit,S14_EDIT_INCOMPATIBLE,!enabled?L"编辑功能已关闭，未执行。":L"已有请求正在等待或执行，本次请求未入队。");ReleaseSRWLockExclusive(&lock);return S14_EDIT_INCOMPATIBLE;}
    generic_request=*edit;generic_id=record_result(edit,S14_EDIT_QUEUED,L"已排队，等待游戏规划回调。");
    submitted=GetTickCount64();result=S14_PE_PENDING;*id=generic_id;
    ReleaseSRWLockExclusive(&lock);return S14_EDIT_QUEUED;
}
int s14_edit_result(uint64_t id,S14EditResult *out){
    if(!id || !out)return 0;AcquireSRWLockExclusive(&lock);
    if(generic_id==id && !busy && (s14_battle_is_loading() || generic_request.before.epoch!=s14_battle_session_epoch())){result=S14_PE_STALE;generic_complete(S14_EDIT_EXPIRED,L"存档会话变化，未执行的请求已取消。",0,0);}
    if(generic_id==id && !busy && GetTickCount64()-submitted>10000){result=S14_PE_UNAVAILABLE;generic_complete(S14_EDIT_EXPIRED,L"请求超过 10 秒，未执行；请刷新后重试。",0,0);}
    int ok=generic_results[id%64].id==id;if(ok)*out=generic_results[id%64];ReleaseSRWLockExclusive(&lock);return ok;
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
void s14_personality_enabled(int on){AcquireSRWLockExclusive(&lock);enabled=on && ready;if(!enabled && result==S14_PE_PENDING && !busy){result=S14_PE_UNAVAILABLE;generic_complete(S14_EDIT_EXPIRED,L"编辑已关闭，未执行的请求已取消。",0,0);}ReleaseSRWLockExclusive(&lock);}
int s14_personality_submit(uintptr_t world,int id,const unsigned short before[9],int add,int slot){
    int target=0,status=s14_personality_plan(before,add,slot,&target);if(status!=S14_PE_PENDING)return status;
    unsigned short current[9];if(!s14_personality_capture(world,id,current) || memcmp(current,before,18) || s14_battle_is_loading())return S14_PE_STALE;
    AcquireSRWLockExclusive(&lock);
    if(!enabled || busy || result==S14_PE_PENDING){ReleaseSRWLockExclusive(&lock);return S14_PE_UNAVAILABLE;}
    generic_id=0;
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
static int execute_generic(const S14EditRequest *r,int *changed,int *verified,wchar_t detail[192]){
    wcscpy(detail,L"身份或修改前快照已改变，未执行；请刷新。");
    if(!current_snapshot(&r->before))return S14_EDIT_EXPIRED;
    unsigned short before[9];memcpy(before,r->before.raw+0x150,18);
    if(s14_edit_personality_validate(before,r->personalities)!=S14_PE_PENDING){wcscpy(detail,L"个性编号或重复项校验失败，未执行。");return S14_EDIT_FAILED;}
    /* Validate every definition and the entire army pool before the first
       cleanup. An unreadable candidate is not silently treated as absent. */
    for(int i=0;i<9;i++)if(before[i]!=r->personalities[i] && r->personalities[i]){
        uintptr_t def=pointer(r->before.world+0x7d440+r->personalities[i]*8);unsigned char raw[224];
        if(!get(def,raw,sizeof(raw)) || pointer(def)!=image+0x12a0658){swprintf(detail,192,L"第%d格的个性 #%u 定义不可读或类型不兼容，未执行。",i+1,r->personalities[i]);return S14_EDIT_INCOMPATIBLE;}
    }
    uintptr_t pool=pointer(r->before.world+0x7df60);void *army=NULL;if(!pool){wcscpy(detail,L"原生部队池不可读，未执行旧效果清理或修改。");return S14_EDIT_FAILED;}
    for(int i=1;i<=500;i++){
        uintptr_t a=pointer(r->before.world+0x7df60+(uintptr_t)i*8);unsigned char b[48];
        if(a!=pool+(uintptr_t)i*512 || !get(a,b,48) || pointer(a)!=image+0x123e288){swprintf(detail,192,L"部队槽 #%d 的身份、地址或类型不兼容，未执行。",i);return S14_EDIT_INCOMPATIBLE;}
        unsigned short leader;memcpy(&leader,b+0x12,2);
        if(b[0x10]==1 && leader==r->before.officer){if(army){wcscpy(detail,L"该武将出现多个活动部队，无法确认旧效果所属部队，未执行。");return S14_EDIT_FAILED;}army=(void*)a;}
    }
    if(!current_snapshot(&r->before))return S14_EDIT_EXPIRED;
    for(int i=0;i<9;i++)if(before[i]!=r->personalities[i]){
        if(army && before[i])native_clear(army,i);
        native_set((void*)r->before.person,i,r->personalities[i]);(*changed)++;
    }
    unsigned short after[9];
    if(!s14_personality_capture(r->before.world,r->before.officer,after)){swprintf(detail,192,L"已执行%d个槽位调用，但执行后快照不可读；请立即刷新检查当前状态。",*changed);return S14_EDIT_FAILED;}
    int failed=-1;for(int i=0;i<9;i++){if(after[i]!=r->personalities[i] && failed<0)failed=i;if(before[i]!=r->personalities[i] && after[i]==r->personalities[i])(*verified)++;}
    if(failed>=0){swprintf(detail,192,L"第%d格复读为 #%u，预期 #%u；已调用%d格，核对成功%d格。请刷新检查。",failed+1,after[failed],r->personalities[failed],*changed,*verified);return S14_EDIT_FAILED;}
    wcscpy(detail,L"修改已复读核对；请正常保存游戏。");return S14_EDIT_SUCCESS;
}
void s14_personality_service(void *state){
    DWORD error=GetLastError();AcquireSRWLockExclusive(&lock);
    if(result!=S14_PE_PENDING || busy){ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
    if(!enabled){result=S14_PE_UNAVAILABLE;ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
    /* Called only by the already validated native planning callback. Do not
       apply while loading or during turn execution. */
    int phase=-1;
    if(!generic_id && (s14_battle_is_loading() || request.epoch!=s14_battle_session_epoch() || (state && get((uintptr_t)state+0x470,&phase,4) && phase>=3))){result=S14_PE_STALE;ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
    if(generic_id && (s14_battle_is_loading() || generic_request.before.epoch!=s14_battle_session_epoch() ||
       (state && get((uintptr_t)state+0x470,&phase,4) && phase>=3))){result=S14_PE_STALE;generic_complete(S14_EDIT_EXPIRED,L"读档、存档会话变化或回合执行已取消请求。",0,0);ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
    if(!state || !get((uintptr_t)state+0x470,&phase,4) || phase<0 || phase>2 || s14_battle_is_loading()){
        if(GetTickCount64()-submitted>10000){result=S14_PE_UNAVAILABLE;generic_complete(S14_EDIT_EXPIRED,L"等待规划回调超时，未执行。",0,0);}ReleaseSRWLockExclusive(&lock);SetLastError(error);return;
    }
    if(generic_id){
        S14EditRequest edit=generic_request;uint64_t id=generic_id;busy=1;ReleaseSRWLockExclusive(&lock);
        wchar_t detail[192];int changed=0,verified=0,status=execute_generic(&edit,&changed,&verified,detail);
        AcquireSRWLockExclusive(&lock);busy=0;result=status==S14_EDIT_SUCCESS?S14_PE_APPLIED:S14_PE_FAILED;
        generic_complete(status,detail,changed,verified);
        snprintf(log_line,sizeof(log_line),"{\"event\":\"officer_edit\",\"request_id\":%llu,\"operation\":%d,\"officer_id\":%d,\"changed\":%d,\"verified\":%d,\"result\":%d,\"game_thread\":%lu}\n",(unsigned long long)id,edit.operation,edit.before.officer,changed,verified,status,(unsigned long)GetCurrentThreadId());log_pending=1;
        ReleaseSRWLockExclusive(&lock);SetLastError(error);return;
    }
    S14PersonalityEdit edit=request;busy=1;ReleaseSRWLockExclusive(&lock);int completed=execute(&edit);
    AcquireSRWLockExclusive(&lock);busy=0;result=completed;
    snprintf(log_line,sizeof(log_line),"{\"event\":\"personality_edit\",\"officer_id\":%d,\"slot\":%d,\"add\":%d,\"before\":%u,\"after\":6,\"result\":%d,\"game_thread\":%lu}\n",edit.officer,edit.slot+1,edit.add,edit.before[edit.slot],result,(unsigned long)GetCurrentThreadId());log_pending=1;
    ReleaseSRWLockExclusive(&lock);SetLastError(error);
}
int s14_personality_next_log(char *text,size_t size){AcquireSRWLockExclusive(&lock);int ok=log_pending && text && size>=sizeof(log_line);if(ok){strcpy(text,log_line);log_pending=0;}ReleaseSRWLockExclusive(&lock);return ok;}
