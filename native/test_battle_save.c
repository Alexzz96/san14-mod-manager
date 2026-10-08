#define main legacy_battle_fixture
#include "test_battle_probe.c"
#undef main
static unsigned char archive_data[0x70];
static wchar_t fixture_root[MAX_PATH];
static int fail_save,fail_load,native_loads,native_saves;
static void archive_name(const char *name) {memset(archive_data,0,sizeof(archive_data));size_t n=strlen(name);assert(n<16);memcpy(archive_data+0x50,name,n+1);*(uintptr_t*)(archive_data+0x60)=n;*(uintptr_t*)(archive_data+0x68)=15;}
static uintptr_t save_commit(void *world,void *data) {
    assert(world==mock_g && data==archive_data && GetLastError()==1234);native_saves++;
    // Commit frees its archive in the game; name must have been copied already.
    memset(archive_data+0x50,0,32);SetLastError(777);return fail_save?sentinel:0;
}
static uintptr_t load_reader(void *data,int slot,void *name) {
    assert(data==archive_data && slot==7 && name==(void*)sentinel && GetLastError()==1234);archive_name("svdexSC00.s14");SetLastError(777);return 0;
}
static uintptr_t load_game(void *world,int slot,void *name) {
    assert(world==mock_g && slot==7 && name==(void*)sentinel && GetLastError()==1234 && !s14_battle_enabled());native_loads++;
    SetLastError(1234);assert(hooked_battle_load_reader(archive_data,slot,name)==0 && GetLastError()==777);
    mock_settings[0x36]=6;mock_settings[0x37]=1;SetLastError(777);return fail_load?0:1;
}
static uintptr_t new_game(void *world,int scenario) {assert(world==mock_g && scenario==8 && GetLastError()==1234 && !s14_battle_enabled());SetLastError(777);return 1;}
static BattleEvent *latest(int k) {BattleEvent *r=NULL;for(int i=0;i<BATTLE_QSIZE;i++) if(battle_queue[i].state==2 && battle_queue[i].e.kind==k && (!r || r->id<battle_queue[i].e.id)) r=&battle_queue[i].e;return r;}
int main(int argc,char **argv) {
    assert(argc==2);assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,fixture_root,MAX_PATH));wchar_t path[MAX_PATH];
    const wchar_t *dirs[]={L"SAN14ModManager",L"SAN14ModManager\\logs",L"Profile",L"Profile\\TEST",L"Profile\\TEST\\Saves"};
    for(int i=0;i<5;i++) {swprintf(path,MAX_PATH,L"%ls\\%ls",fixture_root,dirs[i]);assert(CreateDirectoryW(path,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);}
    swprintf(path,MAX_PATH,L"%ls\\Profile\\TEST\\Saves\\svdexSC00.s14",fixture_root);HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);assert(f!=INVALID_HANDLE_VALUE);char raw[2048]={1};DWORD n;assert(WriteFile(f,raw,sizeof(raw),&n,NULL) && n==sizeof(raw));CloseHandle(f);
    mock_g=calloc(1,0x85200);mock_settings=calloc(1,0x50);assert(mock_g && mock_settings);*(void**)(mock_g+0x85130)=mock_settings;
    word(mock_settings+0x34,263);mock_settings[0x36]=5;mock_settings[0x37]=21;battle_manager=&mock_g;battle_ready=1;wcscpy(battle_save_root,fixture_root);
    original_save_commit=save_commit;original_load=load_game;original_load_reader=load_reader;original_new_game=new_game;s14_battle_configure(S14_MASTER,1);
    archive_name("svdexSC00.s14");SetLastError(1234);assert(hooked_battle_save_commit(mock_g,archive_data)==0 && GetLastError()==777);
    BattleEvent *e=latest(B_SAVE);assert(e && e->save_hash_valid && e->planning_day==94820 && !wcscmp(e->text,L"svdexSC00.s14"));
    unsigned char hash[32];memcpy(hash,e->save_hash,32);int count=queued();archive_name("configS_SC.s14");SetLastError(1234);assert(hooked_battle_save_commit(mock_g,archive_data)==0 && GetLastError()==777 && queued()==count);
    fail_save=1;archive_name("svdexSC00.s14");SetLastError(1234);assert(hooked_battle_save_commit(mock_g,archive_data)==sentinel && GetLastError()==777 && !latest(B_SAVE)->save_hash_valid);
    SetLastError(1234);assert(hooked_battle_load(mock_g,7,(void*)sentinel)==1 && GetLastError()==777 && s14_battle_enabled());e=latest(B_LOAD_END);
    assert(e && e->save_hash_valid && !memcmp(e->save_hash,hash,32) && e->planning_day==94830 && e->parent==latest(B_LOAD_BEGIN)->id);
    fail_load=1;SetLastError(1234);assert(hooked_battle_load(mock_g,7,(void*)sentinel)==0 && GetLastError()==777 && !latest(B_LOAD_END)->save_hash_valid && !battle_load_context && !battle_loading);
    SetLastError(1234);assert(hooked_battle_new_game(mock_g,8)==1 && GetLastError()==777 && latest(B_NEW_GAME));
    s14_battle_worker(fixture_root);assert(!battle_fault && !battle_dropped);FlushFileBuffers(battle_file);CloseHandle(battle_file);
    printf("{\"status\":\"passed\",\"written_events\":%lld,\"save_name_before_free\":true,\"successful_load_only\":true,\"failed_save_no_checkpoint\":true,\"load_mutes_combat\":true,\"file_digest_binding\":true,\"noncampaign_files_excluded\":true,\"last_error_preserved\":true}\n",(long long)battle_events);free(mock_g);free(mock_settings);return 0;
}
