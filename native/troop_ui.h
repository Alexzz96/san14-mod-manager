#ifndef S14_TROOP_UI_H
#define S14_TROOP_UI_H
#include "troop_runtime.h"
typedef struct {HWND window,owner;HFONT font;S14TroopFrame frame;int width,height,scale;} S14TroopUI;
void s14_troop_ui_tick(S14TroopUI *ui,HINSTANCE instance,HWND owner,int blocked);
void s14_troop_ui_destroy(S14TroopUI *ui);
#endif
