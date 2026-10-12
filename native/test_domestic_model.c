#include "domestic.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static S14DSnapshot s;static S14DOptions o;static S14DPlan p;
static void setup(int n){memset(&s,0,sizeof(s));memset(&o,0,sizeof(o));s.unit_cost=2;s.world=1;s.epoch=4;s.cities[1]=(S14DCity){.active=1,.account=1,.balance=40};for(int a=1;a<=n;a++)s.areas[a]=(S14DArea){.active=1,.city=1,.project=1};}
static void person(int id,int intelligence,int politics,int war){s.people[id]=(S14DPerson){.active=1,.eligible=1,.city=1,.ability={50,war,intelligence,politics,50}};}
static int best_total;
static void forecasts_fixture(int n,int k){s.forecast_ready=1;s.cities[1].area_count=n;for(int a=1;a<=n;a++){s.areas[a].ordinal=a-1;s.areas[a].security=60;}
 for(int id=1;id<=k;id++){s.people[id].forecast_start=s.forecast_count;s.people[id].forecast_count=n;for(int a=1;a<=n;a++)s.forecasts[s.forecast_count++]=(S14DForecast){.development={1,1,1},.security=1,.gold_cost=10};}}
static S14DForecast *forecast(int a,int id){return &s.forecasts[s.people[id].forecast_start+a-1];}
static int best_dev,best_order;
static void brute_forecast(int a,int n,int k,int used,int dev,int order){
 if(a>n){int better=o.security_first?(order>best_order||(order==best_order&&dev>best_dev)):(dev>best_dev||(dev==best_dev&&order>best_order));if(better){best_dev=dev;best_order=order;}return;}
 brute_forecast(a+1,n,k,used,dev,order);for(int id=1;id<=k;id++)if(!(used&(1<<id))){S14DForecast *f=forecast(a,id);int d=o.policy==S14_D_COMMERCIAL?f->development[0]:o.policy==S14_D_AGRICULTURAL?f->development[1]:f->development[0];if(o.policy==S14_D_MAX_DEVELOPMENT){if(f->development[1]>d)d=f->development[1];if(f->development[2]>d)d=f->development[2];}brute_forecast(a+1,n,k,used|(1<<id),dev+d,order+f->security);}}
static void brute(int a,int n,int k,int used,int total){if(a>n){if(total>best_total)best_total=total;return;}int stat=s.areas[a].project==2?3:s.areas[a].project==3?1:2;brute(a+1,n,k,used,total);for(int id=1;id<=k;id++)if(!(used&(1<<id)))brute(a+1,n,k,used|(1<<id),total+s.people[id].ability[stat]);}
int main(void){
 setup(3);person(1,90,10,10);person(2,10,91,10);person(3,10,10,92);s.areas[2].project=2;s.areas[3].project=3;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==1&&p.rows[1].after==2&&p.rows[2].after==3);int n,c,l;assert(s14_domestic_budget(&s,&p,&n,&c,&l)&&n==3&&c==6&&l==34);
 s.cities[1].balance=4;assert(!s14_domestic_budget(&s,&p,&n,&c,&l));s14_domestic_select(&p,2,0);assert(s14_domestic_budget(&s,&p,&n,&c,&l)&&n==2&&c==4&&l==0);
 setup(3);person(1,90,10,10);assert(s14_domestic_plan(&s,&o,&p));n=0;for(int i=0;i<3;i++)n+=p.rows[i].after!=0;assert(n==1);
 setup(2);person(1,20,100,0);person(2,100,20,0);s.areas[1].officer=1;s.areas[2].officer=2;s.areas[2].project=2;o.mode=S14_D_OPTIMIZE;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==2&&p.rows[1].after==1&&p.rows[0].group==p.rows[1].group);s14_domestic_select(&p,0,0);assert(!p.rows[0].selected&&!p.rows[1].selected);s14_domestic_select(&p,1,1);assert(p.rows[0].selected&&p.rows[1].selected);
 o.locked[1]=1;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==1&&p.rows[0].reason==S14_D_LOCKED&&p.rows[1].after==2);o.locked[1]=0;o.excluded[2]=1;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[1].after==2&&p.rows[1].reason==S14_D_PROTECTED);
 o.excluded[2]=0;s.people[1].busy=1;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==1&&p.rows[0].reason==S14_D_PROTECTED);
 s.people[1].busy=0;s.people[1].protected_job=1;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==1&&p.rows[1].after==2);
 setup(2);person(1,99,0,0);person(2,80,0,0);s.areas[1].officer=1;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==1&&p.rows[1].after==2);s.people[2].protected_job=1;assert(s14_domestic_plan(&s,&o,&p));assert(!p.rows[1].after);
 setup(1);s.areas[1].project=0;person(1,99,30,20);s.cities[1].direction=2;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].project==2&&p.rows[0].first);s.cities[1].direction=0;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].project==1);o.excluded[1]=1;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].project==0&&!p.rows[0].first&&!p.rows[0].selected);
 setup(2);person(1,80,80,80);person(2,80,80,80);s.areas[1].officer=2;s.areas[2].officer=1;o.mode=S14_D_OPTIMIZE;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==2&&p.rows[1].after==1);assert(!s14_domestic_budget(&s,&p,&n,&c,&l)&&c==0);
 setup(1);person(2,80,0,0);person(1,80,0,0);assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==1);s.people[2].ability[0]=70;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==2);s.people[1].ability[0]=70;s.people[1].ability[4]=90;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==1);
 s.people[1].city=2;assert(s14_domestic_plan(&s,&o,&p));assert(p.rows[0].after==2);o.city=2;assert(s14_domestic_plan(&s,&o,&p)&&p.count==0);
 /* Random small assignments checked against exhaustive enumeration. */
 srand(314);for(int trial=0;trial<300;trial++){int n=1+rand()%5,k=rand()%6;setup(n);for(int a=1;a<=n;a++)s.areas[a].project=1+rand()%3;for(int id=1;id<=k;id++)person(id,rand()%101,rand()%101,rand()%101);best_total=0;brute(1,n,k,0,0);assert(s14_domestic_plan(&s,&o,&p));int total=0;unsigned used=0;for(int i=0;i<p.count;i++){total+=p.rows[i].after_value;int id=p.rows[i].after;if(id){assert(!(used&(1u<<id)));used|=1u<<id;}}assert(total==best_total);S14DPlan repeat;assert(s14_domestic_plan(&s,&o,&repeat)&&!memcmp(&p,&repeat,sizeof(p)));}
 setup(1);person(1,100,10,10);person(2,60,10,10);forecasts_fixture(1,2);forecast(1,1)->development[0]=10;forecast(1,2)->development[0]=20;o.policy=S14_D_COMMERCIAL;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].after==2&&p.rows[0].after_gain==20); /* trait gain can beat raw ability */
 forecast(1,1)->security=30;forecast(1,2)->security=5;o.security_first=1;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].after==1);forecast(1,1)->security=forecast(1,2)->security=0;s.areas[1].security=100;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].after==2);
 o.policy=S14_D_MAX_DEVELOPMENT;forecast(1,1)->development[2]=40;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].after==1&&p.rows[0].project==3);o.policy=S14_D_AGRICULTURAL;forecast(1,2)->development[1]=50;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].after==2&&p.rows[0].project==2);
 s.areas[1].officer=2;s.areas[1].project=2;assert(s14_domestic_plan(&s,&o,&p)&&!p.rows[0].selected);o.mode=S14_D_OPTIMIZE;o.policy=S14_D_COMMERCIAL;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].project==1&&p.rows[0].selected);o.locked[1]=1;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].project==2&&!p.rows[0].selected);o.locked[1]=0;s.people[2].protected_job=1;assert(s14_domestic_plan(&s,&o,&p)&&p.rows[0].project==2&&!p.rows[0].selected);
 s.forecast_ready=0;assert(!s14_domestic_plan(&s,&o,&p)&&wcsstr(p.reason,L"未通过"));
 setup(2);person(1,90,90,90);forecasts_fixture(2,1);s.areas[1].officer=1;forecast(1,1)->development[0]=1;forecast(2,1)->development[0]=20;o.mode=S14_D_OPTIMIZE;o.policy=S14_D_COMMERCIAL;assert(s14_domestic_plan(&s,&o,&p));assert(!p.rows[0].after&&!p.rows[0].project&&p.rows[1].after==1&&p.rows[0].group==p.rows[1].group);assert(s14_domestic_budget(&s,&p,&n,&c,&l)&&n==2&&c==4);s14_domestic_select(&p,1,0);assert(!p.rows[0].selected&&!p.rows[1].selected);
 srand(111);for(int trial=0;trial<300;trial++){int n=1+rand()%4,k=rand()%5;setup(n);for(int id=1;id<=k;id++)person(id,100-id,10,10);forecasts_fixture(n,k);o.policy=1+rand()%3;o.security_first=rand()%2;
 for(int id=1;id<=k;id++)for(int a=1;a<=n;a++){S14DForecast *f=forecast(a,id);for(int d=0;d<3;d++)f->development[d]=rand()%25;f->security=rand()%20;}best_dev=best_order=-1;brute_forecast(1,n,k,0,0,0);assert(s14_domestic_plan(&s,&o,&p));int dev=0,order=0;unsigned used=0;for(int i=0;i<p.count;i++){S14DRow *r=&p.rows[i];dev+=r->after_gain;order+=r->after_security;if(r->after){assert(!(used&(1u<<r->after)));used|=1u<<r->after;}if(r->after&&o.policy==S14_D_COMMERCIAL)assert(r->project==1);if(r->after&&o.policy==S14_D_AGRICULTURAL)assert(r->project==2);}assert(dev==best_dev&&order==best_order);S14DPlan repeat;assert(s14_domestic_plan(&s,&o,&repeat)&&!memcmp(&p,&repeat,sizeof(p)));}
 /* Budget trimming never breaks swaps and spends each shared account once. */
 setup(4);s.cities[1].balance=2;s.cities[2]=s.cities[1];s.cities[2].account=2;memset(&p,0,sizeof(p));p.valid=1;p.count=4;
 for(int i=0;i<4;i++)p.rows[i]=(S14DRow){.city=i==3?2:1,.area=i+1,.reason=S14_D_CHANGE,.group=i<2?0:i,.after_gain=i==2?80:20,.after_security=i==2?5:10};
 o.security_first=1;assert(s14_domestic_select_budget(&s,&o,&p)==2&&!p.rows[0].selected&&!p.rows[1].selected&&p.rows[2].selected&&p.rows[3].selected);assert(s14_domestic_budget(&s,&p,&n,&c,&l)&&n==2&&c==4&&l==0);
 s.cities[1].balance=4;assert(s14_domestic_select_budget(&s,&o,&p)==3&&p.rows[0].selected&&p.rows[1].selected&&!p.rows[2].selected&&p.rows[3].selected);
 o.security_first=0;assert(s14_domestic_select_budget(&s,&o,&p)==2&&p.rows[2].selected&&p.rows[3].selected);s.unit_cost=0;assert(s14_domestic_select_budget(&s,&o,&p)==4);
 printf("{\"maximum_weight_bruteforce_cases\":600,\"native_gain_cases\":300,\"three_development_policies\":true,\"security_priority\":true,\"gain_over_ability\":true,\"fill\":true,\"shortage\":true,\"directions\":true,\"retention\":true,\"stable_ties\":true,\"protected_jobs\":true,\"excluded\":true,\"locks\":true,\"swap_groups\":true,\"city_scope\":true,\"budget\":true,\"automatic_partial_budget\":true,\"budget_preserves_linked_groups\":true,\"budget_account_isolation\":true}\n");return 0;
}
