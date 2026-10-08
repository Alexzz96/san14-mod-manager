#define WIN32_LEAN_AND_MEAN
#include "toast.h"
#include "theme.h"
#include <wchar.h>
#include <windowsx.h>
#include <string.h>

static const wchar_t toast_class[]=L"S14BuildLimit.Toast.v1";
static int px(const S14Toast *toast,int value) { return MulDiv(value,toast->scale,96); }

void s14_toast_paint(S14Toast *toast,HDC dc) {
    RECT bounds={0,0,toast->width,toast->height};
    HBRUSH background=CreateSolidBrush(S14_PAPER);
    FillRect(dc,&bounds,background); DeleteObject(background);
    HBRUSH border=CreateSolidBrush(S14_BORDER); FrameRect(dc,&bounds,border); DeleteObject(border);
    HBRUSH accent=CreateSolidBrush(S14_ACCENT);
    RECT bar={0,0,px(toast,4),toast->height};
    FillRect(dc,&bar,accent); DeleteObject(accent);
    SetBkMode(dc,TRANSPARENT);
    HGDIOBJ previous=SelectObject(dc,toast->title_font);
    RECT title={px(toast,20),px(toast,14),toast->width-px(toast,toast->second_report?56:16),px(toast,42)};
    SetTextColor(dc,S14_ACCENT);
    DrawTextW(dc,toast->title[0]?toast->title:L"超过连接数量上限",-1,&title,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SelectObject(dc,toast->body_font); SetTextColor(dc,S14_INK);
    if(toast->second_report) {
        RECT close={toast->width-px(toast,48),px(toast,14),toast->width-px(toast,16),px(toast,42)};
        DrawTextW(dc,L"×",-1,&close,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        const wchar_t *tabs[]={L"探索",L"战斗"};
        for(int i=0;i<2;i++) {
            RECT tab={px(toast,20+i*152),px(toast,52),px(toast,160+i*152),px(toast,86)};
            HBRUSH brush=CreateSolidBrush(toast->selected_tab==i?S14_ACCENT_SOFT:S14_CARD);FillRect(dc,&tab,brush);DeleteObject(brush);
            SetTextColor(dc,toast->selected_tab==i?S14_ACCENT:S14_MUTED);DrawTextW(dc,tabs[i],-1,&tab,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }SetTextColor(dc,S14_INK);
    }
    RECT text={px(toast,20),px(toast,48),toast->width-px(toast,16),toast->height-px(toast,12)};
    if (toast->report_text) {
        if(toast->second_report) text.top=px(toast,100);
        text.bottom=toast->height-px(toast,44); int saved=SaveDC(dc);
        IntersectClipRect(dc,text.left,text.top,text.right,text.bottom);
        text.top-=toast->scroll; text.bottom=text.top+toast->content_height;
        DrawTextW(dc,toast->second_report && toast->selected_tab?toast->second_report:toast->report_text,-1,&text,DT_WORDBREAK|DT_NOPREFIX);
        RestoreDC(dc,saved);
        SetTextColor(dc,S14_MUTED);
        RECT footer={px(toast,20),toast->height-px(toast,32),toast->width-px(toast,16),toast->height-px(toast,8)};
        DrawTextW(dc,toast->second_report?L"点击标签切换 · 滚轮查看明细 · 点击正文或 × 关闭":L"滚轮查看全部明细 · 点击弹窗关闭",-1,&footer,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    } else DrawTextW(dc,toast->message[0]?toast->message:L"同一势力领地内，相连土垒和石墙最多 5 个。",-1,&text,DT_WORDBREAK|DT_NOPREFIX);
    SelectObject(dc,previous);
}

static LRESULT CALLBACK toast_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    S14Toast *toast=(S14Toast*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if (message==WM_NCCREATE) {
        toast=(S14Toast*)((CREATESTRUCTW*)lparam)->lpCreateParams;
        SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)toast);
    }
    if (message==WM_NCHITTEST) return toast && toast->report_text?HTCLIENT:HTTRANSPARENT;
    if (message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message==WM_LBUTTONUP && toast && toast->report_text) {
        int x=MulDiv(GET_X_LPARAM(lparam),96,toast->scale),y=MulDiv(GET_Y_LPARAM(lparam),96,toast->scale);
        if(toast->second_report && y>=52 && y<86) {
            int tab=x>=20 && x<160?0:x>=172 && x<312?1:-1;
            if(tab>=0) {
                toast->tab_scroll[toast->selected_tab]=toast->scroll;toast->selected_tab=tab;
                toast->scroll=toast->tab_scroll[tab];toast->content_height=toast->tab_height[tab];InvalidateRect(window,NULL,FALSE);
            }return 0;
        }
        ShowWindow(window,SW_HIDE); toast->deadline=0; toast->persistent=0; return 0;
    }
    if (message==WM_MOUSEWHEEL && toast && toast->report_text) {
        int maximum=toast->content_height-(toast->height-px(toast,toast->second_report?144:92)); if (maximum<0) maximum=0;
        toast->scroll-=MulDiv(GET_WHEEL_DELTA_WPARAM(wparam),px(toast,80),WHEEL_DELTA);
        if (toast->scroll<0) toast->scroll=0; if (toast->scroll>maximum) toast->scroll=maximum;
        InvalidateRect(window,NULL,FALSE); return 0;
    }
    if (message==WM_ERASEBKGND) return 1;
    if (message==WM_PAINT && toast) {
        PAINTSTRUCT paint; HDC dc=BeginPaint(window,&paint);
        s14_toast_paint(toast,dc); EndPaint(window,&paint); return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
static void resize_region(S14Toast *toast) {
    HRGN region=CreateRoundRectRgn(0,0,toast->width+1,toast->height+1,px(toast,12),px(toast,12));
    if (region && !SetWindowRgn(toast->window,region,FALSE)) DeleteObject(region);
}

static int create_toast(S14Toast *toast,HINSTANCE instance,HWND owner) {
    WNDCLASSEXW window_class={0}; window_class.cbSize=sizeof(window_class);
    window_class.lpfnWndProc=toast_proc; window_class.hInstance=instance;
    window_class.lpszClassName=toast_class;
    if (!RegisterClassExW(&window_class) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 0;
    toast->instance=instance; toast->owner=owner;
    typedef UINT (WINAPI *GetWindowDpi)(HWND);
    GetWindowDpi get_dpi=(GetWindowDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");
    toast->scale=get_dpi?(int)get_dpi(owner):96;
    if (toast->scale<96 || toast->scale>384) toast->scale=96;
    toast->width=px(toast,450); toast->height=px(toast,105);
    toast->title_font=CreateFontW(-px(toast,19),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    toast->body_font=CreateFontW(-px(toast,16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    if (!toast->title_font || !toast->body_font) { s14_toast_destroy(toast); return 0; }
    toast->window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED,
        toast_class,L"墙体连接上限提示",WS_POPUP,0,0,toast->width,toast->height,owner,NULL,instance,toast);
    if (!toast->window) { s14_toast_destroy(toast); return 0; }
    if (!SetLayeredWindowAttributes(toast->window,0,245,LWA_ALPHA)) { s14_toast_destroy(toast); return 0; }
    resize_region(toast);
    return 1;
}

int s14_toast_show(S14Toast *toast,HINSTANCE instance,HWND owner,POINT click,ULONGLONG now) {
    return s14_toast_text(toast,instance,owner,click,now,L"超过连接数量上限",L"同一势力领地内，相连土垒和石墙最多 5 个。",5000);
}
int s14_toast_text(S14Toast *toast,HINSTANCE instance,HWND owner,POINT click,ULONGLONG now,
                   const wchar_t *title,const wchar_t *message,unsigned int duration) {
    if (!duration || duration>60000) return 0;
    if (!IsWindow(owner) || !IsWindowVisible(owner) || IsIconic(owner)) return 0;
    if (toast->window && toast->owner!=owner) s14_toast_destroy(toast);
    if (!toast->window && !create_toast(toast,instance,owner)) return 0;
    if (toast->report_text) { HeapFree(GetProcessHeap(),0,toast->report_text); toast->report_text=NULL; }
    if(toast->second_report) {HeapFree(GetProcessHeap(),0,toast->second_report);toast->second_report=NULL;}
    toast->persistent=0; toast->scroll=0; toast->width=px(toast,450); toast->height=px(toast,105);
    SetWindowLongPtrW(toast->window,GWL_EXSTYLE,GetWindowLongPtrW(toast->window,GWL_EXSTYLE)|WS_EX_TRANSPARENT);
    resize_region(toast);
    wcsncpy(toast->title,title,95); toast->title[95]=0;
    wcsncpy(toast->message,message,255); toast->message[255]=0;
    RECT client; if (!GetClientRect(owner,&client)) return 0;
    POINT origin={0,0}; if (!ClientToScreen(owner,&origin)) return 0;
    OffsetRect(&client,origin.x,origin.y);
    int x=click.x+px(toast,18),y=click.y+px(toast,22);
    if (x+toast->width>client.right-px(toast,12)) x=click.x-toast->width-px(toast,18);
    if (y+toast->height>client.bottom-px(toast,12)) y=click.y-toast->height-px(toast,22);
    if (x<client.left+px(toast,12)) x=client.left+px(toast,12);
    if (y<client.top+px(toast,12)) y=client.top+px(toast,12);
    if (!SetWindowPos(toast->window,HWND_TOPMOST,x,y,toast->width,toast->height,SWP_NOACTIVATE|SWP_SHOWWINDOW)) return 0;
    InvalidateRect(toast->window,NULL,FALSE); UpdateWindow(toast->window);
    toast->shown_at=now; toast->deadline=now+duration;
    return 1;
}
int s14_toast_report(S14Toast *toast,HINSTANCE instance,HWND owner,ULONGLONG now,const wchar_t *text) {
    if (!text || !IsWindow(owner) || !IsWindowVisible(owner) || IsIconic(owner)) return 0;
    if (toast->window && toast->owner!=owner) s14_toast_destroy(toast);
    if (!toast->window && !create_toast(toast,instance,owner)) return 0;
    size_t length=wcslen(text); if (length>6001ull*512+4096) return 0;
    wchar_t *copy=HeapAlloc(GetProcessHeap(),0,(length+1)*sizeof(wchar_t)); if (!copy) return 0;
    memcpy(copy,text,(length+1)*sizeof(wchar_t));
    if (toast->report_text) HeapFree(GetProcessHeap(),0,toast->report_text); toast->report_text=copy;
    if(toast->second_report) {HeapFree(GetProcessHeap(),0,toast->second_report);toast->second_report=NULL;}
    RECT client; if (!GetClientRect(owner,&client)) return 0;
    POINT origin={0}; if (!ClientToScreen(owner,&origin)) return 0; OffsetRect(&client,origin.x,origin.y);
    int maximum_width=client.right-client.left-px(toast,24),maximum_height=client.bottom-client.top-px(toast,24);
    toast->width=px(toast,720); if (toast->width>maximum_width) toast->width=maximum_width;
    if (toast->width<px(toast,160) || maximum_height<px(toast,160)) return 0;
    HDC dc=GetDC(toast->window); if (!dc) return 0; HGDIOBJ font=SelectObject(dc,toast->body_font);
    RECT measured={0,0,toast->width-px(toast,36),0};
    DrawTextW(dc,copy,-1,&measured,DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);
    toast->content_height=measured.bottom; SelectObject(dc,font); ReleaseDC(toast->window,dc);
    toast->height=toast->content_height+px(toast,104);
    if (toast->height>maximum_height) toast->height=maximum_height;
    if (toast->height<px(toast,180)) toast->height=px(toast,180);
    wcscpy(toast->title,L"本回合探索结果"); toast->scroll=0; toast->persistent=1; toast->shown_at=now; toast->deadline=~(ULONGLONG)0;
    SetWindowLongPtrW(toast->window,GWL_EXSTYLE,GetWindowLongPtrW(toast->window,GWL_EXSTYLE)&~(LONG_PTR)WS_EX_TRANSPARENT);
    resize_region(toast);
    int x=client.left+(client.right-client.left-toast->width)/2,y=client.top+(client.bottom-client.top-toast->height)/2;
    if (!SetWindowPos(toast->window,HWND_TOPMOST,x,y,toast->width,toast->height,SWP_NOACTIVATE|SWP_SHOWWINDOW)) return 0;
    InvalidateRect(toast->window,NULL,FALSE); UpdateWindow(toast->window); return 1;
}
int s14_toast_report_tabs(S14Toast *toast,HINSTANCE instance,HWND owner,ULONGLONG now,const wchar_t *search,const wchar_t *battle) {
    if(!search || !battle) return 0;size_t n=wcslen(battle);if(n>6001ull*512+4096) return 0;
    wchar_t *copy=HeapAlloc(GetProcessHeap(),0,(n+1)*sizeof(wchar_t));if(!copy) return 0;memcpy(copy,battle,(n+1)*sizeof(wchar_t));
    if(!s14_toast_report(toast,instance,owner,now,search)) {HeapFree(GetProcessHeap(),0,copy);return 0;}
    toast->second_report=copy;toast->selected_tab=1;toast->tab_scroll[0]=toast->tab_scroll[1]=0;
    wcscpy(toast->title,L"本回合报告 · 自势力");
    RECT client;POINT origin={0};if(!GetClientRect(owner,&client) || !ClientToScreen(owner,&origin)) return 0;OffsetRect(&client,origin.x,origin.y);
    toast->width=px(toast,820);if(toast->width>client.right-client.left-px(toast,24)) toast->width=client.right-client.left-px(toast,24);
    toast->height=px(toast,620);if(toast->height>client.bottom-client.top-px(toast,24)) toast->height=client.bottom-client.top-px(toast,24);
    HDC dc=GetDC(toast->window);if(!dc) return 0;HGDIOBJ font=SelectObject(dc,toast->body_font);
    for(int i=0;i<2;i++) {RECT measured={0,0,toast->width-px(toast,36),0};DrawTextW(dc,i?copy:toast->report_text,-1,&measured,DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);toast->tab_height[i]=measured.bottom;}
    SelectObject(dc,font);ReleaseDC(toast->window,dc);toast->content_height=toast->tab_height[1];resize_region(toast);
    SetWindowPos(toast->window,HWND_TOPMOST,client.left+(client.right-client.left-toast->width)/2,client.top+(client.bottom-client.top-toast->height)/2,toast->width,toast->height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(toast->window,NULL,FALSE);UpdateWindow(toast->window);return 1;
}

int s14_toast_tick(S14Toast *toast,ULONGLONG now,int owner_foreground) {
    if (!toast->deadline) return 0;
    int reason=!toast->persistent && now>=toast->deadline?1:(!owner_foreground || !IsWindowVisible(toast->owner) || IsIconic(toast->owner)?2:0);
    if (toast->persistent) {
        if (reason) ShowWindow(toast->window,SW_HIDE);
        else if (!IsWindowVisible(toast->window)) SetWindowPos(toast->window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        return reason;
    }
    if (!reason) return 0;
    ShowWindow(toast->window,SW_HIDE); toast->deadline=0; return reason;
}

void s14_toast_destroy(S14Toast *toast) {
    if (toast->window) DestroyWindow(toast->window);
    if (toast->title_font) DeleteObject(toast->title_font);
    if (toast->body_font) DeleteObject(toast->body_font);
    if (toast->report_text) HeapFree(GetProcessHeap(),0,toast->report_text);
    if(toast->second_report) HeapFree(GetProcessHeap(),0,toast->second_report);
    *toast=(S14Toast){0};
}
