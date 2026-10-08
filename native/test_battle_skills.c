// Reuse the same private objects as the argument/concurrency fixture; no game process.
#define main legacy_battle_fixture
#include "test_battle_probe.c"
#undef main
static unsigned char manager_mock[0xa0],source_entry_mock[16],effect_entry_mock[16],definition_mock[0x88];
static unsigned char hexes_mock[102*32];
static int parameters_mock[10]={3,40,10,130,130,163,160,40,40,8},action_targets=1,fire_hex=101,fire_request=120;
static uintptr_t skill_category(void *definition,int slot) {
    assert(definition==*(void**)(effect_entry_mock+8) && (slot==0 || slot==1) && GetLastError()==1234);
    native_calls++;SetLastError(777);return slot==0?0xfedcba9876543211ull:0xfedcba9876543200ull;
}
static uintptr_t skill_fire(void *hex,int value) {
    assert(hex==hexes_mock+fire_hex*32 && value==fire_request);
    assert(GetLastError()==1234);word((unsigned char*)hex+0x16,(unsigned short)(value<0?0:value));
    native_calls++;SetLastError(777);return sentinel;
}
static uintptr_t skill_damage(int type,int id,int amount,int source_type,int source_id) {
    assert(type==27 && id==2 && amount==88 && source_type==255 && source_id==-1);
    assert(GetLastError()==1234);unsigned char *army=mock_armies+id*512;
    word(army+0x16,battle_word(army+0x16)-88);word(army+0x18,battle_word(army+0x18)+17);
    native_calls++;SetLastError(777);return sentinel;
}
static uintptr_t skill_effect(void *manager,void *entry,void *values) {
    assert(manager==manager_mock && entry==effect_entry_mock && values==(void*)0x123456789abcdef0ull);
    assert(GetLastError()==1234);native_calls++;
    SetLastError(1234);assert(hooked_battle_category(*(void**)(effect_entry_mock+8),0)==0xfedcba9876543211ull && GetLastError()==777);
    SetLastError(1234);assert(hooked_battle_fire(hexes_mock+fire_hex*32,fire_request)==sentinel && GetLastError()==777);
    SetLastError(1234);assert(hooked_battle_troops(27,2,88,255,-1)==sentinel && GetLastError()==777);
    SetLastError(1234);assert(hooked_battle_category(*(void**)(effect_entry_mock+8),1)==0xfedcba9876543200ull && GetLastError()==777);
    SetLastError(777);return sentinel;
}
static uintptr_t skill_action(void *manager,void *source,void *target) {
    assert(manager==manager_mock && source==source_entry_mock && target==(void*)0xfedcba9876543210ull);
    assert(GetLastError()==1234);native_calls++;
    for(int i=0;i<action_targets;i++) {
        SetLastError(1234);assert(hooked_battle_effect(manager,effect_entry_mock,(void*)0x123456789abcdef0ull)==sentinel && GetLastError()==777);
    }
    SetLastError(777);return sentinel;
}
static BattleEvent *find_kind(int k,LONG64 after) {
    for(int i=0;i<BATTLE_QSIZE;i++) if(battle_queue[i].state==2 && battle_queue[i].e.kind==k && battle_queue[i].e.id>after) return &battle_queue[i].e;
    return NULL;
}
int main(int argc,char **argv) {
    assert(argc==2);
    mock_image=VirtualAlloc(NULL,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(mock_image);
    mock_g=calloc(1,0x85200);mock_settings=calloc(1,0x50);mock_armies=calloc(501,512);mock_people=calloc(2,0x220);
    assert(mock_g && mock_settings && mock_armies && mock_people);
    battle_base=(uintptr_t)mock_image;battle_end=battle_base+0x300000;battle_manager=&mock_g;battle_ready=1;
    *(void**)(mock_g+0x85130)=mock_settings;
    for(int i=0;i<501;i++) *(void**)(mock_g+0x7df60+i*8)=mock_armies+i*512;
    for(int i=0;i<2;i++) {
        unsigned char *p=mock_people+i*0x220;*(uintptr_t*)p=battle_base+0x12a00d0;word(p+0x10,10+10*i);
        memcpy(p+0x12,i?L"曹":L"张",4);memcpy(p+0x24,i?L"仁":L"嶷",4);
        *(void**)(mock_g+0x148+(10+10*i)*8)=p;
        unsigned char *a=mock_armies+(i+1)*512;*(uintptr_t*)a=battle_base+0x123e288;a[0x10]=1;
        word(a+0x12,10+10*i);word(a+0x16,5000);word(a+0x2a,100+i);
    }
    for(int i=0;i<102;i++) *(void**)(mock_g+0xdfe0+i*8)=hexes_mock+i*32;
    word(hexes_mock+101*32+0x1c,2);word(hexes_mock+100*32+0x1c,1);
    *(void**)(manager_mock+0x98)=mock_armies+512;
    word(source_entry_mock,1);*(int*)(source_entry_mock+4)=27;
    // The real dispatcher descriptor names the caster, while the fire setter
    // acts on the victim's hex. Do not equate the two objects.
    *(int*)effect_entry_mock=27;word(effect_entry_mock+4,1);*(void**)(effect_entry_mock+8)=definition_mock;
    *(uintptr_t*)definition_mock=battle_base+0x12a0298;memcpy(definition_mock+0x10,L"火矢",6);
    definition_mock[0x62]=17;
    *(void**)(definition_mock+0x70)=parameters_mock;*(void**)(definition_mock+0x78)=parameters_mock+10;
    *(void**)(mock_g+0x76c00+5*8)=definition_mock;
    original_action=skill_action;original_effect=skill_effect;original_fire=skill_fire;original_troops=skill_damage;original_category=skill_category;
    s14_battle_configure(S14_MASTER,0);SetLastError(1234);
    assert(hooked_battle_action(manager_mock,source_entry_mock,(void*)sentinel)==sentinel && GetLastError()==777 && queued()==0);
    word(hexes_mock+101*32+0x16,0);word(mock_armies+2*512+0x16,5000);word(mock_armies+2*512+0x18,0);
    s14_battle_configure(S14_MASTER,1);s14_battle_planning((uintptr_t)mock_g,100,1);s14_battle_progress();
    SetLastError(1234);assert(hooked_battle_action(manager_mock,source_entry_mock,(void*)sentinel)==sentinel && GetLastError()==777);
    BattleEvent *action=find_kind(B_ACTION,0),*effect=find_kind(B_EFFECT,0),*fire=find_kind(B_FIRE,0),*damage=find_kind(B_TROOPS,0);
    assert(action && effect && fire && damage && queued()==8);
    assert(action->source.id==1 && effect->source.id==1 && effect->target.id==1);
    assert(effect->action_id==action->id && effect->parent==action->id && effect->effect_id==0);
    assert(effect->tactic_id==5 && !wcscmp(effect->tactic_name,L"火矢") && effect->effect_count==2 && effect->effects[0]==17 && effect->effects[1]==0);
    assert(effect->parameter_count==10 && effect->parameters[0]==3 && effect->parameters[1]==40);
    assert(fire->action_id==action->id && fire->effect_id==effect->id && fire->parent==effect->id && !fire->scope_matches_target);
    assert(fire->effect_category==17 && fire->effect_slot==0 && fire->source_context_verified && fire->target_from_hex_occupant);
    assert(fire->source.id==1 && fire->target.id==2 && fire->fire_before==0 && fire->fire_after==120);
    assert(damage->effect_id==effect->id && damage->action_id==action->id && damage->source.kind==0);
    assert(damage->target.troops==5000 && damage->target_after.troops==4912 && damage->fire_before==120);
    assert(battle_word(damage->target.raw+0x16)==5000 && battle_word(damage->target_after.raw+0x16)==4912);
    assert(!battle_parent && !battle_action && !battle_effect);
    // One action may produce multiple target effects; they must not be counted as multiple casts.
    LONG64 start=battle_serial;action_targets=2;SetLastError(1234);
    assert(hooked_battle_action(manager_mock,source_entry_mock,(void*)sentinel)==sentinel && GetLastError()==777);
    BattleEvent *multi=find_kind(B_ACTION,start);assert(multi);int effects=0;
    for(int i=0;i<BATTLE_QSIZE;i++) if(battle_queue[i].state==2 && battle_queue[i].e.kind==B_EFFECT && battle_queue[i].e.action_id==multi->id) effects++;
    assert(effects==2);
    // Deferred effect has a native source but no invented action association.
    start=battle_serial;SetLastError(1234);
    assert(hooked_battle_effect(manager_mock,effect_entry_mock,(void*)0x123456789abcdef0ull)==sentinel && GetLastError()==777);
    effect=find_kind(B_EFFECT,start);assert(effect && effect->action_id==0 && effect->source.id==1);
    // A skill starting fire on another tile cannot be attributed to its listed target.
    start=battle_serial;fire_hex=100;SetLastError(1234);
    assert(hooked_battle_effect(manager_mock,effect_entry_mock,(void*)0x123456789abcdef0ull)==sentinel && GetLastError()==777);
    fire=find_kind(B_FIRE,start);assert(fire && fire->scope_matches_target && fire->target.id==1 && fire->hex_id==100 && fire->target_from_hex_occupant);
    // Failed/zero ignition is retained inside an effect, as before==after; no success invented.
    start=battle_serial;fire_hex=101;fire_request=0;word(hexes_mock+101*32+0x16,0);SetLastError(1234);
    assert(hooked_battle_effect(manager_mock,effect_entry_mock,(void*)0x123456789abcdef0ull)==sentinel && GetLastError()==777);
    fire=find_kind(B_FIRE,start);assert(fire && fire->fire_before==0 && fire->fire_after==0);
    // Invalid definition leaves the tactic unknown, while native execution still happens.
    start=battle_serial;*(void**)(effect_entry_mock+8)=(void*)1;SetLastError(1234);
    assert(hooked_battle_effect(manager_mock,effect_entry_mock,(void*)0x123456789abcdef0ull)==sentinel && GetLastError()==777);
    effect=find_kind(B_EFFECT,start);assert(effect && effect->tactic_id==-1 && !effect->parameter_count && effect->effect_count==2);
    // Countdown updates outside skill context must not flood the event queue.
    int before_count=queued();LONG64 before_serial=battle_serial;fire_request=119;
    for(int i=0;i<1000;i++) {
        word(hexes_mock+101*32+0x16,120);SetLastError(1234);
        assert(hooked_battle_fire(hexes_mock+101*32,119)==sentinel && GetLastError()==777);
    }
    assert(queued()==before_count && battle_serial==before_serial && !battle_fault);
    // An actual extinction outside a skill remains visible, with no invented source.
    start=battle_serial;fire_request=-1;SetLastError(1234);
    assert(hooked_battle_fire(hexes_mock+101*32,-1)==sentinel && GetLastError()==777);
    fire=find_kind(B_FIRE,start);assert(fire && fire->fire_before==119 && fire->fire_after==0 && !fire->source.kind && !fire->effect_id);
    wchar_t root[MAX_PATH],dir[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));CreateDirectoryW(root,NULL);
    swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);CreateDirectoryW(dir,NULL);swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\logs",root);CreateDirectoryW(dir,NULL);
    int total=queued();s14_battle_worker(root);assert(battle_events==total && !battle_fault && !battle_dropped);
    // The next planning phase has a bounded, worker-side snapshot of both active armies.
    s14_battle_planning((uintptr_t)mock_g,101,1);s14_battle_worker(root);
    total+=3;assert(battle_events==total && !battle_fault && !battle_snapshot_pending);
    s14_battle_planning((uintptr_t)mock_g,101,1);s14_battle_worker(root);assert(battle_events==total);
    FlushFileBuffers(battle_file);CloseHandle(battle_file);
    printf("{\"status\":\"passed\",\"written_events\":%d,\"skill_id_not_effect_category\":true,\"source_target_chain\":true,\"multiple_effects_one_action\":true,\"deferred_effect_no_invented_action\":true,\"actual_target_not_dispatch_subject\":true,\"zero_fire_not_success\":true,\"before_after_raw_immutable\":true,\"disabled_passthrough\":true,\"last_error_and_64bit_return_preserved\":true}\n",total);
    VirtualFree(mock_image,0,MEM_RELEASE);free(mock_g);free(mock_settings);free(mock_armies);free(mock_people);return 0;
}
