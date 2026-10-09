// Separate versioned sidecar: existing .s14career files and counters stay intact.
#include "special_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stddef.h>
typedef struct {char magic[16];unsigned int version,bytes,crc;unsigned char hash[32];S14SpecialSnapshot snapshot;} Checkpoint;
static S14SpecialSnapshot data;
static wchar_t storage[MAX_PATH];
static int initialized;
static unsigned int crc(const unsigned char *p,size_t n){unsigned int c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
static int directory(const wchar_t *p,int create){
    if(create && !CreateDirectoryW(p,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    DWORD a=GetFileAttributesW(p);return a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT);
}
static int path_for(const unsigned char hash[32],wchar_t out[MAX_PATH],int create){
    if(!storage[0] || wcslen(storage)+115>=MAX_PATH)return 0;
    wchar_t folder[MAX_PATH];wcscpy(folder,storage);
    // Check every existing path component to avoid following junctions.
    for(wchar_t *p=folder+3;;p++){if(*p && *p!=L'\\')continue;wchar_t c=*p;*p=0;int ok=directory(folder,0);*p=c;if(!ok)return 0;if(!c)break;}
    const wchar_t *parts[]={L"\\SAN14ModManager",L"\\career",L"\\checkpoints"};
    for(int i=0;i<3;i++){wcscat(folder,parts[i]);if(!directory(folder,create))return 0;}
    wchar_t hex[65];for(int i=0;i<32;i++)swprintf(hex+i*2,3,L"%02x",hash[i]);
    swprintf(out,MAX_PATH,L"%ls\\%ls.s14special",folder,hex);return 1;
}
static int read_file(const wchar_t *path,Checkpoint *p){
    DWORD a=GetFileAttributesW(path);if(a==INVALID_FILE_ATTRIBUTES || (a&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))return 0;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;
    LARGE_INTEGER n;DWORD got=0;int ok=GetFileSizeEx(f,&n) && n.QuadPart==sizeof(*p) && ReadFile(f,p,sizeof(*p),&got,NULL) && got==sizeof(*p);CloseHandle(f);
    if(!ok || memcmp(p->magic,"S14SPECIAL.v1",13) || p->version!=1 || p->bytes!=sizeof(*p) || p->snapshot.version!=1 || p->snapshot.count>S14_SPECIAL_HISTORY)return 0;
    if(p->crc!=crc((unsigned char*)&p->hash,sizeof(*p)-offsetof(Checkpoint,hash)))return 0;
    wchar_t expected[90];for(int i=0;i<32;i++)swprintf(expected+i*2,3,L"%02x",p->hash[i]);wcscat(expected,L".s14special");
    const wchar_t *leaf=wcsrchr(path,L'\\');leaf=leaf?leaf+1:path;if(!_wcsicmp(expected,leaf))return 1;wcscat(expected,L".tmp");return !_wcsicmp(expected,leaf);
}
int s14_special_checkpoint_owned(const wchar_t *path){Checkpoint *p=malloc(sizeof(*p));if(!p)return 0;int ok=read_file(path,p);free(p);return ok;}
void s14_special_root(const wchar_t *root){if(root && wcslen(root)<MAX_PATH)wcscpy(storage,root);}
void s14_special_gap(void){if(initialized)data.incomplete=1;}
void s14_special_begin(uintptr_t world,int day){
    if(!world || day<0)return;
    if(!initialized || data.world!=world || day<data.last_day)s14_special_load(world,day,1,NULL,0,1);
    else data.last_day=day;
}
void s14_special_load(uintptr_t world,int day,int success,const unsigned char hash[32],int valid,int fresh){
    if(!success)return;Checkpoint *p=malloc(sizeof(*p));wchar_t path[MAX_PATH];
    int found=!fresh && valid && p && path_for(hash,path,0) && read_file(path,p) && !memcmp(p->hash,hash,32);
    if(found){data=p->snapshot;data.world=world;data.last_day=day;data.bound=1;for(unsigned int i=0;i<data.count;i++)data.events[i].id=0;}
    else{memset(&data,0,sizeof(data));data.version=1;data.world=world;data.start_day=data.last_day=day;}
    free(p);initialized=1;
}
int s14_special_save(uintptr_t world,const unsigned char hash[32],int valid){
    if(!initialized || data.world!=world || !valid)return 0;
    wchar_t path[MAX_PATH],temp[MAX_PATH];if(!path_for(hash,path,1))return 0;
    DWORD a=GetFileAttributesW(path);if(a!=INVALID_FILE_ATTRIBUTES && !s14_special_checkpoint_owned(path))return 0;
    swprintf(temp,MAX_PATH,L"%ls.tmp",path);if(GetFileAttributesW(temp)!=INVALID_FILE_ATTRIBUTES && (!s14_special_checkpoint_owned(temp)||!DeleteFileW(temp)))return 0;
    Checkpoint *p=calloc(1,sizeof(*p));if(!p)return 0;memcpy(p->magic,"S14SPECIAL.v1",13);p->version=1;p->bytes=sizeof(*p);memcpy(p->hash,hash,32);p->snapshot=data;p->snapshot.bound=1;
    p->crc=crc((unsigned char*)&p->hash,sizeof(*p)-offsetof(Checkpoint,hash));
    HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD n=0;
    int ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,p,sizeof(*p),&n,NULL) && n==sizeof(*p) && FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);free(p);
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temp);else data.bound=1;return ok;
}
void s14_special_consume(uintptr_t world,const S14SpecialEvent *event){
    if(!event || !event->id || (event->kind!=S14_SPECIAL_DUEL && event->kind!=S14_SPECIAL_CAPTURE))return;
    if(!initialized || data.world!=world || event->day<data.last_day)s14_special_load(world,event->day,1,NULL,0,1);
    if(event->target<1 || event->target>=S14_SPECIAL_OFFICERS)return;
    // IDs are session-local; restored observations use zero so a new session cannot collide.
    for(unsigned int i=data.count;i>0;i--)if(data.events[i-1].id==event->id)return;
    int actor=event->actor>0 && event->actor<S14_SPECIAL_OFFICERS?event->actor:0;
    int prior=0;if(actor && event->kind==S14_SPECIAL_CAPTURE)for(unsigned int i=0;i<data.count;i++){
        const S14SpecialEvent *e=&data.events[i];if(e->verified && e->actor_role==S14_SPECIAL_ACTOR_PERSON && e->actor_force>0 && e->target_force>0 && e->actor_force!=e->target_force && e->kind==S14_SPECIAL_CAPTURE && e->actor==actor && e->target==event->target)prior=1;
    }
    data.last_day=event->day;data.bound=0;
    if(data.count<S14_SPECIAL_HISTORY){data.events[data.count]=*event;data.events[data.count].actor=actor;data.events[data.count].actor_name[31]=data.events[data.count].target_name[31]=0;data.count++;}
    else{data.truncated=1;data.incomplete=1;return;} // Never claim complete lists/counters after bounded capacity is exceeded.
    if(!event->verified && event->kind==S14_SPECIAL_DUEL)data.incomplete=1;
    // Static candidates must never enter career totals before in-game semantic validation.
    if(!event->verified || !actor || actor==event->target || event->actor_force<=0 || event->target_force<=0 || event->actor_force==event->target_force)return;
    S14SpecialTotals *a=&data.rows[actor],*t=&data.rows[event->target];
    if(event->kind==S14_SPECIAL_DUEL && event->actor_role==S14_SPECIAL_ACTOR_PERSON && (event->outcome==0 || event->outcome==1)){
        a->valid_mask|=S14_SPECIAL_DUEL;t->valid_mask|=S14_SPECIAL_DUEL;a->duels++;t->duels++;
        if(event->outcome==0){a->wins++;t->losses++;}else{a->losses++;t->wins++;}
    }else if(event->kind==S14_SPECIAL_CAPTURE && event->actor_role==S14_SPECIAL_ACTOR_PERSON){
        a->valid_mask|=S14_SPECIAL_CAPTURE;t->valid_mask|=S14_SPECIAL_CAPTURE;a->captures++;t->captured++;if(!prior)a->unique_captives++;
    }
}
int s14_special_snapshot(uintptr_t world,S14SpecialSnapshot *out){if(!out || !initialized || data.world!=world)return 0;*out=data;return 1;}
