// Isolated native bridge checks. This executable never opens the real game.
#include <assert.h>
#ifdef NDEBUG
#error "Assertions are required"
#endif
#include "battle_probe.c"
static unsigned char *mock_image,*mock_g,*mock_settings,*mock_armies,*mock_people;
static int native_calls;
static const uintptr_t sentinel=0xfedcba9876543210ull;
static void word(unsigned char *p,int value) { unsigned short v=(unsigned short)value; memcpy(p,&v,2); }
static int queued(void) { int n=0; for(int i=0;i<BATTLE_QSIZE;i++) n+=battle_queue[i].state==2; return n; }
static BattleEvent *kind(int value) { for(int i=0;i<BATTLE_QSIZE;i++) if(battle_queue[i].state==2 && battle_queue[i].e.kind==value) return &battle_queue[i].e; return NULL; }
static void clear_queue(void) { memset(battle_queue,0,sizeof(battle_queue)); }
static uintptr_t mock_status(void *person,int status) {
    assert(person==mock_people && status==9); native_calls++; ((unsigned char*)person)[0x11e]=9; SetLastError(777); return sentinel;
}
static uintptr_t mock_injury(void *target,int mode,void *source,int option) {
    assert(target==mock_people && mode==3 && source==mock_people+0x220 && option==-2);
    assert(GetLastError()==1234); native_calls++; ((unsigned char*)target)[0x198]=2;
    uintptr_t args[10]={0x123456789abcdef0ull,1,2,3,4,5,6,7,8,0xfedcba9876543210ull};
    s14_battle_log(0x25628d,args,L"短消息",L"赵云受伤\n引用\"与\\路径");
    assert(hooked_battle_status(target,9)==sentinel);
    SetLastError(777); return sentinel;
}
static uintptr_t mock_troops(int type,int id,int amount,int source_type,int source_id) {
    assert(type==27 && id==2 && amount==100 && source_type==27 && source_id==1);
    assert(GetLastError()==1234); native_calls++;
    word(mock_armies+2*512+0x16,1400); word(mock_armies+2*512+0x18,40);
    word(mock_armies+512+0x16,2002); SetLastError(777); return sentinel;
}
static uintptr_t mock_remove(void *target,void *source,void *other,int reason,int option) {
    assert(target==mock_armies+2*512 && source==mock_armies+512 && other==mock_people);
    assert(reason==-1 && option==0x12345678 && GetLastError()==1234); native_calls++;
    ((unsigned char*)target)[0x10]=0; word((unsigned char*)target+0x12,0); SetLastError(777); return sentinel;
}
static uintptr_t mock_concurrent(int type,int id,int amount,int source_type,int source_id) {
    assert(type==27 && id==1 && amount==1 && source_type==27 && source_id==2); return sentinel;
}
static DWORD WINAPI parallel_calls(LPVOID arg) {
    (void)arg; for(int i=0;i<100;i++) { assert(hooked_battle_troops(27,1,1,27,2)==sentinel); assert(battle_parent==0); } return 0;
}
int main(int argc,char **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    assert(argc==2);
    mock_image=VirtualAlloc(NULL,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE); assert(mock_image);
    mock_g=calloc(1,0x85200); mock_settings=calloc(1,0x50); mock_armies=calloc(501,512); mock_people=calloc(2,0x220);
    assert(mock_g && mock_settings && mock_armies && mock_people);
    battle_base=(uintptr_t)mock_image; battle_end=battle_base+0x300000; battle_manager=&mock_g;
    *(void**)(mock_g+0x85130)=mock_settings;
    for(int i=0;i<501;i++) *(void**)(mock_g+0x7df60+i*8)=mock_armies+i*512;
    for(int i=0;i<2;i++) {
        unsigned char *p=mock_people+i*0x220; *(uintptr_t*)p=battle_base+0x12a00d0;
        word(p+0x10,10+10*i); p[0x118]=(unsigned char)(1+i); p[0x11e]=1;
        memcpy(p+0x12,i?L"曹":L"赵",4); memcpy(p+0x24,i?L"操":L"云",4);
        *(void**)(mock_g+0x148+(10+10*i)*8)=p;
        unsigned char *a=mock_armies+(i+1)*512; *(uintptr_t*)a=battle_base+0x123e288;
        a[0x10]=1; a[0x11]=(unsigned char)(i+1); word(a+0x12,i?10:20); word(a+0x16,i?1500:2000); word(a+0x2a,100+i);
    }
    // All signatures must validate before any hook is created.
    for(int i=0;i<BATTLE_BASE_ENTRY_COUNT;i++) memcpy(mock_image+battle_entries[i].rva,battle_entries[i].bytes,16);
    assert(MH_Initialize()==MH_OK);
    mock_image[battle_entries[2].rva]^=1;
    assert(!s14_battle_install(battle_base,battle_end,&mock_g) && !battle_ready);
    mock_image[battle_entries[2].rva]^=1;
    assert(s14_battle_install(battle_base,battle_end,&mock_g));
    assert(special_hooks==0); // Missing optional probes must leave base hooks working.
    for(int i=0;i<BATTLE_BASE_ENTRY_COUNT;i++) assert(MH_RemoveHook(mock_image+battle_entries[i].rva)==MH_OK);
    assert(MH_Uninitialize()==MH_OK);
    original_troops=mock_troops; original_remove=mock_remove; original_injury=mock_injury; original_status=mock_status;
    s14_battle_configure(S14_MASTER,0); SetLastError(1234);
    assert(hooked_battle_troops(27,2,100,27,1)==sentinel && GetLastError()==777 && !queued());
    s14_battle_configure(0,1); assert(!s14_battle_enabled());
    s14_battle_configure(S14_MASTER,1); assert(s14_battle_enabled());
    word(mock_armies+2*512+0x16,1500); word(mock_armies+2*512+0x18,0); word(mock_armies+512+0x16,2000);
    s14_battle_planning((uintptr_t)mock_settings,100,1); s14_battle_progress();
    SetLastError(1234); assert(hooked_battle_troops(27,2,100,27,1)==sentinel && GetLastError()==777);
    BattleEvent *damage=kind(B_TROOPS); assert(damage && damage->post_identity_matches && damage->planning_day==100);
    assert(damage->target.id==2 && damage->target.leader==10 && !wcscmp(damage->target.name,L"赵云"));
    assert(damage->source.id==1 && damage->source.leader==20 && !wcscmp(damage->source.name,L"曹操"));
    assert(damage->target.troops==1500 && damage->target_after.troops==1400 && damage->target_after.field18==40);
    assert(damage->source.troops==2000 && damage->source_after.troops==2002 && damage->args[4]==1);
    assert(!battle_parent);
    SetLastError(1234); assert(hooked_battle_injury(mock_people,3,mock_people+0x220,-2)==sentinel && GetLastError()==777);
    BattleEvent *injury=kind(B_INJURY),*status=kind(B_STATUS),*log=kind(B_LOG);
    assert(injury && status && log && status->parent==injury->id && log->parent==injury->id && !battle_parent);
    assert(injury->target.health==0 && injury->target_after.health==2 && injury->target_after.raw_status==9);
    assert(log->args[0]==0x123456789abcdef0ull && log->args[9]==sentinel && !log->text_truncated);
    int n=queued(); battle_parent=injury->id;
    assert(hooked_battle_status(mock_people,9)==sentinel && queued()==n); battle_parent=0;
    LONG64 serial_before=battle_serial;
    for(int i=0;i<6000;i++) {
        mock_people[0x11e]=1; SetLastError(1234);
        assert(hooked_battle_status(mock_people,9)==sentinel && GetLastError()==777);
        assert(mock_people[0x11e]==9 && !battle_parent);
    }
    assert(queued()==n && battle_serial==serial_before && !battle_fault);
    SetLastError(1234); assert(hooked_battle_remove(mock_armies+2*512,mock_armies+512,mock_people,-1,0x12345678)==sentinel && GetLastError()==777);
    BattleEvent *removal=kind(B_REMOVE); assert(removal && !removal->post_identity_matches && !removal->target_after.active);
    assert(removal->args[4]==0x12345678 && removal->other.id==10);
    BattleObject invalid=battle_object((uintptr_t)mock_g,(void*)1); assert(!invalid.kind && !invalid.active);
    wchar_t long_text[600]; for(int i=0;i<599;i++) long_text[i]=L'墙'; long_text[599]=0;
    int truncated=0; wchar_t text[512]={0}; battle_text(text,long_text,&truncated); assert(truncated && wcslen(text)==511);
    char json[BATTLE_JSON_SIZE]; assert(battle_json(log,json)>0 && strstr(json,"\\u000a") && strstr(json,"\\\"") && strstr(json,"\\\\"));
    // Exercise the actual worker output, not a mirrored serializer.
    wchar_t root[MAX_PATH],dir[MAX_PATH]; assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));
    assert(wcslen(root)<MAX_PATH-100); CreateDirectoryW(root,NULL);
    swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root); CreateDirectoryW(dir,NULL);
    swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\logs",root); CreateDirectoryW(dir,NULL);
    s14_battle_worker(root); assert(battle_file!=INVALID_HANDLE_VALUE && !queued() && battle_events==n+1);
    assert(!battle_fault && !battle_dropped); FlushFileBuffers(battle_file);
    // Discarded snapshot IDs must not cause false overflow in an empty queue.
    for(int i=0;i<64;i++) {
        InterlockedAdd64(&battle_serial,BATTLE_QSIZE-1);
        BattleEvent e=battle_begin(B_PROGRESS,0); battle_enqueue(&e);
    }
    assert(queued()==64 && !battle_fault && !battle_dropped);
    s14_battle_worker(root); assert(!queued() && battle_events==n+65);
    // Native telemetry has per-thread parent context and a bounded queue.
    original_troops=mock_concurrent; HANDLE threads[8]; ULONGLONG began=GetTickCount64();
    for(int i=0;i<8;i++) { threads[i]=CreateThread(NULL,0,parallel_calls,NULL,0,NULL); assert(threads[i]); }
    assert(WaitForMultipleObjects(8,threads,TRUE,10000)==WAIT_OBJECT_0);
    for(int i=0;i<8;i++) CloseHandle(threads[i]);
    assert(queued()==800 && !battle_fault && !battle_dropped);
    s14_battle_worker(root); assert(!queued() && battle_events==n+865);
    ULONGLONG batch=GetTickCount64()-began;
    clear_queue();
    for(int i=0;i<BATTLE_QSIZE+1;i++) { BattleEvent e=battle_begin(B_PROGRESS,0); battle_enqueue(&e); }
    assert(battle_dropped==1 && battle_fault==1 && !s14_battle_enabled());
    s14_battle_configure(S14_MASTER,1); assert(!s14_battle_enabled());
    clear_queue(); CloseHandle(battle_file); battle_file=INVALID_HANDLE_VALUE;
    printf("{\"status\":\"passed\",\"native_arg_return_preservation\":true,\"fifth_argument_preserved\":true,\"parent_context\":true,\"invalid_pointer_safe\":true,\"no_change_status_filtered\":true,\"noncombat_status_burst_bypassed\":6000,\"sparse_ids_no_false_overflow\":true,\"queue_overflow_stops_capture\":true,\"native_calls\":%d,\"concurrent_calls\":800,\"concurrent_batch_ms\":%llu,\"written_events\":%lld}\n",native_calls,(unsigned long long)batch,(long long)battle_events);
    VirtualFree(mock_image,0,MEM_RELEASE); free(mock_g); free(mock_settings); free(mock_armies); free(mock_people); return 0;
}
