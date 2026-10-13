#include <assert.h>
#include "archer_tech.c"
static void putptr(void *p,int offset,uintptr_t value){memcpy((char*)p+offset,&value,8);}
static void putword(void *p,int offset,unsigned short value){memcpy((char*)p+offset,&value,2);}
int main(void){
 unsigned char *game=calloc(1,0x2020000),*world=calloc(1,0x86000),*pool=calloc(4,512),*settings=calloc(1,128),person[512]={0},group[32]={0};assert(game&&world&&pool&&settings);
 putptr(game,0x1fc91d0,(uintptr_t)world);putptr(world,0x85130,(uintptr_t)settings);settings[0x3a]=1;
 putptr(person,0,(uintptr_t)game+0x12a00d0);putword(person,0x10,260);person[0x118]=1;unsigned perm=1u<<3;memcpy(person+0x164,&perm,4);
 putptr(world,0x148+260*8,(uintptr_t)person);putptr(world,0xde48,(uintptr_t)group);putptr(group,0,(uintptr_t)game+0x129fec8);group[0x10]=1;
 putptr(world,0x7df60,(uintptr_t)pool);for(int i=1;i<=3;i++){putptr(world,0x7df60+i*8,(uintptr_t)(pool+i*512));putptr(pool+i*512,0,(uintptr_t)game+0x123e288);pool[i*512+0x10]=1;pool[i*512+0x1b]=i==2?1:3;putword(pool+i*512,0x12,i==3?261:260);}
 wchar_t temp[MAX_PATH],root[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));swprintf(root,MAX_PATH,L"%lsS14-tech-test-%lu-%llu",temp,GetCurrentProcessId(),(unsigned long long)GetTickCount64());assert(CreateDirectoryW(root,NULL));
 s14_archer_tech_attach((uintptr_t)game,root,1);s14_archer_tech_configure(1);assert(s14_archer_tech_can_learn(260));assert(!s14_archer_tech_can_learn(261));
 group[0x10]=2;assert(!s14_archer_tech_learn(260,1));group[0x10]=1;
 assert(!s14_archer_tech_learn(260,2));assert(s14_archer_tech_learn(260,1));assert(fabsf(s14_archer_tech_attribute(pool+512,S14_TROOP_ATTACK,1,100)-105)<.0001f);
 assert(s14_archer_tech_attribute(pool+512,S14_TROOP_DEFENSE,1,100)==97.5f);
 assert(s14_archer_tech_attribute(pool+512,S14_TROOP_ATTACK,0,100)==100);
 assert(s14_archer_tech_attribute(pool+1024,S14_TROOP_ATTACK,1,100)==100);assert(s14_archer_tech_attribute(pool+1536,S14_TROOP_ATTACK,1,100)==100);
 assert(s14_archer_tech_learn(260,5));assert(s14_archer_tech_learn(260,9));assert(fabsf(s14_archer_tech_attribute(pool+512,S14_TROOP_MOBILITY,1,100)-115)<.0001f);
 assert(s14_archer_tech_learn(260,10));int cap,rate;s14_archer_tech_ammo(260,40,2,&cap,&rate);assert(cap==50&&rate==180);
 assert(s14_archer_tech_learn(260,14));s14_archer_tech_ammo(260,40,2,&cap,&rate);assert(cap==45&&rate==230);
 unsigned short saved=s14_archer_tech_mask(260);unsigned char hash[32]={19};assert(s14_archer_tech_save((uintptr_t)world,hash));
 assert(s14_archer_tech_learn(260,2));s14_archer_tech_load_begin();assert(!s14_archer_tech_learn(260,3));assert(s14_archer_tech_attribute(pool+512,0,1,100)==100);
 s14_archer_tech_load_end((uintptr_t)world,0,NULL,0);assert(s14_archer_tech_mask(260)!=(saved));
 s14_archer_tech_load_begin();s14_archer_tech_load_end((uintptr_t)world,1,hash,1);assert(s14_archer_tech_mask(260)==saved);
 assert(s14_archer_tech_learn(260,2));assert(s14_archer_tech_learn(260,3));assert(s14_archer_tech_learn(260,4));assert(s14_tech_points(s14_archer_tech_mask(260))==8);assert(!s14_archer_tech_learn(260,6));
 s14_archer_tech_configure(0);assert(s14_archer_tech_attribute(pool+512,0,1,100)==100);s14_archer_tech_ammo(260,40,2,&cap,&rate);assert(cap==40&&rate==200);s14_archer_tech_configure(1);
 wchar_t path[MAX_PATH];assert(checkpoint_path(hash,path,0));TechCheckpoint c;assert(checkpoint_read(path,hash,&c));c.masks[260]=65535;assert(checksum(c.masks,sizeof(c.masks),c.digest));assert(!checkpoint_valid(&c,hash));
 unsigned char missing[32]={20};s14_archer_tech_load_begin();s14_archer_tech_load_end((uintptr_t)world,1,missing,1);assert(s14_archer_tech_mask(260)==1);
 s14_archer_tech_new_game((uintptr_t)world);assert(s14_archer_tech_mask(260)==1);
 assert(DeleteFileW(path));swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager\\archer-tech",root);assert(RemoveDirectoryW(path));swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager",root);assert(RemoveDirectoryW(path));assert(RemoveDirectoryW(root));
 free(settings);free(pool);free(world);free(game);
 puts("{\"status\":\"passed\",\"actual_attribute_values\":true,\"own_force_only_learning\":true,\"eight_points\":true,\"officer_isolation\":true,\"only_native_archers\":true,\"save_load_checkpoint\":true,\"failed_load_rollback\":true,\"master_gate\":true,\"no_advanced_effects\":true}");
}
