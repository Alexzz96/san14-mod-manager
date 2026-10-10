#ifndef S14_MAP_EFFECTS_MODEL_H
#define S14_MAP_EFFECTS_MODEL_H
#include "officer_model.h"
#define S14_MAP_CACHE_MAX 512
typedef struct {uintptr_t root,face;} S14MapMarker;
typedef struct {
    uintptr_t array,top,vtable,depth;unsigned int lifecycle,phase;int main_map;
} S14MapScene;
typedef struct {
    uintptr_t base,world,table,anchor,heads[2];unsigned int indices[2],count;
    ULONGLONG scan_due;int initialized,markers;
    S14MapMarker marker[S14_MAP_CACHE_MAX];
    uintptr_t basic,personality,dialogs[2];
} S14MapCache;
typedef struct {
    uintptr_t world,army,root,face;int army_id,leader,native_serial,effect,halo,tooltip,modal,discovered;
    uintptr_t registry_table,detail_handle,detail_head;unsigned int detail_index;ULONGLONG sampled_at;
    RECT portrait,card;unsigned int calls,bytes;
    float center_x,center_y,radius;
    S14MapScene scene;
} S14MapFrame;
typedef struct {int count;S14MapFrame frames[500];} S14MapBatch;
typedef int (*S14MapQualify)(void*,uintptr_t,uintptr_t,int);
int s14_map_capture_all(S14OfficerRead,void*,uintptr_t,S14MapCache*,ULONGLONG,S14MapFrame*,S14MapBatch*,S14MapQualify,void*);
int s14_map_validate(S14OfficerRead,void*,uintptr_t);
int s14_map_capture(S14OfficerRead,void*,uintptr_t,S14MapCache*,ULONGLONG,S14MapFrame*);
int s14_map_scale(const RECT*,int,int,RECT*,int*);
int s14_map_card(const RECT *panel,const RECT *traits,RECT *card);
int s14_map_present_capture(S14OfficerRead,void*,uintptr_t,const S14MapFrame*,const uintptr_t[2],S14MapFrame*);
#endif
