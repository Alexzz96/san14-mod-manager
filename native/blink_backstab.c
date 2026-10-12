#define WIN32_LEAN_AND_MEAN
#include "blink_backstab.h"
#include "blink_anchors.h"
#include "MinHook.h"
#include <bcrypt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <limits.h>

/* Test ability: read-only observed ID 271 and exact, unique original name 黄忠.
   Native calls run only in the existing combat / post-combat game thread. */
#define BLINK_UNITS 501
#define BLINK_REAR 3
#define BLINK_DAYS 5
#define BLINK_BONUS 30
#define BLINK_NO_TILE 48400
typedef struct {S14BlinkKey source,target;int cooldown,expires,target_tile,direction,opportunity;} BlinkRecord;
typedef struct {BlinkRecord units[BLINK_UNITS];} BlinkState;
typedef struct {S14BlinkArmy source,target;unsigned generation,epoch;DWORD thread;int day,active;} BlinkPending;
static BlinkState blink_state,blink_backup;
static BlinkPending blink_pending[BLINK_UNITS];
static unsigned char blink_reserved[BLINK_UNITS];
static SRWLOCK blink_lock=SRWLOCK_INIT;
static uintptr_t blink_base,blink_end,blink_world,blink_backup_world;
static volatile LONG blink_ready,blink_enabled,blink_loading,blink_fault;
static int blink_backup_fault,blink_leader,blink_leader_scanned;
static unsigned blink_epoch;
static wchar_t blink_root[MAX_PATH];
typedef int (*BlinkDirection)(int,int);
typedef int (*BlinkNeighbor)(int,int);
typedef int (*BlinkPass)(int,int,void*,int);
typedef int (*BlinkForce)(void*);
typedef int (*BlinkEnemy)(int,int);
typedef int (*BlinkFormation)(void*);
typedef void (*BlinkDestination)(void*,int);
typedef void (*BlinkClear)(int);
typedef void (*BlinkMove)(void*,int);
typedef void (*BlinkPath)(void*,int,int,int);
typedef void (*BlinkBatch)(void);
static BlinkDirection blink_original_direction;
static BlinkNeighbor blink_neighbor;
static BlinkPass blink_pass;
static BlinkForce blink_force;
static BlinkEnemy blink_enemy;
static BlinkFormation blink_formation;
static BlinkDestination blink_destination;
static BlinkClear blink_clear;
static BlinkMove blink_move;
static BlinkPath blink_path;
static void (*blink_occupy)(void*,int);
static BlinkBatch blink_original_batch;
static _Thread_local struct {int active,from,to,direction;} blink_direction_scope;
static _Thread_local int blink_batch_depth;
#ifdef S14_BLINK_TEST
static uintptr_t blink_direction_test_caller;
#define BLINK_DIRECTION_CALLER blink_direction_test_caller
#else
#define BLINK_DIRECTION_CALLER ((uintptr_t)__builtin_return_address(0))
#endif
typedef struct {volatile LONG full;char text[768];} BlinkLog;
static BlinkLog blink_logs[64];static volatile LONG blink_log_number;
static void blink_log(const char *event,int source,int target,int from,int to,int detail){
    LONG n=InterlockedIncrement(&blink_log_number);BlinkLog *e=&blink_logs[(unsigned)n%64];
    if(InterlockedCompareExchange(&e->full,1,0))return;
    snprintf(e->text,sizeof(e->text),"{\"event\":\"blink_%s\",\"source_army\":%d,\"target_army\":%d,\"from_tile\":%d,\"to_tile\":%d,\"detail\":%d,\"gameplay_modified\":%s}\n",event,source,target,from,to,detail,(!strcmp(event,"landed") || !strcmp(event,"backstab") || !strcmp(event,"move_fault"))?"true":"false");
    InterlockedExchange(&e->full,2);
}
int s14_blink_next_log(char *out,size_t cap){
    if(!out || cap<768)return 0;
    for(int i=0;i<64;i++)if(InterlockedCompareExchange(&blink_logs[i].full,3,2)==2){strcpy(out,blink_logs[i].text);InterlockedExchange(&blink_logs[i].full,0);return 1;}return 0;
}
static int blink_read(uintptr_t at,void *out,size_t n){SIZE_T got;return at && at+n>=at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static uintptr_t blink_ptr(uintptr_t at){uintptr_t v=0;blink_read(at,&v,8);return v;}
static unsigned short blink_word(uintptr_t at){unsigned short v=0;blink_read(at,&v,2);return v;}
static uintptr_t blink_world_now(void){return blink_base?blink_ptr(blink_base+0x1fc91d0):0;}
static int blink_day(uintptr_t world){uintptr_t settings=blink_ptr(world+0x85130);unsigned char c[6];if(!settings || !blink_read(settings+0x34,c,6))return -1;int year=c[0]+256*c[1];return year>0 && c[2]>=1 && c[2]<=12 && c[3]>=1 && c[3]<=30?year*360+(c[2]-1)*30+c[3]-1:-1;}
static int blink_key_same(const S14BlinkKey *a,const S14BlinkKey *b){return !memcmp(a,b,sizeof(*a));}
static int blink_identity(uintptr_t world,void *army,S14BlinkArmy *out){
    unsigned char raw[512];uintptr_t pool=blink_ptr(world+0x7df60),at=(uintptr_t)army,vt=0;unsigned short leader=0,serial=0,tile=0,troops=0;
    if(!world || !pool || at<=pool || (at-pool)%512 || (at-pool)/512>=BLINK_UNITS || !blink_read(at,raw,sizeof(raw)))return 0;
    int id=(int)((at-pool)/512);memcpy(&vt,raw,8);memcpy(&leader,raw+0x12,2);memcpy(&serial,raw+0x28,2);memcpy(&tile,raw+0x2a,2);memcpy(&troops,raw+0x16,2);
    if(vt!=blink_base+0x123e288 || raw[0x10]!=1 || !troops || leader<1 || leader>6000 || tile>=BLINK_NO_TILE || blink_ptr(world+0x7df60+(uintptr_t)id*8)!=at)return 0;
    uintptr_t person=blink_ptr(world+0x148+(uintptr_t)leader*8);unsigned char p[0x119];
    if(!person || !blink_read(person,p,sizeof(p)) || *(uintptr_t*)p!=blink_base+0x12a00d0 || blink_word(person+0x10)!=leader || p[0x118]<1 || p[0x118]>51)return 0;
    uintptr_t group=blink_ptr(world+0xde40+(uintptr_t)p[0x118]*8);unsigned char g[17];
    if(!group || !blink_read(group,g,sizeof(g)) || *(uintptr_t*)g!=blink_base+0x129fec8 || g[16]<1 || g[16]>51)return 0;
    *out=(S14BlinkArmy){world,at,{(uint16_t)id,leader,serial,raw[0x1b],g[16]},tile,troops};return 1;
}
static int blink_again(const S14BlinkArmy *old,S14BlinkArmy *out){return blink_world_now()==old->world && blink_identity(old->world,(void*)old->army,out) && blink_key_same(&old->key,&out->key);}
static int blink_officer(uintptr_t world){
    if(blink_leader_scanned && blink_world==world)return blink_leader;
    int found=0,count=0;
    for(int id=1;id<=6000;id++){uintptr_t person=blink_ptr(world+0x148+(uintptr_t)id*8);unsigned char p[72];
        if(!person || !blink_read(person,p,sizeof(p)) || *(uintptr_t*)p!=blink_base+0x12a00d0 || blink_word(person+0x10)!=id)continue;
        wchar_t name[19]={0};int at=0;for(int part=0;part<2;part++)for(int i=0;i<9;i++){wchar_t c;memcpy(&c,p+0x12+18*part+2*i,2);if(!c)break;name[at++]=c;}
        if(!wcscmp(name,L"黄忠")){found=id;count++;}
    }
    blink_leader=count==1 && found==S14_BLINK_OFFICER?found:0;blink_leader_scanned=1;blink_log("officer_resolved",0,0,0,0,blink_leader);return blink_leader;
}
static int blink_active(void){return blink_ready && blink_enabled && !blink_loading && !blink_fault;}
static void blink_bind(uintptr_t world){
    if(blink_world==world)return;
    AcquireSRWLockExclusive(&blink_lock);memset(&blink_state,0,sizeof(blink_state));memset(blink_pending,0,sizeof(blink_pending));memset(blink_reserved,0,sizeof(blink_reserved));blink_world=world;blink_leader_scanned=0;blink_epoch++;ReleaseSRWLockExclusive(&blink_lock);
}
static int blink_pair(const S14BlinkArmy *a,const S14BlinkArmy *b){
    if(a->world!=b->world || a->key.id==b->key.id || a->key.force==b->key.force)return 0;
    return blink_enemy(a->key.force,b->key.force)!=0;
}
static int blink_hex(uintptr_t world,int tile,unsigned short *occupant,unsigned short *other){
    if(tile<0 || tile>=BLINK_NO_TILE)return 0;uintptr_t hex=blink_ptr(world+0xdfe0+(uintptr_t)tile*8);unsigned char h[32];
    if(!hex || !blink_read(hex,h,sizeof(h)) || *(uintptr_t*)h!=blink_base+0x129f660 || h[0x10]>30)return 0;
    memcpy(other,h+0x1a,2);memcpy(occupant,h+0x1c,2);return 1;
}
static int blink_occupied(const S14BlinkArmy *a){unsigned short occupant,other;return blink_hex(a->world,a->tile,&occupant,&other) && occupant==a->key.id;}
static int blink_land(const S14BlinkArmy *a){int formation=blink_formation((void*)a->army);return formation>=1 && formation<=10;}
static int blink_rear_contains(int target,int direction,int source){int t=target;for(int k=0;k<BLINK_REAR;k++){t=blink_neighbor(t,direction);if(t<0 || t>=BLINK_NO_TILE)return 0;if(t==source)return 1;}return 0;}
static int blink_generation(unsigned *generation){uintptr_t state=blink_ptr(blink_base+0x2486000);unsigned g=0;return state && blink_ptr(state)==blink_base+0x12cc760 && blink_ptr(blink_base+0x2486008)==blink_world_now() && blink_read(blink_base+0x2486014,&g,4) && (*generation=g,1);}
static int blink_damage_caller(uintptr_t caller){return caller==blink_base+0x2578d76 || caller==blink_base+0x2579004;}
int s14_blink_hit_begin(int type,int id,int amount,int source_type,int source_id,uintptr_t caller,S14BlinkHit *hit){
    memset(hit,0,sizeof(*hit));if(!blink_active() || type!=27 || source_type!=27 || id<1 || id>=BLINK_UNITS || source_id<1 || source_id>=BLINK_UNITS || amount<=0 || !blink_damage_caller(caller))return amount;
    uintptr_t world=blink_world_now();if(!world)return amount;blink_bind(world);
    S14BlinkArmy a,b;if(!blink_identity(world,(void*)blink_ptr(world+0x7df60+(uintptr_t)source_id*8),&a) || !blink_identity(world,(void*)blink_ptr(world+0x7df60+(uintptr_t)id*8),&b) || a.key.leader!=blink_officer(world) || !blink_pair(&a,&b) || !blink_occupied(&a) || !blink_occupied(&b) || !blink_land(&a) || !blink_land(&b))return amount;
    unsigned generation=0;int day=blink_day(world);if(day<0 || !blink_generation(&generation))return amount;
    *hit=(S14BlinkHit){a,b,blink_epoch,generation,day,1,0};
    AcquireSRWLockExclusive(&blink_lock);BlinkRecord *r=&blink_state.units[source_id];
    if(!blink_key_same(&r->source,&a.key)){memset(r,0,sizeof(*r));r->source=a.key;blink_reserved[source_id]=0;}
    if(r->opportunity && (day>=r->expires || !blink_key_same(&r->target,&b.key) || r->target_tile!=b.tile)){
        /* An attack against another target must not erase a still valid
           opportunity. Expiry and original target relocation are separate. */
        if(day>=r->expires)r->opportunity=0;
    }
    if(r->opportunity && !blink_reserved[source_id] && day<r->expires && blink_key_same(&r->target,&b.key) && r->target_tile==b.tile && blink_rear_contains(b.tile,r->direction,a.tile)){
        int64_t raised=((int64_t)amount*(100+BLINK_BONUS)+99)/100;
        if(raised<=INT_MAX){amount=(int)raised;hit->boosted=1;blink_reserved[source_id]=1;}
    }
    ReleaseSRWLockExclusive(&blink_lock);return amount;
}
void s14_blink_hit_end(const S14BlinkHit *hit){
    if(!hit->active)return;S14BlinkArmy a,b;int valid=blink_active() && hit->epoch==blink_epoch && blink_again(&hit->source,&a) && blink_again(&hit->target,&b);
    /* A defeated target may no longer pass live identity. It still consumed
       this hit if the same canonical object/serial now has zero soldiers. */
    int loss=valid && b.troops<hit->target.troops;
    if(!valid && hit->boosted && blink_world_now()==hit->target.world && blink_ptr(hit->target.world+0x7df60+(uintptr_t)hit->target.key.id*8)==hit->target.army && blink_word(hit->target.army+0x12)==hit->target.key.leader && blink_word(hit->target.army+0x28)==hit->target.key.serial && blink_word(hit->target.army+0x16)<hit->target.troops)loss=1;
    AcquireSRWLockExclusive(&blink_lock);
    int id=hit->source.key.id;BlinkRecord *r=&blink_state.units[id];
    if(hit->boosted){blink_reserved[id]=0;if(loss && hit->epoch==blink_epoch && blink_key_same(&r->source,&hit->source.key)){r->opportunity=0;blink_log("backstab",id,hit->target.key.id,hit->source.tile,hit->target.tile,BLINK_BONUS);}}
    if(valid && loss && !hit->boosted && !r->opportunity && hit->day>=r->cooldown && !blink_pending[id].active && a.tile==hit->source.tile && b.tile==hit->target.tile){
        int d=blink_original_direction(a.tile,b.tile);
        if(d>=1 && d<=6 && blink_neighbor(a.tile,d)==b.tile)blink_pending[id]=(BlinkPending){a,b,hit->generation,hit->epoch,GetCurrentThreadId(),hit->day,1};
    }
    ReleaseSRWLockExclusive(&blink_lock);
}
static int blink_direction_hook(int from,int to){
    DWORD error=GetLastError();int result=blink_original_direction(from,to);DWORD after=GetLastError();(void)error;
    if(!result && blink_direction_scope.active && BLINK_DIRECTION_CALLER==blink_base+0x2a98f9 && from==blink_direction_scope.from && to==blink_direction_scope.to)result=blink_direction_scope.direction;
    SetLastError(after);return result;
}
static int blink_choose(const S14BlinkArmy *a,const S14BlinkArmy *b,int *direction){
    int d=blink_original_direction(a->tile,b->tile);if(d<1 || d>6 || blink_neighbor(a->tile,d)!=b->tile)return -1;
    int tile=b->tile;for(int k=0;k<BLINK_REAR;k++){
        tile=blink_neighbor(tile,d);if(tile<0 || tile>=BLINK_NO_TILE)break;
        unsigned short occupant,other;if(!blink_hex(a->world,tile,&occupant,&other))return -1;
        if(tile!=a->tile && tile!=b->tile && !occupant && !other && blink_pass(tile,1,(void*)a->army,0)){*direction=d;return tile;}
    }return -1;
}
static int blink_position_ok(const S14BlinkArmy *a,const S14BlinkArmy *b,int old,int destination){
    S14BlinkArmy now,target;unsigned short occupant,other;
    return blink_again(a,&now) && now.tile==destination && blink_again(b,&target) && target.tile==b->tile && blink_occupied(&now) && blink_occupied(&target) && blink_hex(a->world,old,&occupant,&other) && !occupant;
}
static void blink_service_one(const BlinkPending *p){
    S14BlinkArmy a,b;unsigned generation=0;
    if(!blink_active() || p->epoch!=blink_epoch || p->thread!=GetCurrentThreadId() || !blink_generation(&generation) || generation!=p->generation || !blink_again(&p->source,&a) || !blink_again(&p->target,&b) || a.tile!=p->source.tile || b.tile!=p->target.tile || a.key.leader!=blink_officer(a.world) || !blink_pair(&a,&b) || !blink_land(&a) || !blink_land(&b) || !blink_occupied(&a) || !blink_occupied(&b))return;
    int day=blink_day(a.world),direction=0,destination=blink_choose(&a,&b,&direction);if(day<0 || destination<0)return;
    AcquireSRWLockShared(&blink_lock);BlinkRecord r=blink_state.units[a.key.id];ReleaseSRWLockShared(&blink_lock);
    if(r.opportunity || day<r.cooldown)return;
    /* Final landing/object recheck before the first mutation. No native
       callbacks occur between this validation and temporary destination. */
    S14BlinkArmy again,target;unsigned short occupant,other;
    if(!blink_active() || !blink_again(&a,&again) || again.tile!=a.tile || !blink_again(&b,&target) || target.tile!=b.tile || !blink_occupied(&a) || !blink_occupied(&b) || !blink_hex(a.world,destination,&occupant,&other) || occupant || other)return;
    unsigned short saved=blink_word(a.army+0x48);
    blink_destination((void*)a.army,destination);blink_clear(a.tile);
    blink_direction_scope.active=1;blink_direction_scope.from=a.tile;blink_direction_scope.to=destination;blink_direction_scope.direction=direction;
    blink_move((void*)a.army,0);blink_direction_scope.active=0;
    if(!blink_position_ok(&a,&b,a.tile,destination)){
        /* Do not retry partial moves. No promise of full native rollback. */
        if(blink_again(&a,&again) && again.tile==a.tile){
            uintptr_t hex=blink_ptr(a.world+0xdfe0+(uintptr_t)a.tile*8);
            if(blink_hex(a.world,a.tile,&occupant,&other) && !occupant)blink_occupy((void*)hex,a.key.id);
            blink_destination((void*)a.army,saved);
        }
        InterlockedExchange(&blink_fault,1);blink_log("move_fault",a.key.id,b.key.id,a.tile,destination,1);return;
    }
    blink_destination((void*)a.army,BLINK_NO_TILE);blink_path((void*)a.army,0,1,0);
    if(!blink_position_ok(&a,&b,a.tile,destination)){InterlockedExchange(&blink_fault,1);blink_log("move_fault",a.key.id,b.key.id,a.tile,destination,2);return;}
    AcquireSRWLockExclusive(&blink_lock);BlinkRecord *record=&blink_state.units[a.key.id];
    if(p->epoch==blink_epoch){*record=(BlinkRecord){a.key,b.key,day+BLINK_DAYS,day+BLINK_DAYS,b.tile,direction,blink_active()?1:0};}
    ReleaseSRWLockExclusive(&blink_lock);blink_log("landed",a.key.id,b.key.id,a.tile,destination,direction);
}
static void blink_batch_hook(void){
    DWORD error=GetLastError();blink_batch_depth++;SetLastError(error);blink_original_batch();DWORD after=GetLastError();
    if(blink_batch_depth==1 && blink_active()){
        int day=blink_world?blink_day(blink_world):-1;
        for(int i=1;i<BLINK_UNITS;i++){BlinkPending p;AcquireSRWLockExclusive(&blink_lock);p=blink_pending[i];blink_pending[i].active=0;
            BlinkRecord *r=&blink_state.units[i];
            if(r->opportunity){S14BlinkArmy a,b;int valid=blink_world_now()==blink_world && blink_identity(blink_world,(void*)blink_ptr(blink_world+0x7df60+(uintptr_t)i*8),&a) && blink_key_same(&r->source,&a.key) && blink_identity(blink_world,(void*)blink_ptr(blink_world+0x7df60+(uintptr_t)r->target.id*8),&b) && blink_key_same(&r->target,&b.key) && b.tile==r->target_tile;
                if(day<0 || day>=r->expires || !valid)r->opportunity=0;}
            ReleaseSRWLockExclusive(&blink_lock);if(p.active)blink_service_one(&p);}
    }
    blink_batch_depth--;SetLastError(after);
}
int s14_blink_ready(void){return blink_ready!=0;}
int s14_blink_faulted(void){return blink_fault!=0;}
void s14_blink_configure(int enabled){
    int active=enabled && blink_ready;LONG before=InterlockedExchange(&blink_enabled,active);
    if(before && !active){AcquireSRWLockExclusive(&blink_lock);memset(blink_pending,0,sizeof(blink_pending));memset(blink_reserved,0,sizeof(blink_reserved));for(int i=1;i<BLINK_UNITS;i++)blink_state.units[i].opportunity=0;ReleaseSRWLockExclusive(&blink_lock);}
}
void s14_blink_forget(void *army){
    uintptr_t world=blink_world_now(),pool=blink_ptr(world+0x7df60),at=(uintptr_t)army;
    if(!pool || at<=pool || (at-pool)%512 || (at-pool)/512>=BLINK_UNITS)return;
    /* Called after removal; refusal leaves the still-active unit intact. */
    unsigned char active=1;blink_read(at+0x10,&active,1);if(active==1)return;
    int id=(int)((at-pool)/512);AcquireSRWLockExclusive(&blink_lock);if(world==blink_world){memset(&blink_state.units[id],0,sizeof(BlinkRecord));blink_pending[id].active=0;blink_reserved[id]=0;for(int i=1;i<BLINK_UNITS;i++)if(blink_state.units[i].target.id==id)blink_state.units[i].opportunity=0;}ReleaseSRWLockExclusive(&blink_lock);
}
void s14_blink_created(void *army){
    if(!blink_ready)return;S14BlinkArmy a;uintptr_t world=blink_world_now();if(!blink_identity(world,army,&a))return;
    AcquireSRWLockExclusive(&blink_lock);if(world==blink_world){memset(&blink_state.units[a.key.id],0,sizeof(BlinkRecord));blink_pending[a.key.id].active=0;blink_reserved[a.key.id]=0;for(int i=1;i<BLINK_UNITS;i++)if(blink_state.units[i].target.id==a.key.id)blink_state.units[i].opportunity=0;}ReleaseSRWLockExclusive(&blink_lock);
}
int s14_blink_name(void *army,const wchar_t *native,wchar_t *out,size_t cap){
    if(!blink_active() || !native || cap<4)return 0;S14BlinkArmy a;uintptr_t world=blink_world_now();
    if(!blink_identity(world,army,&a))return 0;blink_bind(world);if(a.key.leader!=blink_officer(world))return 0;
    int n=swprintf(out,cap,L"闪 %ls",native);return n>0 && (size_t)n<cap;
}
void s14_blink_status(wchar_t *out,size_t cap){
    const wchar_t *text=!blink_ready?L"当前 EXE 接口不兼容":blink_fault?L"闪袭异常停用，请查看日志":!blink_enabled?L"默认关闭 · 仅黄忠主将部队":!blink_leader_scanned?L"等待黄忠部队首次交战 · 实机待验收":!blink_leader?L"未找到唯一的黄忠原始武将记录":L"仅黄忠主将 · 后方 1→2→3 格 · 一次直击背刺 +30%";
    swprintf(out,cap,L"%ls",text);
}
static uint64_t blink_fnv(const unsigned char *p,size_t n){uint64_t v=UINT64_C(14695981039346656037);for(size_t i=0;i<n;i++){v^=p[i];v*=UINT64_C(1099511628211);}return v;}
int s14_blink_install(uintptr_t base,uintptr_t end,const wchar_t *root){
    if(!base || end<=base || blink_ready || !root || wcslen(root)>=MAX_PATH)return 0;
    for(size_t i=0;i<sizeof(blink_anchors)/sizeof(blink_anchors[0]);i++){const BlinkAnchor *a=&blink_anchors[i];unsigned char bytes[768];
        if(a->size>sizeof(bytes) || a->rva>end-base || a->size>end-base-a->rva || !blink_read(base+a->rva,bytes,a->size) || blink_fnv(bytes,a->size)!=a->hash)return 0;
    }
    blink_base=base;blink_end=end;wcscpy(blink_root,root);
    blink_neighbor=(BlinkNeighbor)(base+0x279eb0);blink_pass=(BlinkPass)(base+0x296420);blink_force=(BlinkForce)(base+0x20a280);blink_enemy=(BlinkEnemy)(base+0x299bc0);blink_formation=(BlinkFormation)(base+0x208bb0);
    blink_destination=(BlinkDestination)(base+0x2bb4d0);blink_clear=(BlinkClear)(base+0x25a3e0);blink_move=(BlinkMove)(base+0x257b2e0);blink_path=(BlinkPath)(base+0x2ccd50);
    blink_occupy=(void(*)(void*,int))(base+0x21d720);
    if(MH_CreateHook((void*)(base+0x280570),blink_direction_hook,(void**)&blink_original_direction)!=MH_OK)return 0;
    if(MH_CreateHook((void*)(base+0x2482466),blink_batch_hook,(void**)&blink_original_batch)!=MH_OK){MH_RemoveHook((void*)(base+0x280570));return 0;}
    InterlockedExchange(&blink_ready,1);return 1;
}

/* Successful native saves own this small, versioned sidecar. Pointer-free
   keys are rebound to canonical objects after a load; pending intents are
   never serialized. Missing legacy data starts with no cooldown or token. */
typedef struct {char magic[16];uint32_t version,bytes;unsigned char hash[32],digest[32];BlinkState state;} BlinkCheckpoint;
static int blink_digest(const void *p,size_t n,unsigned char out[32]){BCRYPT_ALG_HANDLE alg=NULL;int ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0;ok=ok && BCryptHash(alg,NULL,0,(PUCHAR)p,(ULONG)n,out,32)>=0;if(alg)BCryptCloseAlgorithmProvider(alg,0);return ok;}
static int blink_folder(const wchar_t *path){DWORD a=GetFileAttributesW(path);if(a==INVALID_FILE_ATTRIBUTES){if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;a=GetFileAttributesW(path);}return (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT);}
static int blink_file(const unsigned char *hash,wchar_t out[MAX_PATH],int create){
    if(!hash || !blink_root[0] || wcslen(blink_root)+125>=MAX_PATH)return 0;wchar_t folder[MAX_PATH],hex[65];
    swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager",blink_root);if(create && !blink_folder(folder))return 0;
    swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager\\blink-backstab",blink_root);if(create && !blink_folder(folder))return 0;
    for(int i=0;i<32;i++)swprintf(hex+i*2,3,L"%02x",hash[i]);return swprintf(out,MAX_PATH,L"%ls\\%ls.s14blink",folder,hex)>0;
}
static int blink_valid_key(const S14BlinkKey *k,int id){return k->id==id && k->leader>=1 && k->leader<=6000 && k->carrier>=1 && k->carrier<=20 && k->force>=1 && k->force<=51;}
static int blink_state_valid(const BlinkState *s){
    BlinkRecord empty={0};if(memcmp(&s->units[0],&empty,sizeof(empty)))return 0;
    for(int i=1;i<BLINK_UNITS;i++){const BlinkRecord *r=&s->units[i];if(!r->source.id){if(memcmp(r,&empty,sizeof(empty)))return 0;continue;}
        if(!blink_valid_key(&r->source,i) || r->cooldown<0 || r->expires<0 || r->opportunity<0 || r->opportunity>1)return 0;
        if(r->target.id && (!blink_valid_key(&r->target,r->target.id) || r->target.id>=BLINK_UNITS || r->target.id==i || r->target_tile<0 || r->target_tile>=BLINK_NO_TILE || r->direction<1 || r->direction>6))return 0;
        if(!r->target.id){S14BlinkKey zero={0};if(memcmp(&r->target,&zero,sizeof(zero)) || r->direction || r->expires || r->target_tile)return 0;}
        if(r->opportunity && (!r->target.id || !r->expires))return 0;
    }return 1;
}
static int blink_checkpoint_read(const wchar_t *path,const unsigned char *hash,BlinkCheckpoint *c){
    DWORD a=GetFileAttributesW(path);if(a==INVALID_FILE_ATTRIBUTES)return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND?0:-1;
    if(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return -1;HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return -1;
    LARGE_INTEGER size;DWORD got=0;unsigned char digest[32];int ok=GetFileSizeEx(f,&size) && size.QuadPart==sizeof(*c) && ReadFile(f,c,sizeof(*c),&got,NULL) && got==sizeof(*c);CloseHandle(f);
    return ok && !memcmp(c->magic,"S14-BLINK.v1",12) && c->version==1 && c->bytes==sizeof(*c) && !memcmp(c->hash,hash,32) && blink_digest(&c->state,sizeof(c->state),digest) && !memcmp(c->digest,digest,32) && blink_state_valid(&c->state)?1:-1;
}
int s14_blink_checkpoint_owned(const wchar_t *path){
    if(!path)return 0;const wchar_t *leaf=wcsrchr(path,L'\\');leaf=leaf?leaf+1:path;if(wcslen(leaf)!=73 || wcscmp(leaf+64,L".s14blink"))return 0;
    unsigned char hash[32];for(int i=0;i<32;i++){int v=0;for(int j=0;j<2;j++){wchar_t c=leaf[2*i+j];int n=c>=L'0' && c<=L'9'?c-L'0':c>=L'a' && c<=L'f'?c-L'a'+10:-1;if(n<0)return 0;v=16*v+n;}hash[i]=(unsigned char)v;}
    BlinkCheckpoint *c=malloc(sizeof(*c));int owned=c && blink_checkpoint_read(path,hash,c)==1;free(c);return owned;
}
int s14_blink_save(uintptr_t world,const unsigned char *hash){
    if(!blink_ready || blink_loading || blink_fault)return 0;
    if(!hash){if(blink_enabled){InterlockedExchange(&blink_fault,1);blink_log("save_fault",0,0,0,0,1);}return 0;}
    BlinkCheckpoint *c=calloc(1,sizeof(*c)),*old=malloc(sizeof(*old));wchar_t path[MAX_PATH],temp[MAX_PATH];
    if(!c || !old || !blink_file(hash,path,1)){free(c);free(old);InterlockedExchange(&blink_fault,1);blink_log("save_fault",0,0,0,0,2);return 0;}
    AcquireSRWLockShared(&blink_lock);int ok=blink_world==world || !blink_world;c->state=blink_state;ReleaseSRWLockShared(&blink_lock);
    memcpy(c->magic,"S14-BLINK.v1",12);c->version=1;c->bytes=sizeof(*c);memcpy(c->hash,hash,32);int prior=blink_checkpoint_read(path,hash,old);
    ok=ok && prior>=0 && blink_state_valid(&c->state) && blink_digest(&c->state,sizeof(c->state),c->digest) && swprintf(temp,MAX_PATH,L"%ls.tmp.%lu",path,GetCurrentProcessId())>0;
    HANDLE f=ok?CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL):INVALID_HANDLE_VALUE;DWORD got=0;
    ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,c,sizeof(*c),&got,NULL) && got==sizeof(*c) && FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok && f!=INVALID_HANDLE_VALUE)DeleteFileW(temp);
    if(!ok)InterlockedExchange(&blink_fault,1);free(c);free(old);blink_log(ok?"saved":"save_fault",0,0,0,0,0);return ok;
}
void s14_blink_load_begin(void){
    InterlockedExchange(&blink_loading,1);AcquireSRWLockExclusive(&blink_lock);blink_backup=blink_state;blink_backup_world=blink_world;blink_backup_fault=blink_fault;memset(blink_pending,0,sizeof(blink_pending));memset(blink_reserved,0,sizeof(blink_reserved));blink_epoch++;ReleaseSRWLockExclusive(&blink_lock);
}
void s14_blink_load_end(uintptr_t world,int success,const unsigned char *hash,int valid){
    if(!blink_ready){InterlockedExchange(&blink_loading,0);return;}BlinkCheckpoint *c=malloc(sizeof(*c));wchar_t path[MAX_PATH];int restored=valid && c && blink_file(hash,path,0)?blink_checkpoint_read(path,hash,c):-1;
    AcquireSRWLockExclusive(&blink_lock);if(!success){blink_state=blink_backup;blink_world=blink_backup_world;blink_fault=blink_backup_fault;}else{
        blink_world=world;blink_fault=restored<0;memset(&blink_state,0,sizeof(blink_state));if(restored==1)blink_state=c->state;
    }blink_leader_scanned=0;memset(blink_pending,0,sizeof(blink_pending));memset(blink_reserved,0,sizeof(blink_reserved));blink_epoch++;ReleaseSRWLockExclusive(&blink_lock);
    free(c);InterlockedExchange(&blink_loading,0);blink_log(!success?"load_cancelled":restored==1?"restored":restored==0?"legacy_empty":"sidecar_fault",0,0,0,0,restored);
}
void s14_blink_new_game(uintptr_t world){
    AcquireSRWLockExclusive(&blink_lock);memset(&blink_state,0,sizeof(blink_state));memset(blink_pending,0,sizeof(blink_pending));memset(blink_reserved,0,sizeof(blink_reserved));blink_world=world;blink_leader_scanned=0;blink_fault=0;blink_epoch++;ReleaseSRWLockExclusive(&blink_lock);InterlockedExchange(&blink_loading,0);
}
