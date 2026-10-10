#define WIN32_LEAN_AND_MEAN
#include "ai_affix.h"
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SplitMix64: all arithmetic/state belongs to this plugin. Multiply-high
   maps each 64-bit draw to [0,100) without biased %100 or game RNG calls. */
uint64_t s14_ai_random(uint64_t *state){uint64_t z=(*state+=UINT64_C(0x9e3779b97f4a7c15));z=(z^(z>>30))*UINT64_C(0xbf58476d1ce4e5b9);z=(z^(z>>27))*UINT64_C(0x94d049bb133111eb);return z^(z>>31);}
int s14_ai_roll_hit(uint64_t x){return (uint64_t)(((unsigned __int128)x*100)>>64)<10;}
int s14_ai_percent_max(int a,int b){return a>b?a:b;}
void s14_ai_state_seed(S14AiState *s,uint64_t seed){memset(s,0,sizeof(*s));s->seed=seed;s->rng=seed;}
void s14_ai_draw(const S14AiState *s,int city,S14AiDraw *d){
    d->rng=s->rng;d->next=s->rng;d->sequence=s->rolls+1;d->random_hit=s14_ai_roll_hit(s14_ai_random(&d->next));
    d->city=city>0 && city<S14_AI_CITIES?city:-1;d->city_count=d->city>0?s->city_departures[d->city]:0;d->city_sequence=d->city>0?d->city_count+1:0;
    d->guaranteed=d->city>0 && d->city_sequence%9==0;d->hit=d->random_hit || d->guaranteed;
}
/* Pure successful-creation model. present includes misses/off/player armies,
   so repeated callbacks can never reroll a living identity. */
int s14_ai_state_commit(S14AiState *s,int slot,int leader,int serial,int success,int enabled,int force,int player,int hit,S14AiDraw *draw){
    if(!success || slot<1 || slot>=S14_AI_UNITS || leader<1 || leader>6000 || serial<0 || serial>65535)return 0;
    S14AiBinding *b=&s->units[slot];if(b->present && b->leader==leader && b->serial==serial)return 0;
    int eligible=enabled && force>=1 && force<=51 && player>=1 && player<=51 && force!=player && draw && draw->eligible;
    if(eligible && (draw->rng!=s->rng || draw->sequence!=s->rolls+1))return -1;
    int city=eligible && draw->city>0 && draw->city<S14_AI_CITIES?draw->city:-1;
    if(city>0 && (draw->city_count!=s->city_departures[city] || draw->city_sequence!=draw->city_count+1))return -1;
    memset(b,0,sizeof(*b));b->present=1;b->leader=(uint16_t)leader;b->serial=(uint16_t)serial;b->generation=++s->generation;
    if(!eligible)return 1;
    s->rng=draw->next;s->rolls++;if(city>0)s->city_departures[city]++;b->hit=!!hit;return 2;
}
static uintptr_t image,live_world;
static wchar_t root[MAX_PATH];
static volatile LONG ready,enabled,loading,fault;
static SRWLOCK lock=SRWLOCK_INIT;
static S14AiState state,backup;
static uintptr_t backup_world;
static LONG backup_fault;
static _Thread_local S14AiDraw creating;
typedef struct {volatile LONG full;char line[768];} Event;
static Event events[128];static volatile LONG events_sequence,dropped;
static void event(const char *kind,const S14AiDraw *d,int id,int serial,uint64_t generation){
    LONG n=InterlockedIncrement(&events_sequence);Event *e=&events[(unsigned)n%128];if(InterlockedCompareExchange(&e->full,1,0)){InterlockedIncrement(&dropped);return;}
    snprintf(e->line,sizeof(e->line),"{\"event\":\"ai_affix_%s\",\"force\":%d,\"player_force\":%d,\"officer_id\":%d,\"army_id\":%d,\"native_serial\":%d,\"creation_sequence\":%llu,\"roll_sequence\":%llu,\"city_id\":%d,\"city_departure\":%llu,\"guaranteed\":%s,\"random_hit\":%s,\"hit\":%s,\"caller_rva\":\"0x%llx\",\"queue_dropped\":%ld}\n",kind,d?d->force:0,d?d->player:0,d?d->leader:0,id,serial,(unsigned long long)generation,(unsigned long long)(d?d->sequence:0),d?d->city:-1,(unsigned long long)(d?d->city_sequence:0),d&&d->guaranteed?"true":"false",d&&d->random_hit?"true":"false",d&&d->hit?"true":"false",(unsigned long long)(d?d->caller:0),dropped);InterlockedExchange(&e->full,2);
}
int s14_ai_next_log(char *out,size_t cap){if(!out || cap<768)return 0;for(int i=0;i<128;i++)if(InterlockedCompareExchange(&events[i].full,3,2)==2){strcpy(out,events[i].line);InterlockedExchange(&events[i].full,0);return 1;}return 0;}
static int read_at(uintptr_t p,void *out,size_t n){SIZE_T got;return p && ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got) && got==n;}
static uintptr_t ptr(uintptr_t p){uintptr_t v=0;read_at(p,&v,8);return v;}
static uintptr_t world_now(void){return image?ptr(image+0x1fc91d0):0;}
static int force_for(uintptr_t world,int leader,int *force,int *player){
    unsigned char person[0x119],group[0x18],settings[0x3b];uintptr_t p=ptr(world+0x148+(uintptr_t)leader*8),s=ptr(world+0x85130);
    if(leader<1 || leader>6000 || !read_at(p,person,sizeof(person)) || ptr(p)!=image+0x12a00d0 || !read_at(s,settings,sizeof(settings)))return 0;
    unsigned short id;memcpy(&id,person+0x10,2);int g=person[0x118];if(id!=leader || g<1 || g>51)return 0;
    uintptr_t at=ptr(world+0xde40+(uintptr_t)g*8);if(!read_at(at,group,sizeof(group)) || ptr(at)!=image+0x129fec8)return 0;
    *force=group[0x10];*player=settings[0x3a];return *force>=1 && *force<=51 && *player>=1 && *player<=51;
}
static int identity(void *army,uintptr_t *world,int *id,S14AiBinding *b){
    uintptr_t w=world_now(),pool=ptr(w+0x7df60),at=(uintptr_t)army;unsigned char bytes[48];
    if(!w || !pool || at<=pool || (at-pool)%512 || (at-pool)/512>500 || !read_at(at,bytes,sizeof(bytes)) || ptr(at)!=image+0x123e288 || bytes[0x10]!=1)return 0;
    int slot=(int)((at-pool)/512);if(ptr(w+0x7df60+(uintptr_t)slot*8)!=at)return 0;
    memset(b,0,sizeof(*b));memcpy(&b->leader,bytes+0x12,2);memcpy(&b->serial,bytes+0x28,2);b->present=1;
    if(b->leader<1 || b->leader>6000)return 0;*world=w;*id=slot;return 1;
}
static int same(const S14AiBinding *a,const S14AiBinding *b){return a->present && b->present && a->leader==b->leader && a->serial==b->serial;}
static int origin_city(uintptr_t w,int leader){
    uintptr_t person=ptr(w+0x148+(uintptr_t)leader*8);unsigned short city=0,id=0;
    if(!read_at(person+0x11a,&city,2) || city<1 || city>=S14_AI_CITIES)return -1;
    uintptr_t object=ptr(w+0xdaa8+(uintptr_t)city*8);
    return object && ptr(object)==image+0x129fd10 && read_at(object+0x10,&id,2) && id==city?city:-1;
}
void s14_ai_attach(uintptr_t base,const wchar_t *folder,int available){image=base;if(folder && wcslen(folder)<MAX_PATH)wcscpy(root,folder);InterlockedExchange(&ready,!!available);}
void s14_ai_configure(int on){InterlockedExchange(&enabled,on && ready);}
int s14_ai_faulted(void){return InterlockedCompareExchange(&fault,0,0)!=0;}
static uint64_t hash_seed(const unsigned char *hash){uint64_t seed=UINT64_C(0x53414e3134414947);for(int i=0;i<32;i++){seed^=hash[i];seed*=UINT64_C(0x100000001b3);}return seed?seed:UINT64_C(0x53414e3134414947);}
void s14_ai_prepare(int leader,uintptr_t caller,S14AiDraw *d){
    memset(d,0,sizeof(*d));d->city=-1;d->leader=leader;d->world=world_now();d->caller=caller>=image?caller-image:0;
    if(!ready || loading || fault || !d->world || !force_for(d->world,leader,&d->force,&d->player))return;
    /* Native serials can restart when no armies survive. Retire dead bindings
       on the game thread before creation, even if the worker has not swept yet. */
    s14_ai_sweep();
    int city=origin_city(d->world,leader);
    AcquireSRWLockShared(&lock);if(live_world==d->world && enabled && state.seed){s14_ai_draw(&state,city,d);d->eligible=d->force!=d->player;}ReleaseSRWLockShared(&lock);
    /* An already living army of this leader is a duplicate/restore, not an
       eligible new deployment. Never use UI preview initialization hooks. */
    if(d->eligible){
        uintptr_t units[S14_AI_UNITS],pool=ptr(d->world+0x7df60);unsigned char *raw=malloc(S14_AI_UNITS*512);
        if(!raw || !pool || !read_at(d->world+0x7df60,units,sizeof(units)) || !read_at(pool,raw,S14_AI_UNITS*512))d->eligible=0;
        else for(int i=1;i<S14_AI_UNITS;i++){unsigned short id;memcpy(&id,raw+i*512+0x12,2);if(units[i]==pool+(uintptr_t)i*512 && raw[i*512+0x10]==1 && id==leader){d->eligible=0;break;}}
        free(raw);
    }
}
void s14_ai_creation_begin(const S14AiDraw *d){creating=*d;}
void s14_ai_creation_end(void){memset(&creating,0,sizeof(creating));}
void s14_ai_created(void *army,const S14AiDraw *d){
    uintptr_t w=0;int id=0;S14AiBinding b;int f=0,p=0;
    if(!ready || loading)return;
    if(!identity(army,&w,&id,&b) || w!=d->world || b.leader!=d->leader){event("creation_failed",d,0,0,0);return;}
    if(!force_for(w,b.leader,&f,&p)){event("force_unavailable",d,id,b.serial,0);return;}
    AcquireSRWLockExclusive(&lock);int result=0;if(live_world==w && state.seed && !fault)result=s14_ai_state_commit(&state,id,b.leader,b.serial,1,enabled && d->eligible,f,p,d->hit,(S14AiDraw*)d);uint64_t generation=state.units[id].generation;ReleaseSRWLockExclusive(&lock);
    S14AiDraw logged=*d;logged.force=f;logged.player=p;logged.hit=result==2 && d->hit;if(result!=2){logged.sequence=logged.city_sequence=0;logged.guaranteed=logged.random_hit=0;}
    if(result<0){InterlockedExchange(&fault,1);event("transaction_fault",&logged,id,b.serial,generation);}else event(result==2?"rolled":result==1?"ineligible":"duplicate",&logged,id,b.serial,generation);
}
int s14_ai_active(void *army){
    if(!enabled || !ready || loading || fault)return 0;uintptr_t w=world_now(),pool=ptr(w+0x7df60),at=(uintptr_t)army;int id;S14AiBinding b,copy;int f,p;
    if(!w || !pool || at<=pool || (at-pool)%512 || (at-pool)/512>500)return 0;
    id=(int)((at-pool)/512);AcquireSRWLockShared(&lock);int valid=live_world==w;copy=state.units[id];ReleaseSRWLockShared(&lock);
    if(!(valid && copy.hit) && !(creating.eligible && creating.hit && creating.world==w))return 0;
    if(!identity(army,&w,&id,&b) || !force_for(w,b.leader,&f,&p) || f==p)return 0;
    /* Candidate is read-only and local to the native successful-create call.
       Native final attribute getters see the same rule before initial caching;
       state advances only after the original creator returns a canonical army. */
    if(creating.eligible && creating.hit && creating.world==w && creating.leader==b.leader)return 1;
    AcquireSRWLockShared(&lock);valid=live_world==w;copy=state.units[id];ReleaseSRWLockShared(&lock);return valid && copy.hit && same(&copy,&b);
}
int s14_ai_person_active(uintptr_t w,int leader){
    if(!enabled || loading || fault || !ready || !w)return 0;int found=0;
    S14AiBinding candidates[S14_AI_UNITS];AcquireSRWLockShared(&lock);int valid=live_world==w;memcpy(candidates,state.units,sizeof(candidates));ReleaseSRWLockShared(&lock);if(!valid)return 0;
    for(int i=1;i<S14_AI_UNITS;i++)if(candidates[i].hit && candidates[i].leader==leader && s14_ai_active((void*)ptr(w+0x7df60+(uintptr_t)i*8)))found++;
    return found==1;
}
static int prefix(const wchar_t *name,wchar_t *out,size_t cap){if(!name || !out || cap<4)return 0;if((name[0]==L'神' || name[0]==L'禁') && name[1]==L' ')name+=2;size_t n=wcslen(name);if(n+3>cap)return 0;memmove(out+2,name,(n+1)*sizeof(wchar_t));out[0]=L'禁';out[1]=L' ';return 1;}
int s14_ai_name(void *army,const wchar_t *name,wchar_t *out,size_t cap){return s14_ai_active(army) && prefix(name,out,cap);}
int s14_ai_person_name(uintptr_t w,int id,const wchar_t *name,wchar_t *out,size_t cap){return s14_ai_person_active(w,id) && prefix(name,out,cap);}
void s14_ai_forget(void *army){uintptr_t w=world_now(),pool=ptr(w+0x7df60),at=(uintptr_t)army;if(!pool || at<=pool || (at-pool)%512 || (at-pool)/512>500)return;
    /* A rejected native removal must not discard a living army's draw. */
    uintptr_t current;int slot;S14AiBinding key;if(identity(army,&current,&slot,&key))return;
    int id=(int)((at-pool)/512);AcquireSRWLockExclusive(&lock);if(live_world==w)memset(&state.units[id],0,sizeof(state.units[id]));ReleaseSRWLockExclusive(&lock);}
void s14_ai_sweep(void){
    if(!ready || loading || fault)return;uintptr_t w=world_now();if(!w)return;
    S14AiBinding candidates[S14_AI_UNITS];AcquireSRWLockShared(&lock);int valid=live_world==w;memcpy(candidates,state.units,sizeof(candidates));ReleaseSRWLockShared(&lock);if(!valid)return;
    for(int i=1;i<S14_AI_UNITS;i++)if(candidates[i].present){uintptr_t again;int slot,f,p;S14AiBinding b;uintptr_t army=ptr(w+0x7df60+(uintptr_t)i*8);
        int dead=!identity((void*)army,&again,&slot,&b) || !same(&b,&candidates[i]);
        int transferred=!dead && candidates[i].hit && force_for(w,b.leader,&f,&p) && f==p;
        if(dead || transferred){AcquireSRWLockExclusive(&lock);if(live_world==w && !loading && state.units[i].generation==candidates[i].generation) {if(dead)memset(&state.units[i],0,sizeof(state.units[i]));else state.units[i].hit=0;}ReleaseSRWLockExclusive(&lock);}
    }
}
typedef struct {char magic[16];uint32_t version,bytes;unsigned char hash[32],digest[32];S14AiState state;} Checkpoint;
typedef struct {uint64_t seed,rng,rolls,generation;S14AiBinding units[S14_AI_UNITS];} LegacyState;
typedef struct {char magic[16];uint32_t version,bytes;unsigned char hash[32],digest[32];LegacyState state;} LegacyCheckpoint;
static int digest(const void *bytes,size_t len,unsigned char out[32]){BCRYPT_ALG_HANDLE alg=NULL;int ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0;ok=ok && BCryptHash(alg,NULL,0,(PUCHAR)bytes,(ULONG)len,out,32)>=0;if(alg)BCryptCloseAlgorithmProvider(alg,0);return ok;}
static int folder(const wchar_t *path){DWORD attr=GetFileAttributesW(path);if(attr==INVALID_FILE_ATTRIBUTES){if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;attr=GetFileAttributesW(path);}return (attr&FILE_ATTRIBUTE_DIRECTORY) && !(attr&FILE_ATTRIBUTE_REPARSE_POINT);}
static int file_path(const unsigned char *hash,wchar_t out[MAX_PATH],int make){if(!hash || !root[0] || wcslen(root)+122>=MAX_PATH)return 0;wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager",root);if(make && !folder(path))return 0;swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager\\ai-affixes",root);if(make && !folder(path))return 0;wchar_t hex[65];for(int i=0;i<32;i++)swprintf(hex+2*i,3,L"%02x",hash[i]);return swprintf(out,MAX_PATH,L"%ls\\%ls.s14aiaffix",path,hex)>0;}
static int checkpoint_read(const wchar_t *path,const unsigned char *hash,Checkpoint *c){
    DWORD attr=GetFileAttributesW(path);if(attr==INVALID_FILE_ATTRIBUTES)return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND?0:-1;
    if(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return -1;HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return -1;
    LARGE_INTEGER size;DWORD got;unsigned char sum[32];int ok=GetFileSizeEx(f,&size);
    if(ok && size.QuadPart==sizeof(LegacyCheckpoint)){
        LegacyCheckpoint old;ok=ReadFile(f,&old,sizeof(old),&got,NULL) && got==sizeof(old);
        ok=ok && !memcmp(old.magic,"S14AI-AFFIX.v1",14) && old.version==1 && old.bytes==sizeof(old) && !memcmp(old.hash,hash,32) && digest(&old.state,sizeof(old.state),sum) && !memcmp(sum,old.digest,32);
        memset(c,0,sizeof(*c));if(ok){memcpy(c->magic,"S14AI-AFFIX.v2",14);c->version=2;c->bytes=sizeof(*c);memcpy(c->hash,hash,32);memcpy(&c->state,&old.state,sizeof(old.state));}
    }else{
        ok=ok && size.QuadPart==sizeof(*c) && ReadFile(f,c,sizeof(*c),&got,NULL) && got==sizeof(*c);
        ok=ok && !memcmp(c->magic,"S14AI-AFFIX.v2",14) && c->version==2 && c->bytes==sizeof(*c) && !memcmp(c->hash,hash,32) && digest(&c->state,sizeof(c->state),sum) && !memcmp(sum,c->digest,32);
    }
    CloseHandle(f);ok=ok && c->state.seed;
    for(int i=0;ok && i<S14_AI_UNITS;i++){S14AiBinding *b=&c->state.units[i];ok=b->present<=1 && b->hit<=1 && (!b->present?(!b->hit && !b->generation):i>0 && b->leader>=1 && b->leader<=6000 && b->generation && b->generation<=c->state.generation);}
    uint64_t total=0;for(int i=0;ok && i<S14_AI_CITIES;i++){uint64_t count=c->state.city_departures[i];ok=count<=c->state.rolls && total<=c->state.rolls-count && (i>0 || count==0);if(ok)total+=count;}
    return ok?1:-1;
}
int s14_ai_checkpoint_owned(const wchar_t *path){
    if(!path)return 0;const wchar_t *leaf=wcsrchr(path,L'\\');leaf=leaf?leaf+1:path;if(wcslen(leaf)!=75 || wcscmp(leaf+64,L".s14aiaffix"))return 0;
    unsigned char hash[32];for(int i=0;i<32;i++){unsigned v=0;for(int j=0;j<2;j++){wchar_t c=leaf[i*2+j];int n=c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:-1;if(n<0)return 0;v=v*16+(unsigned)n;}hash[i]=(unsigned char)v;}
    Checkpoint *c=malloc(sizeof(*c));if(!c)return 0;int owned=checkpoint_read(path,hash,c)==1;free(c);return owned;
}
int s14_ai_save(uintptr_t w,const unsigned char *hash){
    if(!ready || loading || fault || !hash)return 0;s14_ai_sweep();Checkpoint *c=calloc(1,sizeof(*c)),*old=malloc(sizeof(*old));wchar_t path[MAX_PATH],temp[MAX_PATH];
    if(!c || !old || !file_path(hash,path,1)){free(c);free(old);event("save_failed",NULL,0,0,0);return 0;}
    AcquireSRWLockShared(&lock);int ok=live_world==w && state.seed;c->state=state;ReleaseSRWLockShared(&lock);
    memcpy(c->magic,"S14AI-AFFIX.v2",14);c->version=2;c->bytes=sizeof(*c);memcpy(c->hash,hash,32);
    int prior=checkpoint_read(path,hash,old);ok=ok && prior>=0 && digest(&c->state,sizeof(c->state),c->digest) && swprintf(temp,MAX_PATH,L"%ls.tmp.%lu",path,GetCurrentProcessId())>0;
    HANDLE f=ok?CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL):INVALID_HANDLE_VALUE;DWORD got=0;
    ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,c,sizeof(*c),&got,NULL) && got==sizeof(*c) && FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok && f!=INVALID_HANDLE_VALUE)DeleteFileW(temp);
    if(prior<0)InterlockedExchange(&fault,1);free(c);free(old);event(ok?"saved":"save_failed",NULL,0,0,0);return ok;
}
void s14_ai_load_begin(void){InterlockedExchange(&loading,1);AcquireSRWLockExclusive(&lock);backup=state;backup_world=live_world;backup_fault=fault;ReleaseSRWLockExclusive(&lock);s14_ai_creation_end();}
void s14_ai_load_end(uintptr_t w,int success,const unsigned char *hash,int valid){
    if(!ready){InterlockedExchange(&loading,0);return;}Checkpoint *c=calloc(1,sizeof(*c));wchar_t path[MAX_PATH];int restored=valid && c && file_path(hash,path,0)?checkpoint_read(path,hash,c):-1;
    AcquireSRWLockExclusive(&lock);
    if(!success){state=backup;live_world=backup_world;fault=backup_fault;}
    else{live_world=w;fault=restored<0; if(restored==1)state=c->state;else if(restored==0)s14_ai_state_seed(&state,hash_seed(hash));else memset(&state,0,sizeof(state));}
    ReleaseSRWLockExclusive(&lock);free(c);InterlockedExchange(&loading,0);if(success)s14_ai_sweep();event(!success?"load_cancelled":restored==1?"restored":restored==0?"legacy_seeded":"sidecar_fault",NULL,0,0,0);
}
void s14_ai_new_game(uintptr_t w){uint64_t seed=0;int ok=BCryptGenRandom(NULL,(PUCHAR)&seed,sizeof(seed),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0 && seed;AcquireSRWLockExclusive(&lock);s14_ai_state_seed(&state,seed);live_world=w;fault=!ok;ReleaseSRWLockExclusive(&lock);InterlockedExchange(&loading,0);event(ok?"new_campaign":"seed_fault",NULL,0,0,0);}
void s14_ai_status(wchar_t *out,size_t cap){AcquireSRWLockShared(&lock);uint64_t rolls=state.rolls;int bound=live_world && state.seed;ReleaseSRWLockShared(&lock);swprintf(out,cap,fault?L"随机词条已停用：附属文件或状态异常，请查看日志。":!bound?L"等待载入存档 · 不为已有部队补抽":L"各城每 9 次出征保底 · 其余 10%% · 已参与 %llu 次",(unsigned long long)rolls);}
