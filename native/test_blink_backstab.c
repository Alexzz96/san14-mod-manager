#define S14_BLINK_TEST
#include "blink_backstab.c"
#include <assert.h>
static unsigned char *fixture_image,*fixture_world,*fixture_pool,*fixture_hex,*fixture_progress,*fixture_settings;
static unsigned char persons[3][512],groups[2][64];
static int moves,paths,block_tile=-1,move_fail,post_move_bad,path_bad,during_batch_move_target;
static void putptr(void *p,uintptr_t n){memcpy(p,&n,8);}
static void putword(void *p,int n){unsigned short v=(unsigned short)n;memcpy(p,&v,2);}
static unsigned char *army(int id){return fixture_pool+id*512;}
static unsigned char *hex(int tile){return fixture_hex+tile*32;}
static int neighbor(int tile,int d){
    int x=tile%220,y=tile/220;if(tile<0 || tile>=48400 || d<1 || d>6)return 48400;
    switch(d){case 1:y-=x&1;x--;break;case 2:y--;break;case 3:y-=x&1;x++;break;case 4:y+=!(x&1);x--;break;case 5:y++;break;case 6:y+=!(x&1);x++;break;}
    return x>=0 && x<220 && y>=0 && y<220?y*220+x:48400;
}
static int direction(int from,int to){for(int d=1;d<=6;d++)if(neighbor(from,d)==to)return d;return 0;}
static int enemy(int a,int b){return a!=b;}
static int formation(void *p){return ((unsigned char*)p)[0x1b];}
static int pass(int t,int mode,void *p,int option){assert(mode==1 && p==army(1) && option==0);return t!=block_tile;}
static void dest(void *p,int t){putword((unsigned char*)p+0x48,t);}
static void clear(int t){putword(hex(t)+0x1c,0);}
static void occupy(void *p,int id){putword((unsigned char*)p+0x1c,id);}
static void move(void *p,int cost){
    assert(p==army(1) && !cost);moves++;if(move_fail)return;int old=blink_word((uintptr_t)p+0x2a),t=blink_word((uintptr_t)p+0x48);
    blink_direction_test_caller=blink_base+0x2a98f9;int d=blink_direction_hook(old,t);assert(d>=1 && d<=6);
    ((unsigned char*)p)[0x36]=(unsigned char)d;putword((unsigned char*)p+0x2a,t);if(!post_move_bad)occupy(hex(t),1);
}
static void path(void *p,int a,int b,int c){assert(p==army(1) && !a && b==1 && !c);paths++;if(path_bad)putword(hex(blink_word((uintptr_t)p+0x2a))+0x1c,0);}
static void batch(void){if(during_batch_move_target){int old=blink_word((uintptr_t)army(2)+0x2a),t=neighbor(old,3);clear(old);putword(army(2)+0x2a,t);occupy(hex(t),2);}SetLastError(888);}
static void place(int id,int tile){int old=blink_word((uintptr_t)army(id)+0x2a);if(old<48400 && blink_word((uintptr_t)hex(old)+0x1c)==id)clear(old);putword(army(id)+0x2a,tile);occupy(hex(tile),id);}
static void fixture(int d){
    if(!fixture_image){fixture_image=VirtualAlloc(NULL,0x24a0000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);fixture_world=calloc(1,0x90000);fixture_pool=calloc(501,512);fixture_hex=calloc(48400,32);fixture_progress=calloc(1,0x500);fixture_settings=calloc(1,0x100);assert(fixture_image && fixture_world && fixture_pool && fixture_hex && fixture_progress && fixture_settings);}
    blink_base=(uintptr_t)fixture_image;blink_end=blink_base+0x24a0000;memset(fixture_world,0,0x90000);memset(fixture_pool,0,501*512);memset(fixture_hex,0,48400*32);memset(persons,0,sizeof(persons));memset(groups,0,sizeof(groups));
    memset(&blink_state,0,sizeof(blink_state));memset(blink_pending,0,sizeof(blink_pending));memset(blink_reserved,0,sizeof(blink_reserved));blink_ready=blink_enabled=1;blink_loading=blink_fault=0;blink_world=(uintptr_t)fixture_world;blink_leader_scanned=0;blink_epoch++;blink_direction_scope.active=0;
    moves=paths=move_fail=post_move_bad=path_bad=during_batch_move_target=0;block_tile=-1;
    putptr(fixture_image+0x1fc91d0,(uintptr_t)fixture_world);putptr(fixture_world+0x7df60,(uintptr_t)fixture_pool);
    putptr(fixture_world+0x85130,(uintptr_t)fixture_settings);putword(fixture_settings+0x34,200);fixture_settings[0x36]=1;fixture_settings[0x37]=1;
    putptr(fixture_progress,blink_base+0x12cc760);putptr(fixture_image+0x2486000,(uintptr_t)fixture_progress);putptr(fixture_image+0x2486008,(uintptr_t)fixture_world);*(unsigned*)(fixture_image+0x2486014)=9;
    for(int t=0;t<48400;t++){putptr(hex(t),blink_base+0x129f660);putptr(fixture_world+0xdfe0+t*8,(uintptr_t)hex(t));}
    for(int i=0;i<2;i++){putptr(groups[i],blink_base+0x129fec8);groups[i][0x10]=(unsigned char)(i+1);putptr(fixture_world+0xde40+(i+1)*8,(uintptr_t)groups[i]);}
    /* The 黄忠 ID was observed read-only; all objects here are synthetic. */
    for(int i=0;i<3;i++){putptr(persons[i],blink_base+0x12a00d0);putword(persons[i]+0x10,S14_BLINK_OFFICER+i);persons[i][0x118]=(unsigned char)(i==0?1:2);putptr(fixture_world+0x148+(S14_BLINK_OFFICER+i)*8,(uintptr_t)persons[i]);}
    memcpy(persons[0]+0x12,L"黄",4);memcpy(persons[0]+0x24,L"忠",4);memcpy(persons[1]+0x12,L"吕",4);memcpy(persons[1]+0x24,L"布",4);
    for(int i=1;i<=2;i++){putptr(army(i),blink_base+0x123e288);army(i)[0x10]=1;putword(army(i)+0x12,S14_BLINK_OFFICER-1+i);putword(army(i)+0x16,1000);army(i)[0x1a]=100;army(i)[0x1b]=4;putword(army(i)+0x28,1);putword(army(i)+0x48,48400);putptr(fixture_world+0x7df60+i*8,(uintptr_t)army(i));}
    place(1,100*220+100);place(2,neighbor(100*220+100,d));
    blink_original_direction=direction;blink_neighbor=neighbor;blink_enemy=enemy;blink_formation=formation;blink_pass=pass;blink_destination=dest;blink_clear=clear;blink_move=move;blink_path=path;blink_occupy=occupy;blink_original_batch=batch;
}
static S14BlinkHit hit(int source,int target,int amount,uintptr_t caller,int *adjusted){S14BlinkHit h;*adjusted=s14_blink_hit_begin(27,target,amount,27,source,caller,&h);return h;}
static void real_hit(int source,int target,int amount){int changed;S14BlinkHit h=hit(source,target,amount,blink_base+0x2578d76,&changed);int before=blink_word((uintptr_t)army(target)+0x16);putword(army(target)+0x16,before>changed?before-changed:0);s14_blink_hit_end(&h);}
static void land(void){SetLastError(777);blink_batch_hook();assert(GetLastError()==888);}
static void saved_path(const unsigned char hash[32],wchar_t out[MAX_PATH]){assert(blink_file(hash,out,0));}
int wmain(int argc,wchar_t **argv){
    unsigned char hash[32]={7};wchar_t temp[MAX_PATH],root[MAX_PATH],file[MAX_PATH];
    if(argc==3 && !wcscmp(argv[1],L"--restore")){fixture(2);wcscpy(blink_root,argv[2]);int target=blink_word((uintptr_t)army(2)+0x2a);place(1,neighbor(target,2));s14_blink_load_begin();s14_blink_load_end((uintptr_t)fixture_world,1,hash,1);assert(!blink_fault && blink_state.units[1].opportunity && blink_state.units[1].cooldown==72005);int adjusted;S14BlinkHit h=hit(1,2,100,blink_base+0x2578d76,&adjusted);assert(h.boosted && adjusted==130);return 0;}
    assert(GetTempPathW(MAX_PATH,temp));swprintf(root,MAX_PATH,L"%lsS14-blink-test-%lu",temp,GetCurrentProcessId());assert(CreateDirectoryW(root,NULL));wcscpy(blink_root,root);
    for(int d=1;d<=6;d++){fixture(d);int old=blink_word((uintptr_t)army(1)+0x2a),t=blink_word((uintptr_t)army(2)+0x2a),b1=neighbor(t,d);real_hit(1,2,100);assert(blink_pending[1].active && !moves);land();assert(moves==1 && paths==1 && blink_word((uintptr_t)army(1)+0x2a)==b1 && !blink_word((uintptr_t)hex(old)+0x1c) && blink_word((uintptr_t)hex(b1)+0x1c)==1 && blink_word((uintptr_t)army(2)+0x2a)==t && blink_state.units[1].opportunity);int adjusted;S14BlinkHit h=hit(1,2,100,blink_base+0x2578d76,&adjusted);assert(adjusted==130 && h.boosted);putword(army(2)+0x16,770);s14_blink_hit_end(&h);assert(!blink_state.units[1].opportunity);land();assert(moves==1);}
    fixture(2);int t=blink_word((uintptr_t)army(2)+0x2a),b1=neighbor(t,2),b2=neighbor(b1,2),b3=neighbor(b2,2);
    occupy(hex(b1),3);block_tile=b2;real_hit(1,2,100);land();assert(blink_word((uintptr_t)army(1)+0x2a)==b3 && blink_word((uintptr_t)hex(b1)+0x1c)==3 && blink_word((uintptr_t)army(2)+0x2a)==t);
    fixture(2);t=blink_word((uintptr_t)army(2)+0x2a);for(int i=0;i<3;i++){t=neighbor(t,2);occupy(hex(t),3);}real_hit(1,2,100);land();assert(!moves && !blink_state.units[1].opportunity && !blink_state.units[1].cooldown);
    fixture(2);place(1,220);place(2,0);real_hit(1,2,100);land();assert(!moves);
    fixture(2);int adjusted;S14BlinkHit h=hit(2,1,100,blink_base+0x2578d76,&adjusted);assert(!h.active && adjusted==100);
    memcpy(persons[2]+0x12,L"黄",4);memcpy(persons[2]+0x24,L"忠",4);blink_leader_scanned=0;h=hit(1,2,100,blink_base+0x2578d76,&adjusted);assert(!h.active);
    fixture(2);h=hit(1,2,100,blink_base+0x2578feb,&adjusted);assert(!h.active && adjusted==100);h=hit(1,2,100,blink_base+0x166861,&adjusted);assert(!h.active);
    h=hit(1,2,100,blink_base+0x2579004,&adjusted);assert(h.active);s14_blink_hit_end(&h);land();assert(!moves); // No actual loss.
    real_hit(1,2,100);real_hit(1,2,100);land();assert(moves==1);land();assert(moves==1);
    fixture(2);real_hit(1,2,100);during_batch_move_target=1;land();assert(!moves);
    fixture(2);real_hit(1,2,100);(*(unsigned*)(fixture_image+0x2486014))++;land();assert(!moves);
    fixture(2);real_hit(1,2,100);putword(army(1)+0x28,2);land();assert(!moves);
    fixture(2);real_hit(1,2,100);s14_blink_configure(0);land();assert(!moves && !blink_pending[1].active);s14_blink_configure(1);
    fixture(2);real_hit(1,2,100);move_fail=1;land();assert(blink_fault && !blink_state.units[1].opportunity && blink_word((uintptr_t)hex(blink_word((uintptr_t)army(1)+0x2a))+0x1c)==1 && blink_word((uintptr_t)army(1)+0x48)==48400);
    fixture(2);real_hit(1,2,100);post_move_bad=1;land();assert(blink_fault && !blink_state.units[1].opportunity);land();assert(moves==1);
    fixture(2);real_hit(1,2,100);path_bad=1;land();assert(blink_fault && !blink_state.units[1].opportunity);
    fixture(2);real_hit(1,2,100);land();blink_direction_scope.active=1;blink_direction_scope.from=10;blink_direction_scope.to=20;blink_direction_scope.direction=3;blink_direction_test_caller=blink_base+1;assert(blink_direction_hook(10,20)==0);blink_direction_test_caller=blink_base+0x2a98f9;assert(blink_direction_hook(10,20)==3 && blink_direction_hook(11,20)==0);blink_direction_scope.active=0;
    h=hit(1,2,INT_MAX,blink_base+0x2578d76,&adjusted);assert(adjusted==INT_MAX && !h.boosted);s14_blink_hit_end(&h);
    h=hit(1,2,100,blink_base+0x2578d76,&adjusted);assert(h.boosted);S14BlinkHit nested=hit(1,2,100,blink_base+0x2578d76,&adjusted);assert(!nested.boosted);s14_blink_hit_end(&nested);s14_blink_hit_end(&h);assert(blink_state.units[1].opportunity);
    assert(s14_blink_save((uintptr_t)fixture_world,hash));saved_path(hash,file);assert(s14_blink_checkpoint_owned(file));
    wchar_t executable[MAX_PATH],command[1024];assert(GetModuleFileNameW(NULL,executable,MAX_PATH));swprintf(command,1024,L"\"%ls\" --restore \"%ls\"",executable,root);STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi={0};assert(CreateProcessW(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi));assert(WaitForSingleObject(pi.hProcess,15000)==WAIT_OBJECT_0);DWORD code;assert(GetExitCodeProcess(pi.hProcess,&code) && code==0);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    s14_blink_configure(0);assert(!blink_state.units[1].opportunity && blink_state.units[1].cooldown==72005);s14_blink_configure(1);s14_blink_load_begin();s14_blink_load_end((uintptr_t)fixture_world,1,hash,1);assert(blink_state.units[1].opportunity);s14_blink_load_begin();s14_blink_load_end((uintptr_t)fixture_world,0,NULL,0);assert(!blink_fault && blink_state.units[1].opportunity);
    fixture_settings[0x37]=6;land();assert(!blink_state.units[1].opportunity);fixture_settings[0x37]=1;s14_blink_load_begin();s14_blink_load_end((uintptr_t)fixture_world,1,hash,1);assert(blink_state.units[1].opportunity);s14_blink_created(army(1));assert(!blink_state.units[1].source.id);
    s14_blink_load_begin();s14_blink_load_end((uintptr_t)fixture_world,1,hash,1);army(2)[0x10]=0;s14_blink_forget(army(2));assert(!blink_state.units[1].opportunity);army(2)[0x10]=1;
    HANDLE f=CreateFileW(file,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);assert(f!=INVALID_HANDLE_VALUE);DWORD written;assert(WriteFile(f,"corrupt",7,&written,NULL));CloseHandle(f);assert(!s14_blink_checkpoint_owned(file));s14_blink_load_begin();s14_blink_load_end((uintptr_t)fixture_world,1,hash,1);assert(blink_fault);assert(!s14_blink_save((uintptr_t)fixture_world,hash));assert(DeleteFileW(file));
    unsigned char legacy[32]={8};s14_blink_load_begin();s14_blink_load_end((uintptr_t)fixture_world,1,legacy,1);assert(!blink_fault && !blink_state.units[1].source.id);s14_blink_new_game((uintptr_t)fixture_world);assert(!blink_state.units[1].source.id);
    unsigned char bytes[]={1,2,3};assert(blink_fnv(bytes,3)!=blink_fnv(bytes,2));assert(!s14_blink_install(0,0,root));
    wchar_t folder[MAX_PATH];swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager\\blink-backstab",root);assert(RemoveDirectoryW(folder));swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager",root);assert(RemoveDirectoryW(folder));assert(RemoveDirectoryW(root));
    printf("{\"status\":\"passed\",\"huang_zhong_only\":true,\"unique_original_name\":true,\"six_directions\":true,\"rear_search_three\":true,\"occupied_and_illegal_landing\":true,\"boundary\":true,\"no_damage_no_flash\":true,\"dedup\":true,\"stale_and_generation\":true,\"partial_move_fault\":true,\"scoped_multi_hex_direction\":true,\"direct_hit_backstab_30\":true,\"nested_and_overflow\":true,\"disable_preserves_cooldown\":true,\"save_load_rollback\":true,\"separate_process_restore\":true,\"corrupt_preserved\":true,\"slot_reuse\":true,\"in_game_acceptance\":\"pending\",\"game_process_touched\":false}\n");return 0;
}
