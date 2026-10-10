#ifndef S14_ARMY_MODEL_H
#define S14_ARMY_MODEL_H
#include "detail_model.h"
enum { S14_ARMY_ATTACK,S14_ARMY_CITY,S14_ARMY_BREAK,S14_ARMY_MOVE,S14_ARMY_DEFENSE,S14_ARMY_ATTRIBUTES };
typedef struct {
    uintptr_t world,army,page;ULONGLONG tick;unsigned int serial;
    int officer_id,army_id,baseline[5],actual[5],packets;
    int aggregate[5],all[5],extra[5],conditional[5],observed[5],extra_observed[5],steps[5],limits[5];
    float factor[2];int factor_seen[2];signed char temporary[5];
    int baseline_mode,actual_mode,preview_mode;unsigned char identity[16];
    int base_aggregate[5],base_all[5],base_extra[5],base_conditional[5],base_observed[5],base_extra_observed[5];
    int policy[2][5],policy_seen[2][5],policy_id[2][5],policy_strength[2][5];
    int formation[5],formation_seen[5],global_percent[5],global_seen[5];
    int leadership[2],leadership_seen[2],sqrt_divisor,leadership_scale,foundation_flat,foundation_seen;
    double leadership_exponent;
    float area_factor;int area_seen,area_units,area_id,area_force;
    int link_count,link_seen,link_step,link_step_seen;
    unsigned short source_ids[45],source_mask;int sources_seen,source_mode;
    float native_attribute[5],buffed_attribute[5];int buff_seen[5],buff_percent[5];
} S14ArmyTrace;
typedef struct {
    uintptr_t world,dialog,page,army,person;RECT panel;int army_id,officer_id;
    wchar_t name[24],formation[24],location[80],personalities[1200];
    int troops,wounded,morale,abilities[5],sea_preview;
    unsigned int calls,bytes;S14ArmyTrace trace;int connected;
    wchar_t policy_names[5][24],active_sources[1600],area_sources[640];
    int sources_verified;
} S14ArmyFrame;
int s14_army_capture(S14OfficerRead,void*,uintptr_t,S14ArmyFrame*);
int s14_army_trace_matches(const S14ArmyFrame*,const S14ArmyTrace*);
int s14_army_percent(const S14ArmyTrace*,int,int*);
int s14_army_phase_percent(const S14ArmyTrace*,int,int,int*);
int s14_army_formation(S14OfficerRead,void*,uintptr_t,uintptr_t,int[5]);
int s14_army_white_formula(const S14ArmyTrace*,int,float*);
int s14_army_foundation(const S14ArmyTrace*,int,float*,float*,float*);
void s14_army_explain(S14OfficerRead,void*,uintptr_t,S14ArmyFrame*);
int s14_army_format_trace(const S14ArmyFrame*,int,char*,size_t);
const wchar_t *s14_army_attribute_name(int);
#endif
