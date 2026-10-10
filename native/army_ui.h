#ifndef S14_ARMY_UI_H
#define S14_ARMY_UI_H
#include "army_observer.h"
typedef struct {
    HWND bar,popup,owner;HINSTANCE instance;HFONT small,body,value;
    uintptr_t base;ULONGLONG next_read;int scale,width,height,enabled,shown,popup_shown,ready,scroll;
    S14ArmyFrame frame;
} S14ArmyUI;
void s14_army_ui_tick(S14ArmyUI*,HINSTANCE,HWND,uintptr_t,int,ULONGLONG);
void s14_army_ui_paint(S14ArmyUI*,HDC,int);
void s14_army_ui_destroy(S14ArmyUI*);
#endif
