#include "battle_stats.h"
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <stddef.h>
#define SEEN 32768
typedef struct {int leader,routed;} UnitLife;
typedef struct {unsigned char magic[16];unsigned int version,bytes,crc;unsigned char save_hash[32];S14BattleStatsSnapshot snapshot;UnitLife life[501];} Checkpoint;
static S14BattleStatsSnapshot totals;
static UnitLife lives[501];
static uint64_t seen[SEEN];
static int loading,initialized;
static wchar_t storage_root[MAX_PATH];
static unsigned int crc(const unsigned char *p,size_t n) {
    unsigned int c=~0u;for(size_t i=0;i<n;i++) {c^=p[i];for(int b=0;b<8;b++) c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;
}
static unsigned int payload_crc(const Checkpoint *p) {return crc((const unsigned char*)&p->save_hash,sizeof(*p)-offsetof(Checkpoint,save_hash));}
static void guid(unsigned char out[16]) {if(BCryptGenRandom(NULL,out,16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) memset(out,0,16);}
static void fresh(uintptr_t world,int day) {
    memset(&totals,0,sizeof(totals));memset(lives,0,sizeof(lives));memset(seen,0,sizeof(seen));
    totals.version=1;totals.world=world;totals.start_day=totals.last_day=day;guid(totals.campaign);guid(totals.branch);initialized=1;
    for(int i=1;i<S14_STATS_MAX;i++) totals.rows[i].valid_mask=S14_STATS_DAMAGE|S14_STATS_ROUTS|S14_STATS_DEFEATS|S14_STATS_LOSSES|S14_STATS_INJURIES;
}
void s14_stats_root(const wchar_t *root) {if(root && wcslen(root)<MAX_PATH) wcscpy(storage_root,root);}
static int plain_folder(const wchar_t *path) {
    wchar_t full[MAX_PATH];DWORD n=GetFullPathNameW(path,MAX_PATH,full,NULL);if(!n || n>=MAX_PATH) return 0;
    for(wchar_t *q=full+3;;q++) {if(*q && *q!=L'\\') continue;wchar_t saved=*q;*q=0;DWORD a=GetFileAttributesW(full);*q=saved;
        if(a==INVALID_FILE_ATTRIBUTES || !(a&FILE_ATTRIBUTE_DIRECTORY) || (a&FILE_ATTRIBUTE_REPARSE_POINT)) return 0;if(!saved) break;}
    return 1;
}
static int path_for(const unsigned char hash[32],wchar_t path[MAX_PATH],int make) {
    if(!storage_root[0] || wcslen(storage_root)+109>=MAX_PATH) return 0;
    wchar_t folder[MAX_PATH];swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager",storage_root);if(!plain_folder(folder)) return 0;
    swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager\\career",storage_root);
    if(make) {DWORD a=GetFileAttributesW(folder);if(a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_REPARSE_POINT)) return 0;if(a==INVALID_FILE_ATTRIBUTES && !CreateDirectoryW(folder,NULL)) return 0;}
    if(!plain_folder(folder)) return 0;
    swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager\\career\\checkpoints",storage_root);
    DWORD attr=GetFileAttributesW(folder);if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
    if(make && attr==INVALID_FILE_ATTRIBUTES && !CreateDirectoryW(folder,NULL)) return 0;
    if(!plain_folder(folder)) return 0;
    wchar_t hex[65];for(int i=0;i<32;i++) swprintf(hex+i*2,3,L"%02x",hash[i]);
    swprintf(path,MAX_PATH,L"%ls\\%ls.s14career",folder,hex);return 1;
}
static int read_checkpoint(const wchar_t *path,Checkpoint *p) {
    DWORD attr=GetFileAttributesW(path);if(attr==INVALID_FILE_ATTRIBUTES || (attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))) return 0;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size;DWORD got=0;int ok=GetFileSizeEx(f,&size) && size.QuadPart==sizeof(*p) && ReadFile(f,p,sizeof(*p),&got,NULL) && got==sizeof(*p);CloseHandle(f);
    if(!ok || memcmp(p->magic,"S14CAREER.v1",12) || p->version!=1 || p->bytes!=sizeof(*p) || p->crc!=payload_crc(p) || p->snapshot.version!=1) return 0;
    const wchar_t *base=wcsrchr(path,L'\\');base=base?base+1:path;
    wchar_t expected[84];for(int i=0;i<32;i++) swprintf(expected+i*2,3,L"%02x",p->save_hash[i]);wcscat(expected,L".s14career");if(!_wcsicmp(base,expected)) return 1;wcscat(expected,L".tmp");return !_wcsicmp(base,expected);
}
int s14_stats_checkpoint_owned(const wchar_t *path) {Checkpoint *p=malloc(sizeof(*p));if(!p) return 0;int ok=read_checkpoint(path,p);free(p);return ok;}
void s14_stats_gap(void) {if(initialized) totals.incomplete=1;}
void s14_stats_load_begin(void) {loading=1;}
void s14_stats_load_end(uintptr_t world,int day,int success,const unsigned char hash[32],int hash_valid,int new_game) {
    loading=0;if(!success) return;
    Checkpoint *p=malloc(sizeof(*p));wchar_t path[MAX_PATH];int found=!new_game && hash_valid && p && path_for(hash,path,0) && read_checkpoint(path,p) && !memcmp(hash,p->save_hash,32);
    if(found) {totals=p->snapshot;totals.world=world;totals.last_day=day;totals.bound_to_save=1;guid(totals.branch);memcpy(lives,p->life,sizeof(lives));memset(seen,0,sizeof(seen));initialized=1;}
    else fresh(world,day);
    free(p);
}
int s14_stats_save(uintptr_t world,const unsigned char hash[32],int hash_valid) {
    if(!initialized || loading || totals.world!=world || !hash_valid) return 0;
    wchar_t path[MAX_PATH],temp[MAX_PATH];if(!path_for(hash,path,1) || wcslen(path)+4>=MAX_PATH) return 0;
    DWORD attr=GetFileAttributesW(path);if(attr!=INVALID_FILE_ATTRIBUTES && !s14_stats_checkpoint_owned(path)) return 0;
    swprintf(temp,MAX_PATH,L"%ls.tmp",path);if(GetFileAttributesW(temp)!=INVALID_FILE_ATTRIBUTES && (!s14_stats_checkpoint_owned(temp) || !DeleteFileW(temp))) return 0;
    Checkpoint *p=calloc(1,sizeof(*p));if(!p) return 0;
    memcpy(p->magic,"S14CAREER.v1",12);p->version=1;p->bytes=sizeof(*p);memcpy(p->save_hash,hash,32);p->snapshot=totals;p->snapshot.bound_to_save=1;
    memcpy(p->life,lives,sizeof(lives));p->crc=payload_crc(p);
    HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD wrote=0;
    int ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,p,sizeof(*p),&wrote,NULL) && wrote==sizeof(*p) && FlushFileBuffers(f);
    if(f!=INVALID_HANDLE_VALUE) CloseHandle(f);free(p);
    if(ok) ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) DeleteFileW(temp);else totals.bound_to_save=1;return ok;
}
static int known(const S14RoundObject *o) {return (o->kind==1 || o->kind==2) && o->active && o->leader>0 && o->leader<S14_STATS_MAX && o->force>0;}
static void touch(const S14RoundObject *o) {
    if(o->kind!=2 || o->id<1 || o->id>500) return;UnitLife *u=&lives[o->id];
    if(u->leader!=o->leader || (u->routed && o->troops>0)) {u->leader=o->leader;u->routed=0;}
}
void s14_stats_consume(const S14RoundEvent *e) {
    if(loading) return;
    if(e->kind==S14_ROUND_BEGIN || e->kind==S14_ROUND_END) {
        if(!initialized || totals.world!=e->world || e->day<totals.last_day) fresh(e->world,e->day);
        if(e->kind==S14_ROUND_BEGIN) memset(seen,0,sizeof(seen));
        totals.last_day=e->day;if(e->fault) totals.incomplete=1;return;
    }
    if(!initialized || totals.world!=e->world || e->day<totals.start_day) return;
    if(e->fault) totals.incomplete=1;
    if(!e->id) return;unsigned int slot=(unsigned int)e->id&(SEEN-1),n;
    for(n=0;n<SEEN;n++,slot=(slot+1)&(SEEN-1)) {if(seen[slot]==e->id) return;if(!seen[slot]) {seen[slot]=e->id;break;}}
    if(n==SEEN) {memset(seen,0,sizeof(seen));totals.incomplete=1;seen[(unsigned int)e->id&(SEEN-1)]=e->id;}
    touch(&e->source);touch(&e->target);
    int destroyed_identity=e->kind==S14_ROUND_REMOVE && e->target_after.kind==2 && e->target_after.id==e->target.id && !e->target_after.active;
    if(!e->stable && !destroyed_identity) return;
    int source=known(&e->source)?e->source.leader:0,target=known(&e->target)?e->target.leader:0;
    if(e->kind==S14_ROUND_DAMAGE && target && e->target.kind==2) {
        int loss=e->target.troops-e->target_after.troops;if(loss<=0) return;
        totals.rows[target].own_loss+=loss;
        if(source && e->source.force!=e->target.force) totals.rows[source].enemy_loss+=loss;
    } else if(e->kind==S14_ROUND_REMOVE && target && e->target.kind==2 && e->target.troops==0 && e->reason==1 && e->target.id>=1 && e->target.id<=500) {
        UnitLife *u=&lives[e->target.id];if(u->routed) return;u->routed=1;u->leader=target;
        if(source && e->source.kind==2 && e->source.force!=e->target.force) totals.rows[source].units_routed++;
        totals.rows[target].units_defeated++;
    } else if(e->kind==S14_ROUND_INJURY && source && target && e->source.force!=e->target.force && e->target_after.health>e->target.health && e->target.health>=0 && e->target_after.health<=3) totals.rows[source].officers_injured++;
}
int s14_stats_snapshot(void *unused,uintptr_t world,S14BattleStatsSnapshot *out) {
    (void)unused;if(!out || !initialized || loading || totals.world!=world) return 0;*out=totals;return 1;
}
int s14_stats_row(uintptr_t world,int officer,S14BattleTotals *out,int *incomplete,int *bound) {
    if(!out || !incomplete || !bound || !initialized || loading || totals.world!=world || officer<=0 || officer>=S14_STATS_MAX) return 0;
    *out=totals.rows[officer];*incomplete=totals.incomplete;*bound=totals.bound_to_save;return 1;
}
