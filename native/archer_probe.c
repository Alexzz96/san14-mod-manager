#define WIN32_LEAN_AND_MEAN
#include "archer_probe.h"
#include "MinHook.h"
#include "archer_tech.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <bcrypt.h>

typedef int (*Attack)(void*,void*,void*);
typedef int (*Daily)(void*);
static Attack original_attack;
static Daily original_daily;
static uintptr_t image,image_end;
static volatile LONG ready,enabled;
typedef struct {int id,leader,serial,carrier,troops;uintptr_t address;} Army;
typedef struct {Army key;int day,attempts,damaging,ammo,maximum,attacked;unsigned lifetime;int credit;} Record;
static Record records[501];
static Record backup[501];
static uintptr_t backup_world;
static int backup_day,capacity=25,recovery=2;
static volatile LONG loading,session_epoch;
static unsigned ammo_epoch(void){return (unsigned)InterlockedCompareExchange(&session_epoch,0,0);}
static wchar_t storage[MAX_PATH];
static uintptr_t measured_world;
static unsigned measured_epoch;
static int measured_day=-1;
static SRWLOCK lock=SRWLOCK_INIT;
typedef struct {volatile LONG full;char text[1024];} Log;
static Log queue[256];
static volatile LONG cursor,dropped;
static int read_at(uintptr_t at,void *out,size_t n){SIZE_T got=0;return at&&at+n>=at&&ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got)&&got==n;}
static uintptr_t ptr(uintptr_t at){uintptr_t p=0;read_at(at,&p,8);return p;}
static int word(const unsigned char *p){unsigned short n;memcpy(&n,p,2);return n;}
static int integer(const unsigned char *p){int n;memcpy(&n,p,4);return n;}
static int clock_read(uintptr_t world,unsigned char c[6]){
 uintptr_t settings=ptr(world+0x85130);
 if(!settings||!read_at(settings+0x34,c,6))return -1;
 int y=word(c);return y>0&&c[2]>=1&&c[2]<=12&&c[3]>=1&&c[3]<=30?y*360+(c[2]-1)*30+c[3]-1:-1;
}
static int army_read(uintptr_t world,int id,Army *out){
 if(id<1||id>500||!world)return 0;
 uintptr_t pool=ptr(world+0x7df60),at=ptr(world+0x7df60+(uintptr_t)id*8);unsigned char b[48];
 if(!pool||at!=pool+(uintptr_t)id*512||!read_at(at,b,sizeof(b))||ptr(at)!=image+0x123e288||b[0x10]!=1||word(b+0x12)<1||word(b+0x12)>6000||!word(b+0x16))return 0;
 *out=(Army){id,word(b+0x12),word(b+0x28),b[0x1b],word(b+0x16),at};return 1;
}
static int same(const Army *a,const Army *b){return a->id==b->id&&a->leader==b->leader&&a->serial==b->serial&&a->carrier==b->carrier&&a->address==b->address;}
static int generation(const Army *a,const Army *b){return a->id==b->id&&a->leader==b->leader&&a->serial==b->serial&&a->carrier==b->carrier;}
/* Keep the already consumed amount when a configured capacity changes. */
static void record_capacity(Record *r){int cap,rate;s14_archer_tech_ammo(r->key.leader,capacity,recovery,&cap,&rate);if(r->key.id&&r->maximum!=cap){int spent=r->maximum-r->ammo;r->maximum=cap;r->ammo=spent<cap?cap-spent:0;}if(r->ammo==r->maximum)r->credit=0;}
static Record *record(const Army *a,int day){Record *r=&records[a->id];if(!generation(&r->key,a)){int cap,rate;s14_archer_tech_ammo(a->leader,capacity,recovery,&cap,&rate);*r=(Record){.key=*a,.day=day,.ammo=cap,.maximum=cap};}else{r->key=*a;record_capacity(r);}return r;}
static Log *reserve(void){
 LONG n=InterlockedIncrement(&cursor);Log *e=&queue[(unsigned)n%256];
 if(InterlockedCompareExchange(&e->full,1,0)){InterlockedIncrement(&dropped);return NULL;}return e;
}
static void daily_log(const Record *r,uintptr_t world){
 Log *e=reserve();if(!e)return;
 snprintf(e->text,sizeof(e->text),"{\"event\":\"archer_attack_day\",\"world\":\"0x%llx\",\"day\":%d,\"army_id\":%d,\"officer_id\":%d,\"serial\":%d,\"ordinary_settlements\":%d,\"positive_loss_settlements\":%d,\"observed_total\":%u,\"dropped\":%ld,\"gameplay_modified\":false}\n",(unsigned long long)world,r->day,r->key.id,r->key.leader,r->key.serial,r->attempts,r->damaging,r->lifetime,dropped);
 InterlockedExchange(&e->full,2);
}
/* A successful native daily dispatcher supplies idle days as well as attacks.
   Same-day animation frames never reset a counter or emit duplicate summaries. */
static void bind_day(uintptr_t world,int day,unsigned epoch){
 if(world!=measured_world||epoch!=measured_epoch||day<measured_day){memset(records,0,sizeof(records));measured_world=world;measured_epoch=epoch;measured_day=day;}
 if(day>measured_day){for(int i=1;i<=500;i++)if(records[i].key.id){Record *r=&records[i];daily_log(r,world);
  /* Credit only one confirmed game-day opportunity, never render frames,
     wall-clock time or gaps caused by disabled hooks/load transitions. */
  record_capacity(r);
  if(day==measured_day+1&&!r->attacked){int cap,rate;s14_archer_tech_ammo(r->key.leader,capacity,recovery,&cap,&rate);r->credit+=rate;r->ammo+=r->credit/100;r->credit%=100;if(r->ammo>=r->maximum){r->ammo=r->maximum;r->credit=0;}}
  r->day=day;r->attempts=r->damaging=r->attacked=0;
 }measured_day=day;}
}
static int attack_hook(void *manager,void *pair,void *damage){
 /* 0x257c370 passes the caller's RBP as a hidden sixth argument. Hook the
    ordinary three-argument dispatcher instead, leaving that ABI untouched. */
 if(!enabled||loading)return original_attack(manager,pair,damage);
 DWORD before=GetLastError();uintptr_t world=ptr(image+0x1fc91d0);unsigned char c[6],p[24],source[16],d[4];Army a={0},target={0};
 int day=clock_read(world,c),valid=day>=0&&read_at((uintptr_t)pair,p,sizeof(p))&&read_at(ptr((uintptr_t)pair),source,sizeof(source))&&integer(source+4)==27&&army_read(world,word(source),&a)&&a.carrier==3;
 int target_type=valid?integer(p+12):0,target_id=valid?word(p+8):0;
 int target_valid=valid&&target_type==27&&army_read(world,target_id,&target);
 valid=valid&&(target_valid||((target_type==5||target_type==6)&&target_id>=1&&target_id<=(target_type==5?51:10)&&ptr(world+(target_type==5?0xdaa8:0xdc48)+(uintptr_t)target_id*8)));
 int amount=valid&&read_at((uintptr_t)damage+0x20,d,4)?word(d)+word(d+2):-1;
 unsigned epoch=ammo_epoch();uintptr_t caller=(uintptr_t)__builtin_return_address(0)-image;
 int blocked=0,remaining=-1,maximum=0;
 if(valid){AcquireSRWLockExclusive(&lock);bind_day(world,day,epoch);Record *r=record(&a,day);blocked=r->ammo==0;
  if(!blocked){r->ammo--;r->attacked=1;}remaining=r->ammo;maximum=r->maximum;ReleaseSRWLockExclusive(&lock);}
 if(blocked){Log *e=reserve();if(e){snprintf(e->text,sizeof(e->text),"{\"event\":\"archer_ammo_empty\",\"day\":%d,\"army_id\":%d,\"officer_id\":%d,\"remaining\":0,\"maximum\":%d,\"gameplay_modified\":true}\n",day,a.id,a.leader,maximum);InterlockedExchange(&e->full,2);}SetLastError(before);return 8;}
 SetLastError(before);int result=original_attack(manager,pair,damage);DWORD after=GetLastError();
 if(valid){Log *e=reserve();if(e){snprintf(e->text,sizeof(e->text),"{\"event\":\"archer_ammo_spent\",\"day\":%d,\"army_id\":%d,\"officer_id\":%d,\"remaining\":%d,\"maximum\":%d,\"gameplay_modified\":true}\n",day,a.id,a.leader,remaining,maximum);InterlockedExchange(&e->full,2);}}
 Army again={0},t_after={0};int loss=-1;
 if(target_valid&&army_read(world,target_id,&t_after)&&same(&target,&t_after))loss=target.troops-t_after.troops;
 else if(target_valid&&ptr(image+0x1fc91d0)==world&&ptr(target.address)==image+0x123e288){unsigned char b[48];if(read_at(target.address,b,48)&&word(b+0x12)==target.leader&&word(b+0x28)==target.serial&&b[0x1b]==target.carrier)loss=target.troops-word(b+0x16);}
 if(valid&&ptr(image+0x1fc91d0)==world&&epoch==ammo_epoch()&&army_read(world,a.id,&again)&&same(&a,&again)){
  AcquireSRWLockExclusive(&lock);bind_day(world,day,epoch);Record *r=&records[a.id];
  r=record(&a,day);r->attempts++;r->lifetime++;if(loss>0)r->damaging++;
  Log *e=reserve();if(e){snprintf(e->text,sizeof(e->text),"{\"event\":\"archer_attack_observed\",\"day\":%d,\"clock_raw\":\"%02x%02x%02x%02x%02x%02x\",\"army_id\":%d,\"officer_id\":%d,\"serial\":%d,\"target_type\":%d,\"target_id\":%d,\"packet_damage\":%d,\"observed_loss\":%d,\"native_result\":%d,\"caller_rva\":\"0x%llx\",\"day_settlements\":%d,\"dropped\":%ld,\"gameplay_modified\":false}\n",day,c[0],c[1],c[2],c[3],c[4],c[5],a.id,a.leader,a.serial,target_type,target_id,amount,loss,result,(unsigned long long)caller,r->attempts,dropped);InterlockedExchange(&e->full,2);}
  ReleaseSRWLockExclusive(&lock);
 }
 SetLastError(after);return result;
}
static int daily_hook(void *manager){
 DWORD before=GetLastError();SetLastError(before);int result=original_daily(manager);DWORD after=GetLastError();
 if(enabled&&result&&!loading){
  uintptr_t world=ptr(image+0x1fc91d0);unsigned char c[6];int day=clock_read(world,c);unsigned epoch=ammo_epoch();
  if(day>=0){AcquireSRWLockExclusive(&lock);bind_day(world,day,epoch);
   for(int i=1;i<=500;i++){Army a;if(army_read(world,i,&a)&&a.carrier==3)record(&a,day);else memset(&records[i],0,sizeof(records[i]));}
   ReleaseSRWLockExclusive(&lock);
  }
 }
 SetLastError(after);return result;
}
void s14_archer_probe_configure(int on){int active=on&&ready;LONG old=InterlockedExchange(&enabled,active);if(active&&!old){AcquireSRWLockExclusive(&lock);unsigned char c[6];int day=clock_read(measured_world,c);if(day>=0){measured_day=day;for(int i=1;i<=500;i++)if(records[i].key.id){records[i].day=day;records[i].attacked=0;}}ReleaseSRWLockExclusive(&lock);}}
int s14_archer_ammo_enabled(void){return enabled&&!loading;}
int s14_archer_probe_next_log(char *out,size_t cap){
 if(!out||cap<1024)return 0;for(int i=0;i<256;i++)if(InterlockedCompareExchange(&queue[i].full,3,2)==2){strcpy(out,queue[i].text);InterlockedExchange(&queue[i].full,0);return 1;}return 0;
}
void s14_archer_ammo_settings(const wchar_t *root,int cap,int refill){
 if(root&&wcslen(root)<MAX_PATH)wcscpy(storage,root);
 AcquireSRWLockExclusive(&lock);
 if(cap>=1&&cap<=1000)capacity=cap;if(refill>=1&&refill<=1000)recovery=refill;
 for(int i=1;i<=500;i++)record_capacity(&records[i]);
 ReleaseSRWLockExclusive(&lock);
}
void s14_archer_ammo_parameters(int officer,int *cap,int *rate){s14_archer_tech_ammo(officer,capacity,recovery,cap,rate);}
int s14_archer_ammo_snapshot(uintptr_t army,int *remaining,int *maximum){
 if(!enabled||loading||!remaining||!maximum)return 0;
 uintptr_t world=ptr(image+0x1fc91d0),pool=ptr(world+0x7df60);Army a;unsigned char c[6];int day=clock_read(world,c);
 if(!pool||army<=pool||(army-pool)%512||(army-pool)/512>500||day<0||!army_read(world,(int)((army-pool)/512),&a)||a.carrier!=3)return 0;
 AcquireSRWLockExclusive(&lock);unsigned epoch=ammo_epoch();
 if(measured_world!=world||measured_epoch!=epoch||measured_day<0||day<measured_day)bind_day(world,day,epoch);
 Record *r=record(&a,measured_day);*remaining=r->ammo;*maximum=r->maximum;ReleaseSRWLockExclusive(&lock);return 1;
}
void s14_archer_ammo_forget(void *army){
 uintptr_t world=ptr(image+0x1fc91d0),pool=ptr(world+0x7df60),at=(uintptr_t)army;unsigned char active=1;
 if(!pool||at<=pool||(at-pool)%512||(at-pool)/512>500||!read_at(at+0x10,&active,1)||active==1)return;
 AcquireSRWLockExclusive(&lock);memset(&records[(at-pool)/512],0,sizeof(Record));ReleaseSRWLockExclusive(&lock);
}
typedef struct {char magic[12];unsigned version,bytes;unsigned char hash[32],digest[32];int day;Record units[501];} Checkpoint;
/* Preserve installed v1 saves while adding fractional loading progress. */
typedef struct {Army key;int day,attempts,damaging,ammo,maximum,attacked;unsigned lifetime;} LegacyRecord;
typedef struct {char magic[12];unsigned version,bytes;unsigned char hash[32],digest[32];int day;LegacyRecord units[501];} LegacyCheckpoint;
static int digest(const void *data,size_t n,unsigned char out[32]){BCRYPT_ALG_HANDLE a=NULL;int ok=BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0;ok=ok&&BCryptHash(a,NULL,0,(PUCHAR)data,(ULONG)n,out,32)>=0;if(a)BCryptCloseAlgorithmProvider(a,0);return ok;}
static int folder(const wchar_t *path){DWORD a=GetFileAttributesW(path);if(a==INVALID_FILE_ATTRIBUTES){if(!CreateDirectoryW(path,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 0;a=GetFileAttributesW(path);}return (a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT);}
static int file_path(const unsigned char *hash,wchar_t out[MAX_PATH],int create){
 if(!hash||!storage[0]||wcslen(storage)+120>=MAX_PATH)return 0;wchar_t dir[MAX_PATH],hex[65];
 swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",storage);if(create&&!folder(dir))return 0;
 swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\archer-ammo",storage);if(create&&!folder(dir))return 0;
 for(int i=0;i<32;i++)swprintf(hex+i*2,3,L"%02x",hash[i]);return swprintf(out,MAX_PATH,L"%ls\\%ls.s14ammo",dir,hex)>0;
}
static int valid_checkpoint(const Checkpoint *c,const unsigned char *hash){
 unsigned char d[32];Record empty={0};
 if(memcmp(c->magic,"S14AMMO.v2",11)||c->version!=2||c->bytes!=sizeof(*c)||memcmp(c->hash,hash,32)||c->day<0||c->day>65535*360||!digest(&c->day,sizeof(*c)-offsetof(Checkpoint,day),d)||memcmp(d,c->digest,32)||memcmp(&c->units[0],&empty,sizeof(empty)))return 0;
 for(int i=1;i<=500;i++){const Record *r=&c->units[i];if(!r->key.id){if(memcmp(r,&empty,sizeof(empty)))return 0;continue;}
  if(r->key.id!=i||r->key.leader<1||r->key.leader>6000||r->key.carrier!=3||r->key.address||r->maximum<1||r->maximum>1000||r->ammo<0||r->ammo>r->maximum||r->day!=c->day||r->attempts<0||r->damaging<0||r->damaging>r->attempts||r->attacked<0||r->attacked>1||r->credit<0||r->credit>=100)return 0;
 }return 1;
}
static int checkpoint_read(const wchar_t *path,const unsigned char *hash,Checkpoint *c){
 DWORD attr=GetFileAttributesW(path);if(attr==INVALID_FILE_ATTRIBUTES||attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return 0;
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;
 DWORD n=0;LARGE_INTEGER size;unsigned version=0;unsigned char header[20];int ok=GetFileSizeEx(f,&size)&&ReadFile(f,header,sizeof(header),&n,NULL)&&n==sizeof(header);
 if(ok)memcpy(&version,header+12,4);SetFilePointer(f,0,NULL,FILE_BEGIN);
 if(ok&&version==1&&size.QuadPart==sizeof(LegacyCheckpoint)){
  LegacyCheckpoint *old=malloc(sizeof(*old));unsigned char d[32];ok=old&&ReadFile(f,old,sizeof(*old),&n,NULL)&&n==sizeof(*old)&&
   !memcmp(old->magic,"S14AMMO.v1",11)&&old->version==1&&old->bytes==sizeof(*old)&&!memcmp(old->hash,hash,32)&&
   digest(&old->day,sizeof(*old)-offsetof(LegacyCheckpoint,day),d)&&!memcmp(d,old->digest,32);
  if(ok){memset(c,0,sizeof(*c));memcpy(c->magic,"S14AMMO.v2",11);c->version=2;c->bytes=sizeof(*c);memcpy(c->hash,old->hash,32);c->day=old->day;
   for(int i=0;i<=500;i++){memcpy(&c->units[i],&old->units[i],sizeof(LegacyRecord));c->units[i].credit=0;}ok=digest(&c->day,sizeof(*c)-offsetof(Checkpoint,day),c->digest);}
  free(old);
 }else ok=ok&&size.QuadPart==sizeof(*c)&&ReadFile(f,c,sizeof(*c),&n,NULL)&&n==sizeof(*c);
 CloseHandle(f);return ok&&valid_checkpoint(c,hash);
}
int s14_archer_ammo_save(uintptr_t world,const unsigned char *hash){
 if(!ready||loading||!hash)return 0;Checkpoint *c=calloc(1,sizeof(*c));wchar_t path[MAX_PATH],temp[MAX_PATH];if(!c||!file_path(hash,path,1)){free(c);return 0;}
 AcquireSRWLockShared(&lock);int matches=measured_world==world&&measured_day>=0;if(matches){memcpy(c->units,records,sizeof(records));c->day=measured_day;}ReleaseSRWLockShared(&lock);
 if(!matches){free(c);return 0;}
 for(int i=1;i<=500;i++)c->units[i].key.address=0;
 memcpy(c->magic,"S14AMMO.v2",11);c->version=2;c->bytes=sizeof(*c);memcpy(c->hash,hash,32);
 int ok=digest(&c->day,sizeof(*c)-offsetof(Checkpoint,day),c->digest)&&valid_checkpoint(c,hash)&&swprintf(temp,MAX_PATH,L"%ls.tmp",path)>0;
 HANDLE f=ok?CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL):INVALID_HANDLE_VALUE;DWORD n=0;
 ok=f!=INVALID_HANDLE_VALUE&&WriteFile(f,c,sizeof(*c),&n,NULL)&&n==sizeof(*c)&&FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok&&f!=INVALID_HANDLE_VALUE)DeleteFileW(temp);free(c);return ok;
}
void s14_archer_ammo_load_begin(void){InterlockedExchange(&loading,1);AcquireSRWLockExclusive(&lock);memcpy(backup,records,sizeof(records));backup_world=measured_world;backup_day=measured_day;ReleaseSRWLockExclusive(&lock);}
void s14_archer_ammo_load_end(uintptr_t world,int success,const unsigned char *hash,int valid){
 Checkpoint *c=malloc(sizeof(*c));wchar_t path[MAX_PATH];int restored=success&&valid&&c&&file_path(hash,path,0)&&checkpoint_read(path,hash,c);
 AcquireSRWLockExclusive(&lock);
 if(!success){memcpy(records,backup,sizeof(records));measured_world=backup_world;measured_day=backup_day;}
 else{memset(records,0,sizeof(records));measured_world=world;measured_day=-1;if(restored){memcpy(records,c->units,sizeof(records));for(int i=1;i<=500;i++)record_capacity(&records[i]);measured_day=c->day;}measured_epoch=(unsigned)InterlockedIncrement(&session_epoch);}
 ReleaseSRWLockExclusive(&lock);free(c);InterlockedExchange(&loading,0);
}
void s14_archer_ammo_new_game(uintptr_t world){AcquireSRWLockExclusive(&lock);memset(records,0,sizeof(records));measured_world=world;measured_day=-1;measured_epoch=(unsigned)InterlockedIncrement(&session_epoch);ReleaseSRWLockExclusive(&lock);InterlockedExchange(&loading,0);}
int s14_archer_probe_install(uintptr_t base,uintptr_t end){
 static const uintptr_t rvas[]={0x166760,0x15bb40};
 static const unsigned char bytes[2][18]={{0x40,0x55,0x57,0x41,0x56,0x48,0x83,0xec,0x40,0x4c,0x8b,0x32,0x48,0x8d,0x7a,0x08,0x49,0x8b},{0x40,0x53,0x41,0x57,0x48,0x81,0xec,0x88,0,0,0,0x48,0x8b,0x05,0x7e,0xd6,0xe6,0x01}};
 if(ready||!base||end<=base)return 0;
 for(int i=0;i<2;i++){unsigned char b[18];if(rvas[i]>end-base||18>end-base-rvas[i]||!read_at(base+rvas[i],b,18)||memcmp(b,bytes[i],18))return 0;}
 /* Validate the native ordinary damage call whose semantics are measured. */
 unsigned char call[5];static const unsigned char expected[]={0xe8,0x0f,0x5b,0x41,0x02};
 if(end-base<0x257c38b||!read_at(base+0x16685c,call,5)||memcmp(call,expected,5))return 0;
 image=base;image_end=end;
 if(MH_CreateHook((void*)(base+rvas[0]),attack_hook,(void**)&original_attack)!=MH_OK)return 0;
 if(MH_CreateHook((void*)(base+rvas[1]),daily_hook,(void**)&original_daily)!=MH_OK){MH_RemoveHook((void*)(base+rvas[0]));return 0;}
 InterlockedExchange(&ready,1);return 1;
}
