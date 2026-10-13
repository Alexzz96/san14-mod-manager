#include <assert.h>
#ifdef NDEBUG
#error assertions required
#endif
#include "battle_stats.c"
static S14RoundObject unit(int id,int leader,int force,int troops){return (S14RoundObject){.kind=2,.id=id,.leader=leader,.force=force,.troops=troops,.active=1};}
static uint64_t balance(uintptr_t world,int officer){S14MeritAccount m;assert(s14_merit_row(world,officer,&m));return m.earned-m.spent;}
int main(int argc,char **argv){
 assert(argc==2||argc==3);wchar_t root[MAX_PATH],folder[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));
 swprintf(folder,MAX_PATH,L"%ls\\SAN14ModManager",root);CreateDirectoryW(folder,NULL);s14_stats_root(root);
 unsigned char old[32]={10},saved[32]={11},legacy[32]={12};
 if(argc==3){s14_stats_load_end(9,101,1,legacy,1,0);assert(balance(9,10)==153&&merits[10].spent==100&&merits[10].enemy_remainder==0);puts("{\"cross_process_restore\":true}");return 0;}
 s14_stats_load_end(1,100,1,old,0,1);assert(s14_stats_save(1,old,1));
 S14RoundEvent e={.world=1,.day=100,.id=1,.kind=S14_ROUND_DAMAGE,.stable=1,.source=unit(1,10,1,5000),.target=unit(2,20,2,5000),.target_after=unit(2,20,2,4960)};
 s14_stats_consume(&e);s14_stats_consume(&e);assert(balance(1,10)==0&&merits[10].enemy_remainder==40);
 e.id=2;e.target_after.troops=4940;s14_stats_consume(&e);assert(balance(1,10)==1&&merits[10].enemy_remainder==0);
 e.id=3;e.kind=S14_ROUND_INJURY;e.target.health=0;e.target_after.health=1;s14_stats_consume(&e);s14_stats_consume(&e);assert(balance(1,10)==51);
 e.id=4;e.kind=S14_ROUND_REMOVE;e.target.troops=0;e.reason=1;s14_stats_consume(&e);e.id=5;s14_stats_consume(&e);assert(balance(1,10)==151);
 assert(!s14_merit_spend(1,10,152,0)&&!s14_merit_spend(1,10,0,0));assert(s14_merit_spend(1,10,50,0)&&balance(1,10)==101);
 assert(!s14_merit_spend(1,10,50,0)&&balance(1,10)==101); // Retry cannot charge again.
 e.id=6;e.kind=S14_ROUND_DAMAGE;e.target=unit(3,20,2,1000);e.target_after=unit(3,20,2,750);s14_stats_consume(&e);assert(balance(1,10)==103&&merits[10].enemy_remainder==50);
 e.id=7;e.source.force=2;s14_stats_consume(&e);assert(balance(1,10)==103);e.source.force=1;
 assert(!s14_merit_spend(2,10,1,50)&&!s14_merit_spend(1,20,1,0)&&balance(1,20)==0);
 assert(s14_stats_save(1,saved,1));s14_stats_load_begin();assert(!s14_merit_spend(1,10,1,50));s14_stats_load_end(2,100,1,saved,1,0);assert(balance(2,10)==103&&merits[10].spent==50);
 s14_stats_load_end(2,100,1,old,1,0);assert(balance(2,10)==0);s14_stats_load_end(2,100,1,saved,1,0);assert(balance(2,10)==103);
 // Real v1 byte layout and CRC: one-time backfill, followed by v2 save.
 wchar_t path[MAX_PATH];assert(path_for(legacy,path,1));Checkpoint *p=calloc(1,sizeof(*p));assert(p);
 memcpy(p->magic,"S14CAREER.v1",12);p->version=1;p->bytes=sizeof(LegacyCheckpoint);memcpy(p->save_hash,legacy,32);p->snapshot=totals;
 p->snapshot.rows[10]=(S14BattleTotals){.valid_mask=31,.enemy_loss=250,.units_routed=2,.officers_injured=1};p->crc=payload_crc(p);
 HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);assert(f!=INVALID_HANDLE_VALUE);DWORD wrote;assert(WriteFile(f,p,p->bytes,&wrote,NULL)&&wrote==p->bytes);CloseHandle(f);free(p);
 assert(s14_stats_checkpoint_owned(path));s14_stats_load_end(3,100,1,legacy,1,0);assert(balance(3,10)==252&&merits[10].enemy_remainder==50);
 assert(s14_merit_spend(3,10,100,0));e.world=3;e.id=8;e.target_after.troops=950;s14_stats_consume(&e);assert(balance(3,10)==153&&merits[10].enemy_remainder==0);
 assert(s14_stats_save(3,legacy,1));s14_stats_load_end(3,100,1,legacy,1,0);assert(balance(3,10)==153&&merits[10].spent==100); // No backfill on v2.
 assert(add(UINT64_MAX,1)==UINT64_MAX&&multiply(UINT64_MAX,100)==UINT64_MAX);
 puts("{\"status\":\"passed\",\"spend_then_earn\":true,\"fractional_damage_carry\":true,\"duplicate_charge_rejected\":true,\"save_rollback\":true,\"legacy_checkpoint_migration\":true,\"officer_isolation\":true}");return 0;
}
