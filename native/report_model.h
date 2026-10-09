#ifndef S14_REPORT_MODEL_H
#define S14_REPORT_MODEL_H
#include <stdint.h>
#include <wchar.h>
#define S14_REPORT_ACTORS 6001
#define S14_REPORT_LINES 4096
enum {S14_REPORT_DAMAGE=1,S14_REPORT_SKILL,S14_REPORT_FIRE,S14_REPORT_STATE,S14_REPORT_ROUT,S14_REPORT_INJURY,S14_REPORT_DUEL,S14_REPORT_CAPTURE,S14_REPORT_OTHER};
typedef struct {
    int id,portrait,routs,defeats,injuries,wins,losses,captures,captured,effects;
    uint64_t inflicted,lost,absorbed;
    wchar_t name[32],place[96];
} S14ReportActor;
typedef struct {
    int actor,target,kind,important,verified;
    unsigned char clock[6];
    wchar_t text[256],place[96];
} S14ReportLine;
typedef struct {
    int start,end,force,available,incomplete,skipped,actor_count,line_count,routs,defeats,injuries;
    uintptr_t world;
    unsigned char clock[6],end_clock[6];
    uint64_t inflicted,lost,absorbed;
    S14ReportActor *actors;
    S14ReportLine *lines;
} S14ReportBattle;
typedef struct {int type;wchar_t text[512];} S14ReportSearchLine;
typedef struct {
    int start,end,force,available,completed,empty,people,items,books,money,pending,truncated,line_count;
    S14ReportSearchLine *lines;
} S14ReportSearch;
void s14_report_battle_free(S14ReportBattle*);
int s14_report_battle_copy(S14ReportBattle*,const S14ReportBattle*);
void s14_report_search_free(S14ReportSearch*);
int s14_report_search_copy(S14ReportSearch*,const S14ReportSearch*);
int s14_report_actor_compare(const void*,const void*);
void s14_report_ratio(uint64_t,uint64_t,wchar_t*,unsigned int);
#endif
