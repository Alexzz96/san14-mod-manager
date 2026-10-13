#include "tech_tree_model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
const S14TechNode s14_tech_nodes[S14_TECH_NODES]={
 {"ready",L"弓兵整备",L"基础最多8点。前一阶任意节点开放下一阶，可跨列学习。进阶暂未接入。",-1,0},
 {"attack.1",L"校弦",L"攻军+5%，防御-2.5%。消耗1点。",0,0},
 {"attack.2",L"劲弓",L"攻军+10%，防御-5%。消耗1点。",1,0},
 {"attack.3",L"破阵",L"攻军+15%，防御-7.5%。消耗1点。",2,0},
 {"defense.1",L"护具",L"防御+5%，攻军-2.5%。消耗1点。",0,1},
 {"defense.2",L"固位",L"防御+10%，攻军-5%。消耗1点。",4,1},
 {"defense.3",L"列盾",L"防御+15%，攻军-7.5%。消耗1点。",5,1},
 {"mobility.1",L"轻装",L"机动+5%。消耗1点。",0,2},
 {"mobility.2",L"疾退",L"机动+10%。消耗1点。",7,2},
 {"mobility.3",L"游射",L"机动+15%。消耗1点。",8,2},
 {"capacity.1",L"扩匣",L"弹药上限+10，基础装填速率-10%。消耗1点。",0,3},
 {"capacity.2",L"备矢",L"弹药上限+10，基础装填速率-10%。消耗1点。",10,3},
 {"capacity.3",L"重箭匣",L"弹药上限+10，基础装填速率-10%。消耗1点。",11,3},
 {"loading.1",L"轻匣",L"弹药上限-5，基础装填速率+25%。消耗1点。",0,4},
 {"loading.2",L"速装",L"弹药上限-5，基础装填速率+25%。消耗1点。",13,4},
 {"loading.3",L"轮换装填",L"弹药上限-5，基础装填速率+25%。消耗1点。",14,4}
};
int s14_tech_choices(int officer,unsigned mask,const S14TroopRegistry *registry,S14TechTroop *out,int capacity){
 int n=0;if(officer<=0||officer>=6000||!out||capacity<=0)return 0;
 for(int id=1;id<=20&&n<capacity;id++)if(mask&(1u<<id)){
  S14TechTroop *t=&out[n++];memset(t,0,sizeof(*t));snprintf(t->id,sizeof(t->id),"native.%d",id);
  snprintf(t->name,sizeof(t->name),"%s",s14_troop_native_carrier_name(id));t->carrier=id;
 }
 for(size_t i=0;i<s14_troop_registry_count(registry)&&n<capacity;i++){
  const S14TroopDefinition *d=s14_troop_registry_at(registry,i);
  if(d&&d->native_carrier>=1&&d->native_carrier<=20&&(mask&(1u<<d->native_carrier))&&s14_troop_commander_allowed(d,officer)){
   S14TechTroop *t=&out[n++];memset(t,0,sizeof(*t));memcpy(t->id,d->id,sizeof(t->id));memcpy(t->name,d->name,sizeof(t->name));t->carrier=d->native_carrier;t->is_mod=1;
  }
 }
 return n;
}
int s14_tech_troop(S14TechSession *s,int index){
 if(!s||!s->current||index<0||index>=s->count)return 0;
 S14TechRecord *r=s->current->records;for(;r&&strcmp(r->troop,s->choices[index].id);r=r->next){}
 if(!r){r=calloc(1,sizeof(*r));if(!r)return 0;strcpy(r->troop,s->choices[index].id);r->learned=1;r->next=s->current->records;s->current->records=r;}
 s->record=r;s->selected=index;strcpy(s->current->troop,r->troop);return 1;
}
int s14_tech_officer(S14TechSession *s,int id,unsigned mask,const S14TroopRegistry *registry){
 if(!s)return 0;s->record=NULL;s->current=NULL;s->selected=-1;
 s->count=s14_tech_choices(id,mask,registry,s->choices,S14_TECH_CHOICES);if(id<=0||id>=6000)return 0;
 S14TechOfficer *p=s->officers;for(;p&&p->id!=id;p=p->next){}
 if(!p){p=calloc(1,sizeof(*p));if(!p)return 0;p->id=id;p->next=s->officers;s->officers=p;}
 s->current=p;if(!s->count)return 1;
 int choice=0;for(int i=0;i<s->count;i++)if(!strcmp(p->troop,s->choices[i].id)){choice=i;break;}
 return s14_tech_troop(s,choice);
}
int s14_tech_status(const S14TechRecord *r,int n){
 if(!r||n<0||n>=S14_TECH_NODES)return 0;if(r->learned&(1u<<n))return 2;
 if(s14_tech_points(r->learned)>=S14_TECH_POINTS)return 0;
 int tier=(n-1)%3;if(tier==0)return 1;
 for(int b=0;b<5;b++)if(r->learned&(1u<<(b*3+tier)))return 1;
 return 0;
}
int s14_tech_upgrade(S14TechSession *s,int node){if(!s||s14_tech_status(s->record,node)!=1)return 0;s->record->learned|=(unsigned short)(1u<<node);return 1;}
int s14_tech_points(unsigned short bits){int count=0;for(int i=1;i<S14_TECH_NODES;i++)if(bits&(1u<<i))count++;return count;}
int s14_tech_mask_valid(unsigned short bits){
 if(!(bits&1)||s14_tech_points(bits)>S14_TECH_POINTS)return 0;
 int tiers[3]={0};for(int i=1;i<S14_TECH_NODES;i++)if(bits&(1u<<i))tiers[(i-1)%3]++;
 return (!tiers[1]||tiers[0])&&(!tiers[2]||tiers[1]);
}
S14TechBonus s14_tech_bonus(unsigned short bits){
 S14TechBonus out={0};if(!s14_tech_mask_valid(bits))return out;
 for(int i=1;i<S14_TECH_NODES;i++)if(bits&(1u<<i)){int branch=(i-1)/3,rank=(i-1)%3+1;
  if(branch==0){out.attack_bp+=500*rank;out.defense_bp-=250*rank;}
  else if(branch==1){out.defense_bp+=500*rank;out.attack_bp-=250*rank;}
  else if(branch==2)out.mobility_bp+=500*rank;
  else if(branch==3){out.capacity+=10;out.recovery_bp-=1000;}
  else {out.capacity-=5;out.recovery_bp+=2500;}
 }return out;
}
void s14_tech_reset(S14TechSession *s){if(!s)return;S14TechOfficer *p=s->officers;while(p){S14TechRecord *r=p->records;while(r){S14TechRecord *next=r->next;free(r);r=next;}S14TechOfficer *next=p->next;free(p);p=next;}memset(s,0,sizeof(*s));s->selected=-1;}
