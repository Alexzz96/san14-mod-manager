#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "personality_edit.c"
static int loading_test;static unsigned epoch_test=1;
int s14_battle_is_loading(void){return loading_test;}
unsigned int s14_battle_session_epoch(void){return epoch_test;}
#undef assert
#define assert(x) do{if(!(x)){fprintf(stderr,"FAILED %d: %s\n",__LINE__,#x);exit(2);}}while(0)
static unsigned char *world,*officer,*units,*state,*definition;
static int sets,clears,previous,expected_slot,generic_mode,fail_set;
static void ptrput(void *at,uintptr_t p){memcpy(at,&p,8);}static void halfput(void *at,unsigned short v){memcpy(at,&v,2);}
static void setter(void *p,int slot,int id){assert(p==officer && (generic_mode || (slot==expected_slot && id==6)));sets++;if(fail_set)return;halfput(officer+0x150+slot*2,id);unsigned short mask;memcpy(&mask,officer+0x1bc,2);mask&=~(1u<<slot);halfput(officer+0x1bc,mask);halfput(officer+0x1be + slot*2,0);SetLastError(333);}
static void cleanup(void *army,int slot){assert(army==units+512 && (generic_mode || slot==expected_slot));unsigned short old;memcpy(&old,officer+0x150+slot*2,2);assert(generic_mode?old!=0:old==previous);clears++;s14_personality_service(state);}
int main(void){
    unsigned short slots[9]={1,2,3,4,5,300,0,355,0};int target=-1;
    assert(s14_personality_plan(slots,1,0,&target)==S14_PE_PENDING && target==6);assert(s14_personality_plan(slots,0,5,&target)==S14_PE_PENDING && target==5);
    assert(s14_personality_plan(slots,0,6,&target)==S14_PE_FAILED);assert(s14_personality_plan(slots,0,-1,&target)==S14_PE_FAILED);assert(s14_personality_plan(slots,0,9,&target)==S14_PE_FAILED);
    unsigned short full[9]={1,2,3,4,5,7,8,9,10};assert(s14_personality_plan(full,1,0,&target)==S14_PE_FULL);full[8]=6;assert(s14_personality_plan(full,1,0,&target)==S14_PE_DUPLICATE);full[8]=356;assert(s14_personality_plan(full,1,0,&target)==S14_PE_FAILED);
    image=(uintptr_t)calloc(1,0x2100000);uintptr_t image_end=image+0x2100000;world=calloc(1,0x100000);officer=calloc(1,512);units=calloc(501,512);state=calloc(1,0x480);definition=calloc(1,224);assert(image&&world&&officer&&units&&state&&definition);
    ptrput((void*)(image+0x1fc91d0),(uintptr_t)world);ptrput(world+0x148+518*8,(uintptr_t)officer);ptrput(officer,image+0x12a00d0);halfput(officer+0x10,518);
    for(int i=0;i<=500;i++){ptrput(world+0x7df60+i*8,(uintptr_t)(units+i*512));ptrput(units+i*512,image+0x123e288);}
    units[512+0x10]=1;halfput(units+512+0x12,518);ptrput(world+0x7d440+6*8,(uintptr_t)definition);ptrput(definition,image+0x12a0658);memcpy(definition+0x10,L"远矢",6);definition[0xb6]=105;definition[0xb8]=1;
    memcpy((void*)(image+0x21d5f0),(unsigned char[]){0x83,0xfa,0x08,0x77,0x4f,0x4c,0x63,0xca},8);memcpy((void*)(image+0x2748b0),(unsigned char[]){0x40,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x30},9);
    *(unsigned char*)(image+0x2748b0)=0;assert(!s14_personality_init(image,image_end) && !ready);*(unsigned char*)(image+0x2748b0)=0x40;
    assert(s14_personality_init(image,image_end));native_set=setter;native_clear=cleanup;s14_personality_enabled(1);memcpy(officer+0x150,slots,18);halfput(officer+0x1bc,511);halfput(officer+0x1be + 6*2,12);
    unsigned short captured[9];assert(s14_personality_capture((uintptr_t)world,518,captured) && !memcmp(captured,slots,18));
    expected_slot=6;assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_PENDING);assert(sets==0 && clears==0);assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_UNAVAILABLE);
    *(int*)(state+0x470)=3;s14_personality_service(state);assert(sets==0 && s14_personality_result(NULL)==S14_PE_STALE);*(int*)(state+0x470)=0;assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_PENDING);SetLastError(777);s14_personality_service(state);assert(GetLastError()==777 && sets==1 && clears==0 && s14_personality_result(&target)==S14_PE_APPLIED && target==6);
    slots[6]=6;assert(s14_personality_capture((uintptr_t)world,518,captured) && !memcmp(slots,captured,18));assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_DUPLICATE);
    slots[6]=0;memcpy(officer+0x150,slots,18);expected_slot=5;previous=300;assert(s14_personality_submit((uintptr_t)world,518,slots,0,5)==S14_PE_PENDING);s14_personality_service(state);assert(clears==1 && sets==2 && s14_personality_result(NULL)==S14_PE_APPLIED);assert(!memcmp(officer+0x150,slots,10) && !memcmp(officer+0x15c,slots+6,6));
    memcpy(officer+0x150,slots,18);assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_PENDING);epoch_test++;s14_personality_service(state);assert(s14_personality_result(NULL)==S14_PE_STALE && sets==2);
    assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_PENDING);halfput(officer+0x150,99);s14_personality_service(state);assert(s14_personality_result(NULL)==S14_PE_STALE && sets==2);memcpy(officer+0x150,slots,18);
    assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_PENDING);s14_personality_enabled(0);s14_personality_service(state);assert(s14_personality_result(NULL)==S14_PE_UNAVAILABLE && sets==2);s14_personality_enabled(1);
    loading_test=1;assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_STALE);loading_test=0;
    assert(s14_personality_submit((uintptr_t)world,518,slots,1,0)==S14_PE_PENDING);definition[0xb6]=1;s14_personality_service(state);assert(s14_personality_result(NULL)==S14_PE_UNAVAILABLE && sets==2);
    char log[512];assert(s14_personality_next_log(log,sizeof(log)) && strstr(log,"personality_edit"));
    generic_mode=1;definition[0xb6]=105;
    for(int i=1;i<356;i++)ptrput(world+0x7d440+i*8,(uintptr_t)definition);
    unsigned short staged[9];memcpy(staged,slots,18);staged[6]=42;staged[0]=0;staged[5]=354;
    assert(s14_edit_personality_validate(slots,staged)==S14_PE_PENDING);
    staged[0]=42;assert(s14_edit_personality_validate(slots,staged)==S14_PE_DUPLICATE);staged[0]=0;
    staged[0]=356;assert(s14_edit_personality_validate(slots,staged)==S14_PE_FAILED);staged[0]=0;
    unsigned short duplicates[9]={3,3};assert(s14_edit_personality_validate(duplicates,duplicates)==S14_PE_PENDING);
    duplicates[2]=3;assert(s14_edit_personality_validate(slots,duplicates)==S14_PE_DUPLICATE);
    S14EditRequest edit={.version=1,.operation=S14_EDIT_PERSONALITIES};S14EditResult outcome;uint64_t id=0,first=0;
    assert(s14_edit_capability(S14_EDIT_PERSONALITIES) && !s14_edit_capability(S14_EDIT_RANK));
    assert(s14_edit_capture((uintptr_t)world,518,&edit.before));memcpy(edit.personalities,staged,18);
    int before_sets=sets,before_clears=clears;assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED && id);first=id;
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_QUEUED && sets==before_sets);
    s14_personality_service(state);assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_SUCCESS && outcome.changed==3 && outcome.verified==3);
    assert(sets==before_sets+3 && clears==before_clears+2 && !memcmp(officer+0x150,staged,18));
    assert(s14_personality_next_log(log,sizeof(log)) && strstr(log,"request_id"));
    assert(s14_edit_submit(&edit,&id)==S14_EDIT_EXPIRED && id);
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_EXPIRED && outcome.changed==0);
    assert(s14_edit_capture((uintptr_t)world,518,&edit.before));memcpy(edit.personalities,staged,18);edit.personalities[8]=43;
    assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);officer[0x124]++;before_sets=sets;s14_personality_service(state);
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_EXPIRED && sets==before_sets);
    assert(s14_edit_capture((uintptr_t)world,518,&edit.before));assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);
    *(int*)(state+0x470)=3;s14_personality_service(state);*(int*)(state+0x470)=0;
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_EXPIRED && sets==before_sets);
    assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);loading_test=1;
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_EXPIRED);loading_test=0;s14_personality_service(state);assert(sets==before_sets);
    assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);epoch_test++;
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_EXPIRED && sets==before_sets);
    assert(s14_edit_capture((uintptr_t)world,518,&edit.before));assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);
    s14_personality_enabled(0);assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_EXPIRED);s14_personality_enabled(1);
    ptrput(world+0x7d440+43*8,0);assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);s14_personality_service(state);
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_INCOMPATIBLE && sets==before_sets);
    assert(wcsstr(outcome.detail,L"#43") && wcsstr(outcome.detail,L"第9格"));
    ptrput(world+0x7d440+43*8,(uintptr_t)definition);fail_set=1;
    assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);s14_personality_service(state);fail_set=0;
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_FAILED && outcome.changed==1 && outcome.verified==0);
    assert(wcsstr(outcome.detail,L"第9格") && wcsstr(outcome.detail,L"#43"));
    assert(s14_edit_result(first,&outcome) && outcome.status==S14_EDIT_SUCCESS);
    assert(s14_edit_capture((uintptr_t)world,518,&edit.before));assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);submitted-=10001;
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_EXPIRED);s14_personality_service(state);
    S14EditCapability capability;
    for(int op=0;op<S14_EDIT_OPERATION_COUNT;op++){assert(s14_edit_capability_info(op,&capability) && capability.operation==op && capability.reason[0]);assert(capability.available==(op==S14_EDIT_PERSONALITIES));}
    assert(!s14_edit_capability_info(-1,&capability) && !s14_edit_capability_info(S14_EDIT_OPERATION_COUNT,&capability));
    S14EditRequest rejected=edit;rejected.operation=S14_EDIT_RANK;
    assert(s14_edit_submit(&rejected,&id)==S14_EDIT_INCOMPATIBLE && id);
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_INCOMPATIBLE && wcsstr(outcome.detail,L"官职"));
    rejected=edit;rejected.personalities[8]=356;
    assert(s14_edit_submit(&rejected,&id)==S14_EDIT_FAILED && id);
    assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_FAILED && sets==before_sets+1);
    assert(s14_edit_submit(NULL,&id)==S14_EDIT_INCOMPATIBLE && id);
    assert(s14_edit_result(id,&outcome) && outcome.operation==-1);
    assert(s14_edit_capture((uintptr_t)world,518,&edit.before));assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);
    uint64_t pending_id=id;
    for(int n=0;n<130;n++){assert(s14_edit_submit(&edit,&id)==S14_EDIT_INCOMPATIBLE && id!=pending_id);assert(s14_edit_result(id,&outcome) && outcome.status==S14_EDIT_INCOMPATIBLE);}
    assert(s14_edit_result(pending_id,&outcome) && outcome.status==S14_EDIT_QUEUED);
    s14_personality_service(state);assert(s14_edit_result(pending_id,&outcome) && outcome.status==S14_EDIT_SUCCESS);
    for(int n=0;n<65;n++){assert(s14_edit_capture((uintptr_t)world,518,&edit.before));assert(s14_edit_submit(&edit,&id)==S14_EDIT_QUEUED);s14_personality_enabled(0);s14_personality_enabled(1);}
    assert(!s14_edit_result(first,&outcome));
    puts("{\"status\":\"passed\",\"nine_slots\":true,\"empty_only_add\":true,\"explicit_replace\":true,\"hidden_ids_preserved\":true,\"full_rejected\":true,\"duplicate_rejected\":true,\"native_setter\":true,\"cleanup_before_replace\":true,\"game_thread_queue\":true,\"nested_callback_safe\":true,\"load_epoch_rejected\":true,\"stale_snapshot_rejected\":true,\"execution_phase_blocked\":true,\"disabled_queue_cancelled\":true,\"definition_verified\":true,\"generic_request_ids\":true,\"catalog_personalities\":true,\"staged_batch_add_replace_delete\":true,\"whole_snapshot_guard\":true,\"execution_queue_cancelled\":true,\"failure_never_full_success\":true,\"bounded_result_history\":true}");return 0;
}
