#define WIN32_LEAN_AND_MEAN
#include "detail_ui.h"
#include "theme.h"
#include <wchar.h>
#include <stdio.h>
#include <string.h>
static const wchar_t detail_class[]=L"SAN14ModManager.NativeOfficerStats.v1";
static int memory(void *unused,uintptr_t at,void *out,size_t n) {(void)unused;SIZE_T got;return ReadProcessMemory(GetCurrentProcess(),(const void*)at,out,n,&got) && got==n;}
static int px(S14DetailUI *ui,int n) {return MulDiv(n,ui->scale,96);}
static void text(HDC dc,HFONT font,const wchar_t *s,RECT r,COLORREF color) {HGDIOBJ old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s,-1,&r,DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);SelectObject(dc,old);}
void s14_detail_paint(S14DetailUI *ui,HDC dc) {
    RECT box={0,0,ui->width,ui->height};HBRUSH bg=CreateSolidBrush(S14_PAPER),border=CreateSolidBrush(S14_BORDER);FillRect(dc,&box,bg);FrameRect(dc,&box,border);DeleteObject(bg);DeleteObject(border);
    wchar_t title[128];swprintf(title,128,L"%ls · 存档累计战绩%ls",ui->frame.name,ui->connected?L"":L" · 未接入");
    text(dc,ui->header_font,title,(RECT){px(ui,14),px(ui,5),ui->width-px(ui,245),px(ui,27)},S14_ACCENT);
    text(dc,ui->header_font,ui->incomplete?L"有采集缺口 · 数值为已记录部分":ui->bound?L"从接入起累计 · 新战绩请保存游戏":L"从接入起累计 · 请保存游戏",
         (RECT){ui->width-px(ui,240),px(ui,5),ui->width-px(ui,12),px(ui,27)},S14_MUTED);
    const wchar_t *labels[]={L"杀敌数（含伤兵）",L"击溃部队",L"自身部队覆灭",L"自身兵力损失",L"击伤武将"};
    unsigned int flags[]={S14_STATS_DAMAGE,S14_STATS_ROUTS,S14_STATS_DEFEATS,S14_STATS_LOSSES,S14_STATS_INJURIES};
    uint64_t values[]={ui->stats.enemy_loss,ui->stats.units_routed,ui->stats.units_defeated,ui->stats.own_loss,ui->stats.officers_injured};
    int cell=(ui->width-px(ui,28))/5;
    for(int i=0;i<5;i++) {int x=px(ui,14)+i*cell;wchar_t value[32];if(ui->connected && (ui->stats.valid_mask&flags[i])) swprintf(value,32,L"%llu",(unsigned long long)values[i]);else wcscpy(value,L"未采集");
        text(dc,ui->label_font,labels[i],(RECT){x,px(ui,29),x+cell-px(ui,8),px(ui,47)},S14_MUTED);
        text(dc,ui->value_font,value,(RECT){x,px(ui,46),x+cell-px(ui,8),px(ui,73)},S14_INK);
    }
}
static LRESULT CALLBACK procedure(HWND window,UINT msg,WPARAM w,LPARAM l) {
    S14DetailUI *ui=(void*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(msg==WM_NCCREATE) {ui=((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui);}
    if(msg==WM_NCHITTEST) return HTTRANSPARENT;if(msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if(msg==WM_ERASEBKGND) return 1;
    if(msg==WM_PAINT && ui) {PAINTSTRUCT p;HDC dc=BeginPaint(window,&p);s14_detail_paint(ui,dc);EndPaint(window,&p);return 0;}
    return DefWindowProcW(window,msg,w,l);
}
static void hide(S14DetailUI *ui) {if(ui->window) ShowWindow(ui->window,SW_HIDE);ui->shown=0;}
static int fonts(S14DetailUI *ui,int scale) {
    if(ui->scale==scale && ui->value_font) return 1;
    if(ui->header_font) DeleteObject(ui->header_font);if(ui->label_font) DeleteObject(ui->label_font);if(ui->value_font) DeleteObject(ui->value_font);
    ui->scale=scale;
    ui->header_font=CreateFontW(-px(ui,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->label_font=CreateFontW(-px(ui,14),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->value_font=CreateFontW(-px(ui,22),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    return ui->header_font && ui->label_font && ui->value_font;
}
static int create(S14DetailUI *ui,HINSTANCE instance,HWND owner) {
    WNDCLASSEXW c={.cbSize=sizeof(c),.lpfnWndProc=procedure,.hInstance=instance,.lpszClassName=detail_class};
    if(!RegisterClassExW(&c) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 0;
    ui->window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED,detail_class,L"武将详情战绩",WS_POPUP,0,0,1,1,owner,NULL,instance,ui);
    if(!ui->window) return 0;if(!SetLayeredWindowAttributes(ui->window,0,245,LWA_ALPHA)) {DestroyWindow(ui->window);ui->window=NULL;return 0;}
    ui->owner=owner;ui->instance=instance;return 1;
}
void s14_detail_tick(S14DetailUI *ui,HINSTANCE instance,HWND owner,uintptr_t base,int enabled,ULONGLONG now) {
    ui->enabled=enabled;if(!enabled || !owner || !IsWindowVisible(owner) || IsIconic(owner) || GetForegroundWindow()!=owner) {hide(ui);return;}
    if(now<ui->next_read) return;ui->next_read=now+250;
    if(ui->base!=base) {ui->base=base;ui->ready=s14_detail_validate(memory,NULL,base);}
    if(!ui->ready) {hide(ui);return;}
    S14DetailFrame frame;if(!s14_detail_capture(memory,NULL,base,&frame)) {hide(ui);return;}
    RECT client,bar;POINT origin={0};int scale;
    if(!GetClientRect(owner,&client) || !ClientToScreen(owner,&origin) || !s14_detail_rect(&frame.panel,client.right,client.bottom,&bar,&scale) || !fonts(ui,scale)) {hide(ui);return;}
    if(!ui->window && !create(ui,instance,owner)) return;
    ui->frame=frame;memset(&ui->stats,0,sizeof(ui->stats));ui->incomplete=ui->bound=0;
    ui->connected=s14_stats_row(frame.world,frame.officer_id,&ui->stats,&ui->incomplete,&ui->bound);
    int width=bar.right-bar.left,height=bar.bottom-bar.top;
    if(width!=ui->width || height!=ui->height) {ui->width=width;ui->height=height;HRGN region=CreateRoundRectRgn(0,0,width+1,height+1,px(ui,10),px(ui,10));if(region && !SetWindowRgn(ui->window,region,FALSE)) DeleteObject(region);}
    ui->shown=SetWindowPos(ui->window,HWND_TOPMOST,origin.x+bar.left,origin.y+bar.top,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=0;
    InvalidateRect(ui->window,NULL,FALSE);
}
void s14_detail_destroy(S14DetailUI *ui) {if(ui->window) DestroyWindow(ui->window);if(ui->header_font) DeleteObject(ui->header_font);if(ui->label_font) DeleteObject(ui->label_font);if(ui->value_font) DeleteObject(ui->value_font);memset(ui,0,sizeof(*ui));}
