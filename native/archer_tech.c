#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "archer_tech.h"

/* Stable officer IDs only. No native attribute or save field is overwritten. */
static uintptr_t tech_image,tech_world;
static wchar_t tech_root[MAX_PATH];
static volatile LONG tech_ready,tech_enabled,tech_loading;
static unsigned short tech_masks[6001];
static SRWLOCK tech_lock=SRWLOCK_INIT;
typedef struct {volatile LONG state;char text[768];} TechLog;
static TechLog tech_logs[64];static volatile LONG tech_sequence,tech_dropped;
static int read_tech(uintptr_t p,void *out,size_t n){SIZE_T got=0;return p&&ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got)&&got==n;}
static uintptr_t tech_ptr(uintptr_t p){uintptr_t out=0;read_tech(p,&out,8);return out;}
static uintptr_t world_now(void){return tech_image?tech_ptr(tech_image+0x1fc91d0):0;}
static void tech_log(const char *kind,int officer,int node,int attr,float before,float after){
 LONG i=InterlockedIncrement(&tech_sequence);TechLog *r=&tech_logs[(unsigned)i%64];
 if(InterlockedCompareExchange(&r->state,1,0)){InterlockedIncrement(&tech_dropped);return;}
 snprintf(r->text,sizeof(r->text),"{\"event\":\"archer_tech_%s\",\"officer_id\":%d,\"node\":%d,\"attribute\":%d,\"before\":%.9g,\"after\":%.9g,\"queue_dropped\":%ld}\n",kind,officer,node,attr,(double)before,(double)after,tech_dropped);InterlockedExchange(&r->state,2);
}
int s14_archer_tech_next_log(char *out,size_t cap){if(!out||cap<768)return 0;for(int i=0;i<64;i++)if(InterlockedCompareExchange(&tech_logs[i].state,3,2)==2){strcpy(out,tech_logs[i].text);InterlockedExchange(&tech_logs[i].state,0);return 1;}return 0;}
void s14_archer_tech_attach(uintptr_t base,const wchar_t *root,int ready){tech_image=base;if(root&&wcslen(root)<MAX_PATH)wcscpy(tech_root,root);InterlockedExchange(&tech_ready,!!ready);}
void s14_archer_tech_configure(int on){InterlockedExchange(&tech_enabled,on&&tech_ready);}
static void bind_world(uintptr_t world){if(world&&world!=tech_world){memset(tech_masks,0,sizeof(tech_masks));tech_world=world;}}
static int eligible_person(int officer,uintptr_t world){
 if(officer<1||officer>6000||!world)return 0;
 uintptr_t person=tech_ptr(world+0x148+(uintptr_t)officer*8),settings=tech_ptr(world+0x85130);
 unsigned short id=0;unsigned permission=0;unsigned char district=0,player=0,force=0;
 if(tech_ptr(person)!=tech_image+0x12a00d0||!read_tech(person+0x10,&id,2)||id!=officer||
    !read_tech(person+0x164,&permission,4)||!(permission&(1u<<3))||!read_tech(person+0x118,&district,1)||district<1||district>51||
    !read_tech(settings+0x3a,&player,1)||player<1||player>51)return 0;
 uintptr_t group=tech_ptr(world+0xde40+(uintptr_t)district*8);
 return tech_ptr(group)==tech_image+0x129fec8&&read_tech(group+0x10,&force,1)&&force==player;
}
int s14_archer_tech_can_learn(int officer){return tech_enabled&&!tech_loading&&eligible_person(officer,world_now());}
unsigned short s14_archer_tech_mask(int officer){
 if(officer<1||officer>6000||tech_loading)return 1;uintptr_t world=world_now();if(!world)return 1;
 AcquireSRWLockExclusive(&tech_lock);bind_world(world);unsigned short bits=tech_masks[officer];ReleaseSRWLockExclusive(&tech_lock);return bits?bits:1;
}
int s14_archer_tech_learn(int officer,int node){
 uintptr_t world=world_now();if(!tech_enabled||tech_loading||!eligible_person(officer,world))return 0;
 AcquireSRWLockExclusive(&tech_lock);int ok=0;if(!tech_loading&&world_now()==world){bind_world(world);S14TechRecord r={.learned=tech_masks[officer]?tech_masks[officer]:1};S14TechSession s={.record=&r};
  if(s14_tech_upgrade(&s,node)){tech_masks[officer]=r.learned;ok=1;}}
 ReleaseSRWLockExclusive(&tech_lock);if(ok)tech_log("learned",officer,node,-1,0,0);return ok;
}
static S14TechBonus live_bonus(int officer){
 S14TechBonus out={0};uintptr_t world=world_now();if(!tech_enabled||tech_loading||officer<1||officer>6000||!world)return out;
 AcquireSRWLockShared(&tech_lock);if(tech_world==world)out=s14_tech_bonus(tech_masks[officer]);ReleaseSRWLockShared(&tech_lock);return out;
}
void s14_archer_tech_ammo(int officer,int base_capacity,int base_recovery,int *cap,int *refill){
 S14TechBonus b=live_bonus(officer);*cap=base_capacity+b.capacity;if(*cap<1)*cap=1;if(*cap>1000)*cap=1000;
 /* Integer hundredths retain 1.4/day across idle days, without fractional shots. */
 *refill=base_recovery*(10000+b.recovery_bp)/100;if(*refill<0)*refill=0;
}
float s14_archer_tech_attribute(void *army,int attr,int actual,float value){
 if(actual!=1||!tech_enabled||tech_loading||!isfinite(value)||value<0||
    (attr!=S14_TROOP_ATTACK&&attr!=S14_TROOP_DEFENSE&&attr!=S14_TROOP_MOBILITY))return value;
 uintptr_t world=world_now(),pool=tech_ptr(world+0x7df60),at=(uintptr_t)army;unsigned char raw[48];unsigned short leader=0;
 if(!pool||at<=pool||(at-pool)%512||(at-pool)/512>500||tech_ptr(world+0x7df60+(at-pool)/512*8)!=at||
    !read_tech(at,raw,sizeof(raw))||tech_ptr(at)!=tech_image+0x123e288||raw[0x10]!=1||raw[0x1b]!=3)return value;
 memcpy(&leader,raw+0x12,2);S14TechBonus b=live_bonus(leader);int bp=attr==S14_TROOP_ATTACK?b.attack_bp:attr==S14_TROOP_DEFENSE?b.defense_bp:b.mobility_bp;
 float out=value*(1.f+(float)bp/10000.f);if(!bp||!isfinite(out))return value;
 /* Log changed values once per officer/attribute per thread; UI calls repeat. */
 typedef struct {uintptr_t world;int leader,bp;float before,after;} Seen;static _Thread_local Seen seen[64][3];
 int index=attr==S14_TROOP_ATTACK?0:attr==S14_TROOP_DEFENSE?1:2;Seen *last=&seen[leader%64][index];
 if(last->world!=world||last->leader!=leader||last->bp!=bp||last->before!=value||last->after!=out){*last=(Seen){world,leader,bp,value,out};tech_log("attribute",leader,-1,attr,value,out);}
 return out;
}
typedef struct {char magic[12];unsigned version,bytes;unsigned char hash[32],digest[32];unsigned short masks[6001];} TechCheckpoint;
static int checksum(const void *p,size_t n,unsigned char out[32]){BCRYPT_ALG_HANDLE a=NULL;int ok=BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0;ok=ok&&BCryptHash(a,NULL,0,(PUCHAR)p,(ULONG)n,out,32)>=0;if(a)BCryptCloseAlgorithmProvider(a,0);return ok;}
static int dir(const wchar_t *p,int create){if(create&&!CreateDirectoryW(p,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 0;DWORD a=GetFileAttributesW(p);return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT);}
static int checkpoint_path(const unsigned char *hash,wchar_t out[MAX_PATH],int create){
 if(!hash||!tech_root[0]||wcslen(tech_root)+120>=MAX_PATH)return 0;wchar_t path[MAX_PATH],hex[65];wcscpy(path,tech_root);
 for(wchar_t *p=path+3;;p++){if(*p&&*p!=L'\\'&&*p!=L'/')continue;wchar_t c=*p;*p=0;int ok=dir(path,0);*p=c;if(!ok)return 0;if(!c)break;}
 wcscat(path,L"\\SAN14ModManager");if(!dir(path,create))return 0;wcscat(path,L"\\archer-tech");if(!dir(path,create))return 0;
 for(int i=0;i<32;i++)swprintf(hex+i*2,3,L"%02x",hash[i]);return swprintf(out,MAX_PATH,L"%ls\\%ls.s14tech",path,hex)>0;
}
static int checkpoint_valid(const TechCheckpoint *c,const unsigned char *hash){unsigned char digest[32];
 if(memcmp(c->magic,"S14TECH.v1",11)||c->version!=1||c->bytes!=sizeof(*c)||memcmp(hash,c->hash,32)||c->masks[0]||
    !checksum(c->masks,sizeof(c->masks),digest)||memcmp(digest,c->digest,32))return 0;
 for(int i=1;i<=6000;i++)if(c->masks[i]&&!s14_tech_mask_valid(c->masks[i]))return 0;return 1;
}
static int checkpoint_read(const wchar_t *path,const unsigned char *hash,TechCheckpoint *c){
 DWORD a=GetFileAttributesW(path);if(a==INVALID_FILE_ATTRIBUTES||a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return 0;
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;
 LARGE_INTEGER size;DWORD n=0;int ok=GetFileSizeEx(f,&size)&&size.QuadPart==sizeof(*c)&&ReadFile(f,c,sizeof(*c),&n,NULL)&&n==sizeof(*c);CloseHandle(f);return ok&&checkpoint_valid(c,hash);
}
int s14_archer_tech_save(uintptr_t world,const unsigned char *hash){
 if(!tech_ready||tech_loading||!hash)return 0;wchar_t path[MAX_PATH],temp[MAX_PATH];TechCheckpoint *c=calloc(1,sizeof(*c));int ok=0;
 if(c&&checkpoint_path(hash,path,1)&&swprintf(temp,MAX_PATH,L"%ls.tmp",path)>0){
  memcpy(c->magic,"S14TECH.v1",11);c->version=1;c->bytes=sizeof(*c);memcpy(c->hash,hash,32);
  AcquireSRWLockExclusive(&tech_lock);bind_world(world);memcpy(c->masks,tech_masks,sizeof(tech_masks));ReleaseSRWLockExclusive(&tech_lock);
  if(checksum(c->masks,sizeof(c->masks),c->digest)){
   HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD n=0;
   ok=f!=INVALID_HANDLE_VALUE&&WriteFile(f,c,sizeof(*c),&n,NULL)&&n==sizeof(*c)&&FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
   if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok&&f!=INVALID_HANDLE_VALUE)DeleteFileW(temp);
  }
 }free(c);tech_log(ok?"saved":"save_failed",0,-1,-1,0,0);return ok;
}
void s14_archer_tech_load_begin(void){InterlockedExchange(&tech_loading,1);}
void s14_archer_tech_load_end(uintptr_t world,int success,const unsigned char *hash,int valid){
 TechCheckpoint *c=malloc(sizeof(*c));wchar_t path[MAX_PATH];int found=success&&valid&&hash&&c&&checkpoint_path(hash,path,0)&&checkpoint_read(path,hash,c);
 if(success){AcquireSRWLockExclusive(&tech_lock);memset(tech_masks,0,sizeof(tech_masks));if(found)memcpy(tech_masks,c->masks,sizeof(tech_masks));tech_world=world;ReleaseSRWLockExclusive(&tech_lock);}
 free(c);InterlockedExchange(&tech_loading,0);tech_log(found?"restored":success?"no_checkpoint":"load_failed",0,-1,-1,0,0);
}
void s14_archer_tech_new_game(uintptr_t world){AcquireSRWLockExclusive(&tech_lock);memset(tech_masks,0,sizeof(tech_masks));tech_world=world;ReleaseSRWLockExclusive(&tech_lock);InterlockedExchange(&tech_loading,0);}
