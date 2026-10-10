#include "army_model.h"
#include "battle_place.h"
#include "career_affix.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
typedef struct {S14OfficerRead read;void *context;S14ArmyFrame *out;} Reader;
static uintptr_t ptr(const unsigned char *p){uintptr_t v;memcpy(&v,p,8);return v;}
static unsigned int word(const unsigned char *p){unsigned int v;memcpy(&v,p,4);return v;}
static unsigned short half(const unsigned char *p){unsigned short v;memcpy(&v,p,2);return v;}
static int get(Reader *r,uintptr_t at,void *out,size_t n){
    if(!at || !n || n>8192 || ++r->out->calls>8192 || r->out->bytes+n>262144)return 0;
    r->out->bytes+=(unsigned int)n;return r->read(r->context,at,out,n);
}
static int place_read(void *c,uintptr_t at,void *out,size_t n){return get(c,at,out,n);}
static void name(wchar_t *out,size_t size,const unsigned char *p,int n){int i;for(i=0;i<n && (size_t)i<size-1;i++){out[i]=half(p+i*2);if(!out[i])break;}out[i]=0;}
static int candidate(Reader *r,uintptr_t base,uintptr_t object,uintptr_t world,S14ArmyFrame *f){
    unsigned char dlg[0x330],tab[0x148],basic[0x178],army[512],person[512],after[0x148];uintptr_t page=0,canonical=0,pool=0;
    if(!get(r,object,dlg,sizeof(dlg)) || ptr(dlg)!=base+0x137e970 || (word(dlg+0x40)&3)!=3)return 0;
    uintptr_t body=ptr(dlg+0x2e0);
    if(!get(r,body,tab,sizeof(tab)) || ptr(tab)!=base+0x1351680 || (word(tab+0x40)&3)!=3 || word(tab+0x138)!=2)return 0;
    uintptr_t selected=ptr(tab+0x140),pages=ptr(tab+0x130);
    if(!selected || !get(r,pages,&page,8) || !get(r,page,basic,sizeof(basic)) || ptr(basic)!=base+0x137ebf0 || ptr(basic+0x168)!=selected)return 0;
    if(!get(r,selected,army,sizeof(army)) || ptr(army)!=base+0x123e288 || army[0x10]!=1 || half(army+0x12)!=518)return 0;
    if(!get(r,world+0x7df60,&pool,8) || selected<=pool || (selected-pool)%512 || (selected-pool)/512>500)return 0;
    int id=(int)((selected-pool)/512);
    if(!get(r,world+0x7df60+(uintptr_t)id*8,&canonical,8) || canonical!=selected)return 0;
    int leader=half(army+0x12);uintptr_t person_at=0;
    if(!get(r,world+0x148+(uintptr_t)leader*8,&person_at,8) || !get(r,person_at,person,sizeof(person)) || ptr(person)!=base+0x12a00d0 || half(person+0x10)!=leader)return 0;
    f->world=world;f->dialog=object;f->page=page;f->army=selected;f->person=person_at;f->army_id=id;f->officer_id=leader;
    name(f->name,24,person+0x12,9);wchar_t second[12];name(second,12,person+0x24,9);wcsncat(f->name,second,23-wcslen(f->name));
    memcpy(&f->panel,dlg+0x20,16);long x=f->panel.left,y=f->panel.top,w=f->panel.right,h=f->panel.bottom;
    if(x<0 || y<0 || w<800 || w>1920 || h<300 || h>1080 || x+w>1920 || y+h>1080)return 0;
    f->panel.right=x+w;f->panel.bottom=y+h;f->troops=half(army+0x16);f->wounded=half(army+0x18);f->morale=army[0x1a];f->sea_preview=(int)word(basic+0x170);
    if(f->sea_preview<0 || f->sea_preview>1)return 0;
    for(int i=0;i<5;i++)f->abilities[i]=person[0x124+i];
    int formation=army[f->sea_preview?0x1b:0x1c];uintptr_t formation_at=0;unsigned char formation_raw[48];
    if(formation<=20 && get(r,world+0x76b58+(uintptr_t)formation*8,&formation_at,8) && get(r,formation_at,formation_raw,sizeof(formation_raw)))name(f->formation,24,formation_raw+0x10,6);
    S14BattlePlace place;if(s14_place_capture(place_read,r,base,world,half(army+0x2a),&place))s14_place_text(&place,f->location,80);else swprintf(f->location,80,L"棋格 (%u, %u)",half(army+0x2a)%220,half(army+0x2a)/220);
    for(int i=0;i<9;i++){
        int personality=half(person+0x150+i*2);if(!personality)continue;
        uintptr_t at=0;unsigned char def[224];wchar_t label[24],description[64],line[160];
        if(personality>=356 || !get(r,world+0x7d440+(uintptr_t)personality*8,&at,8) || !get(r,at,def,sizeof(def)) || ptr(def)!=base+0x12a0658)continue;
        name(label,24,def+0x10,6);name(description,64,def+0x3c,51);
        if(!label[0])swprintf(label,24,L"隐藏个性 #%d",personality);
        swprintf(line,160,L"%ls：%ls\n",label,description[0]?description:L"效果说明未提供");
        wcsncat(f->personalities,line,1199-wcslen(f->personalities));
    }
    // Reject selected-unit changes and stale data across close/load/reuse races.
    unsigned char header[72],identity_after[16];uintptr_t after_world=0;
    if(!get(r,body,after,sizeof(after)) || ptr(after+0x140)!=selected ||
       !get(r,object,header,sizeof(header)) || (word(header+0x40)&3)!=3 || ptr(header)!=base+0x137e970 ||
       !get(r,selected+0x10,identity_after,16) || memcmp(army+0x10,identity_after,16) ||
       !get(r,base+0x1fc91d0,&after_world,8) || after_world!=world)return 0;
    // Kept here for comparison with a passive observation of the same identity.
    wchar_t titled[24];s14_affix_display(world,S14_ELITE_OFFICER,f->name,titled,24);if(titled[0])wcscpy(f->name,titled);
    memcpy(f->trace.identity,army+0x10,16);return f->name[0]!=0;
}
int s14_army_capture(S14OfficerRead read,void *context,uintptr_t base,S14ArmyFrame *out){
    if(!out)return 0;memset(out,0,sizeof(*out));if(!read || !base)return 0;Reader r={read,context,out};
    unsigned char reg[128],controller[404],after[128];uintptr_t world=0;
    if(!get(&r,base+0x1fc91d0,&world,8) || !world || !get(&r,base+0x201c320,reg,128) || !get(&r,base+0x19e6390,controller,404))return 0;
    uintptr_t table=ptr(reg+0x40),handle=ptr(controller+56);unsigned int count=word(reg+0x70),index=0;
    if(!ptr(reg+0x38) || !table || !count || count>0x40000 || !get(&r,handle,&index,4) || index>=count)return 0;
    uintptr_t node=0;if(!get(&r,table+(uintptr_t)index*8,&node,8))return 0;
    int matches=0;unsigned int n=0;S14ArmyFrame found={0};
    for(;node && n<2048;n++){
        uintptr_t raw[2],vt=0;if(!get(&r,node,raw,16) || raw[1]==node)return 0;
        if(raw[0] && get(&r,raw[0],&vt,8) && vt==base+0x137e970){S14ArmyFrame f={0};if(candidate(&r,base,raw[0],world,&f)){found=f;if(++matches>1)return 0;}}
        node=raw[1];
    }
    if(node || matches!=1 || !get(&r,base+0x201c320,after,128) || memcmp(reg+0x38,after+0x38,16) || word(reg+0x70)!=word(after+0x70))return 0;
    found.calls=out->calls;found.bytes=out->bytes;*out=found;return 1;
}
int s14_army_trace_matches(const S14ArmyFrame *f,const S14ArmyTrace *t){
    if(!f || !t || t->packets!=3 || f->world!=t->world || f->army!=t->army || f->page!=t->page ||
       f->army_id!=t->army_id || f->officer_id!=t->officer_id || f->officer_id!=518 || memcmp(f->trace.identity,t->identity,16))return 0;
    for(int i=0;i<5;i++)if(t->baseline[i]<0 || t->actual[i]<0 || t->baseline[i]>10000000 || t->actual[i]>10000000)return 0;
    for(int i=0;i<2;i++)if(t->factor_seen[i] && (!isfinite(t->factor[i]) || t->factor[i]<0))return 0;
    for(int i=0;i<5;i++)if(t->buff_seen[i] &&
        ((i!=S14_ARMY_ATTACK && i!=S14_ARMY_DEFENSE) || (t->buff_percent[i]!=0 && t->buff_percent[i]!=10) ||
         !isfinite(t->native_attribute[i]) || t->native_attribute[i]<0 ||
         !isfinite(t->buffed_attribute[i]) || t->buffed_attribute[i]<0))return 0;
    if(t->area_seen && (!isfinite(t->area_factor) || t->area_factor<0 || t->area_factor>1000))return 0;
    if(t->link_seen && (t->link_count<0 || t->link_count>500))return 0;
    return t->baseline_mode==t->actual_mode && t->baseline_mode>=0 && t->baseline_mode<=1 && t->preview_mode==f->sea_preview;
}
int s14_army_percent(const S14ArmyTrace *t,int i,int *out){
    if(!t || !out || i<0 || i>=5 || !t->observed[i] || !t->extra_observed[i] || t->limits[i]<0 || t->limits[i]>100 || t->steps[i]<0 || t->steps[i]>100)return 0;
    int64_t units=(int64_t)t->aggregate[i]+t->all[i]+t->extra[i]+t->conditional[i];int limit=t->limits[i];
    if(units>limit)units=limit;if(units<-limit)units=-limit;*out=(int)units*t->steps[i];return 1;
}
int s14_army_phase_percent(const S14ArmyTrace *t,int phase,int i,int *out){
    if(phase==1)return s14_army_percent(t,i,out);
    if(phase!=0 || !t)return 0;S14ArmyTrace b=*t;
    memcpy(b.aggregate,t->base_aggregate,sizeof(b.aggregate));memcpy(b.all,t->base_all,sizeof(b.all));
    memcpy(b.extra,t->base_extra,sizeof(b.extra));memcpy(b.conditional,t->base_conditional,sizeof(b.conditional));
    memcpy(b.observed,t->base_observed,sizeof(b.observed));memcpy(b.extra_observed,t->base_extra_observed,sizeof(b.extra_observed));
    return s14_army_percent(&b,i,out);
}
int s14_army_formation(S14OfficerRead read,void *c,uintptr_t base,uintptr_t at,int out[5]){
    unsigned char raw[184],encoded[32];if(!read || !at || !read(c,at,raw,sizeof(raw)) || ptr(raw)!=base+0x12a0300)return 0;
    const int offsets[]={0x83,0x84,0x85,0x7f,0x86},indices[]={12,16,20,0,24},rotations[]={1,3,5,5,7},caps[]={45,70,60,30,40};
    uintptr_t begin=ptr(raw+0xa8),end=ptr(raw+0xb0);int encrypted=begin && end>=begin && ((end-begin)&~(uintptr_t)3)==32;
    uintptr_t world=0,pool=0,decoder=0;unsigned int id=0;
    if(encrypted){
        if(!read(c,base+0x12a0300+0x48,&decoder,8) || decoder!=base+0xbfa0 || !read(c,base+0x1fc91d0,&world,8) ||
           !read(c,world+0x76b58,&pool,8) || at<pool || (at-pool)%192 || (at-pool)/192>20 || !read(c,begin,encoded,32))return 0;
        id=(unsigned int)((at-pool)/192);
    }
    for(int i=0;i<5;i++){
        unsigned int v=raw[offsets[i]];
        if(encrypted){unsigned int n=encoded[indices[i]],r=(unsigned int)rotations[i];v=((n<<r)|(n>>(8-r)))&255;v^=id;if(v<1)v=1;if(v>(unsigned)caps[i])v=caps[i];}
        out[i]=(int)v;
    }return 1;
}
int s14_army_white_formula(const S14ArmyTrace *t,int i,float *out){
    int pct;
    if(!t || !out || (i!=0 && i!=1 && i!=4) || !t->formation_seen[i] || !t->policy_seen[0][i] || !t->global_seen[i] ||
       !t->factor_seen[0] || !s14_army_phase_percent(t,0,i,&pct) || t->policy[0][i]<0 || t->policy[0][i]>10000)return 0;
    float value=(float)(t->formation[i]*t->policy[0][i])/100.0f;
    value*=t->factor[0];value*=(float)t->global_percent[i]/100.0f;value*=1.0f+(float)pct/100.0f;
    if(i==4)value+=1.0f; // Native detail has no combat target; its defense additive defaults to 1.
    if(!isfinite(value) || value<0 || value>10000000)return 0;*out=value;return 1;
}
int s14_army_foundation(const S14ArmyTrace *t,int troops,float *soldier,float *commander,float *total){
    if(!t || !soldier || !commander || !total || !t->foundation_seen || !t->leadership_seen[0] || troops<0 || troops>65535 ||
       t->sqrt_divisor<=0 || t->sqrt_divisor>10000 || t->leadership[0]<0 || t->leadership[0]>4095 ||
       t->leadership_scale<-10000 || t->leadership_scale>10000 || t->foundation_flat<-10000000 || t->foundation_flat>10000000 ||
       !isfinite(t->leadership_exponent) || t->leadership_exponent<0 || t->leadership_exponent>10)return 0;
    double cap=pow((double)((4*t->leadership[0])/3),t->leadership_exponent);
    if(!isfinite(cap) || cap<0 || cap>2147483647)return 0;
    int supported=(int)cap;if(supported>troops)supported=troops;
    *soldier=(float)(sqrt((double)supported*4.0)/(double)t->sqrt_divisor);
    *commander=(float)(t->leadership[0]*t->leadership_scale);*total=*soldier+*commander+(float)t->foundation_flat;
    return isfinite(*total);
}
void s14_army_explain(S14OfficerRead read,void *context,uintptr_t base,S14ArmyFrame *f){
    if(!read || !f || !f->connected)return;Reader r={read,context,f};S14ArmyTrace *t=&f->trace;
    for(int i=0;i<5;i++){
        int id=t->policy_id[0][i];uintptr_t at=0;unsigned char def[64];
        if(id>0 && id<=100 && get(&r,f->world+0x7ffa8+(uintptr_t)id*8,&at,8) && get(&r,at,def,sizeof(def)) && ptr(def)==base+0x12a06c0)name(f->policy_names[i],24,def+0x10,12);
    }
    int totals[119]={0},valid=t->sources_seen && t->source_mode==0;
    for(int j=0;valid && j<45;j++){
        int id=t->source_ids[j];if(!id)continue;uintptr_t at=0;unsigned char d[224];wchar_t label[24],line[192]={0};
        if(id>=356 || !get(&r,f->world+0x7d440+(uintptr_t)id*8,&at,8) || !get(&r,at,d,sizeof(d)) || ptr(d)!=base+0x12a0658){valid=0;break;}
        if(j>=36 && half(d+0xb4)!=65535 && !(t->source_mask&(1u<<(j-36))))continue;
        name(label,24,d+0x10,6);if(!label[0])swprintf(label,24,L"隐藏个性 #%d",id);
        swprintf(line,192,L"%ls%ls",label,j<36?L"（部队效果）":L"（主将）");int relevant=0;
        for(int k=0;k<2;k++){
            int cat=d[k?0xba:0xb6],v=(short)half(d+(k?0xbc:0xb8));if(v<-100)v=-100;if(v>2000)v=2000;
            if(cat<119)totals[cat]+=v;
            int i=-1;const int cats[]={5,6,8,4,7};for(int n=0;n<5;n++)if(cat==cats[n])i=n;
            if(v && (i>=0 || cat==19)){wchar_t part[64];swprintf(part,64,L" · %ls %+d%%",cat==19?L"全能力":s14_army_attribute_name(i),v*(cat==19?t->steps[0]:t->steps[i]));wcsncat(line,part,191-wcslen(line));relevant=1;}
        }
        if(relevant){wcsncat(f->active_sources,line,1599-wcslen(f->active_sources));wcsncat(f->active_sources,L"\n",1599-wcslen(f->active_sources));}
    }
    const int cats[]={5,6,8,4,7};for(int i=0;i<5;i++)if(!t->observed[i] || totals[cats[i]]!=t->aggregate[i] || totals[19]!=t->all[i])valid=0;
    f->sources_verified=valid;
    if(!valid)wcscpy(f->active_sources,L"个性逐项来源尚未与原生汇总闭合；请以表中的原生修正为准。");
    else if(!f->active_sources[0])wcscpy(f->active_sources,L"本次原生列表没有贡献五项百分比的个性；格挡、免疫等独立效果仍可能生效。");
    uintptr_t at=0;unsigned char own[128];
    if(t->area_seen && t->area_id>0 && t->area_id<=500 && get(&r,f->world+0x6c860+(uintptr_t)t->area_id*8,&at,8) && get(&r,at,own,128) && ptr(own)==base+0x129ff98){
        for(int j=0;j<9;j++){
            int id=j?half(own+0x56+(j-1)*2):t->area_id;if(j && !(own[0x66]&(1u<<(j-1))))continue;
            unsigned char a[128];if(id<1 || id>500 || !get(&r,f->world+0x6c860+(uintptr_t)id*8,&at,8) || !get(&r,at,a,128) || ptr(a)!=base+0x129ff98 || a[0x78]!=t->area_force)continue;
            wchar_t label[24],line[96];name(label,24,a+0x10,9);swprintf(line,96,L"%ls%ls %u/%u；",j?L"相连府 ":L"所在府 ",label,half(a+0x68),half(a+0x76));wcsncat(f->area_sources,line,639-wcslen(f->area_sources));
        }
    }
}
static int trace_array(char *out,size_t cap,size_t *used,const char *key,const int *values,int count){
    if(*used>=cap)return 0;int n=snprintf(out+*used,cap-*used,",\"%s\":[",key);if(n<0 || (size_t)n>=cap-*used)return 0;*used+=(size_t)n;
    for(int i=0;i<count;i++){n=snprintf(out+*used,cap-*used,"%s%d",i?",":"",values[i]);if(n<0 || (size_t)n>=cap-*used)return 0;*used+=(size_t)n;}
    if(*used+1>=cap)return 0;out[(*used)++]=']';out[*used]=0;return 1;
}
int s14_army_format_trace(const S14ArmyFrame *f,int visible,char *out,size_t cap){
    if(!f || !out || !cap)return 0;const S14ArmyTrace *t=&f->trace;
    int n=snprintf(out,cap,"{\"event\":\"army_attribute_view\",\"visible\":%d,\"connected\":%d,\"officer_id\":%d,\"army_id\":%d",visible,f->connected,f->officer_id,f->army_id);
    if(n<0 || (size_t)n>=cap)return 0;size_t used=(size_t)n;
#define TRACE_ARRAY(key,p,count) if(!trace_array(out,cap,&used,key,p,count))return 0
    TRACE_ARRAY("baseline",t->baseline,5);TRACE_ARRAY("actual",t->actual,5);TRACE_ARRAY("aggregate",t->aggregate,5);TRACE_ARRAY("all",t->all,5);
    TRACE_ARRAY("extra",t->extra,5);TRACE_ARRAY("observed",t->observed,5);TRACE_ARRAY("extra_observed",t->extra_observed,5);TRACE_ARRAY("conditional",t->conditional,5);
    TRACE_ARRAY("base_aggregate",t->base_aggregate,5);TRACE_ARRAY("base_all",t->base_all,5);TRACE_ARRAY("base_extra",t->base_extra,5);
    TRACE_ARRAY("base_observed",t->base_observed,5);TRACE_ARRAY("base_extra_observed",t->base_extra_observed,5);
    TRACE_ARRAY("policy_baseline",t->policy[0],5);TRACE_ARRAY("policy_actual",t->policy[1],5);
    TRACE_ARRAY("policy_baseline_seen",t->policy_seen[0],5);TRACE_ARRAY("policy_actual_seen",t->policy_seen[1],5);
    TRACE_ARRAY("policy_id",t->policy_id[0],5);TRACE_ARRAY("policy_strength",t->policy_strength[0],5);
    TRACE_ARRAY("formation",t->formation,5);TRACE_ARRAY("formation_seen",t->formation_seen,5);TRACE_ARRAY("global_percent",t->global_percent,5);
    TRACE_ARRAY("leadership",t->leadership,2);TRACE_ARRAY("leadership_seen",t->leadership_seen,2);TRACE_ARRAY("buff_seen",t->buff_seen,5);TRACE_ARRAY("buff_percent",t->buff_percent,5);
    int values[45];for(int i=0;i<5;i++)values[i]=t->temporary[i];TRACE_ARRAY("temporary_units",values,5);
    for(int i=0;i<45;i++)values[i]=t->source_ids[i];TRACE_ARRAY("source_ids",values,45);
    for(int i=0;i<5;i++){float predicted=0;values[i]=s14_army_white_formula(t,i,&predicted)?((int)predicted==t->baseline[i]?1:-1):0;}TRACE_ARRAY("white_formula_check",values,5);
#undef TRACE_ARRAY
    n=snprintf(out+used,cap-used,",\"factor\":[%.6f,%.6f],\"factor_seen\":[%d,%d],\"area_factor\":%.6f,\"area_seen\":%d,\"area_units\":%d,\"area_id\":%d,\"link_count\":%d,\"link_seen\":%d,\"link_step\":%d,\"link_step_seen\":%d,\"source_mask\":%u,\"source_mode\":%d,\"sources_verified\":%d,\"native_attack_float\":%.9g,\"buffed_attack_float\":%.9g,\"native_defense_float\":%.9g,\"buffed_defense_float\":%.9g,\"plugin_buff_applied\":%d,\"read_only\":%s,\"snapshot_read_only\":true}\n",
        (double)t->factor[0],(double)t->factor[1],t->factor_seen[0],t->factor_seen[1],(double)t->area_factor,t->area_seen,t->area_units,t->area_id,t->link_count,t->link_seen,t->link_step,t->link_step_seen,(unsigned)t->source_mask,t->source_mode,f->sources_verified,(double)t->native_attribute[0],(double)t->buffed_attribute[0],(double)t->native_attribute[4],(double)t->buffed_attribute[4],t->buff_percent[0]!=0 || t->buff_percent[4]!=0,(t->buff_percent[0] || t->buff_percent[4])?"false":"true");
    return n>=0 && (size_t)n<cap-used;
}
const wchar_t *s14_army_attribute_name(int i){const wchar_t *names[]={L"攻军",L"攻城",L"破城",L"机动",L"防御"};return i>=0 && i<5?names[i]:L"未知";}
