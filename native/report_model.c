#include "report_model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void s14_report_battle_free(S14ReportBattle *v){if(!v)return;free(v->actors);free(v->lines);memset(v,0,sizeof(*v));}
int s14_report_battle_copy(S14ReportBattle *out,const S14ReportBattle *in){
    if(!out||!in||in->actor_count<0||in->actor_count>S14_REPORT_ACTORS||in->line_count<0||in->line_count>S14_REPORT_LINES||
       (in->actor_count&&!in->actors)||(in->line_count&&!in->lines))return 0;
    S14ReportBattle v=*in;v.actors=NULL;v.lines=NULL;
    if(in->actor_count){v.actors=malloc((size_t)in->actor_count*sizeof(*v.actors));if(!v.actors)return 0;memcpy(v.actors,in->actors,(size_t)in->actor_count*sizeof(*v.actors));}
    if(in->line_count){v.lines=malloc((size_t)in->line_count*sizeof(*v.lines));if(!v.lines){free(v.actors);return 0;}memcpy(v.lines,in->lines,(size_t)in->line_count*sizeof(*v.lines));}
    s14_report_battle_free(out);*out=v;return 1;
}
void s14_report_search_free(S14ReportSearch *v){if(!v)return;free(v->lines);memset(v,0,sizeof(*v));}
int s14_report_search_copy(S14ReportSearch *out,const S14ReportSearch *in){
    if(!out||!in||in->line_count<0||in->line_count>6001||(in->line_count&&!in->lines))return 0;
    S14ReportSearch v=*in;v.lines=NULL;
    if(in->line_count){v.lines=malloc((size_t)in->line_count*sizeof(*v.lines));if(!v.lines)return 0;memcpy(v.lines,in->lines,(size_t)in->line_count*sizeof(*v.lines));}
    s14_report_search_free(out);*out=v;return 1;
}
int s14_report_actor_compare(const void *left,const void *right){
    const S14ReportActor *a=left,*b=right;
    if(a->inflicted!=b->inflicted)return a->inflicted>b->inflicted?-1:1;
    if(a->routs!=b->routs)return a->routs>b->routs?-1:1;
    return(a->id>b->id)-(a->id<b->id);
}
void s14_report_ratio(uint64_t inflicted,uint64_t lost,wchar_t *out,unsigned int capacity){
    if(!lost){swprintf(out,capacity,inflicted?L"∞":L"0.00");return;}
    swprintf(out,capacity,L"%.2f",(double)inflicted/(double)lost);
}
