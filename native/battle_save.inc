// Native save/load hooks. No serialization or game functions are called by
// the probe. Rare successful save/load callbacks hash an existing save file.
typedef uintptr_t (*SaveCommitCall)(void*,void*);
typedef uintptr_t (*LoadCall)(void*,int,void*);
typedef uintptr_t (*NewGameCall)(void*,int);
static SaveCommitCall original_save_commit;
static LoadCall original_load,original_load_reader;
static NewGameCall original_new_game;
static _Thread_local BattleEvent *battle_load_context;
static wchar_t battle_save_root[MAX_PATH];
static int battle_clock_day(uintptr_t world) {
    uintptr_t settings=0;unsigned char c[6];
    if(!world || !battle_read((void*)(world+0x85130),&settings,8) || !battle_read((void*)(settings+0x34),c,6)) return -1;
    int y=c[0]+256*c[1];return y>0 && c[2]>=1 && c[2]<=12 && c[3]>=1 && c[3]<=30?y*360+(c[2]-1)*30+c[3]-1:-1;
}
static int battle_filename(void *data,wchar_t out[512]) {
    unsigned char raw[32];if(!data || !battle_read((unsigned char*)data+0x50,raw,32)) return 0;
    uintptr_t len=0,cap=0,pointer=0;memcpy(&len,raw+16,8);memcpy(&cap,raw+24,8);
    if(!len || len>=MAX_PATH || cap<len || cap>65536) return 0;
    char text[MAX_PATH];const void *p=(unsigned char*)data+0x50;
    if(cap>=16) {memcpy(&pointer,raw,8);p=(void*)pointer;}
    if(!battle_read(p,text,len+1) || text[len]) return 0;
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,out,512)>0;
}
static int battle_save_leaf(const wchar_t *name) {
    const wchar_t *prefixes[]={L"svdexSC",L"autosdexSC",L"svdexccSC",L"autosdexccSC"};
    for(int i=0;i<4;i++) {size_t n=wcslen(prefixes[i]);if(!_wcsnicmp(name,prefixes[i],n) && wcslen(name)==n+6 && name[n]>=L'0' && name[n]<=L'9' && name[n+1]>=L'0' && name[n+1]<=L'9' && !_wcsicmp(name+n+2,L".s14")) return 1;}return 0;
}
static int battle_resolve_save(const wchar_t *name,wchar_t out[MAX_PATH]) {
    if(!name || !name[0] || !battle_save_root[0]) return 0;
    const wchar_t *leaf=wcsrchr(name,L'\\'),*slash=wcsrchr(name,L'/');if(slash && (!leaf || slash>leaf)) leaf=slash;leaf=leaf?leaf+1:name;
    if(!battle_save_leaf(leaf)) return 0;
    // This installation's native remote-save names map to Profile/<user>/Saves.
    // Ambiguous profiles are rejected rather than binding the wrong campaign.
    wchar_t pattern[MAX_PATH];if(swprintf(pattern,MAX_PATH,L"%ls\\Profile\\*",battle_save_root)<0) return 0;
    WIN32_FIND_DATAW data;HANDLE find=FindFirstFileW(pattern,&data);if(find==INVALID_HANDLE_VALUE) return 0;int hits=0,entries=0;
    do {
        if(++entries>64) {hits=2;break;}
        if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) || data.cFileName[0]==L'.') continue;
        wchar_t candidate[MAX_PATH];if(swprintf(candidate,MAX_PATH,L"%ls\\Profile\\%ls\\Saves\\%ls",battle_save_root,data.cFileName,leaf)<0) continue;
        DWORD attr=GetFileAttributesW(candidate);if(attr!=INVALID_FILE_ATTRIBUTES && !(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) {wcscpy(out,candidate);hits++;}
    } while(FindNextFileW(find,&data));FindClose(find);return hits==1;
}
static int battle_save_digest(const wchar_t *name,unsigned char digest[32]) {
    wchar_t path[MAX_PATH];if(!battle_resolve_save(name,path)) return 0;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size;int ok=GetFileSizeEx(file,&size) && size.QuadPart>1024 && size.QuadPart<=16*1024*1024;
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;unsigned char data[16384];DWORD got=0;
    ok=ok && BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0 && BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)>=0;
    while(ok) {if(!ReadFile(file,data,sizeof(data),&got,NULL)) {ok=0;break;}if(!got) break;if(BCryptHashData(hash,data,got,0)<0) ok=0;}
    if(ok) ok=BCryptFinishHash(hash,digest,32,0)>=0;
    if(hash) BCryptDestroyHash(hash);if(alg) BCryptCloseAlgorithmProvider(alg,0);CloseHandle(file);return ok;
}
static uintptr_t hooked_battle_save_commit(void *world,void *data) {
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_SAVE,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.world=(uintptr_t)world;e.args[0]=(uintptr_t)world;e.args[1]=(uintptr_t)data;e.planning_day=battle_clock_day(e.world);battle_filename(data,e.text);
    const wchar_t *leaf=wcsrchr(e.text,L'\\');leaf=leaf?leaf+1:e.text;
    if(!battle_save_leaf(leaf)) {SetLastError(error);return original_save_commit(world,data);}
    SetLastError(error);uintptr_t result=original_save_commit(world,data);error=GetLastError();
    e.native_return=result;
    if(result==0 && battle_clock_day(e.world)==e.planning_day) e.save_hash_valid=battle_save_digest(e.text,e.save_hash);
    if(result==0){s14_troop_save(e.world,e.save_hash_valid?e.save_hash:NULL);s14_ai_save(e.world,e.save_hash_valid?e.save_hash:NULL);}
    e.elapsed=GetTickCount64()-e.tick;battle_enqueue(&e);SetLastError(error);return result;
}
static uintptr_t hooked_battle_load_reader(void *data,int slot,void *name) {
    if(!battle_load_context) return original_load_reader(data,slot,name);
    DWORD error=GetLastError();SetLastError(error);uintptr_t result=original_load_reader(data,slot,name);error=GetLastError();
    if((int)result==0) battle_filename(data,battle_load_context->text);
    SetLastError(error);return result;
}
static uintptr_t hooked_battle_load(void *world,int slot,void *name) {
    DWORD error=GetLastError();BattleEvent start=battle_begin(B_LOAD_BEGIN,(uintptr_t)__builtin_return_address(0)-battle_base);
    start.affix_epoch=s14_affix_suspend();start.world=(uintptr_t)world;start.args[0]=(uintptr_t)world;start.args[1]=(uintptr_t)(intptr_t)slot;start.args[2]=(uintptr_t)name;
    battle_enqueue(&start);s14_troop_load_begin();s14_ai_load_begin();InterlockedIncrement(&battle_loading);
    BattleEvent e=battle_begin(B_LOAD_END,start.caller);e.affix_epoch=start.affix_epoch;e.world=(uintptr_t)world;e.parent=start.id;BattleEvent *old=battle_load_context;battle_load_context=&e;
    SetLastError(error);uintptr_t result=original_load(world,slot,name);error=GetLastError();battle_load_context=old;
    e.native_return=result;e.planning_day=battle_clock_day(e.world);
    if((int)result==1) e.save_hash_valid=battle_save_digest(e.text,e.save_hash);
    s14_troop_load_end(e.world,(int)result==1,e.save_hash,e.save_hash_valid);s14_ai_load_end(e.world,(int)result==1,e.save_hash,e.save_hash_valid);
    if((int)result==1) InterlockedIncrement(&battle_session_epoch);
    InterlockedDecrement(&battle_loading);e.elapsed=GetTickCount64()-e.tick;battle_enqueue(&e);SetLastError(error);return result;
}
static uintptr_t hooked_battle_new_game(void *world,int scenario) {
    DWORD error=GetLastError();
    if(!battle_load_context)s14_ai_load_begin();
    unsigned int epoch=battle_load_context?battle_load_context->affix_epoch:s14_affix_suspend();InterlockedIncrement(&battle_loading);SetLastError(error);
    uintptr_t result=original_new_game(world,scenario);error=GetLastError();InterlockedDecrement(&battle_loading);
    if(!battle_load_context) {if((int)result==1) InterlockedIncrement(&battle_session_epoch);s14_troop_new_game((uintptr_t)world);if((int)result==1)s14_ai_new_game((uintptr_t)world);BattleEvent e=battle_begin(B_NEW_GAME,(uintptr_t)__builtin_return_address(0)-battle_base);e.affix_epoch=epoch;e.world=(uintptr_t)world;e.native_return=result;e.planning_day=battle_clock_day(e.world);battle_enqueue(&e);}
    if(!battle_load_context && (int)result!=1)s14_ai_load_end((uintptr_t)world,0,NULL,0);
    SetLastError(error);return result;
}
