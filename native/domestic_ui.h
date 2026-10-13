#ifndef S14_DOMESTIC_UI_H
#define S14_DOMESTIC_UI_H
#include "domestic_native.h"
#include "domestic_settings.h"
typedef struct {
 HWND window,city,list,people,summary,status,apply,refresh,fill,optimize,lock_button,close,policy,security_first,forecast_note,details,auto_fill,auto_status,outcome,quick_button;
 S14DomesticQuickUI quick;
 HFONT font;HINSTANCE instance;unsigned epoch,revision;int dpi,updating,pending,refresh_needed;
 wchar_t ini[MAX_PATH];
 uint64_t request,preview;S14DOptions options;S14DResult *data,*poll;
} S14DomesticUI;
int s14_domestic_ui_show(S14DomesticUI*,HINSTANCE,HWND,int mode);
void s14_domestic_ui_destroy(S14DomesticUI*);
#endif
