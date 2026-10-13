#define WIN32_LEAN_AND_MEAN
#include "domestic_native.h"
#include "battle_probe.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct {uintptr_t vtable,handle;} NativeList;
typedef struct {NativeList area,person,project;} NativeBatch;
typedef void* (*Ctor)(void*);
typedef void (*Dtor)(void*);
typedef void (*Candidates)(void*,void*);
typedef int (*Ability)(void*,int,int);
typedef void* (*NativeGetter)(void*);
typedef void (*Append)(void*,void*,int,int);
typedef int (*Settle)(void*);
typedef int (*Development)(void*,void*,int,int,int);
typedef int (*Security)(void*,void*,int,int);
typedef int (*Effect)(void*,int);
typedef int (*Resident)(void*);
typedef int (*GoldCost)(void*,void*);
typedef struct {uintptr_t vtable,node,end;int home,flags;} CityIterator;
typedef void* (*Iterator)(void*,CityIterator*,int);
static uintptr_t image;static int ready,enabled,busy,operation,execution_enabled=S14_DOMESTIC_EXECUTION_ENABLED;
#ifdef S14_SELFTEST
__declspec(dllexport) int S14TestDomesticExecutionEnabled(void){return execution_enabled;}
#endif
static wchar_t init_failure[192];
static Ctor ctor;static Dtor destroy_int,destroy_ptr;static Candidates candidates;static Ability effective;static NativeGetter primary;
static Append append;static Settle settle,length;
static Development development;static Security order_gain;static Effect effect,force_effect;static Resident absent;static NativeGetter force_of_city;static Iterator city_begin,city_end;
static GoldCost gold_cost;
static SRWLOCK lock=SRWLOCK_INIT;
static S14DOptions options;static S14DPlan selection;static S14DResult result,last_manual;
static uint64_t serial,consumed;static ULONGLONG submitted;
static int automatic,auto_fault,auto_day=-1,auto_force,auto_search_ok=1;
static uintptr_t auto_world;static unsigned auto_epoch,pref_epoch;
static S14DOptions auto_preferences;
static unsigned char auto_reserved[6001];static volatile LONG revision;
static wchar_t auto_message[192]=L"自动补齐关闭；手动配置仍可使用。";
static uintptr_t quick_world;static int quick_force,quick_pending;static unsigned quick_epoch;
static SRWLOCK log_lock=SRWLOCK_INIT;
static char log_lines[1024][512];static unsigned log_read,log_write,log_dropped;
static void trace(const char *line){AcquireSRWLockExclusive(&log_lock);if(log_write-log_read<1024){strncpy(log_lines[log_write++%1024],line,511);}else log_dropped++;ReleaseSRWLockExclusive(&log_lock);}
int s14_domestic_next_log(char *out,size_t capacity){if(!out||capacity<512)return 0;AcquireSRWLockExclusive(&log_lock);int ok=log_read!=log_write;if(ok){strcpy(out,log_lines[log_read++%1024]);}else if(log_dropped){snprintf(out,capacity,"{\"event\":\"domestic_log_dropped\",\"count\":%u}\n",log_dropped);log_dropped=0;ok=1;}ReleaseSRWLockExclusive(&log_lock);return ok;}
static int get(uintptr_t a,void *b,size_t n){SIZE_T done=0;return a&&ReadProcessMemory(GetCurrentProcess(),(void*)a,b,n,&done)&&done==n;}
static uintptr_t ptr(uintptr_t a){uintptr_t p=0;get(a,&p,8);return p;}
static unsigned short word(const unsigned char *b){unsigned short x;memcpy(&x,b,2);return x;}
static int zero_bytes(const unsigned char *b,size_t n){for(size_t i=0;i<n;i++)if(b[i])return 0;return 1;}
/* Match the F officer reader's reserved/custom template handling. Only skip
   empty identity/gameplay/effect fields; defaults in padding may be nonzero.
   Validate the object type first, and never skip named or active ID-zero data. */
static int empty_person(const unsigned char b[512]){
 for(int i=0;i<5;i++)if(b[0x124+i]>1)return 0;
 return !word(b+0x10)&&zero_bytes(b+0x12,54)&&zero_bytes(b+0x118,7)&&!b[0x198]&&zero_bytes(b+0x150,18)&&zero_bytes(b+0x168,10);
}
static void name(wchar_t *out,size_t n,const unsigned char *b,int count){size_t i=0;for(;i+1<n&&i<(size_t)count;i++){out[i]=word(b+i*2);if(!out[i])break;}out[i]=0;}
static int signature(uintptr_t r,const unsigned char *b,size_t n){unsigned char actual[16];return n<=16&&get(image+r,actual,n)&&!memcmp(b,actual,n);}
static void cleanup(NativeBatch *b){destroy_int(&b->project);destroy_int(&b->person);b->area.vtable=image+0x123f418;destroy_ptr(&b->area);}
int s14_domestic_init(uintptr_t base,uintptr_t end){
 image=base;ready=0;init_failure[0]=0;if(!base||end<=base+0x201c398){wcscpy(init_failure,L"原生模块范围不足，无法校验内政接口。");return 0;}
 const struct {uintptr_t r;unsigned char b[8];int n;} checks[]={
 {0x22480,{0x48,0x89,0x4c,0x24,0x08,0x57},6},{0x1d5ba0,{0x48,0x89,0x5c,0x24,0x08},5},
 {0x1d1e80,{0x48,0x89,0x4c,0x24,0x08},5},{0x62cf40,{0x48,0x8b,0xc4,0x55,0x41,0x56},6},
 {0x20ce20,{0x48,0x8b,0xc4,0x57,0x41,0x54},6},{0x8310,{0x40,0x53,0x48,0x83,0xec,0x30},6},
 {0x5d30f0,{0x40,0x53,0x48,0x83,0xec,0x20},6},{0x2f1fc0,{0x0f,0xb6,0x41,0x3a,0x83,0xf8,0x33},7},
 {0x1d2ba0,{0x48,0x8b,0x41,0x28,0xba,0x00,0x40,0x01},8},
 {0x240e80,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c},8},{0x2449f0,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c},8},
 {0x284600,{0x40,0x57,0x48,0x83,0xec,0x40,0x48,0xc7},8},{0x211870,{0x48,0x89,0x54,0x24,0x10,0x53,0x48,0x83},8},
 {0x211cd0,{0x48,0x89,0x54,0x24,0x10,0x53,0x48,0x83},8},{0x20f700,{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74},8},
 {0x20a060,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b},8},{0x3ca6c0,{0x41,0x57,0x48,0x83,0xec,0x50,0x48,0xc7},8},
 {0x2f6030,{0x48,0x83,0xec,0x28,0x44,0x8b,0x41,0x18},8},
 {0x2402f0,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c},8}};
 for(size_t i=0;i<sizeof(checks)/sizeof(checks[0]);i++)if(!signature(checks[i].r,checks[i].b,checks[i].n)){swprintf(init_failure,192,L"原生内政入口 0x%llx 的签名不匹配，预览与应用暂不可用。",(unsigned long long)checks[i].r);return 0;}
 ctor=(Ctor)(image+0x22480);append=(Append)(image+0x1d5ba0);settle=(Settle)(image+0x1d1e80);length=(Settle)(image+0x1d2ba0);
 destroy_int=(Dtor)(image+0x8310);destroy_ptr=(Dtor)(image+0x5d30f0);candidates=(Candidates)(image+0x62cf40);effective=(Ability)(image+0x20ce20);primary=(NativeGetter)(image+0x2f1fc0);
 development=(Development)(image+0x240e80);order_gain=(Security)(image+0x2449f0);effect=(Effect)(image+0x284600);absent=(Resident)(image+0x20f700);force_of_city=(NativeGetter)(image+0x20a060);force_effect=(Effect)(image+0x3ca6c0);city_begin=(Iterator)(image+0x211870);city_end=(Iterator)(image+0x211cd0);gold_cost=(GoldCost)(image+0x2402f0);ready=1;return 1;
}
static int person_traits(S14DPerson *p,const unsigned char *b,const S14DSnapshot *s,wchar_t *why){for(int j=0;j<9;j++){int id=word(b+0x150+j*2);if(id>=S14_D_TRAITS||(id&&!s->traits[id].defined)){swprintf(why,256,L"武将 #%u 引用的个性 #%d 缺失或越界。",(unsigned)word(b+0x10),id);return 0;}p->traits[j]=id;}return 1;}
static void aura_name(S14DCity *city,const S14DSnapshot *s,const unsigned char *b){
 wchar_t f[20],l[20],text[96];name(f,20,b+0x12,9);name(l,20,b+0x24,9);
 for(int j=0;j<9;j++){int id=word(b+0x150+j*2);if(id<1||id>=356)continue;const S14DTrait *t=&s->traits[id];if(t->effect[0]!=45&&t->effect[1]!=45&&t->effect[0]!=74&&t->effect[1]!=74)continue;
 swprintf(text,96,L"%ls%ls·%ls；",f,l,t->name);if(wcslen(city->aura_description)+wcslen(text)<255)wcscat(city->aura_description,text);}
}
static int iterator_advance(const S14DSnapshot *s,CityIterator *it,wchar_t *why){
 uintptr_t next=0;if(!get(it->node+8,&next,8)){wcscpy(why,L"城市驻留列表链接不可读。");return 0;}it->node=next;
 for(int steps=0;it->node!=it->end;steps++){
  uintptr_t person=0,canonical=0;unsigned char b[512];if(steps>=6000||!get(it->node,&person,8)||!get(person,b,512)||ptr(person)!=image+0x12a00d0||word(b+0x10)<1||word(b+0x10)>=6001||!get(s->world+0x148+word(b+0x10)*8,&canonical,8)||canonical!=person){wcscpy(why,L"城市驻留列表节点不兼容。");return 0;}
  if(((int(*)(void*,void*))(image+0x2f6030))(it,(void*)it->node))break;
  if(!get(it->node+8,&next,8)){wcscpy(why,L"城市驻留列表链接不可读。");return 0;}it->node=next;
 }
 return 1;
}
static int read_traits(S14DSnapshot *s,wchar_t *why){
 /* Modded saves redefine personalities, so read names/effects from this session. */
 uintptr_t trait_table[S14_D_TRAITS];if(!get(s->world+0x7d440,trait_table,sizeof(trait_table))||!trait_table[1]){wcscpy(why,L"原生个性目录不可读或基础目录缺失。");return 0;}
 for(int id=1;id<356;id++){uintptr_t t=trait_table[id];if(!t)continue;unsigned char b[224];if(!get(t,b,sizeof(b))||ptr(t)!=image+0x12a0658){swprintf(why,256,L"原生个性目录不兼容（编号 %d）。",id);return 0;}
 S14DTrait *out=&s->traits[id];out->defined=1;name(out->name,16,b+0x10,6);name(out->description,112,b+0x3c,51);for(int j=0;j<2;j++){out->effect[j]=b[0xb6+j*4];out->value[j]=(short)word(b+0xb8+j*4);}
 out->domestic=wcsstr(out->description,L"县长")||wcsstr(out->description,L"县府")||wcsstr(out->description,L"所属城市")||wcsstr(out->description,L"开发")||wcsstr(out->description,L"治安")||wcsstr(out->description,L"人口");}
 return 1;
}
static int forecasts(S14DSnapshot *s,wchar_t *why){
 int incumbent[6001]={0};for(int a=1;a<501;a++)if(s->areas[a].active&&s->areas[a].officer)incumbent[s->areas[a].officer]=1;
 for(int p=1;p<S14_D_PEOPLE;p++)if(s->people[p].active)for(int j=0;j<9;j++){int id=s->people[p].traits[j];if(id&&!s->traits[id].defined){swprintf(why,256,L"武将 #%d 引用的个性 #%d 缺失。",p,id);return 0;}}
 for(int c=1;c<52;c++)if(s->cities[c].active&&(!options.city||options.city==c)){
 S14DCity *city=&s->cities[c];CityIterator it={0},end={0};city_begin((void*)city->address,&it,0);city_end((void*)city->address,&end,0);
 if(it.vtable!=image+0x129fbb8||end.vtable!=it.vtable||ptr(it.vtable+8)!=image+0x2f6030||it.end!=end.node||it.home!=end.home||it.flags!=0||end.flags!=0){wcscpy(why,L"城市驻留武将迭代接口未通过校验。");return 0;}
 int steps=0;while(it.node!=end.node){unsigned char b[512];uintptr_t person=ptr(it.node);if(!it.node||++steps>6000||!get(person,b,512)||ptr(person)!=image+0x12a00d0){wcscpy(why,L"城市驻留列表不可读，未计算个性加成。");return 0;}
 int id=word(b+0x10);uintptr_t canonical=0;if(id<1||id>=S14_D_PEOPLE||!get(s->world+0x148+id*8,&canonical,8)||canonical!=person){wcscpy(why,L"城市驻留武将与武将表身份不一致。");return 0;}
 for(int j=0;j<9;j++){int trait=word(b+0x150+j*2);if(trait>=S14_D_TRAITS||(trait&&!s->traits[trait].defined)){swprintf(why,256,L"驻城武将 #%d 引用的个性 #%d 缺失或越界。",id,trait);return 0;}}
 if(!absent((void*)person)){int dev=effect((void*)person,45),law=effect((void*)person,74);if(dev>city->development_aura)city->development_aura=dev;if(law>city->security_aura)city->security_aura=law;aura_name(city,s,b);}
 if(!iterator_advance(s,&it,why))return 0;}
 uintptr_t force=(uintptr_t)force_of_city((void*)city->address);if(!force||ptr(force)!=image+0x129fe58){wcscpy(why,L"城市势力政策接口未通过校验。");return 0;}
 int dev_policy=force_effect((void*)force,29),law_policy=force_effect((void*)force,30);
 if(city->development_aura>10000||city->security_aura>10000||dev_policy<0||dev_policy>10000||law_policy<0||law_policy>10000){wcscpy(why,L"城市个性或政策数值超出核对范围。");return 0;}
 for(int id=1;id<6001;id++)if(s->people[id].active&&s->people[id].city==c&&(incumbent[id]||(s->people[id].eligible&&!s->people[id].protected_job&&!s->people[id].busy))){S14DPerson *p=&s->people[id];p->forecast_start=s->forecast_count;p->forecast_count=city->area_count;
 for(int a=1;a<501;a++)if(s->areas[a].active&&s->areas[a].city==c){if(s->forecast_count>=S14_D_FORECASTS){wcscpy(why,L"候选与区域组合超过128000项，请缩小候选规模后重试。");return 0;}S14DArea *area=&s->areas[a];S14DForecast *f=&s->forecasts[s->forecast_count++];
 for(int d=1;d<=3;d++){int n=development((void*)area->address,(void*)p->address,d,city->development_aura,dev_policy);if(n<1||n>100000){wcscpy(why,L"原生开发增量超出核对范围。");return 0;}int room=1000-area->development[d-1];f->development[d-1]=n<room?n:room;}
 int n=order_gain((void*)area->address,(void*)p->address,city->security_aura,law_policy);if(n<1||n>100000){wcscpy(why,L"原生治安增量超出核对范围。");return 0;}int room=100-area->security;f->security=n<room?n:room;
 f->gold_cost=gold_cost((void*)area->address,(void*)p->address);if(f->gold_cost<0||f->gold_cost>100000){wcscpy(why,L"每旬开发金费用超出核对范围。");return 0;}
 }}}
 s->forecast_ready=1;return 1;
}
static int capture(S14DSnapshot *s,wchar_t *why){
 why[0]=0;memset(s,0,sizeof(*s));s->epoch=s14_battle_session_epoch();s->world=ptr(image+0x1fc91d0);
 if(s14_battle_is_loading()||!s->world){wcscpy(why,L"尚未载入存档或正在读档。");return 0;}
 uintptr_t settings=ptr(s->world+0x85130);unsigned char clock[7];
 if(!get(settings+0x34,clock,7)||!clock[0]||!clock[6]||clock[6]>51){wcscpy(why,L"无法确认玩家势力。");return 0;}
 s->force=clock[6];uintptr_t group=(uintptr_t)primary((void*)settings);unsigned char g[32];
 /* +0x11 is the district ordinal: 1 is the primary district. The live
    save has force 29 and district table index 18; these IDs are distinct. */
 if(!get(group,g,sizeof(g))||ptr(group)!=image+0x129fec8||g[0x10]!=clock[6]||g[0x11]!=1){wcscpy(why,L"玩家直辖军团身份未通过校验。");return 0;}
 for(int i=1;i<52;i++)if(ptr(s->world+0xde40+i*8)==group)s->group=i;
 if(!s->group||!get(image+0x18ebec8,&s->unit_cost,4)||s->unit_cost<0||s->unit_cost>200){wcscpy(why,L"直辖军团账户或原生命令书单价不可读。");return 0;}
 int balance=g[0x14];if(balance>200){wcscpy(why,L"命令书余额超出原生范围。");return 0;}
 uintptr_t city_table[S14_D_CITIES],area_table[S14_D_AREAS],people_table[S14_D_PEOPLE];
 if(!get(s->world+0xdaa8,city_table,sizeof(city_table))||!get(s->world+0x6c860,area_table,sizeof(area_table))||!get(s->world+0x148,people_table,sizeof(people_table))){wcscpy(why,L"城市、府或武将指针目录不可读，未使用部分数据。");return 0;}
 int jobs[6001]={0},reserved[6001]={0},home[52]={0};
 for(int c=1;c<52;c++){
  uintptr_t address=city_table[c];if(!address)continue;unsigned char b[208];
  if(!get(address,b,sizeof(b))||ptr(address)!=image+0x129fd10){swprintf(why,256,L"城市表类型不兼容（槽位 %d）。",c);return 0;}
  uintptr_t getter=ptr(ptr(address)+0x80);if(getter<image||getter>=image+0x1200000){wcscpy(why,L"城市军团接口不兼容。");return 0;}
  int recruit=word(b+0xca),train=word(b+0xcc);if(recruit>=6001||train>=6001){wcscpy(why,L"募兵或训练官员编号不兼容。");return 0;}jobs[recruit]=jobs[train]=1;
  uintptr_t account=(uintptr_t)((NativeGetter)getter)((void*)address);if(account!=group)continue;
  S14DCity *city=&s->cities[c];city->active=1;city->account=s->group;city->balance=balance;city->address=address;name(city->name,40,b+0x12,16);home[c]=word(b+0x4e);
  /* +0x1a belongs to CDistrictData, not CityData. Native doctrine 3/2/4
     selects project 1/2/3. Never interpret a city name byte as a doctrine. */
  city->direction=g[0x1a]==3?1:g[0x1a]==2?2:g[0x1a]==4?3:0;
 }
 for(int a=1;a<501;a++){
  uintptr_t address=area_table[a];if(!address)continue;unsigned char b[0x78];
  if(!get(address,b,sizeof(b))||ptr(address)!=image+0x129ff98){swprintf(why,256,L"区域表不可读或类型不兼容（槽位 %d）。",a);return 0;}
  int c=b[0x35],incumbent=word(b+0x36);if(incumbent>=6001){wcscpy(why,L"区域官员编号不兼容。");return 0;}
  if(c>=1&&c<S14_D_CITIES&&!city_table[c]){swprintf(why,256,L"府 #%d 引用的城市 #%d 缺失。",a,c);return 0;}
  if(c<1||c>=52||!s->cities[c].active){reserved[incumbent]=1;continue;}
  int tile=word(b+0x3a);unsigned char force=0;
  uintptr_t hex=tile<48400?ptr(s->world+0xdfe0+tile*8):0;
  if(!hex||!get(hex+0x14,&force,1)){wcscpy(why,L"区域领地归属不可读。");return 0;}
  if(force!=clock[6]){reserved[incumbent]=1;continue;}
  S14DArea *area=&s->areas[a];area->active=1;area->city=c;area->officer=word(b+0x36);area->project=b[0x72];area->address=address;name(area->name,32,b+0x10,9);
  area->ordinal=s->cities[c].area_count++;for(int d=0;d<3;d++){area->development[d]=word(b+0x3c+d*2);if(area->development[d]>1000){wcscpy(why,L"区域开发值不兼容。");return 0;}}area->security=b[0x54];if(area->security>100){wcscpy(why,L"区域治安值不兼容。");return 0;}
  if(area->officer>=6001||area->project>3){wcscpy(why,L"区域官员或开发编号不兼容。");return 0;}
 }
 if(!read_traits(s,why))return 0;
 for(int c=1;c<52;c++)if(s->cities[c].active){
  NativeBatch list={0};ctor(&list);
  if(!list.area.handle||!list.person.handle||!list.project.handle){cleanup(&list);wcscpy(why,L"原生临时候选列表分配失败。");return 0;}
  candidates((void*)s->cities[c].address,&list.area);
  unsigned id=0,limit=0;uintptr_t table=ptr(image+0x201c360),node=0;
  int ok=get(list.area.handle,&id,4)&&get(image+0x201c390,&limit,4)&&table&&id<limit&&get(table+id*8,&node,8);
  int steps=0;unsigned char seen[6001]={0};
  while(ok&&node){
   uintptr_t p=ptr(node);unsigned char b[512];if(++steps>6000||!get(p,b,sizeof(b))||ptr(p)!=image+0x12a00d0){ok=0;break;}
   int person=word(b+0x10);if(person<1||person>=6001||seen[person]||people_table[person]!=p){ok=0;break;}seen[person]=1;
   /* Keep only same-home, same-current city and primary district. The native
      list also includes associated bases, which are intentionally excluded. */
   if(b[0x118]==s->group&&word(b+0x11a)==home[c]&&word(b+0x11c)==home[c]){
    S14DPerson *o=&s->people[person];o->active=1;o->owned=1;o->city=c;o->address=p;o->eligible=1;o->protected_job=jobs[person]||reserved[person];o->busy=word(b+0x196)!=0;
    if(!person_traits(o,b,s,why)){ok=0;break;}
    wchar_t first[20],last[20];name(first,20,b+0x12,9);name(last,20,b+0x24,9);swprintf(o->name,40,L"%ls%ls",first,last);
    for(int k=0;k<5;k++){o->ability[k]=effective((void*)p,k,0xfff);if(o->ability[k]<0||o->ability[k]>1000)ok=0;}
   }
   node=ptr(node+8);
  }
  cleanup(&list);if(!ok){if(!why[0])wcscpy(why,L"原生候选列表结构或有效能力不兼容。");return 0;}
 }
 /* Read the entire player's roster for lists, without promoting non-native
    candidates into appointable officers (field armies/delegated groups included). */
 for(int id=1;id<6001;id++)if(!s->people[id].active){
  uintptr_t p=people_table[id];if(!p)continue;unsigned char b[512];
  if(!get(p,b,512)||ptr(p)!=image+0x12a00d0){swprintf(why,256,L"武将名单对象不兼容（槽位 %d，读取或类型校验失败）。",id);return 0;}
  if(empty_person(b))continue;
  if(word(b+0x10)!=id){swprintf(why,256,L"武将名单对象不兼容（槽位 %d，实际编号 %u）。",id,(unsigned)word(b+0x10));return 0;}
  if(b[0x11e]>9){wcscpy(why,L"武将名单身份状态不兼容。");return 0;}if(b[0x11e]==0||b[0x11e]==7||b[0x11e]==9)continue;
  int district=b[0x118];if(district<1||district>51)continue;
  uintptr_t owner=district==s->group?group:ptr(s->world+0xde40+district*8);unsigned char owner_data[32];
  if(!get(owner,owner_data,32)||ptr(owner)!=image+0x129fec8||owner_data[0x10]!=s->force)continue;
  S14DPerson *o=&s->people[id];o->active=o->owned=1;o->address=p;o->busy=word(b+0x196)!=0;o->protected_job=jobs[id]||reserved[id];
  if(!person_traits(o,b,s,why))return 0;
  for(int c=1;c<52;c++)if(home[c]&&word(b+0x11a)==home[c])o->city=c;
  wchar_t first[20],last[20];name(first,20,b+0x12,9);name(last,20,b+0x24,9);swprintf(o->name,40,L"%ls%ls",first,last);
  for(int k=0;k<5;k++){o->ability[k]=effective((void*)p,k,0xfff);if(o->ability[k]<0||o->ability[k]>1000){wcscpy(why,L"自势力武将有效能力超出核对范围。");return 0;}}
 }
 /* Incumbents outside the eligible list remain protected, with effective
    before-values still displayed. Do not infer eligibility from raw stats. */
 for(int a=1;a<501;a++)if(s->areas[a].active&&s->areas[a].officer){int id=s->areas[a].officer;S14DPerson *o=&s->people[id];if(o->active)continue;unsigned char b[512];uintptr_t p=people_table[id];
  if(!get(p,b,512)||ptr(p)!=image+0x12a00d0||word(b+0x10)!=id){wcscpy(why,L"已有官员身份不兼容。");return 0;}if(!person_traits(o,b,s,why))return 0;o->active=1;o->city=s->areas[a].city;o->address=p;o->busy=1;o->protected_job=jobs[id];wchar_t f[20],l[20];name(f,20,b+0x12,9);name(l,20,b+0x24,9);swprintf(o->name,40,L"%ls%ls",f,l);for(int k=0;k<5;k++){o->ability[k]=effective((void*)p,k,0xfff);if(o->ability[k]<0||o->ability[k]>1000){wcscpy(why,L"已有官员有效能力超出核对范围。");return 0;}}}
 if(!forecasts(s,why))return 0;
 if(s->epoch!=s14_battle_session_epoch()||s14_battle_is_loading()||ptr(image+0x1fc91d0)!=s->world){wcscpy(why,L"读取期间发生会话切换，快照已取消。");return 0;}return 1;
}
static int list_count(const NativeList *l,uintptr_t slot){unsigned id=0;uintptr_t n=0,table=ptr(image+slot);return get(l->handle,&id,4)&&id<0x14000&&table&&get(table+id*8,&n,8)&&n<=6000?(int)n:-1;}
static int batch(const S14DSnapshot *s,const S14DPlan *p,int city){
 NativeBatch b={0};ctor(&b);int n=0;
 if(!b.area.handle||!b.person.handle||!b.project.handle){cleanup(&b);return 0;}
 for(int i=0;i<p->count;i++){const S14DRow *r=&p->rows[i];if(r->city==city&&r->selected&&!r->completed&&r->reason==S14_D_CHANGE){append(&b,(void*)s->areas[r->area].address,r->after,r->project);n++;}}
 /* Verify all three native list lengths individually: min(lengths) can hide
    an allocation failure and would debit/execute a partial association. */
 int expected=length(&b);int ok=expected==n&&list_count(&b.area,0x201c378)==n&&list_count(&b.person,0x19e0c38)==n&&list_count(&b.project,0x19e0c38)==n;
 int done=-1;if(ok){done=settle(&b);ok=done==n;}char line[512];snprintf(line,sizeof(line),"{\"event\":\"domestic_native_batch\",\"request_id\":%llu,\"city\":%d,\"rows\":%d,\"list_minimum\":%d,\"native_return\":%d,\"native_ok\":%s,\"game_thread\":%lu}\n",(unsigned long long)result.id,city,n,expected,done,ok?"true":"false",(unsigned long)GetCurrentThreadId());trace(line);cleanup(&b);return ok;
}
static int (*capture_backend)(S14DSnapshot*,wchar_t*)=capture;
static int (*batch_backend)(const S14DSnapshot*,const S14DPlan*,int)=batch;
static void auto_complete(void){
 auto_search_ok=result.status==S14_D_APPLIED||result.status==S14_D_PREVIEW;auto_fault=!auto_search_ok;
 if(auto_search_ok){memset(auto_reserved,0,sizeof(auto_reserved));for(int a=1;a<501;a++)if(result.snapshot.areas[a].active&&result.snapshot.areas[a].officer)auto_reserved[result.snapshot.areas[a].officer]=1;for(int p=1;p<6001;p++)if(result.snapshot.people[p].active&&result.snapshot.people[p].protected_job)auto_reserved[p]=1;
  swprintf(auto_message,192,L"本轮补齐 %d 项；%.*ls",result.completed,140,result.status==S14_D_APPLIED?L"已按原生结算扣费，探索使用剩余人选与命令书。":result.message);
 }else swprintf(auto_message,192,L"自动补齐暂停：%.*ls 请关闭再开启，或读档后核对；探索暂不派遣。",110,result.message);
 auto_message[191]=0;
}
static void finish(int status,const wchar_t *message){result.status=status;wcsncpy(result.message,message,255);result.message[255]=0;if(operation==1||operation==2)last_manual=result;operation=0;}
void s14_domestic_enabled(int on){AcquireSRWLockExclusive(&lock);enabled=on&&ready;if(!enabled&&operation&&!busy)finish(S14_D_CANCELLED,L"接口关闭，待执行请求已取消。");ReleaseSRWLockExclusive(&lock);}
int s14_domestic_can_apply(wchar_t *why,size_t n){const wchar_t *msg=!ready?(init_failure[0]?init_failure:L"原生入口校验未通过，无法配置。"):!enabled?L"需启用总开关并接入战略更新回调。":!execution_enabled?L"此构建未启用原生任命执行。":L"";if(why&&n){wcsncpy(why,msg,n-1);why[n-1]=0;}return ready&&enabled&&execution_enabled;}
uint64_t s14_domestic_request(const S14DOptions *o){if(!o)return 0;AcquireSRWLockExclusive(&lock);if(operation||busy){ReleaseSRWLockExclusive(&lock);return 0;}options=*o;memset(&result,0,sizeof(result));result.id=++serial;result.preview=serial;result.status=S14_D_PENDING;result.snapshot.epoch=s14_battle_session_epoch();
 if(!ready||!enabled){s14_domestic_can_apply(result.message,256);result.status=S14_D_UNAVAILABLE;operation=0;last_manual=result;}
 else {wcscpy(result.message,L"等待战略规划回调读取原生快照…");operation=1;submitted=GetTickCount64();}uint64_t id=serial;ReleaseSRWLockExclusive(&lock);return id;}
uint64_t s14_domestic_submit(uint64_t preview,const S14DOptions *o,const S14DPlan *p){
 if(!o||!p||!preview)return 0;AcquireSRWLockExclusive(&lock);
 if(operation||busy||result.preview!=preview||consumed==preview||(result.status!=S14_D_PREVIEW&&result.status!=S14_D_STALE)||!ready||!enabled||!execution_enabled){ReleaseSRWLockExclusive(&lock);return 0;}
 options=*o;selection=*p;result.id=++serial;result.status=S14_D_PENDING;operation=2;consumed=preview;submitted=GetTickCount64();wcscpy(result.message,L"等待游戏线程复核并应用；重复点击不会重复提交。");uint64_t id=serial;char line[512];snprintf(line,sizeof(line),"{\"event\":\"domestic_submit\",\"request_id\":%llu,\"preview_id\":%llu,\"epoch\":%u,\"city_filter\":%d,\"mode\":%d,\"policy\":%d,\"security_first\":%d}\n",(unsigned long long)id,(unsigned long long)preview,result.snapshot.epoch,o->city,o->mode,o->policy,o->security_first);trace(line);ReleaseSRWLockExclusive(&lock);return id;
}
void s14_domestic_cancel(void){AcquireSRWLockExclusive(&lock);if(!busy&&operation&&operation!=3)finish(S14_D_CANCELLED,L"窗口关闭，待执行请求已取消。");ReleaseSRWLockExclusive(&lock);}
int s14_domestic_result(uint64_t id,S14DResult *out){AcquireSRWLockExclusive(&lock);if(id==result.id&&operation&&!busy&&(s14_battle_is_loading()||result.snapshot.epoch!=s14_battle_session_epoch()))finish(S14_D_CANCELLED,L"读档或会话变化，待执行请求已取消。");if(id==result.id&&operation&&!busy&&GetTickCount64()-submitted>10000)finish(S14_D_CANCELLED,L"等待规划回调超时，请回到战略地图后刷新。");int current=id&&id==result.id&&!busy,cached=id&&id==last_manual.id&&id!=result.id;int ok=current||cached;if(ok&&out){*out=current?result:last_manual;if(cached&&(out->status==S14_D_PREVIEW||out->status==S14_D_STALE)){out->status=S14_D_STALE;out->plan.valid=0;wcscpy(out->message,L"后台已处理新请求，原预览已失效；正在重新读取快照。");}}ReleaseSRWLockExclusive(&lock);return ok;}
void s14_domestic_service(void *state){
 DWORD error=GetLastError();AcquireSRWLockExclusive(&lock);if(!operation||busy){ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
 if(!enabled||s14_battle_is_loading()||result.snapshot.epoch!=s14_battle_session_epoch()){int auto_request=operation==3;finish(S14_D_CANCELLED,L"接口关闭或存档会话变化，未执行。");if(auto_request)auto_complete();ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
 int phase=-1;if(!state||!get((uintptr_t)state+0x470,&phase,4)||phase<0||phase>2){ReleaseSRWLockExclusive(&lock);SetLastError(error);return;}
 busy=1;int op=operation;ReleaseSRWLockExclusive(&lock);
 S14DSnapshot *live=calloc(1,sizeof(*live));S14DPlan *planned=calloc(1,sizeof(*planned));wchar_t why[256]=L"无法分配快照内存。";int status=S14_D_UNAVAILABLE;
 if(live&&planned&&capture_backend(live,why)&&s14_domestic_plan(live,&options,planned)){
  if(op==3){s14_domestic_select_budget(live,&options,planned);selection=*planned;result.snapshot=*live;}
  if(op==1){status=S14_D_PREVIEW;wcscpy(why,planned->reason);}
  else if(op==3&&(live->world!=auto_world||live->force!=auto_force)){status=S14_D_FAILED;wcscpy(why,L"自动补齐期间玩家势力或会话发生变化，未执行。");}
  else if(op==3&&!planned->valid){status=S14_D_FAILED;wcscpy(why,L"自动补齐方案无效，未执行。");}
  else if(op==3&&!s14_domestic_budget(live,planned,NULL,NULL,NULL)){status=S14_D_PREVIEW;wcscpy(why,L"没有可补齐的空缺，或剩余命令书不足；本轮未任命。");}
  else if(op==2&&!s14_domestic_same(live,&result.snapshot)){status=S14_D_STALE;wcscpy(why,L"预览已过期，已重新生成；旧方案未执行，请重新选择并应用。");}
  else if(!planned->valid||!selection.valid||planned->count!=selection.count){status=S14_D_FAILED;wcscpy(why,L"方案结构已改变，未执行。");}
  else{
   int valid=1;for(int i=0;i<planned->count;i++){S14DRow *r=&planned->rows[i],*t=&selection.rows[i];if(r->area!=t->area||r->before!=t->before||r->after!=t->after||r->project!=t->project||r->group!=t->group||t->completed)valid=0;r->selected=t->selected;
    for(int j=0;j<i;j++)if(r->group==planned->rows[j].group&&r->reason==S14_D_CHANGE&&planned->rows[j].reason==S14_D_CHANGE&&r->selected!=planned->rows[j].selected)valid=0;}
   int n=0,cost=0,left=0;if(!valid||!s14_domestic_budget(live,planned,&n,&cost,&left)){status=S14_D_FAILED;wcscpy(why,L"关联选择无效、没有实际变更或命令书余额不足，未执行。");}
   else{
    status=S14_D_APPLIED;result.completed=0;result.remaining=n;
    for(int c=1;c<52;c++){
     int num=0;for(int i=0;i<planned->count;i++)if(planned->rows[i].city==c&&planned->rows[i].selected&&planned->rows[i].reason==S14_D_CHANGE)num++;
     if(!num)continue;
     S14DSnapshot *before=calloc(1,sizeof(*before));int fresh=before&&capture_backend(before,why)&&s14_domestic_same(live,before);free(before);
     if(!fresh||s14_battle_is_loading()||live->epoch!=s14_battle_session_epoch()){status=S14_D_FAILED;break;}
     int balance_before=live->cities[c].balance;int native_ok=batch_backend(live,planned,c);
     /* Read back both assignments and exact native account debit. No retry
        after any uncertain settlement and no fabricated rollback. */
     S14DSnapshot *after=calloc(1,sizeof(*after));int ok=after&&capture_backend(after,why);int account=live->cities[c].account;
     if(ok)for(int i=0;i<planned->count;i++)if(planned->rows[i].city==c&&planned->rows[i].selected&&planned->rows[i].reason==S14_D_CHANGE){S14DRow *r=&planned->rows[i];if(after->areas[r->area].officer!=r->after||after->areas[r->area].project!=r->project)ok=0;else {r->completed=1;result.completed++;result.remaining--;}}
     if(ok&&after->cities[c].balance!=live->cities[c].balance-num*live->unit_cost)ok=0;
     if(ok){for(int d=1;d<52;d++)if(live->cities[d].active&&live->cities[d].account==account)live->cities[d].balance=after->cities[d].balance;
      for(int i=0;i<planned->count;i++)if(planned->rows[i].city==c&&planned->rows[i].completed){S14DRow *r=&planned->rows[i];live->areas[r->area].officer=r->after;live->areas[r->area].project=r->project;}
      /* Derived forecasts legitimately change with neighbouring appointments.
         Keep ownership, people, traits, development, security and budget strict. */
      memcpy(live->forecasts,after->forecasts,sizeof(live->forecasts));
      if(!s14_domestic_same(live,after))ok=0;
     }
     char line[512];snprintf(line,sizeof(line),"{\"event\":\"domestic_batch_result\",\"request_id\":%llu,\"city\":%d,\"account\":%d,\"rows\":%d,\"unit_cost\":%d,\"balance_before\":%d,\"balance_after\":%d,\"native_ok\":%s,\"readback_verified\":%s,\"game_thread\":%lu}\n",(unsigned long long)result.id,c,account,num,live->unit_cost,balance_before,after?after->cities[c].balance:-1,native_ok?"true":"false",ok?"true":"false",(unsigned long)GetCurrentThreadId());trace(line);
     for(int i=0;i<planned->count;i++){S14DRow *r=&planned->rows[i];if(r->city!=c||!r->selected||r->reason!=S14_D_CHANGE)continue;snprintf(line,sizeof(line),"{\"event\":\"domestic_assignment\",\"request_id\":%llu,\"city\":%d,\"area\":%d,\"officer_before\":%d,\"officer_planned\":%d,\"officer_observed\":%d,\"project_planned\":%d,\"project_observed\":%d,\"completed\":%s}\n",(unsigned long long)result.id,c,r->area,r->before,r->after,after?after->areas[r->area].officer:-1,r->project,after?after->areas[r->area].project:-1,r->completed?"true":"false");trace(line);}
     if(after&&after->world==live->world&&after->epoch==live->epoch)*live=*after;free(after);if(!ok||!native_ok){status=S14_D_FAILED;break;}
    }
    swprintf(why,256,status==S14_D_APPLIED?L"已完成 %d 项；请在原生内政界面核对并正常保存游戏。":L"结算或复读失败，后续批次已停止。核对完成 %d 项，未完成/待核对 %d 项；不会自动重试。",result.completed,result.remaining);
   }
  }
 }else if(planned&&planned->reason[0])wcscpy(why,planned->reason);
 {int cities=0,areas=0,people=0;if(live){for(int c=1;c<52;c++)cities+=!!live->cities[c].active;for(int a=1;a<501;a++)areas+=!!live->areas[a].active;for(int p=1;p<6001;p++)people+=!!live->people[p].active;}
 char message[256]={0},line[512];int chars=(int)wcslen(why);if(chars>70)chars=70;WideCharToMultiByte(CP_UTF8,0,why,chars,message,sizeof(message)-1,NULL,NULL);
 snprintf(line,sizeof(line),"{\"event\":\"domestic_snapshot\",\"request_id\":%llu,\"phase\":%d,\"status\":%d,\"cities\":%d,\"areas\":%d,\"people\":%d,\"rows\":%d,\"forecast_ready\":%d,\"force\":%d,\"group\":%d,\"message\":\"%.240s\"}\n",(unsigned long long)result.id,op,status,cities,areas,people,planned?planned->count:0,live?live->forecast_ready:0,live?live->force:0,live?live->group:0,message);trace(line);}
 if(op>=2){char line[512];snprintf(line,sizeof(line),"{\"event\":\"domestic_result\",\"request_id\":%llu,\"status\":%d,\"completed\":%d,\"remaining\":%d,\"epoch\":%u,\"automatic\":%s,\"game_thread\":%lu}\n",(unsigned long long)result.id,status,result.completed,result.remaining,s14_battle_session_epoch(),op==3?"true":"false",(unsigned long)GetCurrentThreadId());trace(line);}
 /* Failed previews contain no usable plan; do not publish their partially
    collected officers/cities as though the snapshot completed successfully. */
 if(op==1&&status!=S14_D_PREVIEW&&status!=S14_D_STALE){if(live){memset(live,0,sizeof(*live));live->epoch=s14_battle_session_epoch();}if(planned)memset(planned,0,sizeof(*planned));}
 AcquireSRWLockExclusive(&lock);if(live)result.snapshot=*live;if(planned)result.plan=*planned;
 if(status==S14_D_STALE){result.preview=result.id;consumed=0;}finish(status,why);if(op==3)auto_complete();if(op>=2&&result.completed)InterlockedIncrement(&revision);busy=0;ReleaseSRWLockExclusive(&lock);free(live);free(planned);SetLastError(error);
}
void s14_domestic_auto_configure(int on){AcquireSRWLockExclusive(&lock);if(on&&!automatic){auto_fault=0;wcscpy(auto_message,L"自动补齐已开启：确认进行时先补内政，再派遣探索。");}if(!on&&automatic)wcscpy(auto_message,L"自动补齐关闭；手动配置仍可使用。");automatic=!!on;ReleaseSRWLockExclusive(&lock);}
void s14_domestic_preferences(const S14DOptions *o){if(!o)return;AcquireSRWLockExclusive(&lock);auto_preferences=*o;auto_preferences.mode=S14_D_FILL;auto_preferences.city=0;pref_epoch=s14_battle_session_epoch();ReleaseSRWLockExclusive(&lock);}
int s14_domestic_preferences_read(S14DOptions *o){AcquireSRWLockShared(&lock);int ok=o&&pref_epoch==s14_battle_session_epoch();if(ok)*o=auto_preferences;ReleaseSRWLockShared(&lock);return ok;}
unsigned s14_domestic_revision(void){return (unsigned)InterlockedCompareExchange(&revision,0,0);}
void s14_domestic_auto_status(wchar_t *out,size_t n){if(!out||!n)return;AcquireSRWLockShared(&lock);wcsncpy(out,auto_message,n-1);out[n-1]=0;ReleaseSRWLockShared(&lock);}
int s14_domestic_search_allowed(int person){AcquireSRWLockShared(&lock);int ok=person>0&&person<6001&&(!automatic||!auto_reserved[person]);ReleaseSRWLockShared(&lock);return ok;}
int s14_domestic_auto_turn(void *state,uintptr_t world,int day,int force){
 AcquireSRWLockExclusive(&lock);if(!automatic||!enabled||!ready||!execution_enabled){ReleaseSRWLockExclusive(&lock);return 1;}
 unsigned epoch=s14_battle_session_epoch();if(s14_battle_is_loading()||!world||day<0||force<1||force>51){ReleaseSRWLockExclusive(&lock);return 0;}
 if(epoch!=auto_epoch||world!=auto_world||force!=auto_force||day<auto_day){auto_epoch=epoch;auto_world=world;auto_force=force;auto_day=-1;auto_fault=0;memset(auto_reserved,0,sizeof(auto_reserved));}
 if(pref_epoch!=epoch){memset(auto_preferences.locked,0,sizeof(auto_preferences.locked));memset(auto_preferences.excluded,0,sizeof(auto_preferences.excluded));if(!auto_preferences.policy){auto_preferences.policy=S14_D_MAX_DEVELOPMENT;auto_preferences.security_first=1;}pref_epoch=epoch;}
 if(auto_fault){ReleaseSRWLockExclusive(&lock);return 0;}if(auto_day==day){int ok=auto_search_ok;ReleaseSRWLockExclusive(&lock);return ok;}
 if(operation||busy){wcscpy(auto_message,L"手动请求正在处理，本轮先保留内政；探索暂不派遣。");ReleaseSRWLockExclusive(&lock);return 0;}
 int phase=-1;if(!state||!get((uintptr_t)state+0x470,&phase,4)||phase<0||phase>2){wcscpy(auto_message,L"本轮未处于已验证的战略阶段，未自动任命。");ReleaseSRWLockExclusive(&lock);return 0;}
 auto_day=day;auto_search_ok=0;options=auto_preferences;options.mode=S14_D_FILL;options.city=0;memset(&result,0,sizeof(result));result.id=++serial;result.preview=serial;result.status=S14_D_PENDING;result.snapshot.epoch=epoch;operation=3;submitted=GetTickCount64();uint64_t id=serial;
 char line[512];snprintf(line,sizeof(line),"{\"event\":\"domestic_auto_submit\",\"request_id\":%llu,\"day\":%d,\"force\":%d,\"epoch\":%u,\"policy\":%d,\"security_first\":%d}\n",(unsigned long long)id,day,force,epoch,options.policy,options.security_first);trace(line);ReleaseSRWLockExclusive(&lock);
 s14_domestic_service(state);
 AcquireSRWLockExclusive(&lock);
 int ok=auto_search_ok;ReleaseSRWLockExclusive(&lock);return ok;
}
void s14_domestic_progress(uintptr_t world,int day,int force){
 (void)world;(void)day;(void)force; /* Turn completion does not arm setup. */
}
void s14_domestic_planning(uintptr_t world,int day,int force){
 AcquireSRWLockExclusive(&lock);
 if(s14_battle_is_loading()||!world||day<0||force<1||force>51){quick_pending=0;quick_world=0;}
 else if(quick_epoch!=s14_battle_session_epoch()||quick_world!=world||quick_force!=force){quick_epoch=s14_battle_session_epoch();quick_world=world;quick_force=force;quick_pending=1;}
 ReleaseSRWLockExclusive(&lock);
}
int s14_domestic_quick_pending(void){AcquireSRWLockShared(&lock);int ok=quick_pending&&!s14_battle_is_loading()&&quick_epoch==s14_battle_session_epoch();ReleaseSRWLockShared(&lock);return ok;}
void s14_domestic_quick_ack(void){AcquireSRWLockExclusive(&lock);quick_pending=0;ReleaseSRWLockExclusive(&lock);}
