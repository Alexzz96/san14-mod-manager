#ifndef S14_TOAST_H
#define S14_TOAST_H
#include <windows.h>

typedef struct {
    HWND window, owner;
    HINSTANCE instance;
    HFONT title_font, body_font;
    int scale, width, height;
    ULONGLONG deadline, shown_at;
    wchar_t title[96],message[256];
    wchar_t *report_text;
    int persistent,content_height,scroll;
    wchar_t *second_report;
    int selected_tab,tab_scroll[2],tab_height[2];
} S14Toast;

void s14_toast_paint(S14Toast *toast,HDC dc);
int s14_toast_show(S14Toast *toast,HINSTANCE instance,HWND owner,POINT click,ULONGLONG now);
int s14_toast_text(S14Toast *toast,HINSTANCE instance,HWND owner,POINT click,ULONGLONG now,
                   const wchar_t *title,const wchar_t *message,unsigned int duration);
int s14_toast_report(S14Toast *toast,HINSTANCE instance,HWND owner,ULONGLONG now,const wchar_t *text);
int s14_toast_report_tabs(S14Toast *toast,HINSTANCE instance,HWND owner,ULONGLONG now,const wchar_t *search,const wchar_t *battle);
// 1: expired; 2: owner hidden/destroyed or another app in foreground.
int s14_toast_tick(S14Toast *toast,ULONGLONG now,int owner_foreground);
void s14_toast_destroy(S14Toast *toast);

#endif
