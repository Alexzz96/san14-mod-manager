// Native pass-through tests use private objects, never a live game process.
#define main legacy_battle_fixture
#include "test_battle_probe.c"
#undef main
static unsigned int return_bits=0x3e828f5c; // 0.255f
static int wanted_mode=1,wanted_duration=15,wanted_option=-17,apply_state=1;
static unsigned char source_hex[32];
static unsigned char group_own[17],group_enemy[17];
static float rate(void *person,int morale,int type,void *context,void *source,int valid) {
    assert(person==mock_people && morale==109 && type==27 && context==mock_armies+1024);
    assert(source==mock_people+0x220 && valid==-9 && GetLastError()==1234);
    float result;memcpy(&result,&return_bits,4);native_calls++;SetLastError(777);return result;
}
static uintptr_t damage_with_rate(int type,int id,int amount,int source_type,int source_id) {
    assert(type==27 && id==2 && source_type==27 && source_id==1 && GetLastError()==1234);
    SetLastError(1234);float result=hooked_battle_wound_rate(mock_people,109,27,mock_armies+1024,mock_people+0x220,-9);
    assert(GetLastError()==777);unsigned int bits;memcpy(&bits,&result,4);assert(bits==return_bits);
    unsigned char *a=mock_armies+1024;
    // These fixed outcomes correspond to independently checked scalar SSE
    // calculations, rather than deriving assertions from the recorder.
    if(amount==2012) {word(a+0x16,2988);word(a+0x18,212);}
    else if(amount==81) {word(a+0x16,419);word(a+0x18,159);}
    else assert(0);
    native_calls++;SetLastError(777);return sentinel;
}
static uintptr_t abnormal(void *target,void *source,int mode,int duration,void *hex,const wchar_t *name,int option) {
    assert(target==mock_armies+1024 && source==mock_armies+512 && mode==wanted_mode);
    assert(duration==wanted_duration && hex==source_hex && name==(const wchar_t*)sentinel && option==wanted_option);
    assert(GetLastError()==1234);
    if(apply_state && mode>=0 && mode<3) ((unsigned char*)target)[0x23+mode]=(unsigned char)duration;
    native_calls++;SetLastError(777);return sentinel;
}
static BattleEvent *last_kind(int k) {
    BattleEvent *result=NULL;
    for(int i=0;i<BATTLE_QSIZE;i++) if(battle_queue[i].state==2 && battle_queue[i].e.kind==k && (!result || result->id<battle_queue[i].e.id)) result=&battle_queue[i].e;
    return result;
}
int main(int argc,char **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    assert(argc==2);
    mock_image=VirtualAlloc(NULL,0x1900000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(mock_image);
    mock_g=calloc(1,0x85200);mock_settings=calloc(1,0x50);mock_armies=calloc(501,512);mock_people=calloc(2,0x220);
    assert(mock_g && mock_settings && mock_armies && mock_people);
    battle_base=(uintptr_t)mock_image;battle_end=battle_base+0x1900000;battle_manager=&mock_g;battle_ready=1;
    *(void**)(mock_g+0x85130)=mock_settings;
    for(int i=0;i<501;i++) *(void**)(mock_g+0x7df60+i*8)=mock_armies+i*512;
    for(int i=0;i<2;i++) {
        unsigned char *p=mock_people+i*0x220;*(uintptr_t*)p=battle_base+0x12a00d0;word(p+0x10,10+10*i);
        memcpy(p+0x12,i?L"曹":L"张",4);memcpy(p+0x24,i?L"仁":L"嶷",4);*(void**)(mock_g+0x148+(10+10*i)*8)=p;
        unsigned char *a=mock_armies+(i+1)*512;*(uintptr_t*)a=battle_base+0x123e288;a[0x10]=1;word(a+0x12,10+10*i);word(a+0x16,5000);
    }
    *(uintptr_t*)group_own=*(uintptr_t*)group_enemy=battle_base+0x129fec8;group_own[0x10]=1;group_enemy[0x10]=2;
    *(void**)(mock_g+0xde40+3*8)=group_own;*(void**)(mock_g+0xde40+4*8)=group_own;*(void**)(mock_g+0xde40+5*8)=group_enemy;
    // Player settings use actual force 1; person fields use group IDs 3/5.
    mock_people[0x118]=3;mock_people[0x220+0x118]=5;battle_force=1;
    assert(battle_actual_force((uintptr_t)mock_g,3)==1 && battle_actual_force((uintptr_t)mock_g,4)==1 && battle_actual_force((uintptr_t)mock_g,5)==2);
    int percent=15;float divisor=100.f;memcpy(mock_image+0x18ebb8c,&percent,4);memcpy(mock_image+0x123ea5c,&divisor,4);
    original_troops=damage_with_rate;original_wound_rate=rate;original_abnormal=abnormal;
    s14_battle_configure(S14_MASTER,0);SetLastError(1234);
    assert(hooked_battle_troops(27,2,2012,27,1)==sentinel && GetLastError()==777 && !queued());
    SetLastError(1234);assert(hooked_battle_abnormal(mock_armies+1024,mock_armies+512,1,15,source_hex,(void*)sentinel,-17)==sentinel && GetLastError()==777 && !queued());
    s14_battle_configure(S14_MASTER,1);word(mock_armies+1024+0x16,5000);word(mock_armies+1024+0x18,0);
    SetLastError(1234);assert(hooked_battle_troops(27,2,2012,27,1)==sentinel && GetLastError()==777);
    BattleEvent *e=last_kind(B_TROOPS),*r=last_kind(B_WOUND_RATE);
    assert(e && r && e->wound_rate_count==1 && e->wound_rate_bits==return_bits && e->attrition_read && e->attrition_percent==15 && e->attrition_divisor_bits==0x42c80000);
    assert(e->player_force_id==1 && e->source.force_id==1 && e->target.force_id==2 && e->target.raw_force==0);
    assert(r->parent==e->id && r->source.id==1 && r->target.id==2 && r->args[5]==(uintptr_t)(intptr_t)-9);
    assert(e->target.field18==0 && e->target_after.field18==212 && !battle_damage && !battle_parent);
    // Negative net wounded change does not mean no wounds were generated.
    return_bits=0x3db851ec;word(mock_armies+1024+0x16,500);word(mock_armies+1024+0x18,164);
    SetLastError(1234);assert(hooked_battle_troops(27,2,81,27,1)==sentinel && GetLastError()==777);
    e=last_kind(B_TROOPS);assert(e->target.field18==164 && e->target_after.field18==159);
    // Float ABI: ordinary values, signed zero, infinities, and NaN payload must
    // return unchanged even with the recorder active. No arithmetic in detour.
    unsigned int patterns[]={0x3e828f5c,0x80000000,0x7fc12345,0x7f800000,0xff800000};
    BattleEvent parent={0};parent.id=999;parent.target=battle_object((uintptr_t)mock_g,mock_armies+1024);parent.source=battle_object((uintptr_t)mock_g,mock_armies+512);
    battle_damage=&parent;battle_parent=parent.id;
    for(int i=0;i<5;i++) {
        return_bits=patterns[i];SetLastError(1234);float result=hooked_battle_wound_rate(mock_people,109,27,mock_armies+1024,mock_people+0x220,-9);
        unsigned int got;memcpy(&got,&result,4);assert(got==patterns[i] && GetLastError()==777 && last_kind(B_WOUND_RATE)->wound_rate_bits==patterns[i]);
    }
    assert(parent.wound_rate_count==5);battle_damage=NULL;battle_parent=0;
    int count=queued();SetLastError(1234);hooked_battle_wound_rate(mock_people,109,27,mock_armies+1024,mock_people+0x220,-9);assert(queued()==count && GetLastError()==777);
    // Actual recipient differs from dispatcher subject (the caster).
    BattleEvent action={0},effect={0};action.id=1000;effect.id=1001;
    action.source=effect.source=battle_object((uintptr_t)mock_g,mock_armies+512);effect.target=effect.source;
    effect.tactic_id=42;wcscpy(effect.tactic_name,L"止步");battle_action=&action;battle_effect=&effect;battle_category=3;battle_category_slot=0;battle_parent=effect.id;
    mock_armies[1024+0x24]=0;SetLastError(1234);
    assert(hooked_battle_abnormal(mock_armies+1024,mock_armies+512,1,15,source_hex,(void*)sentinel,-17)==sentinel && GetLastError()==777);
    e=last_kind(B_ABNORMAL);assert(e && e->target.id==2 && e->source.id==1 && e->source_context_verified && e->abnormal_before==0 && e->abnormal_after==15 && e->args[6]==(uintptr_t)(intptr_t)-17 && e->native_return==sentinel);
    assert(e->effect_id==1001 && e->action_id==1000 && e->effect_category==3 && e->post_identity_matches);
    // Native rejection is still recorded; it is not a successful application.
    apply_state=0;SetLastError(1234);hooked_battle_abnormal(mock_armies+1024,mock_armies+512,1,15,source_hex,(void*)sentinel,-17);
    e=last_kind(B_ABNORMAL);assert(e->abnormal_before==15 && e->abnormal_after==15);
    // Unsupported mode: no unchecked indexing, while arguments pass through.
    wanted_mode=-3;SetLastError(1234);hooked_battle_abnormal(mock_armies+1024,mock_armies+512,-3,15,source_hex,(void*)sentinel,-17);
    e=last_kind(B_ABNORMAL);assert(e->abnormal_before==-1 && e->abnormal_after==-1);
    // Mismatched originating action cannot manufacture verified attribution.
    action.source=effect.target=battle_object((uintptr_t)mock_g,mock_armies+1024);wanted_mode=1;SetLastError(1234);
    hooked_battle_abnormal(mock_armies+1024,mock_armies+512,1,15,source_hex,(void*)sentinel,-17);assert(!last_kind(B_ABNORMAL)->source_context_verified);
    battle_effect=NULL;battle_action=NULL;battle_parent=0;count=queued();SetLastError(1234);
    hooked_battle_abnormal(mock_armies+1024,mock_armies+512,1,15,source_hex,(void*)sentinel,-17);assert(queued()==count && GetLastError()==777);
    wchar_t root[MAX_PATH],dir[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));CreateDirectoryW(root,NULL);
    swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);CreateDirectoryW(dir,NULL);swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\logs",root);CreateDirectoryW(dir,NULL);
    int total=queued();s14_battle_worker(root);assert(battle_events==total && !battle_fault && !battle_dropped && !battle_read_errors);
    FlushFileBuffers(battle_file);CloseHandle(battle_file);
    printf("{\"status\":\"passed\",\"written_events\":%d,\"float32_return_bits_preserved\":true,\"six_and_seven_argument_abi\":true,\"last_error_preserved\":true,\"actual_abnormal_target\":true,\"failed_state_not_success\":true,\"negative_net_wounded\":true,\"disabled_and_unrelated_calls_bypassed\":true}\n",total);
    VirtualFree(mock_image,0,MEM_RELEASE);free(mock_g);free(mock_settings);free(mock_armies);free(mock_people);return 0;
}
