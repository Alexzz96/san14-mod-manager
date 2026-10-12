#include "domestic.h"
#include <stdlib.h>
#include <string.h>
/* Exact lexicographic matching: security/development (user priority), unchanged
   appointment, leadership, charm, stable ID. No invented money conversion. */
typedef struct {int64_t v[6];} Weight;
static Weight add(Weight a,Weight b,int sign){for(int i=0;i<6;i++)a.v[i]+=sign*b.v[i];return a;}
static int less(Weight a,Weight b){for(int i=0;i<6;i++)if(a.v[i]!=b.v[i])return a.v[i]<b.v[i];return 0;}
static int ability(int project){return project==2?3:project==3?1:2;}
const S14DForecast *s14_domestic_forecast(const S14DSnapshot *s,int area,int person){
 if(!s||!s->forecast_ready||area<1||area>=501||person<1||person>=6001)return NULL;
 const S14DPerson *p=&s->people[person];const S14DArea *a=&s->areas[area];int index=p->forecast_start+a->ordinal;
 return a->city==p->city&&a->ordinal>=0&&a->ordinal<p->forecast_count&&index>=0&&index<s->forecast_count&&index<S14_D_FORECASTS?&s->forecasts[index]:NULL;
}
void s14_domestic_traits_text(const S14DSnapshot *s,int person,wchar_t *out,size_t n){
 if(!out||!n)return;out[0]=0;if(!s||person<1||person>=6001)return;
 for(int j=0;j<9;j++){int id=s->people[person].traits[j];if(id<1||id>=S14_D_TRAITS||!s->traits[id].domestic)continue;int duplicate=0;for(int k=0;k<j;k++)duplicate|=s->people[person].traits[k]==id;if(duplicate)continue;
 size_t used=wcslen(out),len=wcslen(s->traits[id].name);if(used+len+3<n){if(used)wcscat(out,L" / ");wcscat(out,s->traits[id].name);}}
}
static int gain(const S14DSnapshot *s,int area,int person,int project){const S14DForecast *f=s14_domestic_forecast(s,area,person);return project>=1&&project<=3&&f?f->development[project-1]:0;}
static int security(const S14DSnapshot *s,int area,int person){const S14DForecast *f=s14_domestic_forecast(s,area,person);return f?f->security:0;}
static int project_for(const S14DSnapshot *s,const S14DOptions *o,const S14DRow *r,int id){
 if(!id||o->policy==S14_D_KEEP_DIRECTION)return r->project;
 if(o->policy==S14_D_COMMERCIAL)return 1;if(o->policy==S14_D_AGRICULTURAL)return 2;
 int best=1;for(int d=2;d<=3;d++)if(gain(s,r->area,id,d)>gain(s,r->area,id,best))best=d;
 int old=s->areas[r->area].project;if(old>=1&&old<=3&&gain(s,r->area,id,old)==gain(s,r->area,id,best))best=old;
 return best;
}
static int root(S14DPlan *p,int n){while(p->rows[n].group!=n)n=p->rows[n].group;return n;}
static int match(const S14DSnapshot *s,const S14DOptions *options,S14DPlan *p,int *rows,int n,int *people,int k){
 int m=k+n;Weight *u=calloc(n+1,sizeof(*u)),*v=calloc(m+1,sizeof(*v)),*best=calloc(m+1,sizeof(*best));
 int *owner=calloc(m+1,sizeof(int)),*way=calloc(m+1,sizeof(int));unsigned char *used=calloc(m+1,1);
 if(!u||!v||!best||!owner||!way||!used){free(u);free(v);free(best);free(owner);free(way);free(used);return 0;}
 for(int i=1;i<=n;i++){
  owner[0]=i;int j0=0;memset(used,0,m+1);for(int j=1;j<=m;j++)best[j]=(Weight){{INT64_MAX/4,0,0,0,0}};
  do{
   used[j0]=1;int i0=owner[j0],j1=0;Weight delta={{INT64_MAX/4,0,0,0,0}};S14DRow *r=&p->rows[rows[i0-1]];
   for(int j=1;j<=m;j++)if(!used[j]){
    int id=j<=k?people[j-1]:0;const S14DPerson *o=&s->people[id];
    int project=project_for(s,options,r,id),development=s->forecast_ready?gain(s,r->area,id,project):o->ability[ability(project)],order=s->forecast_ready?security(s,r->area,id):0;
    Weight w={{options->security_first?-order:-development,options->security_first?-development:-order,-(id==r->before&&id!=0&&project==s->areas[r->area].project),-o->ability[0],-o->ability[4],id?id:S14_D_PEOPLE}};
    Weight cur=add(add(w,u[i0],-1),v[j],-1);
    if(less(cur,best[j])){best[j]=cur;way[j]=j0;}
    if(less(best[j],delta)){delta=best[j];j1=j;}
   }
   for(int j=0;j<=m;j++)if(used[j]){u[owner[j]]=add(u[owner[j]],delta,1);v[j]=add(v[j],delta,-1);}else if(j)best[j]=add(best[j],delta,-1);
   j0=j1;
  }while(owner[j0]);
  do{int j1=way[j0];owner[j0]=owner[j1];j0=j1;}while(j0);
 }
 for(int j=1;j<=m;j++)if(owner[j]){S14DRow *r=&p->rows[rows[owner[j]-1]];r->after=j<=k?people[j-1]:0;r->project=project_for(s,options,r,r->after);}
 free(u);free(v);free(best);free(owner);free(way);free(used);return 1;
}
int s14_domestic_plan(const S14DSnapshot *s,const S14DOptions *o,S14DPlan *p){
 if(!s||!o||!p)return 0;memset(p,0,sizeof(*p));
 if(o->mode<0||o->mode>1||o->city<0||o->city>=52||o->policy<0||o->policy>3||o->security_first<0||o->security_first>1||s->unit_cost<0||s->unit_cost>200){wcscpy(p->reason,L"配置参数或命令书单价无效。");return 0;}
 if(o->policy!=S14_D_KEEP_DIRECTION&&!s->forecast_ready){wcscpy(p->reason,L"原生开发及治安增量未通过读取校验，三种开发规划暂不可用。");return 0;}
 int occupied[S14_D_PEOPLE]={0},modifiable[S14_D_AREAS]={0};
 for(int a=1;a<501;a++)if(s->areas[a].active){int id=s->areas[a].officer;if(id<0||id>=6001||s->areas[a].city<1||s->areas[a].city>=52||s->areas[a].project<0||s->areas[a].project>3 || (id && occupied[id])){wcscpy(p->reason,L"区域归属、项目或重复任职数据异常。");return 0;}if(id)occupied[id]=a;}
 for(int a=1;a<501;a++){
  const S14DArea *area=&s->areas[a];int c=area->city,id=area->officer;
  if(!area->active||!s->cities[c].active||(o->city && o->city!=c))continue;
  S14DRow *r=&p->rows[p->count];*r=(S14DRow){.area=a,.city=c,.before=id,.after=id,.project=area->project,.group=p->count};p->count++;
  if(o->locked[a])r->reason=S14_D_LOCKED;
  else if(id&&(o->mode==S14_D_FILL))r->reason=S14_D_KEEP;
  else if(id&&(!s->people[id].active||!s->people[id].eligible||s->people[id].city!=c||s->people[id].protected_job||s->people[id].busy||o->excluded[id]))r->reason=S14_D_PROTECTED;
  else {modifiable[a]=1;if(!r->project){int d=s->cities[c].direction;r->project=d>=1&&d<=3?d:1;r->first=1;}}
 }
 for(int c=1;c<52;c++)if(s->cities[c].active){
  int rows[500],people[6000],n=0,k=0;
  for(int r=0;r<p->count;r++)if(p->rows[r].city==c&&modifiable[p->rows[r].area])rows[n++]=r;
  if(!n)continue;
  for(int id=1;id<6001;id++){const S14DPerson *person=&s->people[id];if(person->active&&person->eligible&&person->city==c&&!person->protected_job&&!person->busy&&!o->excluded[id]&&(!occupied[id]||(o->mode==S14_D_OPTIMIZE&&modifiable[occupied[id]])))people[k++]=id;}
  if(s->forecast_ready)for(int j=0;j<n;j++)for(int id=0;id<k;id++)if(!s14_domestic_forecast(s,p->rows[rows[j]].area,people[id])){wcscpy(p->reason,L"同城候选的原生增量数据不完整，未生成方案。");return 0;}
  if(!match(s,o,p,rows,n,people,k)){wcscpy(p->reason,L"无法分配预览内存。");return 0;}
 }
 for(int i=0;i<p->count;i++){
  S14DRow *r=&p->rows[i];if(!r->after&&r->before)r->project=0;r->first=!s->areas[r->area].project&&r->after&&r->project;r->before_value=s->people[r->before].ability[ability(s->areas[r->area].project)];r->after_value=s->people[r->after].ability[ability(r->project)];
  r->before_gain=gain(s,r->area,r->before,s->areas[r->area].project);r->after_gain=gain(s,r->area,r->after,r->project);r->before_security=security(s,r->area,r->before);r->after_security=security(s,r->area,r->after);
  /* Do not spend orders merely to set a direction on an empty, unfilled area. */
  if(!r->after&&!r->before){r->project=s->areas[r->area].project;r->first=0;}
  if(r->after!=r->before||r->project!=s->areas[r->area].project){r->reason=S14_D_CHANGE;r->selected=1;}
  else if(modifiable[r->area])r->reason=r->after?S14_D_KEEP:S14_D_NO_CANDIDATE;
 }
 for(int i=0;i<p->count;i++)if(p->rows[i].reason==S14_D_CHANGE&&p->rows[i].after)for(int j=0;j<p->count;j++)if(i!=j&&p->rows[j].before==p->rows[i].after){int x=root(p,i),y=root(p,j);if(x!=y)p->rows[x].group=y;}
 for(int i=0;i<p->count;i++)p->rows[i].group=root(p,i);
 p->valid=1;wcscpy(p->reason,s->forecast_ready?(o->security_first?L"治安优先：先最大化未满100区域的恢复，再最大化每旬开发净增；按当前相邻任命预测。":L"开发优先：先最大化每旬开发净增，同分优先治安恢复；按当前相邻任命预测。"):L"已按同城有效能力生成方案；关联任命一组选择。");return 1;
}
void s14_domestic_select(S14DPlan *p,int row,int selected){if(!p||row<0||row>=p->count)return;int g=p->rows[row].group;for(int i=0;i<p->count;i++)if(p->rows[i].group==g&&p->rows[i].reason==S14_D_CHANGE&&!p->rows[i].completed)p->rows[i].selected=!!selected;}
int s14_domestic_budget(const S14DSnapshot *s,const S14DPlan *p,int *count,int *cost,int *remaining){
 int used[52]={0},seen[52]={0},balances[52]={0},n=0,total=0,balance=0,ok=p&&p->valid;
 if(!s||!p)return 0;
 for(int c=1;c<52;c++)if(s->cities[c].active){int a=s->cities[c].account;if(a<1||a>=52)return 0;if(seen[a]&&balances[a]!=s->cities[c].balance)return 0;seen[a]=1;balances[a]=s->cities[c].balance;}
 for(int a=1;a<52;a++)if(seen[a])balance+=balances[a];
 for(int i=0;i<p->count;i++)if(p->rows[i].selected&&!p->rows[i].completed&&p->rows[i].reason==S14_D_CHANGE){int a=s->cities[p->rows[i].city].account;used[a]+=s->unit_cost;n++;total+=s->unit_cost;}
 for(int a=1;a<52;a++)if(used[a]>balances[a])ok=0;
 if(count)*count=n;if(cost)*cost=total;if(remaining)*remaining=balance-total;return ok&&n>0;
}
int s14_domestic_same(const S14DSnapshot *a,const S14DSnapshot *b){return a&&b&&!memcmp(a,b,sizeof(*a));}
int s14_domestic_select_budget(const S14DSnapshot *s,const S14DOptions *o,S14DPlan *p){
 if(!s||!o||!p||!p->valid)return 0;
 struct {int id,account,cost,order,development,area;} groups[501];int count=0,available[52]={0};
 for(int c=1;c<52;c++)if(s->cities[c].active){int a=s->cities[c].account;if(a<1||a>=52)return 0;available[a]=s->cities[c].balance;}
 for(int i=0;i<p->count;i++){S14DRow *r=&p->rows[i];r->selected=0;if(r->completed||r->reason!=S14_D_CHANGE)continue;int j=0;for(;j<count&&groups[j].id!=r->group;j++){}int a=s->cities[r->city].account;
  if(j==count){groups[count].id=r->group;groups[count].account=a;groups[count].cost=groups[count].order=groups[count].development=0;groups[count].area=r->area;count++;}
  if(groups[j].account!=a)return 0;groups[j].cost+=s->unit_cost;groups[j].order+=r->after_security-r->before_security;groups[j].development+=r->after_gain-r->before_gain;
 }
 int selected=0;for(int i=0;i<count;i++){int best=i;for(int j=i+1;j<count;j++){int x=o->security_first?groups[j].order:groups[j].development,y=o->security_first?groups[best].order:groups[best].development;
  int xx=o->security_first?groups[j].development:groups[j].order,yy=o->security_first?groups[best].development:groups[best].order;if(x>y||(x==y&&(xx>yy||(xx==yy&&groups[j].area<groups[best].area))))best=j;}
  if(best!=i){unsigned char temp[sizeof(groups[0])];memcpy(temp,&groups[i],sizeof(temp));groups[i]=groups[best];memcpy(&groups[best],temp,sizeof(temp));}
  if(groups[i].cost>available[groups[i].account])continue;available[groups[i].account]-=groups[i].cost;for(int r=0;r<p->count;r++)if(p->rows[r].group==groups[i].id&&!p->rows[r].completed&&p->rows[r].reason==S14_D_CHANGE){p->rows[r].selected=1;selected++;}
 }
 return selected;
}
