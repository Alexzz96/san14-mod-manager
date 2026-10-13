#ifndef S14_DOMESTIC_SETTINGS_H
#define S14_DOMESTIC_SETTINGS_H
#include "domestic_native.h"
typedef struct {
 HWND window,policy,security,threshold_on,threshold,search,black,white,auto_fill,status,save,close;
 HFONT font;HINSTANCE instance;int dpi,updating,pending,corrupt;unsigned epoch;
 wchar_t ini[MAX_PATH];uint64_t request;S14DOptions options;S14DResult *data;
} S14DomesticQuickUI;
/* Persistent installation-wide preferences; locks/city filters remain session-only. */
int s14_domestic_settings_read(const wchar_t*,S14DOptions*);
int s14_domestic_settings_write(const wchar_t*,const S14DOptions*);
int s14_domestic_settings_seen(const wchar_t*);
int s14_domestic_settings_mark_seen(const wchar_t*);
int s14_domestic_settings_configured(const wchar_t*);
int s14_domestic_quick_show(S14DomesticQuickUI*,HINSTANCE,HWND,const wchar_t*);
int s14_domestic_quick_show_loaded(S14DomesticQuickUI*,HINSTANCE,HWND,const wchar_t*,const S14DResult*);
void s14_domestic_quick_destroy(S14DomesticQuickUI*);
#endif
