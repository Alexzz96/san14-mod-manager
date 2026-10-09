#ifndef S14_OFFICER_UI_H
#define S14_OFFICER_UI_H
#include <windows.h>
#include "officer_model.h"
#include "officer_history.h"
typedef struct {
    HWND window,owner,query,scope,place,history,sort,direction,list,pinned,close_button,tabs[2],timeline_button;
    HINSTANCE instance;HFONT title_font,font,small_font,dense_font;HBRUSH brush;
    S14OfficerHistory timeline;
    HANDLE activation;
    uintptr_t base;
    S14OfficerSnapshot *snapshot,*candidate;
    S14SpecialSnapshot *special;
    S14OfficerCareerProvider career;
    S14BattleStatsProvider battle;
    void (*pump_events)(void);
    int menu_active;
    S14OfficerFilter filter;
    int indices[S14_OFFICER_MAX],visible_count,selected_id,scale;
    int key_registered,last_capture_ok;
    int active_tab,dynamic_keys[S14_SORT_COUNT],dynamic_count,sort_keys[S14_SORT_COUNT],sort_count;
    int remembered_sort[2],remembered_desc[2],syncing,layouting;
    ULONGLONG captured_tick;
    wchar_t notice[160];
} S14OfficerUI;
int s14_officer_ui_create(S14OfficerUI*,HINSTANCE,HWND,uintptr_t);
void s14_officer_ui_set_tab(S14OfficerUI*,int);
int s14_officer_ui_column(const S14OfficerUI*,int,int);
void s14_officer_ui_toggle(S14OfficerUI*);
void s14_officer_ui_destroy(S14OfficerUI*);
int s14_owned_foreground(HWND,HWND,HWND);
/* Feature preference only. Focus changes do not close the officer view. */
int s14_officer_ui_sync_enabled(S14OfficerUI*,int);
#endif
