#define WIN32_LEAN_AND_MEAN
#include <windowsx.h>
#include <wchar.h>
#include "manager_ui.h"

static const wchar_t manager_class[]=L"S14ModManager.Panel.v2";
static int px(S14ManagerUI *ui,int v) { return MulDiv(v,ui->scale,96); }
static RECT box(S14ManagerUI *ui,int x,int y,int w,int h) { return (RECT){px(ui,x),px(ui,y),px(ui,x+w),px(ui,y+h)}; }
static void fill(HDC dc,RECT r,COLORREF color) { HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&r,brush); DeleteObject(brush); }
static void rounded(HDC dc,RECT r,COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color); HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,r.left,r.top,r.right,r.bottom,12,12); SelectObject(dc,old_pen); SelectObject(dc,old_brush); DeleteObject(brush);
}
static void label(S14ManagerUI *ui,HDC dc,const wchar_t *text,RECT r,HFONT font,COLORREF color,UINT format) {
    (void)ui; HGDIOBJ old=SelectObject(dc,font); SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,text,-1,&r,format|DT_NOPREFIX); SelectObject(dc,old);
}
static void button(S14ManagerUI *ui,HDC dc,const wchar_t *text,int x,int y,int w,int id,int enabled) {
    RECT r=box(ui,x,y,w,40); rounded(dc,r,enabled?RGB(49,57,69):RGB(34,39,47));
    if (ui->focus==id) { HPEN pen=CreatePen(PS_SOLID,px(ui,1),RGB(226,187,115)); HGDIOBJ p=SelectObject(dc,pen),b=SelectObject(dc,GetStockObject(NULL_BRUSH));
        RoundRect(dc,r.left,r.top,r.right,r.bottom,12,12); SelectObject(dc,b); SelectObject(dc,p); DeleteObject(pen); }
    label(ui,dc,text,r,ui->body_font,enabled?RGB(239,242,248):RGB(122,132,145),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}
static void toggle(S14ManagerUI *ui,HDC dc,int y,int enabled,int active,int id) {
    RECT r=box(ui,643,y,64,30); rounded(dc,r,enabled&&active?RGB(210,167,88):RGB(62,71,84));
    int x=enabled?678:647; RECT knob=box(ui,x,y+4,22,22); rounded(dc,knob,RGB(248,248,249));
    if (ui->focus==id) { RECT f=box(ui,638,y-5,74,40); FrameRect(dc,&f,GetStockObject(WHITE_BRUSH)); }
}

void s14_manager_paint(S14ManagerUI *ui,HDC dc,int width,int height) {
    fill(dc,(RECT){0,0,width,height},RGB(20,24,30));
    fill(dc,box(ui,0,0,760,5),RGB(217,175,97));
    label(ui,dc,L"天下归心 · 功能管理器",box(ui,28,22,670,36),ui->title_font,RGB(244,228,198),DT_SINGLELINE|DT_VCENTER);
    wchar_t subtitle[96]; swprintf(subtitle,96,L"三国志 14  |  v%ls  |  %ls",S14_MANAGER_VERSION,ui->in_game?L"F10 打开 / 收起":L"独立管理与安装");
    label(ui,dc,subtitle,box(ui,30,63,670,24),ui->small_font,RGB(156,168,185),DT_SINGLELINE);
    label(ui,dc,L"×",box(ui,710,18,30,32),ui->title_font,RGB(168,180,194),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    const wchar_t *tabs[]={L"功能开关",L"安装与状态",L"更多拓展"};
    for (int i=0;i<3;i++) {
        RECT r=box(ui,28+i*176,102,164,36); rounded(dc,r,ui->tab==i?RGB(57,52,43):RGB(30,36,44));
        label(ui,dc,tabs[i],r,ui->body_font,ui->tab==i?RGB(244,209,142):RGB(165,178,195),DT_SINGLELINE|DT_CENTER|DT_VCENTER);
    }
    if (ui->tab==0) {
        rounded(dc,box(ui,28,154,704,46),RGB(32,39,49));
        label(ui,dc,L"扩展功能总开关",box(ui,44,164,280,28),ui->body_font,RGB(229,235,244),DT_SINGLELINE);
        toggle(ui,dc,162,!!(ui->requested&S14_MASTER),1,1);
        for (int i=ui->scroll;i<s14_feature_count && i<ui->scroll+3;i++) {
            const S14Feature *feature=&s14_features[i]; int y=214+(i-ui->scroll)*112;
            rounded(dc,box(ui,28,y,704,102),RGB(30,36,44));
            label(ui,dc,feature->category,box(ui,44,y+12,72,22),ui->small_font,RGB(207,172,107),DT_SINGLELINE);
            label(ui,dc,feature->name,box(ui,122,y+10,400,26),ui->body_font,RGB(237,241,248),DT_SINGLELINE);
            label(ui,dc,feature->description,box(ui,44,y+43,562,52),ui->small_font,RGB(163,176,192),DT_WORDBREAK);
            int active=!!(ui->effective&feature->flag);
            toggle(ui,dc,y+20,!!(ui->requested&feature->flag),active,10+i);
            const wchar_t *state=ui->requested&feature->flag?(active?L"已开启":L"等待依赖"):L"已关闭";
            if (!(ui->requested&S14_MASTER) && ui->requested&feature->flag) state=L"总开关关闭";
            label(ui,dc,state,box(ui,622,y+60,105,24),ui->small_font,RGB(155,169,187),DT_CENTER|DT_SINGLELINE);
        }
        if (s14_feature_count>3) label(ui,dc,L"滚轮查看更多功能",box(ui,30,548,700,24),ui->small_font,RGB(145,159,177),DT_RIGHT|DT_SINGLELINE);
    } else if (ui->tab==1) {
        label(ui,dc,L"游戏目录",box(ui,30,160,140,24),ui->body_font,RGB(234,238,245),DT_SINGLELINE);
        if (ui->in_game) label(ui,dc,ui->root,box(ui,30,193,690,48),ui->small_font,RGB(168,182,200),DT_WORDBREAK);
        else button(ui,dc,L"选择目录",640,186,92,30,1);
        label(ui,dc,ui->game_found?L"游戏文件：已找到 SAN14PK_SC.exe，不检查版本":L"游戏文件：当前目录未找到 SAN14PK_SC.exe",box(ui,30,250,690,28),ui->body_font,ui->game_found?RGB(149,218,175):RGB(230,185,114),DT_SINGLELINE);
        label(ui,dc,ui->installed?L"插件文件：已安装":L"插件文件：未安装或文件已改变",box(ui,30,290,690,28),ui->body_font,RGB(192,204,221),DT_SINGLELINE);
        label(ui,dc,ui->running?L"游戏正在运行，安装和移除需要先退出游戏。":L"游戏已关闭，可以安装或更新插件。",box(ui,30,330,690,28),ui->small_font,RGB(159,173,193),DT_SINGLELINE);
        if (!ui->in_game) {
            button(ui,dc,L"安装 / 更新",30,382,210,31,ui->game_found && !ui->running);
            button(ui,dc,L"移除插件",264,382,210,32,ui->installed && !ui->running);
            button(ui,dc,L"打开日志",498,382,234,33,ui->installed);
        } else button(ui,dc,L"打开日志",30,382,210,33,1);
        label(ui,dc,L"功能开关在游戏内生效；新增功能版本需退出后更新。\n移除插件会保留你的设置和备份，方便之后重新安装。",box(ui,30,456,700,72),ui->small_font,RGB(163,178,198),DT_WORDBREAK);
    } else {
        const wchar_t *names[]={L"规则与平衡",L"地图与提示",L"配置与管理"};
        const wchar_t *descriptions[]={L"可继续加入其他建造限制、势力规则与可调参数。",L"可增加范围显示、施工提醒、领地变化提示等辅助功能。",L"可扩展多套规则配置、导入导出与更新。"};
        for (int i=0;i<3;i++) { int y=164+i*119; rounded(dc,box(ui,28,y,704,102),RGB(30,36,44));
            label(ui,dc,names[i],box(ui,46,y+18,650,26),ui->body_font,RGB(236,220,186),DT_SINGLELINE);
            label(ui,dc,descriptions[i],box(ui,46,y+56,650,32),ui->small_font,RGB(168,183,202),DT_WORDBREAK); }
        label(ui,dc,L"以上为拓展方向，当前只提供“功能开关”页列出的功能。",box(ui,30,535,700,28),ui->small_font,RGB(149,163,183),DT_SINGLELINE);
    }
    fill(dc,box(ui,28,578,704,1),RGB(51,60,73));
    label(ui,dc,ui->status,box(ui,30,590,700,26),ui->small_font,ui->fault?RGB(248,169,139):RGB(164,216,182),DT_SINGLELINE|DT_END_ELLIPSIS);
    label(ui,dc,ui->notice[0]?ui->notice:L"设置自动保存。规则开关从下一次检查起生效，已建墙体会保留。",box(ui,30,620,704,26),ui->small_font,RGB(158,173,194),DT_SINGLELINE|DT_END_ELLIPSIS);
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
    }
    return 0;
}

int s14_manager_activate(S14ManagerUI *ui,int id) {
    if (id==90) { if (ui->in_game) s14_manager_toggle(ui); else DestroyWindow(ui->window); return 1; }
    if (id>=100 && id<=102) { ui->tab=id-100; ui->focus=id; s14_manager_refresh(ui); return 1; }
    unsigned int flag=id==1?S14_MASTER:(id>=10 && id<10+s14_feature_count?s14_features[id-10].flag:0);
    if (flag) {
        if (!ui->in_game && !ui->game_found) { wcscpy(ui->notice,L"请先选择含 SAN14PK_SC.exe 的目录，再调整功能开关。"); ui->tab=1; s14_manager_refresh(ui); return 0; }
        int enable=!(ui->requested&flag);
        if (!s14_config_set(ui->ini,flag,enable)) { wcscpy(ui->notice,L"设置保存失败，请检查游戏目录的写入权限。" ); s14_manager_refresh(ui); return 0; }
        ui->requested=s14_config_read(ui->ini); ui->effective=s14_effective_flags(ui->requested);
        wcscpy(ui->notice,L"设置已保存。" );
        if (ui->action) ui->action(ui,S14_ACTION_CONFIG,ui->context);
        s14_manager_refresh(ui); return 1;
    }
    if (ui->action && id>=30 && id<=33) {
        if (id==31 && (ui->in_game || ui->running || !ui->game_found)) return 0;
        if (id==32 && (ui->in_game || ui->running || !ui->installed)) return 0;
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
            if (ui->tab==1) { if (!ui->in_game) { items[count++]=30; items[count++]=31; items[count++]=32; } items[count++]=33; }
            items[count++]=90; for (int i=0;i<count;i++) if (items[i]==ui->focus) found=(i+1)%count;
            ui->focus=items[found];
            if (ui->tab==0 && ui->focus>=10 && ui->focus<10+s14_feature_count) { int index=ui->focus-10;
                if (index<ui->scroll) ui->scroll=index; if (index>=ui->scroll+3) ui->scroll=index-2; }
            s14_manager_refresh(ui); return 0; }
        if (wparam==VK_SPACE || wparam==VK_RETURN) { s14_manager_activate(ui,ui->focus); return 0; }
    }
    if (message==WM_CLOSE) { s14_manager_activate(ui,90); return 0; }
    if (message==WM_CTLCOLOREDIT || message==WM_CTLCOLORSTATIC) { SetTextColor((HDC)wparam,RGB(235,239,245)); SetBkColor((HDC)wparam,RGB(39,46,57)); return (LRESULT)GetStockObject(DKGRAY_BRUSH); }
    return DefWindowProcW(window,message,wparam,lparam);
}

int s14_manager_create(S14ManagerUI *ui,HINSTANCE instance,HWND owner,int in_game) {
    ui->instance=instance; ui->owner=owner; ui->in_game=in_game; ui->scale=96;
    typedef UINT (WINAPI *GetDpi)(HWND); GetDpi dpi=(GetDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");
    if (owner && dpi) ui->scale=(int)dpi(owner);
    if (ui->scale<96 || ui->scale>288) ui->scale=96;
    ui->title_font=CreateFontW(-px(ui,25),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->body_font=CreateFontW(-px(ui,17),0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->small_font=CreateFontW(-px(ui,14),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    WNDCLASSEXW cls={0}; cls.cbSize=sizeof(cls); cls.hInstance=instance; cls.lpfnWndProc=manager_proc; cls.lpszClassName=manager_class; cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    if (!RegisterClassExW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 0;
    ui->window=CreateWindowExW(WS_EX_TOOLWINDOW,manager_class,L"天下归心 · 三国志14功能管理器",WS_POPUP|WS_CLIPCHILDREN,0,0,px(ui,760),px(ui,670),owner,NULL,instance,ui);
    if (!ui->window) { s14_manager_destroy(ui); return 0; }
    if (!in_game) {
        ui->directory_edit=CreateWindowExW(0,L"EDIT",ui->root,WS_CHILD|WS_BORDER|ES_AUTOHSCROLL|ES_READONLY|WS_TABSTOP,px(ui,30),px(ui,190),px(ui,586),px(ui,34),ui->window,(HMENU)401,instance,NULL);
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
    ui->window=NULL; ui->title_font=ui->body_font=ui->small_font=NULL;
}
