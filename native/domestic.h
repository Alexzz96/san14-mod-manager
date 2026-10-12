#ifndef S14_DOMESTIC_H
#define S14_DOMESTIC_H
#include <stdint.h>
#include <wchar.h>
#define S14_D_CITIES 52
#define S14_D_AREAS 501
#define S14_D_PEOPLE 6001
#define S14_D_TRAITS 356
#define S14_D_FORECASTS 128000
enum {S14_D_FILL,S14_D_OPTIMIZE};
/* KEEP is retained for internal legacy callers; the UI offers MAX/COM/AGR. */
enum {S14_D_KEEP_DIRECTION,S14_D_MAX_DEVELOPMENT,S14_D_COMMERCIAL,S14_D_AGRICULTURAL};
enum {S14_D_KEEP,S14_D_LOCKED,S14_D_PROTECTED,S14_D_NO_CANDIDATE,S14_D_CHANGE};
typedef struct {int active,account,balance,direction,area_count,development_aura,security_aura;uintptr_t address;wchar_t name[40],aura_description[256];} S14DCity;
typedef struct {int active,city,officer,project,ordinal,development[3],security;uintptr_t address;wchar_t name[32];} S14DArea;
typedef struct {int active,city,eligible,protected_job,busy;int ability[5],traits[9],forecast_start,forecast_count;uintptr_t address;wchar_t name[40];} S14DPerson;
typedef struct {wchar_t name[16],description[112];int effect[2],value[2],domestic;} S14DTrait;
/* Native one-turn forecasts with current neighbouring appointments held fixed. */
typedef struct {int development[3],security,gold_cost;} S14DForecast;
typedef struct {uintptr_t world;unsigned epoch;int force,group,unit_cost,forecast_ready,forecast_count;S14DCity cities[S14_D_CITIES];S14DArea areas[S14_D_AREAS];S14DPerson people[S14_D_PEOPLE];S14DTrait traits[S14_D_TRAITS];S14DForecast forecasts[S14_D_FORECASTS];} S14DSnapshot;
typedef struct {int mode,city,policy,security_first;unsigned char locked[S14_D_AREAS],excluded[S14_D_PEOPLE];} S14DOptions;
typedef struct {int area,city,before,after,project,first,before_value,after_value,before_gain,after_gain,before_security,after_security,reason,group,selected,completed;} S14DRow;
typedef struct {int count,valid;S14DRow rows[S14_D_AREAS];wchar_t reason[192];} S14DPlan;
int s14_domestic_plan(const S14DSnapshot*,const S14DOptions*,S14DPlan*);
void s14_domestic_select(S14DPlan*,int row,int selected);
int s14_domestic_budget(const S14DSnapshot*,const S14DPlan*,int *count,int *cost,int *remaining);
int s14_domestic_select_budget(const S14DSnapshot*,const S14DOptions*,S14DPlan*);
/* Return 0 after any change to ownership, candidates, abilities or accounts. */
int s14_domestic_same(const S14DSnapshot*,const S14DSnapshot*);
const S14DForecast *s14_domestic_forecast(const S14DSnapshot*,int area,int person);
void s14_domestic_traits_text(const S14DSnapshot*,int person,wchar_t*,size_t);
#endif
