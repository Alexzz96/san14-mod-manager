#include "tech_tree_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 S14TechSession s={0};const S14TroopRegistry *registry=s14_troop_builtin_registry();S14TechTroop choices[S14_TECH_CHOICES];
 assert(s14_tech_choices(254,0,registry,choices,S14_TECH_CHOICES)==0);
 assert(s14_tech_choices(254,(1u<<1)|(1u<<2),registry,choices,S14_TECH_CHOICES)==3);
 assert(!strcmp(choices[2].id,"san14.xianzhen")&&choices[2].carrier==1);
 assert(s14_tech_officer(&s,254,1u<<3,registry)&&s.record->learned==1);
 assert(!s14_tech_upgrade(&s,2)&&!s14_tech_upgrade(&s,0)&&!s14_tech_upgrade(&s,16));
 int path[]={1,5,9,4,2,3,10,14};
 for(int i=0;i<8;i++){assert(s14_tech_upgrade(&s,path[i]));assert(!s14_tech_upgrade(&s,path[i]));assert(s14_tech_points(s.record->learned)==i+1);}
 assert(!s14_tech_upgrade(&s,6));assert(s14_tech_mask_valid(s.record->learned));
 S14TechBonus bonus=s14_tech_bonus(s.record->learned);assert(bonus.attack_bp==2250&&bonus.defense_bp==0&&bonus.mobility_bp==1500&&bonus.capacity==5&&bonus.recovery_bp==1500);
 for(int b=0;b<5;b++){S14TechRecord r={.learned=1};S14TechSession t={.record=&r};for(int rank=0;rank<3;rank++)assert(s14_tech_upgrade(&t,1+b*3+rank));S14TechBonus x=s14_tech_bonus(r.learned);
  if(b==0)assert(x.attack_bp==3000&&x.defense_bp==-1500);
  if(b==1)assert(x.defense_bp==3000&&x.attack_bp==-1500);
  if(b==2)assert(x.mobility_bp==3000&&!x.attack_bp&&!x.defense_bp);
  if(b==3)assert(x.capacity==30&&x.recovery_bp==-3000);
  if(b==4)assert(x.capacity==-15&&x.recovery_bp==7500);
 }
 assert(!s14_tech_mask_valid(0)&&!s14_tech_mask_valid(5)&&!s14_tech_mask_valid(65535));
 unsigned short learned=s.record->learned;assert(s14_tech_officer(&s,253,1u<<3,registry)&&s.record->learned==1);assert(s14_tech_officer(&s,254,1u<<3,registry)&&s.record->learned==learned);
 s14_tech_reset(&s);assert(!s.officers&&!s.record);puts("{\"status\":\"passed\",\"cross_column_prerequisites\":true,\"eight_point_limit\":true,\"node_delta_basis_points\":true,\"qualification_filter\":true,\"no_advanced_nodes\":true}");
}
