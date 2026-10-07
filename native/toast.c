#define WIN32_LEAN_AND_MEAN
#include "toast.h"

static const wchar_t toast_class[]=L"S14BuildLimit.Toast.v1";
static int px(const S14Toast *toast,int value) { return MulDiv(value,toast->scale,96); }

void s14_toast_paint(S14Toast *toast,HDC dc) {
    RECT bounds={0,0,toast->width,toast->height};
    HBRUSH background=CreateSolidBrush(RGB(27,31,38));
    FillRect(dc,&bounds,background); DeleteObject(background);
    HBRUSH accent=CreateSolidBrush(RGB(221,177,93));
    RECT bar={0,0,px(toast,4),toast->height};
    FillRect(dc,&bar,accent); DeleteObject(accent);
    SetBkMode(dc,TRANSPARENT);
    HGDIOBJ previous=SelectObject(dc,toast->title_font);
    RECT title={px(toast,20),px(toast,14),toast->width-px(toast,16),px(toast,42)};
    SetTextColor(dc,RGB(244,209,141));
    DrawTextW(dc,L"超过连接数量上限",-1,&title,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SelectObject(dc,toast->body_font); SetTextColor(dc,RGB(242,244,247));
    RECT text={px(toast,20),px(toast,48),toast->width-px(toast,16),toast->height-px(toast,12)};
    DrawTextW(dc,L"同一势力领地内，相连土垒和石墙最多 5 个。",-1,&text,DT_WORDBREAK|DT_NOPREFIX);
    SelectObject(dc,previous);
}

static LRESULT CALLBACK toast_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    S14Toast *toast=(S14Toast*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if (message==WM_NCCREATE) {
        toast=(S14Toast*)((CREATESTRUCTW*)lparam)->lpCreateParams;
        SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)toast);
    }
    if (message==WM_NCHITTEST) return HTTRANSPARENT;
    if (message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message==WM_ERASEBKGND) return 1;
    if (message==WM_PAINT && toast) {
        PAINTSTRUCT paint; HDC dc=BeginPaint(window,&paint);
        s14_toast_paint(toast,dc); EndPaint(window,&paint); return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
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
    HRGN region=CreateRoundRectRgn(0,0,toast->width+1,toast->height+1,px(toast,12),px(toast,12));
    if (region && !SetWindowRgn(toast->window,region,FALSE)) DeleteObject(region);
    return 1;
}

int s14_toast_show(S14Toast *toast,HINSTANCE instance,HWND owner,POINT click,ULONGLONG now) {
    if (!IsWindow(owner) || !IsWindowVisible(owner) || IsIconic(owner)) return 0;
    if (toast->window && toast->owner!=owner) s14_toast_destroy(toast);
    if (!toast->window && !create_toast(toast,instance,owner)) return 0;
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
    toast->shown_at=now; toast->deadline=now+5000;
    return 1;
}

int s14_toast_tick(S14Toast *toast,ULONGLONG now,int owner_foreground) {
    if (!toast->deadline) return 0;
    int reason=now>=toast->deadline?1:(!owner_foreground || !IsWindowVisible(toast->owner) || IsIconic(toast->owner)?2:0);
    if (!reason) return 0;
    ShowWindow(toast->window,SW_HIDE); toast->deadline=0; return reason;
}

void s14_toast_destroy(S14Toast *toast) {
    if (toast->window) DestroyWindow(toast->window);
    if (toast->title_font) DeleteObject(toast->title_font);
    if (toast->body_font) DeleteObject(toast->body_font);
    *toast=(S14Toast){0};
}
