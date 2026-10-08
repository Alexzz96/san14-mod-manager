#ifndef S14_DETAIL_UI_H
#define S14_DETAIL_UI_H
#include "detail_model.h"
typedef struct {
    HWND window,owner;HINSTANCE instance;HFONT header_font,label_font,value_font;
    uintptr_t base;int ready,enabled,scale,shown,incomplete,bound,connected;
    ULONGLONG next_read;S14DetailFrame frame;S14BattleTotals stats;
    int width,height;
} S14DetailUI;
void s14_detail_tick(S14DetailUI*,HINSTANCE,HWND,uintptr_t,int,ULONGLONG);
void s14_detail_paint(S14DetailUI*,HDC);
void s14_detail_destroy(S14DetailUI*);
#endif
