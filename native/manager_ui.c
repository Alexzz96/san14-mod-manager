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
    const wchar_t *tabs[]={L"功能开关",L"安装与状态",L"更多拓展",L"探索与回合报告"};
    for (int i=0;i<4;i++) {
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
            label(ui,dc,L"移除插件保留设置；彻底卸载清理项目数据。\n请从游戏目录外执行彻底卸载。",box(ui,264,438,468,66),ui->small_font,S14_MUTED,DT_WORDBREAK);
        } else button(ui,dc,L"打开日志",30,382,210,33,1);
        button(ui,dc,ui->in_game?L"检查 GitHub 更新":ui->update_busy?L"正在连接…":L"检查 GitHub 更新",30,520,210,35,!ui->update_busy);
        if(!ui->in_game)button(ui,dc,L"下载并更新",264,520,170,36,ui->update_ready && !ui->update_busy && !ui->running && ui->game_found);
        label(ui,dc,ui->update_status[0]?ui->update_status:ui->in_game?L"将打开独立管理器；安装更新需退出游戏。":L"检查项目发布版本；退出游戏后可下载更新。",box(ui,ui->in_game?264:448,520,ui->in_game?468:284,49),ui->small_font,S14_MUTED,DT_WORDBREAK);
    } else if (ui->tab==3) {
        rounded(dc,box(ui,28,154,704,46),S14_ACCENT_SOFT);
        label(ui,dc,L"确认进行后自动派遣探索",box(ui,44,164,540,28),ui->body_font,S14_INK,DT_SINGLELINE);
        toggle(ui,dc,162,!!(ui->requested&S14_AUTO_SEARCH),!!(ui->effective&S14_AUTO_SEARCH),13);
        const wchar_t *titles[]={L"执行武将",L"返回天数",L"重视项目"};
        const wchar_t *options[3][4]={{L"全武将",L"各官员以外",L"县府官员以外",L"城市官员以外"},
            {L"10 天以内",L"20 天以内",L"无限制",NULL},{L"优先军师推荐",L"优先能力",NULL,NULL}};
        for (int g=0;g<3;g++) {
            int y=211+g*60,n=g==0?4:g==1?3:2,w=704/n;
            label(ui,dc,titles[g],box(ui,30,y,680,22),ui->small_font,S14_MUTED,DT_SINGLELINE);
            for (int i=0;i<n;i++) {
                int id=200+10*g+i,selected=(int)((ui->search_settings>>(g*4))&15)==i;
                RECT r=box(ui,28+i*w,y+25,w-8,29); rounded(dc,r,selected?S14_ACCENT_SOFT:S14_CARD);
                label(ui,dc,options[g][i],r,ui->small_font,selected?S14_ACCENT:S14_INK,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
                if (ui->focus==id) { HBRUSH b=CreateSolidBrush(S14_ACCENT); FrameRect(dc,&r,b); DeleteObject(b); }
            }
        }
        label(ui,dc,ui->search_status[0]?ui->search_status:L"使用剩余政令；正在执行其他任务的武将由游戏自动排除。",box(ui,30,394,700,24),ui->small_font,S14_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
        rounded(dc,box(ui,28,426,704,136),S14_CARD);
        label(ui,dc,ui->search_summary[0]?ui->search_summary:L"回合结束后显示搜索所得。长途探索会在实际发现的回合计入。",box(ui,44,438,672,38),ui->small_font,S14_INK,DT_WORDBREAK);
        const wchar_t *detail=ui->search_details;
        for (int i=0;i<ui->search_scroll && *detail;i++) { const wchar_t *next=wcschr(detail,L'\n'); if (!next) break; detail=next+1; }
        label(ui,dc,*detail?detail:L"结果包含本势力的自动与手动探索；明细过多时可用滚轮查看。",box(ui,44,480,668,73),ui->small_font,S14_MUTED,DT_WORDBREAK);
        if(ui->in_game) {rounded(dc,box(ui,44,559,230,30),S14_ACCENT_SOFT);label(ui,dc,L"查看上一回合报告",box(ui,44,559,230,30),ui->small_font,S14_ACCENT,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    } else {
        const wchar_t *names[]={L"武将一览 · F 快捷键",L"战斗数据记录与战绩",L"原生武将详情 · 战绩条"};
        const wchar_t *descriptions[]={L"中文、拼音及首字母搜索；能力、内心信息和累计战绩排序。",L"采集全势力战斗事件；关闭期间不累计，会标注采集缺口。",L"在游戏武将详情上方显示累计战绩，跟随当前武将；鼠标可穿透。"};
        for (int i=0;i<3;i++) { int y=164+i*119; rounded(dc,box(ui,28,y,704,102),S14_CARD);
            label(ui,dc,names[i],box(ui,46,y+18,650,26),ui->body_font,S14_INK,DT_SINGLELINE);
            label(ui,dc,descriptions[i],box(ui,46,y+56,550,32),ui->small_font,S14_MUTED,DT_WORDBREAK); }
        toggle(ui,dc,194,ui->officers_enabled,1,40);
        toggle(ui,dc,313,ui->battle_enabled,1,41);
        toggle(ui,dc,432,ui->native_stats_enabled,1,43);
        label(ui,dc,L"武将视图开关立即生效；输入搜索文字时不会触发 F 快捷键。",box(ui,30,535,700,28),ui->small_font,S14_MUTED,DT_SINGLELINE);
    }
    fill(dc,box(ui,28,578,704,1),S14_BORDER);
    label(ui,dc,ui->status,box(ui,30,590,700,26),ui->small_font,ui->fault?S14_ERROR:S14_SUCCESS,DT_SINGLELINE|DT_END_ELLIPSIS);
    label(ui,dc,ui->notice[0]?ui->notice:(ui->tab==3?L"统计本势力自动与手动探索。武将数量指发现人数；登用是否成功见明细。":L"设置自动保存。规则开关从下一次检查起生效，已建墙体会保留。"),box(ui,30,617,704,42),ui->small_font,ui->notice_error?S14_ERROR:S14_MUTED,DT_WORDBREAK);
}

int s14_manager_hit(S14ManagerUI *ui,POINT point) {
    int x=MulDiv(point.x,96,ui->scale),y=MulDiv(point.y,96,ui->scale);
    if (x>=704 && x<746 && y>=12 && y<56) return 90;
    if (y>=102 && y<138 && x>=28 && x<732) { int tab=(x-28)/176; return (x-28)%176<164?100+tab:0; }
    if (ui->tab==0 && x>=622 && x<732) {
        if (y>=154 && y<200) return 1;
        for (int i=ui->scroll;i<s14_feature_count && i<ui->scroll+3;i++) if (y>=214+(i-ui->scroll)*112 && y<316+(i-ui->scroll)*112) return 10+i;
    }
    if (ui->tab==1) {
        if(y>=520 && y<560){if(x>=30 && x<240)return 35;if(!ui->in_game && x>=264 && x<434)return 36;}
        if (!ui->in_game && x>=640 && x<732 && y>=186 && y<226) return 30;
        if (y>=382 && y<422) {
            if (x>=30 && x<240) return ui->in_game?33:31;
            if (!ui->in_game && x>=264 && x<474) return 32;
            if (!ui->in_game && x>=498 && x<732) return 33;
        }
        if (!ui->in_game && x>=30 && x<240 && y>=438 && y<478) return 34;
    }
    if (ui->tab==3) {
        if(ui->in_game && x>=44 && x<274 && y>=559 && y<589) return 42;
        if (x>=622 && x<732 && y>=154 && y<200) return 13;
        for (int g=0;g<3;g++) { int top=236+g*60,n=g==0?4:g==1?3:2,w=704/n;
            if (y>=top && y<top+29 && x>=28 && x<732) { int i=(x-28)/w; return i<n && (x-28)%w<w-8?200+g*10+i:0; }
        }
    }
    if (ui->tab==2 && x>=622 && x<732 && y>=184 && y<234) return 40;
    if (ui->tab==2 && x>=622 && x<732 && y>=303 && y<353) return 41;
    if (ui->tab==2 && x>=622 && x<732 && y>=422 && y<472) return 43;
    return 0;
}

int s14_manager_activate(S14ManagerUI *ui,int id) {
    if(id==35 || id==36){
        if(!ui->action || ui->update_busy || (id==36 && (ui->in_game || !ui->update_ready || ui->running || !ui->game_found)))return 0;
        ui->action(ui,id==35?S14_ACTION_CHECK_UPDATE:S14_ACTION_DOWNLOAD_UPDATE,ui->context);s14_manager_refresh(ui);return 1;
    }
    if(id==43) {
        if(!ui->in_game && !ui->game_found) return 0;
        if(!WritePrivateProfileStringW(L"Views",L"NativeOfficerStats",ui->native_stats_enabled?L"0":L"1",ui->ini)) return 0;
        ui->native_stats_enabled=!ui->native_stats_enabled;
        if(ui->action) ui->action(ui,S14_ACTION_CONFIG,ui->context);
        ui->notice_error=0;wcscpy(ui->notice,L"原生详情战绩条开关已保存；不影响战斗采集和 F 列表。");
        s14_manager_refresh(ui);return 1;
    }
    if(id==42 && ui->in_game) {ui->report_requested=1;return 1;}
    if (id==41) {
        if (!ui->in_game && !ui->game_found) return 0;
        if (!WritePrivateProfileStringW(L"Observation",L"BattleEvents",ui->battle_enabled?L"0":L"1",ui->ini)) return 0;
        ui->battle_enabled=!ui->battle_enabled;
        if(ui->action) ui->action(ui,S14_ACTION_CONFIG,ui->context);
        ui->notice_error=0;wcscpy(ui->notice,L"战斗记录设置已保存。F 查看累计战绩；新战绩需保存游戏。由总开关控制。");
        s14_manager_refresh(ui);return 1;
    }
    if (id==90) { if (ui->in_game) s14_manager_toggle(ui); else DestroyWindow(ui->window); return 1; }
    if (id>=100 && id<=103) { ui->tab=id-100; ui->focus=id; s14_manager_refresh(ui); return 1; }
    if (id==40) {
        if (!ui->in_game && !ui->game_found) return 0;
        if (!WritePrivateProfileStringW(L"Views",L"Officers",ui->officers_enabled?L"0":L"1",ui->ini)) return 0;
        ui->officers_enabled=!ui->officers_enabled;
        if (ui->action) ui->action(ui,S14_ACTION_CONFIG,ui->context);
        s14_manager_refresh(ui);return 1;
    }
    if (id>=200 && id<=221) {
        if (!ui->in_game && !ui->game_found) { ui->notice_error=1; wcscpy(ui->notice,L"请先选择游戏目录。"); s14_manager_refresh(ui); return 0; }
        int g=(id-200)/10,value=(id-200)%10;
        if (!s14_search_setting_set(ui->ini,g,value)) { ui->notice_error=1; wcscpy(ui->notice,L"搜索设置保存失败，请检查写入权限。"); s14_manager_refresh(ui); return 0; }
        ui->search_settings=s14_search_settings_read(ui->ini); ui->notice_error=0;
        wcscpy(ui->notice,L"搜索设置已保存，下次确认进行时使用。");
        if (ui->action) ui->action(ui,S14_ACTION_CONFIG,ui->context);
        s14_manager_refresh(ui); return 1;
    }
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
    if (message==WM_MOUSEWHEEL && ui->tab==3) {
        ui->search_scroll+=GET_WHEEL_DELTA_WPARAM(wparam)<0?1:-1;
        int lines=0; for (const wchar_t *p=ui->search_details;*p;p++) if (*p==L'\n') lines++;
        if (ui->search_scroll<0) ui->search_scroll=0; if (ui->search_scroll>lines) ui->search_scroll=lines;
        s14_manager_refresh(ui); return 0;
    }
    if (message==WM_KEYDOWN) {
        if (wparam==VK_ESCAPE) { s14_manager_activate(ui,90); return 0; }
        if (wparam==VK_TAB) { int items[64]={100,101,102,103},count=4,found=0;
            if (ui->tab==0) { items[count++]=1; for (int i=0;i<s14_feature_count && count<62;i++) items[count++]=10+i; }
            if (ui->tab==1) { if (!ui->in_game) { items[count++]=30; items[count++]=31; items[count++]=32; items[count++]=34; items[count++]=36; } items[count++]=33;items[count++]=35; }
            if (ui->tab==2) { items[count++]=40;items[count++]=41;items[count++]=43; }
            if (ui->tab==3) { items[count++]=13; for (int g=0;g<3;g++) for (int i=0;i<(g==0?4:g==1?3:2);i++) items[count++]=200+g*10+i; }
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
    ui->requested=s14_config_read(ui->ini); ui->effective=s14_effective_flags(ui->requested); ui->search_settings=s14_search_settings_read(ui->ini);
    ui->officers_enabled=GetPrivateProfileIntW(L"Views",L"Officers",1,ui->ini)!=0;
    ui->native_stats_enabled=GetPrivateProfileIntW(L"Views",L"NativeOfficerStats",1,ui->ini)!=0;
    ui->battle_enabled=GetPrivateProfileIntW(L"Observation",L"BattleEvents",0,ui->ini)!=0;return 1;
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
