#ifndef S14_REPORT_UI_H
#define S14_REPORT_UI_H
#include <windows.h>
#include "report_model.h"
#include "portrait.h"
typedef struct {
    HWND window,owner;HINSTANCE instance;uintptr_t base;
    HFONT title,name,body,small,number;int scale,width,height,visible,scroll,content_height,columns,expanded;
    int detail,detail_filter,detail_actor,detail_scroll,detail_height,dragging,drag_y,drag_start;
    int body_top,body_bottom,card_top,card_height,event_top,raw_top;
    RECT close,done,search[5],detail_close,thumb;
    wchar_t force_name[32];S14ReportSearch search_data;S14ReportBattle battle;
    int *important,important_count;S14PortraitCache *portraits;wchar_t root[MAX_PATH];
} S14ReportUI;
int s14_report_ui_show(S14ReportUI*,HINSTANCE,HWND,uintptr_t,const wchar_t*,const S14ReportSearch*,const S14ReportBattle*);
int s14_report_ui_reopen(S14ReportUI*);
void s14_report_ui_tick(S14ReportUI*,int owner_foreground);
void s14_report_ui_hide(S14ReportUI*);
void s14_report_ui_clear(S14ReportUI*);
void s14_report_ui_destroy(S14ReportUI*);
void s14_report_ui_paint(S14ReportUI*,HDC);
#endif
