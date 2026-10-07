#define WIN32_LEAN_AND_MEAN
#include <windowsx.h>
#include <wchar.h>
#include "manager_ui.h"
#include "package.h"
#include "theme.h"

static const wchar_t manager_class[]=L"S14ModManager.Panel.v2";
static int px(S14ManagerUI *ui,int v) { return MulDiv(v,ui->scale,96); }
static RECT box(S14ManagerUI *ui,int x,int y,int w,int h) { return (RECT){px(ui,x),px(ui,y),px(ui,x+w),px(ui,y+h)}; }
static void fill(HDC dc,RECT r,COLORREF color) { HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&r,brush); DeleteObject(brush); }
static void rounded(HDC dc,RECT r,COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color); HPEN pen=CreatePen(PS_SOLID,1,S14_BORDER); HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,12,12); SelectObject(dc,old_pen); SelectObject(dc,old_brush); DeleteObject(brush); DeleteObject(pen);
}
static void label(S14ManagerUI *ui,HDC dc,const wchar_t *text,RECT r,HFONT font,COLORREF color,UINT format) {
    (void)ui; HGDIOBJ old=SelectObject(dc,font); SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,text,-1,&r,format|DT_NOPREFIX); SelectObject(dc,old);
}
static void button(S14ManagerUI *ui,HDC dc,const wchar_t *text,int x,int y,int w,int id,int enabled) {
    RECT r=box(ui,x,y,w,40); int primary=id==31;
    rounded(dc,r,enabled?(primary?S14_ACCENT:S14_CARD):RGB(238,236,230));
    if (ui->focus==id) { HPEN pen=CreatePen(PS_SOLID,px(ui,2),S14_ACCENT); HGDIOBJ p=SelectObject(dc,pen),b=SelectObject(dc,GetStockObject(NULL_BRUSH));
        RoundRect(dc,r.left,r.top,r.right,r.bottom,12,12); SelectObject(dc,b); SelectObject(dc,p); DeleteObject(pen); }
    label(ui,dc,text,r,ui->body_font,enabled?(primary?RGB(255,252,248):id==34?S14_ERROR:S14_INK):RGB(137,133,124),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}
static void toggle(S14ManagerUI *ui,HDC dc,int y,int enabled,int active,int id) {
    RECT r=box(ui,643,y,64,30); rounded(dc,r,enabled&&active?S14_ACCENT:RGB(193,189,179));
    int x=enabled?678:647; RECT knob=box(ui,x,y+4,22,22); rounded(dc,knob,RGB(255,254,251));
    if (ui->focus==id) { RECT f=box(ui,638,y-5,74,40); HBRUSH brush=CreateSolidBrush(S14_ACCENT); FrameRect(dc,&f,brush); DeleteObject(brush); }
}

void s14_manager_paint(S14ManagerUI *ui,HDC dc,int width,int height) {
    fill(dc,(RECT){0,0,width,height},S14_PAPER);
    label(ui,dc,L"天下归心 · 功能管理器",box(ui,28,22,670,36),ui->title_font,S14_INK,DT_SINGLELINE|DT_VCENTER);
    wchar_t subtitle[96]; swprintf(subtitle,96,L"三国志 14  |  v%ls  |  %ls",S14_MANAGER_VERSION,ui->in_game?L"F10 打开 / 收起":L"独立管理与安装");
    label(ui,dc,subtitle,box(ui,30,63,670,24),ui->small_font,S14_MUTED,DT_SINGLELINE);
    label(ui,dc,L"×",box(ui,710,18,30,32),ui->title_font,S14_MUTED,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    const wchar_t *tabs[]={L"功能开关",L"安装与状态",L"更多拓展"};
    for (int i=0;i<3;i++) {
        RECT r=box(ui,28+i*176,102,164,36); rounded(dc,r,ui->tab==i?S14_ACCENT_SOFT:S14_PAPER);
        label(ui,dc,tabs[i],r,ui->body_font,ui->tab==i?S14_ACCENT:S14_MUTED,DT_SINGLELINE|DT_CENTER|DT_VCENTER);
        if (ui->focus==100+i) { HBRUSH brush=CreateSolidBrush(S14_ACCENT); FrameRect(dc,&r,brush); DeleteObject(brush); }
    }
    if (ui->tab==0) {
        rounded(dc,box(ui,28,154,704,46),S14_ACCENT_SOFT);
        label(ui,dc,L"扩展功能总开关",box(ui,44,164,280,28),ui->body_font,S14_INK,DT_SINGLELINE);
        toggle(ui,dc,162,!!(ui->requested&S14_MASTER),1,1);
        for (int i=ui->scroll;i<s14_feature_count && i<ui->scroll+3;i++) {
            const S14Feature *feature=&s14_features[i]; int y=214+(i-ui->scroll)*112;
            rounded(dc,box(ui,28,y,704,102),S14_CARD);
            label(ui,dc,feature->category,box(ui,44,y+12,72,22),ui->small_font,S14_ACCENT,DT_SINGLELINE);
            label(ui,dc,feature->name,box(ui,122,y+10,400,26),ui->body_font,S14_INK,DT_SINGLELINE);
            label(ui,dc,feature->description,box(ui,44,y+43,562,52),ui->small_font,S14_MUTED,DT_WORDBREAK);
            int active=!!(ui->effective&feature->flag);
            toggle(ui,dc,y+20,!!(ui->requested&feature->flag),active,10+i);
            const wchar_t *state=ui->requested&feature->flag?(active?L"已开启":L"等待依赖"):L"已关闭";
            if (!(ui->requested&S14_MASTER) && ui->requested&feature->flag) state=L"总开关关闭";
            label(ui,dc,state,box(ui,622,y+60,105,24),ui->small_font,S14_MUTED,DT_CENTER|DT_SINGLELINE);
        }
        if (s14_feature_count>3) label(ui,dc,L"滚轮查看更多功能",box(ui,30,548,700,24),ui->small_font,S14_MUTED,DT_RIGHT|DT_SINGLELINE);
    } else if (ui->tab==1) {
        label(ui,dc,L"游戏目录",box(ui,30,160,140,24),ui->body_font,S14_INK,DT_SINGLELINE);
        if (ui->in_game) label(ui,dc,ui->root,box(ui,30,193,690,48),ui->small_font,S14_MUTED,DT_WORDBREAK);
        else { rounded(dc,box(ui,30,186,588,40),S14_CARD); if (!ui->directory_edit) label(ui,dc,ui->root,box(ui,40,198,570,24),ui->small_font,S14_INK,DT_SINGLELINE|DT_END_ELLIPSIS); button(ui,dc,L"选择目录",640,186,92,30,1); }
        label(ui,dc,ui->game_found?L"游戏文件：已找到 SAN14PK_SC.exe，不检查版本":L"游戏文件：当前目录未找到 SAN14PK_SC.exe",box(ui,30,250,690,28),ui->body_font,ui->game_found?S14_SUCCESS:S14_ACCENT,DT_SINGLELINE);
        const wchar_t *installation=ui->installed?L"插件文件：已识别本项目插件，可更新或卸载":
            ui->detected&(S14_FOUND_MANAGER|S14_FOUND_DATA)?L"插件文件：未安装，已检测到管理器或残留记录":L"插件文件：未检测到本项目安装";
        if (ui->detected&S14_FOUND_UNKNOWN) installation=L"插件文件：发现未知同名文件，清理时会保留";
        label(ui,dc,installation,box(ui,30,290,690,28),ui->body_font,S14_INK,DT_SINGLELINE);
        label(ui,dc,ui->running?L"游戏正在运行，安装和卸载需要先退出游戏。":L"游戏已关闭，可以安装、更新或卸载。",box(ui,30,330,690,28),ui->small_font,S14_MUTED,DT_SINGLELINE);
        if (!ui->in_game) {
            button(ui,dc,L"安装 / 更新",30,382,210,31,ui->game_found && !ui->running);
            button(ui,dc,L"移除插件",264,382,210,32,ui->installed && !ui->running);
            button(ui,dc,L"打开日志",498,382,234,33,ui->installed || !!(ui->detected&S14_FOUND_DATA));
            button(ui,dc,L"彻底卸载",30,438,210,34,!ui->running && !!(ui->detected&(S14_FOUND_DLL|S14_FOUND_MANAGER|S14_FOUND_DATA)));
            label(ui,dc,L"移除插件：保留设置与备份，方便重装。\n彻底卸载：清理本项目设置、日志、备份和管理器。\n请从游戏目录外的安装包执行彻底卸载。",box(ui,264,438,468,84),ui->small_font,S14_MUTED,DT_WORDBREAK);
        } else button(ui,dc,L"打开日志",30,382,210,33,1);
        label(ui,dc,ui->in_game?L"功能开关在游戏内生效；安装、更新和卸载需退出后通过独立管理器操作。":L"同名旧版缺少记录也可识别；未知文件、游戏本体与存档保留。",box(ui,30,ui->in_game?456:540,700,48),ui->small_font,S14_MUTED,DT_WORDBREAK);
    } else {
        const wchar_t *names[]={L"规则与平衡",L"地图与提示",L"配置与管理"};
        const wchar_t *descriptions[]={L"可继续加入其他建造限制、势力规则与可调参数。",L"可增加范围显示、施工提醒、领地变化提示等辅助功能。",L"可扩展多套规则配置、导入导出与更新。"};
        for (int i=0;i<3;i++) { int y=164+i*119; rounded(dc,box(ui,28,y,704,102),S14_CARD);
            label(ui,dc,names[i],box(ui,46,y+18,650,26),ui->body_font,S14_INK,DT_SINGLELINE);
            label(ui,dc,descriptions[i],box(ui,46,y+56,650,32),ui->small_font,S14_MUTED,DT_WORDBREAK); }
        label(ui,dc,L"以上为拓展方向，当前只提供“功能开关”页列出的功能。",box(ui,30,535,700,28),ui->small_font,S14_MUTED,DT_SINGLELINE);
    }
    fill(dc,box(ui,28,578,704,1),S14_BORDER);
    label(ui,dc,ui->status,box(ui,30,590,700,26),ui->small_font,ui->fault?S14_ERROR:S14_SUCCESS,DT_SINGLELINE|DT_END_ELLIPSIS);
    label(ui,dc,ui->notice[0]?ui->notice:L"设置自动保存。规则开关从下一次检查起生效，已建墙体会保留。",box(ui,30,617,704,42),ui->small_font,ui->notice_error?S14_ERROR:S14_MUTED,DT_WORDBREAK);
}

int s14_manager_hit(S14ManagerUI *ui,POINT point) {
    int x=MulDiv(point.x,96,ui->scale),y=MulDiv(point.y,96,ui->scale);
    if (x>=704 && x<746 && y>=12 && y<56) return 90;
    if (y>=102 && y<138 && x>=28 && x<556) { int tab=(x-28)/176; return (x-28)%176<164?100+tab:0; }
    if (ui->tab==0 && x>=622 && x<732) {
        if (y>=154 && y<200) return 1;
        for (int i=ui->scroll;i<s14_feature_count && i<ui->scroll+3;i++) if (y>=214+(i-ui->scroll)*112 && y<316+(i-ui->scroll)*112) return 10+i;
    }
    if (ui->tab==1) {
        if (!ui->in_game && x>=640 && x<732 && y>=186 && y<226) return 30;
        if (y>=382 && y<422) {
            if (x>=30 && x<240) return ui->in_game?33:31;
            if (!ui->in_game && x>=264 && x<474) return 32;
            if (!ui->in_game && x>=498 && x<732) return 33;
        }
        if (!ui->in_game && x>=30 && x<240 && y>=438 && y<478) return 34;
    }
    return 0;
}

int s14_manager_activate(S14ManagerUI *ui,int id) {
    if (id==90) { if (ui->in_game) s14_manager_toggle(ui); else DestroyWindow(ui->window); return 1; }
    if (id>=100 && id<=102) { ui->tab=id-100; ui->focus=id; s14_manager_refresh(ui); return 1; }
    unsigned int flag=id==1?S14_MASTER:(id>=10 && id<10+s14_feature_count?s14_features[id-10].flag:0);
    if (flag) {
        if (!ui->in_game && !ui->game_found) { ui->notice_error=1; wcscpy(ui->notice,L"请先选择含 SAN14PK_SC.exe 的目录，再调整功能开关。"); ui->tab=1; s14_manager_refresh(ui); return 0; }
        int enable=!(ui->requested&flag);
        if (!s14_config_set(ui->ini,flag,enable)) { ui->notice_error=1; wcscpy(ui->notice,L"设置保存失败，请检查游戏目录的写入权限。" ); s14_manager_refresh(ui); return 0; }
        ui->requested=s14_config_read(ui->ini); ui->effective=s14_effective_flags(ui->requested);
        ui->notice_error=0; wcscpy(ui->notice,L"设置已保存。" );
        if (ui->action) ui->action(ui,S14_ACTION_CONFIG,ui->context);
        s14_manager_refresh(ui); return 1;
    }
    if (ui->action && id>=30 && id<=34) {
        if (id==31 && (ui->in_game || ui->running || !ui->game_found)) return 0;
        if (id==32 && (ui->in_game || ui->running || !ui->installed)) return 0;
        if (id==34 && (ui->in_game || ui->running || !(ui->detected&(S14_FOUND_DLL|S14_FOUND_MANAGER|S14_FOUND_DATA)))) return 0;
        ui->action(ui,id-30+S14_ACTION_CHOOSE,ui->context); s14_manager_refresh(ui); return 1;
    }
    return 0;
}

static LRESULT CALLBACK manager_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    S14ManagerUI *ui=(S14ManagerUI*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if (message==WM_NCCREATE) { ui=((CREATESTRUCTW*)lparam)->lpCreateParams; ui->window=window; SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui); }
    if (!ui) return DefWindowProcW(window,message,wparam,lparam);
    if (message==WM_ERASEBKGND) return 1;
    if (message==WM_NCHITTEST) {
        POINT p={GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}; ScreenToClient(window,&p);
        return p.y<px(ui,90) && !s14_manager_hit(ui,p)?HTCAPTION:HTCLIENT;
    }
    if (message==WM_PAINT) {
        PAINTSTRUCT paint; HDC dc=BeginPaint(window,&paint); RECT r; GetClientRect(window,&r);
        HDC back=CreateCompatibleDC(dc); HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom); HGDIOBJ old=SelectObject(back,bitmap);
        s14_manager_paint(ui,back,r.right,r.bottom); BitBlt(dc,0,0,r.right,r.bottom,back,0,0,SRCCOPY);
        SelectObject(back,old); DeleteObject(bitmap); DeleteDC(back); EndPaint(window,&paint); return 0;
    }
    if (message==WM_LBUTTONDOWN) { ui->pressed=s14_manager_hit(ui,(POINT){GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}); ui->focus=ui->pressed; SetCapture(window); s14_manager_refresh(ui); return 0; }
    if (message==WM_LBUTTONUP) { int hit=s14_manager_hit(ui,(POINT){GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}); ReleaseCapture();
        if (hit && hit==ui->pressed) s14_manager_activate(ui,hit); ui->pressed=0; return 0; }
    if (message==WM_MOUSEWHEEL && ui->tab==0 && s14_feature_count>3) {
        ui->scroll+=GET_WHEEL_DELTA_WPARAM(wparam)<0?1:-1;
        if (ui->scroll<0) ui->scroll=0; if (ui->scroll>s14_feature_count-3) ui->scroll=s14_feature_count-3;
        s14_manager_refresh(ui); return 0;
    }
    if (message==WM_KEYDOWN) {
        if (wparam==VK_ESCAPE) { s14_manager_activate(ui,90); return 0; }
        if (wparam==VK_TAB) { int items[64]={100,101,102},count=3,found=0;
            if (ui->tab==0) { items[count++]=1; for (int i=0;i<s14_feature_count && count<62;i++) items[count++]=10+i; }
            if (ui->tab==1) { if (!ui->in_game) { items[count++]=30; items[count++]=31; items[count++]=32; items[count++]=34; } items[count++]=33; }
            items[count++]=90; for (int i=0;i<count;i++) if (items[i]==ui->focus) found=(i+1)%count;
            ui->focus=items[found];
            if (ui->tab==0 && ui->focus>=10 && ui->focus<10+s14_feature_count) { int index=ui->focus-10;
                if (index<ui->scroll) ui->scroll=index; if (index>=ui->scroll+3) ui->scroll=index-2; }
            s14_manager_refresh(ui); return 0; }
        if (wparam==VK_SPACE || wparam==VK_RETURN) { s14_manager_activate(ui,ui->focus); return 0; }
    }
    if (message==WM_CLOSE) { s14_manager_activate(ui,90); return 0; }
    if (message==WM_CTLCOLOREDIT || message==WM_CTLCOLORSTATIC) { SetTextColor((HDC)wparam,S14_INK); SetBkColor((HDC)wparam,S14_CARD); return (LRESULT)ui->edit_brush; }
    return DefWindowProcW(window,message,wparam,lparam);
}

int s14_manager_create(S14ManagerUI *ui,HINSTANCE instance,HWND owner,int in_game) {
    ui->instance=instance; ui->owner=owner; ui->in_game=in_game; ui->scale=96;
    typedef UINT (WINAPI *GetDpi)(HWND); GetDpi dpi=(GetDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");
    if (owner && dpi) ui->scale=(int)dpi(owner);
    if (ui->scale<96 || ui->scale>288) ui->scale=96;
    ui->title_font=CreateFontW(-px(ui,29),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"SimSun");
    ui->body_font=CreateFontW(-px(ui,17),0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->small_font=CreateFontW(-px(ui,14),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->edit_brush=CreateSolidBrush(S14_CARD);
    if (!ui->title_font || !ui->body_font || !ui->small_font || !ui->edit_brush) { s14_manager_destroy(ui); return 0; }
    WNDCLASSEXW cls={0}; cls.cbSize=sizeof(cls); cls.hInstance=instance; cls.lpfnWndProc=manager_proc; cls.lpszClassName=manager_class; cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    if (!RegisterClassExW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 0;
    ui->window=CreateWindowExW(WS_EX_TOOLWINDOW,manager_class,L"天下归心 · 三国志14功能管理器",WS_POPUP|WS_CLIPCHILDREN,0,0,px(ui,760),px(ui,670),owner,NULL,instance,ui);
    if (!ui->window) { s14_manager_destroy(ui); return 0; }
    if (!in_game) {
        ui->directory_edit=CreateWindowExW(0,L"EDIT",ui->root,WS_CHILD|ES_AUTOHSCROLL|ES_READONLY|WS_TABSTOP,px(ui,40),px(ui,198),px(ui,568),px(ui,24),ui->window,(HMENU)401,instance,NULL);
        SendMessageW(ui->directory_edit,WM_SETFONT,(WPARAM)ui->small_font,TRUE); SendMessageW(ui->directory_edit,EM_SETLIMITTEXT,MAX_PATH-1,0);
    }
    ui->requested=s14_config_read(ui->ini); ui->effective=s14_effective_flags(ui->requested); return 1;
}
void s14_manager_toggle(S14ManagerUI *ui) {
    if (!ui->window) return;
    if (IsWindowVisible(ui->window)) { ShowWindow(ui->window,SW_HIDE); if (ui->owner) SetForegroundWindow(ui->owner); return; }
    RECT region; if (ui->owner) { GetWindowRect(ui->owner,&region); } else SystemParametersInfoW(SPI_GETWORKAREA,0,&region,0);
    int w=px(ui,760),h=px(ui,670); int x=region.left+(region.right-region.left-w)/2,y=region.top+(region.bottom-region.top-h)/2;
    SetWindowPos(ui->window,ui->in_game?HWND_TOPMOST:HWND_TOP,x,y,w,h,SWP_SHOWWINDOW); SetForegroundWindow(ui->window); s14_manager_refresh(ui);
}
void s14_manager_refresh(S14ManagerUI *ui) {
    if (!ui->window || !IsWindow(ui->window)) return;
    if (ui->directory_edit) ShowWindow(ui->directory_edit,ui->tab==1?SW_SHOW:SW_HIDE);
    InvalidateRect(ui->window,NULL,FALSE);
}
void s14_manager_destroy(S14ManagerUI *ui) {
    if (ui->window && IsWindow(ui->window)) DestroyWindow(ui->window);
    if (ui->title_font) DeleteObject(ui->title_font); if (ui->body_font) DeleteObject(ui->body_font); if (ui->small_font) DeleteObject(ui->small_font);
    if (ui->edit_brush) DeleteObject(ui->edit_brush); ui->edit_brush=NULL;
    ui->window=NULL; ui->title_font=ui->body_font=ui->small_font=NULL;
}
