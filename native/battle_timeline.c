#include "battle_timeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
typedef struct {char magic[16];unsigned int version,bytes,crc;unsigned char hash[32];S14TimelineSnapshot snapshot;} Checkpoint;
#define MIN_BYTES (offsetof(Checkpoint,snapshot)+offsetof(S14TimelineSnapshot,events))
static S14TimelineSnapshot data;
static wchar_t storage[MAX_PATH];static int initialized,restored;
static unsigned int crc(const unsigned char *p,size_t n){unsigned int c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
static int directory(const wchar_t *p,int make){if(make && !CreateDirectoryW(p,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;DWORD a=GetFileAttributesW(p);return a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT);}
static int path_for(const unsigned char hash[32],wchar_t out[MAX_PATH],int make){
    if(!storage[0] || wcslen(storage)+115>=MAX_PATH)return 0;wchar_t folder[MAX_PATH];wcscpy(folder,storage);
    for(wchar_t *q=folder+3;;q++){if(*q && *q!=L'\\')continue;wchar_t c=*q;*q=0;int ok=directory(folder,0);*q=c;if(!ok)return 0;if(!c)break;}
    const wchar_t *parts[]={L"\\SAN14ModManager",L"\\career",L"\\checkpoints"};for(int i=0;i<3;i++){wcscat(folder,parts[i]);if(!directory(folder,make))return 0;}
    wchar_t hex[65];for(int i=0;i<32;i++)swprintf(hex+i*2,3,L"%02x",hash[i]);swprintf(out,MAX_PATH,L"%ls\\%ls.s14timeline",folder,hex);return 1;
}
static int read_file(const wchar_t *path,Checkpoint *p){
    DWORD a=GetFileAttributesW(path);if(a==INVALID_FILE_ATTRIBUTES || (a&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))return 0;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;
    LARGE_INTEGER n;DWORD got=0;memset(p,0,sizeof(*p));int ok=GetFileSizeEx(f,&n) && n.QuadPart>=MIN_BYTES && n.QuadPart<=sizeof(*p) && ReadFile(f,p,(DWORD)n.QuadPart,&got,NULL) && got==(DWORD)n.QuadPart;CloseHandle(f);
    if(!ok || memcmp(p->magic,"S14TIMELINE.v1",14) || p->version!=1 || p->bytes!=(unsigned int)n.QuadPart || p->snapshot.version!=1 || p->snapshot.count>S14_TIMELINE_MAX || p->snapshot.first!=0 || p->bytes!=MIN_BYTES+p->snapshot.count*sizeof(S14TimelineEvent) || p->crc!=crc(p->hash,p->bytes- offsetof(Checkpoint,hash)))return 0;
    wchar_t expected[90];for(int i=0;i<32;i++)swprintf(expected+i*2,3,L"%02x",p->hash[i]);wcscat(expected,L".s14timeline");const wchar_t *leaf=wcsrchr(path,L'\\');leaf=leaf?leaf+1:path;if(!_wcsicmp(leaf,expected))return 1;wcscat(expected,L".tmp");return !_wcsicmp(leaf,expected);
}
int s14_timeline_checkpoint_owned(const wchar_t *path){Checkpoint *p=malloc(sizeof(*p));if(!p)return 0;int ok=read_file(path,p);free(p);return ok;}
void s14_timeline_root(const wchar_t *root){if(root && wcslen(root)<MAX_PATH)wcscpy(storage,root);}
void s14_timeline_gap(void){if(initialized)data.incomplete=1;}
void s14_timeline_load(uintptr_t world,int day,int success,const unsigned char hash[32],int valid,int fresh){
    if(!success)return;Checkpoint *p=malloc(sizeof(*p));wchar_t path[MAX_PATH];
    restored=!fresh && valid && p && path_for(hash,path,0) && read_file(path,p) && !memcmp(hash,p->hash,32);
    if(restored){data=p->snapshot;data.world=world;data.last_day=day;data.bound=1;for(unsigned int i=0;i<data.count;i++)data.events[(data.first+i)%S14_TIMELINE_MAX].id=0;}
    else{memset(&data,0,sizeof(data));data.version=1;data.world=world;data.start_day=data.last_day=day;}
    free(p);initialized=1;
}
void s14_timeline_begin(uintptr_t world,int day){if(!world || day<0)return;if(!initialized || data.world!=world || day<data.last_day)s14_timeline_load(world,day,1,NULL,0,1);else data.last_day=day;}
int s14_timeline_save(uintptr_t world,const unsigned char hash[32],int valid){
    if(!initialized || data.world!=world || !valid)return 0;wchar_t path[MAX_PATH],temp[MAX_PATH];if(!path_for(hash,path,1))return 0;
    DWORD a=GetFileAttributesW(path);if(a!=INVALID_FILE_ATTRIBUTES && !s14_timeline_checkpoint_owned(path))return 0;
    swprintf(temp,MAX_PATH,L"%ls.tmp",path);if(GetFileAttributesW(temp)!=INVALID_FILE_ATTRIBUTES && (!s14_timeline_checkpoint_owned(temp) || !DeleteFileW(temp)))return 0;
    Checkpoint *p=calloc(1,sizeof(*p));if(!p)return 0;memcpy(p->magic,"S14TIMELINE.v1",14);p->version=1;p->bytes=(unsigned int)(MIN_BYTES+data.count*sizeof(S14TimelineEvent));memcpy(p->hash,hash,32);p->snapshot=data;p->snapshot.bound=1;p->snapshot.first=0;
    for(unsigned int i=0;i<data.count;i++)p->snapshot.events[i]=data.events[(data.first+i)%S14_TIMELINE_MAX];p->crc=crc(p->hash,p->bytes- offsetof(Checkpoint,hash));
    HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD n=0;int ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,p,p->bytes,&n,NULL) && n==p->bytes && FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);free(p);
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok)DeleteFileW(temp);else data.bound=1;return ok;
}
static void append(const S14TimelineEvent *e){
    for(unsigned int i=0;e->id && i<data.count;i++)if(data.events[(data.first+i)%S14_TIMELINE_MAX].id==e->id)return;
    unsigned int at=(data.first+data.count)%S14_TIMELINE_MAX;if(data.count==S14_TIMELINE_MAX){data.first=(data.first+1)%S14_TIMELINE_MAX;data.truncated++;}else data.count++;
    data.events[at]=*e;S14TimelineEvent *v=&data.events[at];v->actor_name[31]=v->target_name[31]=0;v->place.city[39]=v->place.area[39]=0;
    data.last_day=e->day;data.bound=0;
}
void s14_timeline_battle(const S14RoundEvent *e,int attributed){
    if(!e || !initialized || data.world!=e->world || !e->id)return;
    int kind=e->kind==S14_ROUND_REMOVE?S14_TIMELINE_ROUT:e->kind==S14_ROUND_INJURY?S14_TIMELINE_INJURY:0;if(!kind)return;
    S14TimelineEvent v={.id=e->id,.kind=kind,.day=e->day,.actor=attributed?e->source.leader:0,.target=e->target.leader,.actor_force=e->source.force,.target_force=e->target.force,.verified=1,.actor_role=kind==S14_TIMELINE_ROUT?S14_SPECIAL_ACTOR_COMMANDER:S14_SPECIAL_ACTOR_PERSON,.outcome=e->target_after.health,.place=e->place};
    if(!v.place.basis)v.place.tile=v.place.city_id=v.place.area_id=-1;memcpy(v.clock,e->clock,6);if(attributed)memcpy(v.actor_name,e->source.name,sizeof(v.actor_name));memcpy(v.target_name,e->target.name,sizeof(v.target_name));append(&v);
}
void s14_timeline_special(uintptr_t world,const S14SpecialEvent *e,const S14BattlePlace *place){
    if(!e || !initialized || data.world!=world)return;
    S14TimelineEvent v={.id=e->id,.kind=e->kind==S14_SPECIAL_DUEL?S14_TIMELINE_DUEL:S14_TIMELINE_CAPTURE,.day=e->day,.actor=e->actor,.target=e->target,.actor_force=e->actor_force,.target_force=e->target_force,.outcome=e->outcome,.verified=e->verified,.actor_role=e->actor_role};
    memcpy(v.clock,e->clock,6);memcpy(v.actor_name,e->actor_name,sizeof(v.actor_name));memcpy(v.target_name,e->target_name,sizeof(v.target_name));if(place)v.place=*place;if(!place || !place->basis)v.place.tile=v.place.city_id=v.place.area_id=-1;
    v.verified=e->verified && e->actor_role==S14_SPECIAL_ACTOR_PERSON && e->actor>0 && e->actor<S14_SPECIAL_OFFICERS && e->target>0 && e->target<S14_SPECIAL_OFFICERS && e->actor!=e->target && e->actor_force>0 && e->target_force>0 && e->actor_force!=e->target_force && (e->kind!=S14_SPECIAL_DUEL || e->outcome==0 || e->outcome==1);append(&v);
}
void s14_timeline_import_special(const S14SpecialSnapshot *s){
    if(!s || !initialized || restored || data.count || s->version!=1 || s->world!=data.world)return;
    for(unsigned int i=0;i<s->count;i++){s14_timeline_special(data.world,&s->events[i],NULL);if(data.count)data.events[(data.first+data.count-1)%S14_TIMELINE_MAX].legacy=1;}
}
int s14_timeline_snapshot(uintptr_t world,S14TimelineSnapshot *out){if(!out || !initialized || data.world!=world)return 0;*out=data;return 1;}
int s14_timeline_matches(const S14TimelineEvent *e,int person){return person>0 && (e->actor==person || e->target==person);}
void s14_timeline_date(const S14TimelineEvent *e,wchar_t *out,size_t size){const unsigned char *c=e->clock;int year=c[0]+256*c[1];if(year>0 && c[2]>=1 && c[2]<=12 && c[3]>=1 && c[3]<=30)swprintf(out,size,L"%d 年 %d 月 %d 日",year,c[2],c[3]);else swprintf(out,size,L"回合日期 #%d",e->day);}
void s14_timeline_describe(const S14TimelineEvent *e,int person,wchar_t *out,size_t size){
    int actor=e->actor==person;const wchar_t *other=actor?e->target_name:e->actor_name;if(!other[0])other=L"来源未确认";
    if(e->kind==S14_TIMELINE_ROUT)swprintf(out,size,actor?L"击溃 %ls 部队":L"所部被 %ls 击溃",other);
    else if(e->kind==S14_TIMELINE_INJURY)swprintf(out,size,actor?L"击伤 %ls":L"被 %ls 击伤",other);
    else if(e->kind==S14_TIMELINE_DUEL)swprintf(out,size,L"与 %ls 单挑 · %ls",other,e->verified?((e->outcome==0)==actor?L"制胜":L"败北"):L"结果待核实");
    else if(e->verified)swprintf(out,size,actor?L"擒获 %ls":L"被 %ls 俘虏",other);
    else swprintf(out,size,actor?L"%ls 被俘 · 个人抓捕者待核实":L"被俘变化 · 个人抓捕者待核实",other);
}
