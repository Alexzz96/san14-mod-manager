// Native pass-through and raw observations, with private mock objects only.
#define main legacy_battle_fixture
#include "test_battle_probe.c"
#undef main
static int succeed=1;
static uintptr_t status_set(void *p,int s){assert(p==mock_people+0x220 && s==6);((unsigned char*)p)[0x11e]=6;SetLastError(777);return sentinel;}
static uintptr_t duel_set(void *a,void *b,int mode,int outcome){
    assert(a==mock_people && b==mock_people+0x220 && mode==-3 && outcome==1 && GetLastError()==1234);
    assert(battle_parent);native_calls++;SetLastError(777);return sentinel;
}
static uintptr_t capture_set(void *p,void *army,int flag,int option,void *force){
    assert(p==mock_people+0x220 && army==mock_armies+512 && flag==-2 && option==17 && force==(void*)sentinel && GetLastError()==1234);
    assert(battle_parent);native_calls++;if(succeed)assert(hooked_battle_status(p,6)==sentinel);SetLastError(777);return succeed?1:0;
}
int main(int argc,char **argv){
    assert(argc==2);wchar_t root[MAX_PATH],path[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));
    swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager",root);assert(CreateDirectoryW(path,NULL));swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager\\logs",root);assert(CreateDirectoryW(path,NULL));
    mock_image=VirtualAlloc(NULL,0x340000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(mock_image);
    mock_g=calloc(1,0x85200);mock_settings=calloc(1,0x50);mock_people=calloc(2,0x220);mock_armies=calloc(501,512);assert(mock_g&&mock_settings&&mock_people&&mock_armies);
    *(void**)(mock_g+0x85130)=mock_settings;
    for(int i=0;i<2;i++){
        unsigned char *p=mock_people+i*0x220;*(uintptr_t*)p=(uintptr_t)mock_image+0x12a00d0;word(p+0x10,10+10*i);p[0x118]=(unsigned char)(1+i);p[0x11e]=4;
        memcpy(p+0x12,i?L"张":L"曹",4);memcpy(p+0x24,i?L"嶷":L"仁",4);*(void**)(mock_g+0x148+(10+10*i)*8)=p;
    }
    unsigned char *army=mock_armies+512;*(uintptr_t*)army=(uintptr_t)mock_image+0x123e288;army[0x10]=1;army[0x11]=1;word(army+0x12,10);
    for(int i=0;i<501;i++)*(void**)(mock_g+0x7df60+i*8)=mock_armies+i*512;
    for(int i=0;i<BATTLE_ENTRY_COUNT;i++)memcpy(mock_image+battle_entries[i].rva,battle_entries[i].bytes,16);
    assert(MH_Initialize()==MH_OK);assert(s14_battle_install((uintptr_t)mock_image,(uintptr_t)mock_image+0x340000,&mock_g));assert(special_hooks==3);
    assert(!duel_semantics_ready && !capture_semantics_ready); // Prologues alone cannot establish attribution.
    for(int i=0;i<BATTLE_ENTRY_COUNT;i++)assert(MH_RemoveHook(mock_image+battle_entries[i].rva)==MH_OK);
    for(int i=0;i<3;i++)memcpy(mock_image+duel_anchors[i].rva,duel_anchors[i].bytes,duel_anchors[i].size);
    for(int i=0;i<3;i++)memcpy(mock_image+capture_anchors[i].rva,capture_anchors[i].bytes,capture_anchors[i].size);
    assert(s14_battle_install((uintptr_t)mock_image,(uintptr_t)mock_image+0x340000,&mock_g) && duel_semantics_ready && capture_semantics_ready);
    for(int i=0;i<BATTLE_ENTRY_COUNT;i++)assert(MH_RemoveHook(mock_image+battle_entries[i].rva)==MH_OK);assert(MH_Uninitialize()==MH_OK);
    original_duel=duel_set;original_capture=capture_set;original_status=status_set;s14_battle_configure(S14_MASTER,1);
    s14_battle_planning((uintptr_t)mock_settings,100,1);s14_battle_progress();
    SetLastError(1234);assert(hooked_duel_settlement(mock_people,mock_people+0x220,-3,1)==sentinel && GetLastError()==777 && !battle_parent);
    BattleEvent *e=kind(B_DUEL);assert(e && e->source.id==10 && e->target.id==20 && e->post_identity_matches && (int)e->args[2]==-3 && e->args[3]==1);
    SetLastError(1234);assert(hooked_capture_settlement(mock_people+0x220,army,-2,17,(void*)sentinel)==1 && GetLastError()==777 && !battle_parent);
    e=kind(B_CAPTURE);assert(e && e->target.raw_status==4 && e->target_after.raw_status==6 && e->source.leader==10 && kind(B_STATUS)->parent==e->id);
    mock_people[0x220+0x11e]=4;succeed=0;SetLastError(1234);assert(hooked_capture_settlement(mock_people+0x220,army,-2,17,(void*)sentinel)==0 && GetLastError()==777);
    s14_battle_worker(root);assert(!battle_fault && !battle_dropped && !queued());
    S14SpecialSnapshot *s=malloc(sizeof(*s));assert(s && s14_special_snapshot((uintptr_t)mock_g,s));
    assert(s->count==2 && s->events[0].kind==S14_SPECIAL_DUEL && s->events[1].kind==S14_SPECIAL_CAPTURE);
    assert(!s->events[0].verified && !s->events[1].verified && s->events[1].actor_role==S14_SPECIAL_ACTOR_COMMANDER && !s->rows[10].valid_mask && !s->rows[20].valid_mask);
    BattleEvent matched={.kind=B_DUEL,.id=100,.world=(uintptr_t)mock_g,.planning_day=100,.caller=0x331f03,.post_identity_matches=1};
    matched.source=battle_object(matched.world,mock_people);matched.target=battle_object(matched.world,mock_people+0x220);
    matched.source.force_id=1;matched.target.force_id=2;matched.args[2]=0;matched.args[3]=1;
    assert(ordinary_duel_verified(&matched));battle_report_event(&matched);assert(s14_special_snapshot(matched.world,s));
    char verified_json[BATTLE_JSON_SIZE];assert(battle_json(&matched,verified_json)>0 && strstr(verified_json,"\"semantics_verified\":true") && strstr(verified_json,"\"ordinary_duel_normalized_outcome\":0"));
    assert(s->rows[10].wins==1 && s->rows[20].losses==1 && s->events[2].verified && s->events[2].outcome==0);
    BattleObject old_source=matched.source;matched.source=matched.target;matched.target=old_source;
    matched.id=101;matched.caller=0x331f3e;assert(ordinary_duel_verified(&matched));battle_report_event(&matched);assert(s14_special_snapshot(matched.world,s));
    assert(s->rows[20].wins==1 && s->rows[10].losses==1 && s->rows[10].duels==2);
    matched.id=103;matched.args[3]=0;assert(ordinary_duel_verified(&matched));battle_report_event(&matched);assert(s14_special_snapshot(matched.world,s));assert(s->rows[20].wins==2 && s->rows[10].losses==2 && s->events[4].outcome==0);
    matched.id=102;matched.args[3]=2;assert(!ordinary_duel_verified(&matched));battle_report_event(&matched);assert(s14_special_snapshot(matched.world,s));assert(s->rows[10].duels==3 && !s->events[5].verified);
    matched.args[3]=1;matched.args[2]=1;assert(!ordinary_duel_verified(&matched));matched.args[2]=0;matched.caller++;assert(!ordinary_duel_verified(&matched));matched.caller--;
    matched.target.force_id=matched.source.force_id;assert(!ordinary_duel_verified(&matched));matched.target.force_id=1;duel_semantics_ready=0;assert(!ordinary_duel_verified(&matched));duel_semantics_ready=1;
    unsigned char hash[32]={9};assert(s14_special_save((uintptr_t)mock_g,hash,1));
    BattleEvent cap={.id=200,.kind=B_CAPTURE,.world=(uintptr_t)mock_g,.planning_day=100,.caller=0x236391,.native_return=1,.post_identity_matches=1};
    cap.args[2]=cap.args[3]=1;cap.source=battle_object(cap.world,army);cap.target=battle_object(cap.world,mock_people+0x220);cap.source.force_id=1;cap.target.force_id=2;cap.source_after=cap.source;cap.target_after=cap.target;cap.target_after.raw_status=6;
    battle_capture=&cap;battle_parent=cap.id;uintptr_t log_args[10]={0};SetLastError(1234);
    s14_battle_log(0x2aebe8,log_args,NULL,L"我军的^02张嶷^00大人\n被曹操军的^02曹仁^00俘虏了！");assert(GetLastError()==1234 && cap.capture_report_matches);
    cap.capture_report_matches=0;s14_battle_log(0x2aebe8,log_args,NULL,L"曹仁只是报告人，未知武将俘虏了张嶷");assert(!cap.capture_report_matches);
    s14_battle_log(0x2aebe8,log_args,NULL,L"张嶷被曹仁俘虏了！");assert(cap.capture_report_matches && !cap.capture_status_matches);
    assert(hooked_battle_status(mock_people+0x220,6)==sentinel && GetLastError()==777 && cap.capture_status_matches);battle_capture=NULL;battle_parent=0;
    assert(capture_verified(&cap));battle_report_event(&cap);assert(s14_special_snapshot(cap.world,s) && s->rows[10].captures==1 && s->rows[20].captured==1);
    battle_report_event(&cap);assert(s14_special_snapshot(cap.world,s) && s->rows[10].captures==1);
    cap.capture_report_matches=0;assert(!capture_verified(&cap));cap.capture_report_matches=1;cap.capture_status_matches=0;assert(!capture_verified(&cap));cap.capture_status_matches=1;cap.caller++;assert(!capture_verified(&cap));cap.caller--;cap.args[3]=0;assert(!capture_verified(&cap));cap.args[3]=1;capture_semantics_ready=0;assert(!capture_verified(&cap));capture_semantics_ready=1;
    s14_battle_worker(root);assert(!battle_fault && !battle_dropped && !queued()); // Flush mock proof children as raw observations only.
    assert(s14_special_save((uintptr_t)mock_g,hash,1));
    FlushFileBuffers(battle_file);CloseHandle(battle_file);
    printf("{\"status\":\"passed\",\"written_events\":%lld,\"optional_hooks\":3,\"args_and_return_preserved\":true,\"last_error_preserved\":true,\"nested_capture_status\":true,\"failed_capture_excluded\":true,\"candidate_not_credited\":true,\"ordinary_duel_winner_argument_first\":true,\"ordinary_duel_swapped_participants_and_report_context\":true,\"duel_semantics_anchor_guard\":true,\"unknown_result_not_credited\":true,\"capture_native_report_and_status_matched\":true,\"capture_attribution_guard\":true,\"capture_actor_and_victim_counters\":true}\n",(long long)battle_events);
    free(s);free(mock_g);free(mock_settings);free(mock_people);free(mock_armies);VirtualFree(mock_image,0,MEM_RELEASE);return 0;
}
