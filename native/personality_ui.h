#ifndef S14_PERSONALITY_UI_H
#define S14_PERSONALITY_UI_H
#include "officer_model.h"
#include "officer_edit.h"
#define S14_PERSONALITY_CHANGED (WM_APP+614)
typedef struct {
    HWND window,owner,list,add,replace,refresh,close,search,catalog,remove,apply,discard;
    HINSTANCE instance;HFONT font,title,small;HBRUSH paper;uintptr_t world;
    S14Officer officer;S14EditSnapshot before;unsigned short staged[9];uint64_t request_id;
    S14OfficerDefinition definitions[356];int scale,selected,pending,connected;wchar_t notice[192];
} S14PersonalityUI;
int s14_personality_ui_create(S14PersonalityUI*,HINSTANCE,HWND,int);
void s14_personality_ui_show(S14PersonalityUI*,const S14OfficerSnapshot*,const S14Officer*);
void s14_personality_ui_hide(S14PersonalityUI*);
void s14_personality_ui_destroy(S14PersonalityUI*);
#endif
