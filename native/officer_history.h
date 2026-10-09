#ifndef S14_OFFICER_HISTORY_H
#define S14_OFFICER_HISTORY_H
#include <windows.h>
#include "officer_model.h"
#include "battle_timeline.h"
typedef struct {
    HWND window,owner,list,close;HFONT font,small,title;HBRUSH brush;
    S14TimelineSnapshot *snapshot;S14Officer officer;int indices[S14_TIMELINE_MAX],count,scale;
} S14OfficerHistory;
int s14_history_create(S14OfficerHistory*,HINSTANCE,HWND,int);
void s14_history_show(S14OfficerHistory*,const S14Officer*);
void s14_history_hide(S14OfficerHistory*);
void s14_history_destroy(S14OfficerHistory*);
#endif
