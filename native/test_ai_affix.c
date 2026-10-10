#include "ai_affix.c"
#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
static void p64(unsigned char *b,int at,uintptr_t v){memcpy(b+at,&v,8);}
static void p16(unsigned char *b,int at,unsigned short v){memcpy(b+at,&v,2);}
static S14AiDraw next_city(S14AiState *s,int city){S14AiDraw d={.eligible=1};s14_ai_draw(s,city,&d);return d;}
static S14AiDraw next(S14AiState *s){return next_city(s,-1);}
int main(int argc,char **argv){
    assert(s14_ai_roll_hit(0));assert(s14_ai_roll_hit(UINT64_MAX/10));assert(!s14_ai_roll_hit(UINT64_MAX/10+1));assert(!s14_ai_roll_hit(UINT64_MAX));
    uint64_t rng=12345;int hits=0;for(int i=0;i<1000000;i++)hits+=s14_ai_roll_hit(s14_ai_random(&rng));assert(hits>99000 && hits<101000);
    S14AiState a,b;s14_ai_state_seed(&a,123);s14_ai_state_seed(&b,123);
    for(int i=1;i<=500;i++){S14AiDraw x=next(&a),y=next(&b);assert(x.hit==y.hit && x.next==y.next);assert(s14_ai_state_commit(&a,i,i,i,0,1,2,1,x.hit,&x)==0);assert(a.rolls==(unsigned)i-1);assert(s14_ai_state_commit(&a,i,i,i,1,1,2,1,x.hit,&x)==2);assert(s14_ai_state_commit(&b,i,i,i,1,1,2,1,y.hit,&y)==2);assert(!s14_ai_state_commit(&a,i,i,i,1,1,2,1,x.hit,&x));}
    assert(!memcmp(&a,&b,sizeof(a)));assert(s14_ai_percent_max(10,10)==10 && s14_ai_percent_max(0,10)==10);
    S14AiDraw x=next(&a);assert(s14_ai_state_commit(&a,1,1,502,1,0,2,1,x.hit,&x)==1 && a.rolls==500 && !a.units[1].hit);
    x=next(&a);assert(s14_ai_state_commit(&a,2,2,503,1,1,1,1,x.hit,&x)==1 && a.rolls==500);
    x=next(&a);assert(s14_ai_state_commit(&a,3,3,504,1,1,2,1,x.hit,&x)==2 && a.rolls==501);
    S14AiState cities;s14_ai_state_seed(&cities,123);int pity_hits=0,guarantees=0,early_random=0;
    for(int i=1;i<=90000;i++){
        int city=i%2?12:13;S14AiDraw d=next_city(&cities,city);uint64_t count=cities.city_departures[city];assert(d.city_sequence==count+1 && d.guaranteed==((count+1)%9==0));
        S14AiState before=cities;assert(!s14_ai_state_commit(&cities,1,518,i%65536,0,1,2,1,d.hit,&d));assert(!memcmp(&before,&cities,sizeof(cities)));
        assert(s14_ai_state_commit(&cities,1,518,i%65536,1,1,2,1,d.hit,&d)==2);assert(cities.city_departures[city]==count+1);
        assert(!s14_ai_state_commit(&cities,1,518,i%65536,1,1,2,1,d.hit,&d));assert(cities.city_departures[city]==count+1);
        guarantees+=d.guaranteed;pity_hits+=d.hit;early_random+=d.random_hit && !d.guaranteed;
    }
    assert(cities.city_departures[12]==45000 && cities.city_departures[13]==45000 && guarantees==10000 && early_random>0 && pity_hits>17600 && pity_hits<18400);
    /* Counts belong to the city, not the commander or its current AI owner. */
    x=next_city(&cities,12);uint64_t prior=cities.city_departures[12];assert(s14_ai_state_commit(&cities,2,511,600,1,1,4,1,x.hit,&x)==2 && cities.city_departures[12]==prior+1);
    x=next_city(&cities,12);prior=cities.city_departures[12];assert(s14_ai_state_commit(&cities,3,512,601,1,0,4,1,x.hit,&x)==1 && cities.city_departures[12]==prior);
    wchar_t titled[32];assert(prefix(L"神 曹仁",titled,32) && !wcscmp(titled,L"禁 曹仁"));assert(prefix(L"高顺 · 陷阵营",titled,32) && !wcscmp(titled,L"禁 高顺 · 陷阵营"));assert(!prefix(L"曹仁",titled,4));
    unsigned char *game=calloc(1,0x2020000),*world=calloc(1,0x85200),*pool=calloc(501,512),*person=calloc(1,512),*group=calloc(1,64),*settings=calloc(1,128),*city=calloc(1,128);assert(game && world && pool && person && group && settings && city);
    uintptr_t w=(uintptr_t)world;image=(uintptr_t)game;p64(game,0x1fc91d0,w);p64(world,0x7df60,(uintptr_t)pool);p64(world,0x85130,(uintptr_t)settings);settings[0x3a]=1;
    p64(world,0x148+518*8,(uintptr_t)person);p64(person,0,image+0x12a00d0);p16(person,0x10,518);person[0x118]=2;p64(world,0xde40+2*8,(uintptr_t)group);p64(group,0,image+0x129fec8);group[0x10]=3;
    p16(person,0x11a,12);p64(world,0xdaa8+12*8,(uintptr_t)city);p64(city,0,image+0x129fd10);p16(city,0x10,12);
    unsigned char *army=pool+512;p64(world,0x7df60+8,(uintptr_t)army);p64(army,0,image+0x123e288);p16(army,0x12,518);p16(army,0x16,1000);
    wchar_t temp[MAX_PATH],path[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));ready=1;enabled=1;
    if(argc==6 && !strcmp(argv[1],"--restart")){
        assert(MultiByteToWideChar(CP_ACP,0,argv[2],-1,root,MAX_PATH));army[0x10]=1;p16(army,0x28,(unsigned short)strtoul(argv[5],NULL,10));unsigned char saved_hash[32]={7};
        s14_ai_load_begin();s14_ai_load_end(w,1,saved_hash,1);assert(!fault && s14_ai_active(army));assert(state.rng==strtoull(argv[3],NULL,10) && state.rolls==strtoull(argv[4],NULL,10));
        assert(state.city_departures[12]==state.rolls);S14AiDraw replay=next_city(&state,12);uint64_t expected=state.rng;assert(replay.next==expected+UINT64_C(0x9e3779b97f4a7c15) && replay.random_hit==s14_ai_roll_hit(s14_ai_random(&expected)) && replay.guaranteed==((state.rolls+1)%9==0));return 0;
    }
    swprintf(root,MAX_PATH,L"%lsS14-ai-test-%lu",temp,GetCurrentProcessId());assert(CreateDirectoryW(root,NULL));
    unsigned char hash[32]={7},old_hash[32]={8},zero[32]={9};s14_ai_load_begin();s14_ai_load_end(w,1,hash,1);assert(!fault && state.seed==hash_seed(hash) && !state.rolls);
    /* Existing army when loading a legacy save is never retrospectively drawn. */
    army[0x10]=1;S14AiDraw d;s14_ai_prepare(518,image+0x1d1586,&d);assert(!d.eligible);s14_ai_created(army,&d);assert(!state.rolls && !s14_ai_active(army));
    army[0x10]=0;s14_ai_forget(army);assert(s14_ai_save(w,old_hash));S14AiState origin=state;
    int hit=0;for(int i=1;i<=100 && !hit;i++){p16(army,0x28,(unsigned short)i);s14_ai_prepare(518,image+0x1d1586,&d);assert(d.eligible && d.force==3 && d.player==1 && d.city==12 && d.city_sequence==state.rolls+1);uint64_t before=state.rng,rolls=state.rolls;s14_ai_created(NULL,&d);assert(state.rng==before && state.rolls==rolls && state.city_departures[12]==rolls);
        s14_ai_creation_begin(&d);army[0x10]=1;assert(s14_ai_active(army)==d.hit);s14_ai_creation_end();s14_ai_created(army,&d);assert(state.rolls==rolls+1 && s14_ai_active(army)==d.hit);s14_ai_created(army,&d);assert(state.rolls==rolls+1);
        hit=d.hit;if(!hit){army[0x10]=0;s14_ai_forget(army);}}
    assert(hit && s14_ai_person_active(w,518));assert(s14_ai_name(army,L"神 曹仁",titled,32) && !wcscmp(titled,L"禁 曹仁"));
    enabled=0;assert(!s14_ai_active(army));enabled=1;assert(s14_ai_active(army));assert(s14_ai_save(w,hash));assert(file_path(hash,path,0) && s14_ai_checkpoint_owned(path));uint64_t saved_rng=state.rng,saved_rolls=state.rolls;
    wchar_t executable[MAX_PATH],command[1024];assert(GetModuleFileNameW(NULL,executable,MAX_PATH));unsigned short saved_serial;memcpy(&saved_serial,army+0x28,2);
    swprintf(command,1024,L"\"%ls\" --restart \"%ls\" %llu %llu %u",executable,root,(unsigned long long)saved_rng,(unsigned long long)saved_rolls,saved_serial);
    STARTUPINFOW startup={.cb=sizeof(startup),.dwFlags=STARTF_USESHOWWINDOW,.wShowWindow=SW_HIDE};PROCESS_INFORMATION child;
    assert(CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&child));assert(WaitForSingleObject(child.hProcess,10000)==WAIT_OBJECT_0);DWORD code;assert(GetExitCodeProcess(child.hProcess,&code) && code==0);CloseHandle(child.hThread);CloseHandle(child.hProcess);
    /* Return/redeploy faster than a worker tick, same slot/leader/native serial. */
    army[0x10]=0;s14_ai_prepare(518,0,&d);assert(d.eligible && !state.units[1].present);army[0x10]=1;s14_ai_created(army,&d);assert(state.rolls==saved_rolls+1 && s14_ai_active(army)==d.hit);
    s14_ai_load_begin();s14_ai_load_end(w,1,hash,1);assert(s14_ai_active(army) && state.rng==saved_rng && state.rolls==saved_rolls);
    s14_ai_load_begin();s14_ai_load_end(w,0,NULL,0);assert(s14_ai_active(army) && state.rng==saved_rng);
    s14_ai_forget(army);assert(s14_ai_active(army));army[0x10]=0;s14_ai_forget(army);assert(!s14_ai_active(army));army[0x10]=1;s14_ai_load_begin();s14_ai_load_end(w,1,hash,1);assert(s14_ai_active(army) && state.rng==saved_rng && state.rolls==saved_rolls);
    /* Same slot/new native serial cannot inherit a saved binding. */
    unsigned short serial;memcpy(&serial,army+0x28,2);p16(army,0x28,serial+1);assert(!s14_ai_active(army));p16(army,0x28,serial);
    group[0x10]=1;assert(!s14_ai_active(army));s14_ai_sweep();group[0x10]=3;assert(!s14_ai_active(army));
    /* Load the predeployment checkpoint: future RNG and hits are discarded. */
    army[0x10]=0;s14_ai_load_begin();s14_ai_load_end(w,1,old_hash,1);assert(!memcmp(&origin,&state,sizeof(state)));s14_ai_prepare(518,image+0x1d1586,&d);S14AiDraw replay=next_city(&origin,12);assert(d.hit==replay.hit && d.next==replay.next && !state.city_departures[12]);
    enabled=0;army[0x10]=0;p16(army,0x28,500);s14_ai_prepare(518,0,&d);army[0x10]=1;s14_ai_created(army,&d);assert(!state.rolls);enabled=1;assert(!s14_ai_active(army));
    /* V1 retains RNG/bindings while starting unknown historical city counts
       at zero; a subsequent save upgrades atomically to V2. */
    unsigned char legacy_hash[32]={10};Checkpoint stored;assert(file_path(hash,path,0) && checkpoint_read(path,hash,&stored)==1);
    LegacyCheckpoint legacy={0};memcpy(legacy.magic,"S14AI-AFFIX.v1",14);legacy.version=1;legacy.bytes=sizeof(legacy);memcpy(legacy.hash,legacy_hash,32);memcpy(&legacy.state,&stored.state,sizeof(legacy.state));assert(digest(&legacy.state,sizeof(legacy.state),legacy.digest));
    assert(file_path(legacy_hash,path,0));HANDLE legacy_file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD legacy_bytes;assert(legacy_file!=INVALID_HANDLE_VALUE && WriteFile(legacy_file,&legacy,sizeof(legacy),&legacy_bytes,NULL) && legacy_bytes==sizeof(legacy));CloseHandle(legacy_file);assert(s14_ai_checkpoint_owned(path));
    p16(army,0x28,saved_serial);s14_ai_load_begin();s14_ai_load_end(w,1,legacy_hash,1);assert(!fault && s14_ai_active(army) && state.rng==saved_rng && state.rolls==saved_rolls);for(int i=0;i<S14_AI_CITIES;i++)assert(!state.city_departures[i]);
    assert(s14_ai_save(w,legacy_hash));legacy_file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);assert(legacy_file!=INVALID_HANDLE_VALUE && ReadFile(legacy_file,&stored,sizeof(stored),&legacy_bytes,NULL) && legacy_bytes==sizeof(stored) && stored.version==2);CloseHandle(legacy_file);
    /* Corrupted checkpoint is not replaced or used; ordinary armies unaffected. */
    assert(file_path(hash,path,0));HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);assert(f!=INVALID_HANDLE_VALUE);DWORD got;char bad='!';assert(WriteFile(f,&bad,1,&got,NULL));CloseHandle(f);
    s14_ai_load_begin();s14_ai_load_end(w,1,hash,1);assert(fault && !s14_ai_active(army) && !s14_ai_save(w,hash));
    s14_ai_load_begin();s14_ai_load_end(w,1,zero,1);assert(!fault && state.seed==hash_seed(zero) && !s14_ai_active(army));
    const unsigned char *files[]={hash,old_hash,zero,legacy_hash};for(int i=0;i<4;i++)if(file_path(files[i],path,0))DeleteFileW(path);swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager\\ai-affixes",root);RemoveDirectoryW(path);swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager",root);RemoveDirectoryW(path);RemoveDirectoryW(root);
    free(game);free(world);free(pool);free(person);free(group);free(settings);free(city);
    printf("{\"status\":\"passed\",\"samples\":1000000,\"hits\":%d,\"pity_samples\":90000,\"pity_hits\":%d,\"guaranteed_hits\":%d,\"city_periodic_ninth\":true,\"city_counts_independent\":true,\"v1_sidecar_migration\":true,\"independent_rng\":true,\"replay\":true,\"failed_and_duplicate_creation\":true,\"legacy_no_retrofit\":true,\"disabled_no_retrofit\":true,\"slot_reuse_guard\":true,\"player_transfer\":true,\"same_category_max\":true,\"separate_process_restore\":true,\"save_restore_rollback\":true,\"corrupt_fail_closed\":true}\n",hits,pity_hits,guarantees);
}
