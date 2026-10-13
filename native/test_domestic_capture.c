/* Exercise the production reader and batch adapter against isolated native
   object/list fixtures. Never attach to, launch, or modify the game. */
#include "domestic_native.c"
#include <assert.h>
unsigned s14_battle_session_epoch(void){return 1;}int s14_battle_is_loading(void){return 0;}
static unsigned char world[0x85200],settings[64],group_data[32],foreign_group[32],cities[52][208],areas[501][120],people_data[7][512],tiles[4][32];
static unsigned char trait_data[356][224],force_data[32];
static unsigned char empty_person_data[512],later_person_data[512];
static uintptr_t heads[4],ptr_counts[4],int_counts[4],nodes[6][2];static unsigned ids[3]={0,1,2};
static int constructions,destructions,dtor_step,settlements,appended,fail_append,effective_calls;
static struct {void *area;int person,project;} changes[501];
static void *group_of(void *city){return city==cities[1]?group_data:foreign_group;}
static void *player_of(void *s){assert(s==settings);return group_data;}
static void *construct(void *arg){NativeBatch *b=arg;constructions++;dtor_step=0;b->area=(NativeList){image+0x123f418,(uintptr_t)&ids[0]};b->person=(NativeList){image+0x123e210,(uintptr_t)&ids[1]};b->project=(NativeList){image+0x123e210,(uintptr_t)&ids[2]};ptr_counts[0]=int_counts[1]=int_counts[2]=0;appended=0;return b;}
static void destroy_number(void *arg){NativeList *l=arg;assert((dtor_step==0&&l->handle==(uintptr_t)&ids[2])||(dtor_step==1&&l->handle==(uintptr_t)&ids[1]));dtor_step++;l->handle=0;}
static void destroy_pointer(void *arg){NativeList *l=arg;assert(dtor_step==2&&l->vtable==image+0x123f418&&l->handle==(uintptr_t)&ids[0]);destructions++;l->handle=0;}
static void enumerate(void *city,void *list){assert(city==cities[1]);NativeList *l=list;assert(l->handle==(uintptr_t)&ids[0]);heads[0]=(uintptr_t)nodes[0];ptr_counts[0]=5;}
static int effective_value(void *person,int stat,int flags){effective_calls++;assert(stat>=0&&stat<5&&flags==0xfff);return ((unsigned char*)person)[0x124+stat]+5;}
static void push(void *batch,void *area,int id,int project){(void)batch;assert(id>=0&&id<6001&&project>=0&&project<4);changes[appended].area=area;changes[appended].person=id;changes[appended].project=project;appended++;ptr_counts[0]++;int_counts[1]++;if(!fail_append)int_counts[2]++;}
static int min_length(void *arg){(void)arg;int n=(int)ptr_counts[0];if((int)int_counts[1]<n)n=(int)int_counts[1];if((int)int_counts[2]<n)n=(int)int_counts[2];return n;}
static int resident(void *p){return ((unsigned char*)p)[0x196]!=0;}
static int trait_effect(void *p,int e){int id=*(unsigned short*)((unsigned char*)p+0x10);return e==45&&id==2?50:e==74&&id==2?500:0;}
static int policy_effect(void *f,int e){assert(f==force_data&&(e==29||e==30));return e==29?1:2;}
static void *city_force(void *c){assert(c==cities[1]);return force_data;}
static void *begin_city(void *c,CityIterator *it,int flags){assert(c==cities[1]&&!flags);*it=(CityIterator){image+0x129fbb8,(uintptr_t)nodes,0,11,0};return it;}
static void *end_city(void *c,CityIterator *it,int flags){assert(c==cities[1]&&!flags);*it=(CityIterator){image+0x129fbb8,0,0,11,0};return it;}
static int match_city(void *arg,void *node){CityIterator *it=arg;unsigned char *p=*(unsigned char**)node;return *(unsigned short*)(p+0x11a)==it->home;}
static int develop(void *area,void *person,int d,int aura,int policy){assert(area==areas[1]&&person&&d>=1&&d<=3&&aura==50&&policy==1);return 10+d;}
static int security_gain(void *area,void *person,int aura,int policy){assert(area==areas[1]&&person&&aura==500&&policy==2);return 20;}
static int gold(void *a,void *p){assert(a==areas[1]&&p);return 8;}
static int commit(void *arg){(void)arg;settlements++;int charged=0;for(int i=0;i<appended;i++){unsigned char *a=changes[i].area;charged+=*(unsigned short*)(a+0x36)!=changes[i].person||a[0x72]!=changes[i].project;}
 /* Native settlement removes incoming actors from old appointments first,
    then writes the whole association. A swap must be one batch. */
 for(int i=0;i<appended;i++)if(changes[i].person)for(int a=1;a<501;a++)if(areas[a]!=changes[i].area&&*(unsigned short*)(areas[a]+0x36)==changes[i].person){*(unsigned short*)(areas[a]+0x36)=0;areas[a][0x72]=0;}
 for(int i=0;i<appended;i++){unsigned char *a=changes[i].area;*(unsigned short*)(a+0x36)=changes[i].person;a[0x72]=changes[i].project;}group_data[0x14]-=charged*2;return appended;}
static void jump(size_t rva,void *target){unsigned char b[]={0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};memcpy(b+2,&target,8);memcpy((void*)(image+rva),b,sizeof(b));}
int main(void){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 image=(uintptr_t)VirtualAlloc(NULL,0x2020000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(image);*(void**)(image+0x1fc91d0)=world;*(void**)(world+0x85130)=settings;settings[0x34]=220;settings[0x3a]=3;
 *(uintptr_t*)group_data=image+0x129fec8;group_data[0x10]=3;group_data[0x11]=1;group_data[0x14]=20;group_data[0x15]=9;group_data[0x1a]=3;*(void**)(world+0xde40+5*8)=group_data;
 *(void**)(image+0x201c360)=heads;*(void**)(image+0x201c378)=ptr_counts;*(unsigned*)(image+0x201c390)=4;*(void**)(image+0x19e0c38)=int_counts;*(int*)(image+0x18ebec8)=2;
 jump(0x1000,group_of);*(uintptr_t*)(image+0x129fd10+0x80)=image+0x1000;
 jump(0x2f6030,match_city);*(uintptr_t*)(image+0x129fbb8+8)=image+0x2f6030;*(uintptr_t*)force_data=image+0x129fe58;
 for(int id=1;id<356;id++){*(void**)(world+0x7d440+id*8)=trait_data[id];*(uintptr_t*)trait_data[id]=image+0x12a0658;}
 wcscpy((wchar_t*)(trait_data[110]+0x10),L"振兴");wcscpy((wchar_t*)(trait_data[110]+0x3c),L"担任县长时，每旬开发增长");trait_data[110][0xb6]=33;*(short*)(trait_data[110]+0xb8)=30;
 for(int c=1;c<52;c++){*(uintptr_t*)cities[c]=image+0x129fd10;*(void**)(world+0xdaa8+c*8)=cities[c];}wcscpy((wchar_t*)(cities[1]+0x12),L"襄阳");*(unsigned short*)(cities[1]+0x4e)=11;*(unsigned short*)(cities[1]+0xca)=2;
 for(int a=1;a<501;a++){*(uintptr_t*)areas[a]=image+0x129ff98;*(void**)(world+0x6c860+a*8)=areas[a];}
 for(int t=1;t<=3;t++){*(void**)(world+0xdfe0+t*8)=tiles[t];tiles[t][0x14]=t==2?4:3;areas[t][0x35]=t==3?2:1;*(unsigned short*)(areas[t]+0x3a)=t;areas[t][0x72]=1;}
 *(unsigned short*)(areas[1]+0x3c)=997;areas[1][0x54]=95;
 for(int id=1;id<=6;id++){unsigned char *p=people_data[id];*(uintptr_t*)p=image+0x12a00d0;*(unsigned short*)(p+0x10)=id;p[0x118]=5;p[0x11e]=4;*(unsigned short*)(p+0x11a)=11;*(unsigned short*)(p+0x11c)=11;wcscpy((wchar_t*)(p+0x12),L"曹");wcscpy((wchar_t*)(p+0x24),L"操");p[0x126]=80;*(void**)(world+0x148+id*8)=p;}
 people_data[3][0x11c]=12;people_data[4][0x196]=1;people_data[5][0x118]=7;
 *(unsigned short*)(people_data[1]+0x150)=110;
 for(int i=0;i<5;i++){nodes[i][0]=(uintptr_t)people_data[i+1];nodes[i][1]=i==4?0:(uintptr_t)nodes[i+1];}
 ctor=construct;destroy_int=destroy_number;destroy_ptr=destroy_pointer;candidates=enumerate;effective=effective_value;primary=player_of;append=push;settle=commit;length=min_length;ready=enabled=execution_enabled=1;
 development=develop;order_gain=security_gain;gold_cost=gold;effect=trait_effect;force_effect=policy_effect;force_of_city=city_force;city_begin=begin_city;city_end=end_city;absent=resident;
 S14DSnapshot *s=calloc(1,sizeof(*s));S14DPlan p;S14DOptions o={0};wchar_t why[256];group_data[0x11]=2;assert(!capture(s,why));group_data[0x11]=1;assert(capture(s,why));assert(s->group==5&&s->cities[1].account==5&&s->cities[1].balance==20&&!s->cities[2].active&&s->cities[1].direction==1);assert(s->areas[1].active&&!s->areas[2].active&&!s->areas[3].active);assert(s->people[1].ability[2]==85&&s->people[2].protected_job&&s->people[3].owned&&!s->people[3].eligible&&s->people[4].busy&&!s->people[5].active);assert(constructions==destructions);
 assert(s14_domestic_plan(s,&o,&p)&&p.rows[0].after==1);fail_append=1;assert(!batch(s,&p,1)&&!settlements&&constructions==destructions);fail_append=0;assert(batch(s,&p,1)&&settlements==1&&group_data[0x14]==18&&*(unsigned short*)(areas[1]+0x36)==1&&constructions==destructions);
 const S14DForecast *f=s14_domestic_forecast(s,1,1);assert(s->forecast_ready&&f&&f->development[0]==3&&f->development[1]==12&&f->security==5&&f->gold_cost==8&&s->traits[110].domestic&&s->traits[110].effect[0]==33&&s->people[1].traits[0]==110&&s->cities[1].development_aura==50&&s->cities[1].security_aura==500);
 *(unsigned short*)(areas[1]+0x36)=6;assert(capture(s,why)&&s->people[6].owned&&!s->people[6].eligible&&s->people[6].ability[2]==85);o.mode=S14_D_OPTIMIZE;assert(s14_domestic_plan(s,&o,&p)&&p.rows[0].after==6&&p.rows[0].reason==S14_D_PROTECTED);
 /* Reserved/custom slots can be allocated without a person identity. Their
    presence must not hide valid roster members at later table indices. */
 *(void**)(world+0x148+3001*8)=empty_person_data;*(uintptr_t*)empty_person_data=image+0x12a00d0;
 empty_person_data[0x129]=100;*(unsigned short*)(empty_person_data+0x186)=65535;empty_person_data[0x1e8]=7;*(unsigned short*)(empty_person_data+0x1f8)=65535;
 memcpy(later_person_data,people_data[6],512);*(unsigned short*)(later_person_data+0x10)=5001;*(void**)(world+0x148+5001*8)=later_person_data;
 assert(capture(s,why)&&!s->people[3001].active&&s->people[5001].owned&&!s->people[5001].eligible);
 memset(empty_person_data+0x124,1,5);assert(capture(s,why)&&s->people[5001].owned);
 empty_person_data[0x124]=10;assert(!capture(s,why));memset(empty_person_data+0x124,0,5);
 empty_person_data[0x12]=1;assert(!capture(s,why));empty_person_data[0x12]=0;
 *(unsigned short*)(empty_person_data+0x150)=110;assert(!capture(s,why));*(unsigned short*)(empty_person_data+0x150)=0;
 empty_person_data[0x11e]=4;assert(!capture(s,why));empty_person_data[0x11e]=0;
 *(unsigned short*)(later_person_data+0x10)=5002;assert(!capture(s,why));*(unsigned short*)(later_person_data+0x10)=5001;
 *(uintptr_t*)empty_person_data=image+0x129fd10;assert(!capture(s,why));*(uintptr_t*)empty_person_data=image+0x12a00d0;
 *(unsigned short*)(areas[1]+0x36)=3001;assert(!capture(s,why));*(unsigned short*)(areas[1]+0x36)=6;
 assert(capture(s,why)&&s->people[6].owned&&s->people[5001].owned);
 /* Sparse unused directories are legal; referenced missing objects and wrong
    types must still stop capture before any native forecast is attempted. */
 *(void**)(world+0xdaa8+51*8)=NULL;*(void**)(world+0x6c860+500*8)=NULL;*(void**)(world+0x7d440+355*8)=NULL;
 assert(capture(s,why)&&s->cities[1].active&&s->areas[1].active&&s->forecast_ready&&!s->traits[355].defined);
 int earlier_effective_calls=effective_calls;*(void**)(world+0x7d440+110*8)=NULL;assert(!capture(s,why)&&wcsstr(why,L"个性 #110")&&effective_calls==earlier_effective_calls);*(void**)(world+0x7d440+110*8)=trait_data[110];
 *(unsigned short*)(people_data[5]+0x150)=355;assert(!capture(s,why)&&wcsstr(why,L"驻城武将"));*(unsigned short*)(people_data[5]+0x150)=0;
 *(void**)(world+0xdaa8+2*8)=NULL;assert(!capture(s,why)&&wcsstr(why,L"城市 #2"));*(void**)(world+0xdaa8+2*8)=cities[2];
 *(void**)(world+0x148+1*8)=NULL;assert(!capture(s,why));*(void**)(world+0x148+1*8)=people_data[1];
 *(uintptr_t*)trait_data[354]=image+0x129fd10;assert(!capture(s,why));*(uintptr_t*)trait_data[354]=image+0x12a0658;
 *(unsigned short*)(people_data[1]+0x150)=356;assert(!capture(s,why));*(unsigned short*)(people_data[1]+0x150)=110;
 *(void**)(world+0x148+5001*8)=NULL;*(void**)(world+0x148+6000*8)=later_person_data;*(unsigned short*)(later_person_data+0x10)=6000;
 assert(capture(s,why)&&s->people[6000].owned&&!s->people[6000].eligible);
 *(void**)(world+0x148+6000*8)=NULL;*(void**)(world+0x148+5001*8)=later_person_data;*(unsigned short*)(later_person_data+0x10)=5001;
 nodes[4][1]=(uintptr_t)nodes[4];assert(!capture(s,why));nodes[4][1]=0;
 *(void**)(world+0xdfe0+1*8)=NULL;assert(!capture(s,why));*(void**)(world+0xdfe0+1*8)=tiles[1];
 assert(capture(s,why)&&s->areas[1].active&&s->cities[1].active&&constructions==destructions);
 /* Real settlement semantics for swaps and an implicit vacancy, including
    charges for both displayed rows rather than a hidden removal. */
 memset(&p,0,sizeof(p));p.count=2;p.valid=1;p.rows[0]=(S14DRow){.area=1,.city=1,.before=1,.after=2,.project=1,.reason=S14_D_CHANGE,.selected=1};p.rows[1]=(S14DRow){.area=2,.city=1,.before=2,.after=1,.project=2,.reason=S14_D_CHANGE,.selected=1};s->areas[2].address=(uintptr_t)areas[2];*(unsigned short*)(areas[1]+0x36)=1;*(unsigned short*)(areas[2]+0x36)=2;areas[1][0x72]=areas[2][0x72]=1;group_data[0x14]=20;assert(batch(s,&p,1)&&*(unsigned short*)(areas[1]+0x36)==2&&*(unsigned short*)(areas[2]+0x36)==1&&areas[2][0x72]==2&&group_data[0x14]==16);
 p.rows[0].after=0;p.rows[0].project=0;p.rows[1].after=2;p.rows[1].project=1;assert(batch(s,&p,1)&&!*(unsigned short*)(areas[1]+0x36)&&!areas[1][0x72]&&*(unsigned short*)(areas[2]+0x36)==2&&group_data[0x14]==12&&constructions==destructions);
 free(s);VirtualFree((void*)image,0,MEM_RELEASE);printf("{\"sparse_unused_directories_and_reference_guards\":true,\"production_snapshot_reader\":true,\"native_forecast_adapter\":true,\"development_security_caps\":true,\"session_trait_definitions\":true,\"protected_city_aura\":true,\"force_group_separation\":true,\"delegated_cities_excluded\":true,\"enemy_area_excluded\":true,\"same_city_candidates\":true,\"effective_ability\":true,\"native_list_lifetime\":true,\"incomplete_append_rejected\":true,\"production_batch_adapter\":true,\"actual_game_calls\":false}\n");return 0;
}
