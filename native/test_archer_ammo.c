#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "archer_probe.c"
static unsigned char *game,*world,*pool,*settings;
static int called,daily_result=1;
static void putptr(void *p,int off,uintptr_t v){memcpy((char*)p+off,&v,8);}
static void putword(void *p,int off,unsigned short v){memcpy((char*)p+off,&v,2);}
static void army(int id,int leader,int carrier){unsigned char *a=pool+id*512;memset(a,0,512);putptr(a,0,image+0x123e288);a[16]=1;putword(a,18,leader);putword(a,22,5000);a[27]=carrier;putptr(world,0x7df60+id*8,(uintptr_t)a);}
static int original(void *manager,void *pair,void *damage){assert(manager==(void*)17&&pair&&damage);called++;unsigned char *p=pair;int id=word(p+8);unsigned char *a=pool+id*512;putword(a,22,(unsigned short)(word(a+22)-5));SetLastError(29);return 8;}
static int daily(void *manager){assert(manager==(void*)17);SetLastError(37);return daily_result;}
int main(void){
 game=calloc(1,0x2580000);world=calloc(1,0x86000);pool=calloc(501,512);settings=calloc(1,128);assert(game&&world&&pool&&settings);
 image=(uintptr_t)game;putptr(game,0x1fc91d0,(uintptr_t)world);putptr(world,0x7df60,(uintptr_t)pool);putptr(world,0x85130,(uintptr_t)settings);
 putword(settings,0x34,263);settings[0x36]=5;settings[0x37]=21;army(1,613,3);army(2,124,1);army(3,40,3);
 original_attack=original;original_daily=daily;ready=enabled=1;
 unsigned char source[16]={0},pair[32]={0},damage[48]={0};putword(source,0,1);source[4]=27;putptr(pair,0,(uintptr_t)source);putword(pair,8,2);pair[12]=27;putword(damage,32,5);
 int n,max;assert(capacity==25);s14_archer_ammo_settings(NULL,30,2);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==30&&max==30);
 for(int i=0;i<30;i++){SetLastError(11);assert(attack_hook((void*)17,pair,damage)==8&&GetLastError()==29);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==29-i);}
 assert(called==30&&word(pool+1024+22)==4850);
 SetLastError(11);assert(attack_hook((void*)17,pair,damage)==8&&called==30&&GetLastError()==11&&word(pool+1024+22)==4850);
 assert(s14_archer_ammo_snapshot((uintptr_t)pool+3*512,&n,&max)&&n==30);
 assert(!s14_archer_ammo_snapshot((uintptr_t)pool+2*512,&n,&max));
 settings[0x37]=22;assert(daily_hook((void*)17)==1&&GetLastError()==37);assert(records[1].ammo==0);
 for(int i=0;i<100;i++){daily_hook((void*)17);s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max);}assert(records[1].ammo==0);
 /* An empty attempt does not become a successful attack: the completed idle
    day refills two, and a day on which shots fire earns no refill. */
 attack_hook((void*)17,pair,damage);settings[0x37]=23;daily_hook((void*)17);assert(records[1].ammo==2);
 attack_hook((void*)17,pair,damage);assert(records[1].ammo==1);settings[0x37]=24;daily_hook((void*)17);assert(records[1].ammo==1);
 settings[0x37]=25;daily_result=0;daily_hook((void*)17);assert(records[1].ammo==1);daily_result=1;daily_hook((void*)17);assert(records[1].ammo==3);
 for(int day=26;day<=30;day++){settings[0x37]=(unsigned char)day;daily_hook((void*)17);}assert(records[1].ammo==13&&records[3].ammo==30);
 /* Slot reuse and a new world/loaded generation cannot inherit another army. */
 putword(pool+512,40,1);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==30);attack_hook((void*)17,pair,damage);assert(records[1].ammo==29);
 pool[512+16]=0;s14_archer_ammo_forget(pool+512);assert(!records[1].key.id);army(1,613,3);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==30);
 /* Disabled time and camera/render samples never generate supplies. */
 attack_hook((void*)17,pair,damage);s14_archer_probe_configure(0);settings[0x36]=6;settings[0x37]=5;s14_archer_probe_configure(1);assert(records[1].ammo==29);
 wchar_t temp[MAX_PATH],root[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));swprintf(root,MAX_PATH,L"%lsS14-ammo-test-%lu-%llu",temp,GetCurrentProcessId(),(unsigned long long)GetTickCount64());assert(CreateDirectoryW(root,NULL));s14_archer_ammo_settings(root,30,2);
 unsigned char hash[32]={7},wrong[32]={8};assert(s14_archer_ammo_save((uintptr_t)world,hash));
 wchar_t file[MAX_PATH];assert(file_path(hash,file,0));Checkpoint *c=malloc(sizeof(*c));assert(c&&checkpoint_read(file,hash,c)&&!checkpoint_read(file,wrong,c));
 s14_archer_ammo_load_begin();assert(!s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max));s14_archer_ammo_load_end((uintptr_t)world,0,NULL,0);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==29);
 attack_hook((void*)17,pair,damage);assert(records[1].ammo==28);s14_archer_ammo_load_begin();s14_archer_ammo_load_end((uintptr_t)world,1,hash,1);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==29);
 assert(checkpoint_read(file,hash,c));c->units[1].ammo=31;assert(digest(&c->day,sizeof(*c)-offsetof(Checkpoint,day),c->digest));assert(!valid_checkpoint(c,hash));
 /* A saved 29/30 becomes 39/40, retaining the one spent round. Repeated
    loads must not credit additional ammunition. Lower capacities clamp at zero. */
 s14_archer_ammo_settings(root,40,2);assert(records[1].ammo==39&&records[1].maximum==40);
 for(int i=0;i<2;i++){s14_archer_ammo_load_begin();s14_archer_ammo_load_end((uintptr_t)world,1,hash,1);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==39&&max==40);}
 s14_archer_ammo_settings(root,1,2);assert(records[1].ammo==0&&records[1].maximum==1);
 s14_archer_ammo_settings(root,40,2);
 s14_archer_ammo_new_game((uintptr_t)world);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==40);
 assert(!s14_archer_probe_install(1,8));
 free(c);assert(DeleteFileW(file));wchar_t dir[MAX_PATH];swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\archer-ammo",root);assert(RemoveDirectoryW(dir));swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);assert(RemoveDirectoryW(dir));assert(RemoveDirectoryW(root));
 /* Ten native days: four distinct targets exhaust 40; two leave 20.
    Exercise the real attack/daily hooks, including the no-refill rule. */
 for(int targets=2;targets<=4;targets+=2){
  army(1,613,3);for(int t=0;t<4;t++)army(t+2,124+t,1);
  settings[0x36]=5;settings[0x37]=21;s14_archer_ammo_new_game((uintptr_t)world);called=0;
  for(int day=21;day<=30;day++){settings[0x37]=(unsigned char)day;daily_hook((void*)17);
   for(int t=0;t<targets;t++){putword(pair,8,(unsigned short)(t+2));assert(attack_hook((void*)17,pair,damage)==8);}}
  assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&max==40&&n==40-targets*10&&called==targets*10);
  if(targets==4){int target_id=word(pair+8),before_troops=word(pool+target_id*512+22);assert(attack_hook((void*)17,pair,damage)==8&&called==40&&word(pool+target_id*512+22)==before_troops);}
 }
 /* User test configuration: five shots, then empty blocks the native attack;
    a complete idle day replenishes two and allows firing again. */
 s14_archer_ammo_settings(NULL,5,2);settings[0x36]=5;settings[0x37]=21;s14_archer_ammo_new_game((uintptr_t)world);army(1,613,3);army(2,124,1);called=0;putword(pair,8,2);
 assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==5&&max==5);
 for(int i=0;i<5;i++)attack_hook((void*)17,pair,damage);
 assert(records[1].ammo==0&&called==5);int empty_troops=word(pool+1024+22);attack_hook((void*)17,pair,damage);assert(called==5&&word(pool+1024+22)==empty_troops);
 settings[0x37]=22;daily_hook((void*)17);assert(records[1].ammo==0);settings[0x37]=23;daily_hook((void*)17);assert(records[1].ammo==2);
 attack_hook((void*)17,pair,damage);assert(records[1].ammo==1&&called==6);s14_archer_ammo_settings(NULL,40,2);
 /* Native day/attack integration with learned capacity and fractional loading. */
 unsigned char person[512]={0},group[32]={0};putptr(person,0,image+0x12a00d0);putword(person,0x10,613);person[0x118]=1;
 unsigned permission=1u<<3;memcpy(person+0x164,&permission,4);putptr(world,0x148+613*8,(uintptr_t)person);putptr(world,0xde48,(uintptr_t)group);putptr(group,0,image+0x129fec8);group[0x10]=1;settings[0x3a]=1;
 s14_archer_tech_attach(image,NULL,1);s14_archer_tech_configure(1);s14_archer_tech_new_game((uintptr_t)world);
 settings[0x36]=6;settings[0x37]=1;s14_archer_ammo_new_game((uintptr_t)world);army(1,613,3);
 assert(s14_archer_tech_learn(613,10));assert(s14_archer_tech_learn(613,11));assert(s14_archer_tech_learn(613,12));
 assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==70&&max==70);
 records[1].ammo=0;records[1].credit=0;for(int d=2;d<=6;d++){settings[0x37]=(unsigned char)d;daily_hook((void*)17);}assert(records[1].ammo==7&&records[1].credit==0);
 for(int d=7;d<=9;d++){settings[0x37]=(unsigned char)d;daily_hook((void*)17);}assert(records[1].ammo==11&&records[1].credit==20);
 assert(s14_archer_tech_learn(613,13));assert(s14_archer_tech_learn(613,14));assert(s14_archer_tech_learn(613,15));
 assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max)&&n==0&&max==55); /* 59 already spent; never refill on learning. */
 settings[0x37]=10;daily_hook((void*)17);assert(records[1].ammo==3&&records[1].credit==10);
 putword(pair,8,2);int before_ammo=records[1].ammo;attack_hook((void*)17,pair,damage);settings[0x37]=11;daily_hook((void*)17);assert(records[1].ammo==before_ammo-1&&records[1].credit==10);
 /* Old installed v1 checkpoints migrate credit=0 even though the structure
    occupies the same bytes (credit replaces legacy trailing padding). */
 s14_archer_tech_configure(0);assert(s14_archer_ammo_snapshot((uintptr_t)pool+512,&n,&max));
 assert(CreateDirectoryW(root,NULL));s14_archer_ammo_settings(root,40,2);unsigned char legacy_hash[32]={21};assert(s14_archer_ammo_save((uintptr_t)world,legacy_hash));assert(file_path(legacy_hash,file,0));
 LegacyCheckpoint *old=calloc(1,sizeof(*old));assert(old);memcpy(old->magic,"S14AMMO.v1",11);old->version=1;old->bytes=sizeof(*old);memcpy(old->hash,legacy_hash,32);old->day=measured_day;
 for(int i=0;i<=500;i++){memcpy(&old->units[i],&records[i],sizeof(LegacyRecord));old->units[i].key.address=0;}
 assert(digest(&old->day,sizeof(*old)-offsetof(LegacyCheckpoint,day),old->digest));HANDLE legacy=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD bytes=0;assert(legacy!=INVALID_HANDLE_VALUE&&WriteFile(legacy,old,sizeof(*old),&bytes,NULL)&&bytes==sizeof(*old));CloseHandle(legacy);
 Checkpoint migrated;assert(checkpoint_read(file,legacy_hash,&migrated)&&migrated.units[1].credit==0&&migrated.units[1].ammo==records[1].ammo);free(old);
 assert(DeleteFileW(file));swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\archer-ammo",root);assert(RemoveDirectoryW(dir));swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);assert(RemoveDirectoryW(dir));assert(RemoveDirectoryW(root));
 free(settings);free(pool);free(world);free(game);
 puts("{\"status\":\"passed\",\"capacity\":40,\"five_shot_test_capacity\":true,\"four_targets_ten_days_empty\":true,\"two_targets_ten_days_half\":true,\"capacity_migration_keeps_spent\":true,\"consume_per_attack\":true,\"empty_blocks_damage\":true,\"idle_recovers_two\":true,\"no_refill_when_firing\":true,\"same_day_deduplication\":true,\"pause_does_not_refill\":true,\"cap\":true,\"army_isolation\":true,\"generation_reuse\":true,\"save_load_balance\":true,\"failed_load_rollback\":true,\"checkpoint_validation\":true,\"technology_fractional_loading\":true,\"learning_keeps_spent_ammo\":true,\"legacy_v1_ammo_migration\":true}");return 0;
}
