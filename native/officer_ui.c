#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include "officer_ui.h"
#include "theme.h"
static const wchar_t officer_class[]=L"SAN14ModManager.Officers.v1";
static const wchar_t *columns[]={L"姓名",L"势力",L"身份",L"所在位置",L"统率",L"武力",L"智力",L"政治",L"魅力",L"总和",L"个性",L"兵力",L"击杀武将",L"击溃部队",L"野心",L"情义",L"忠诚",L"杀敌数（含伤兵）",L"自身部队覆灭",L"自身兵力损失",L"击伤武将"};
static const int column_width[]={90,80,64,140,50,50,50,50,50,56,200,70,80,90,60,60,60,140,110,110,90};
static int px(S14OfficerUI *ui,int n) { return MulDiv(n,ui->scale,96); }
static int read_memory(void *context,uintptr_t address,void *out,size_t size) {
    SIZE_T got=0;return ReadProcessMemory((HANDLE)context,(const void*)address,out,size,&got) && got==size;
}
static void paint_text(HDC dc,HFONT font,const wchar_t *s,RECT box,COLORREF color,UINT flags) {
    HGDIOBJ old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,s,-1,&box,flags|DT_NOPREFIX);SelectObject(dc,old);
}
static void card(HDC dc,S14OfficerUI *ui,RECT r,COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color);HPEN pen=CreatePen(PS_SOLID,1,S14_BORDER);
    HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,px(ui,14),px(ui,14));
    SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(brush);DeleteObject(pen);
}
static void layout(S14OfficerUI *ui) {
    RECT r;GetClientRect(ui->window,&r);int w=r.right,h=r.bottom;
    MoveWindow(ui->close_button,w-px(ui,128),px(ui,24),px(ui,96),px(ui,36),TRUE);
    MoveWindow(ui->query,px(ui,36),px(ui,116),w-px(ui,480),px(ui,32),TRUE);
    MoveWindow(ui->scope,w-px(ui,430),px(ui,116),px(ui,110),px(ui,200),TRUE);
    MoveWindow(ui->place,w-px(ui,308),px(ui,116),px(ui,122),px(ui,220),TRUE);
    MoveWindow(ui->history,w-px(ui,174),px(ui,116),px(ui,138),px(ui,200),TRUE);
    MoveWindow(ui->sort,px(ui,76),px(ui,158),px(ui,132),px(ui,470),TRUE);
    MoveWindow(ui->direction,px(ui,220),px(ui,157),px(ui,84),px(ui,32),TRUE);
    MoveWindow(ui->list,px(ui,30),px(ui,222),w-px(ui,60),h-px(ui,450),TRUE);
    MoveWindow(ui->details,px(ui,36),h-px(ui,180),w-px(ui,72),px(ui,152),TRUE);
}
static S14OfficerUI *sorting_ui;
static int compare_indices(const void *left,const void *right) {
    S14OfficerUI *ui=sorting_ui;
    return s14_officer_compare(&ui->snapshot->rows[*(const int*)left],&ui->snapshot->rows[*(const int*)right],ui->filter.sort,ui->filter.descending);
}
static void detail(S14OfficerUI *ui,int visible) {
    if (visible<0 || visible>=ui->visible_count) { SetWindowTextW(ui->details,L"选择一位武将，查看所属、位置、完整个性说明和战法。");return; }
    S14Officer *p=&ui->snapshot->rows[ui->indices[visible]];int same_person=ui->selected_id==p->id;ui->selected_id=p->id;
    wchar_t text[4096];swprintf(text,4096,L"%ls  ·  字 %ls  ·  %ls势力  ·  %ls  ·  %ls\r\n所在：%ls    归属据点：%ls    武将编号：%d\r\n基础能力：统率 %d / 武力 %d / 智力 %d / 政治 %d / 魅力 %d\r\n战法：%ls\r\n",
        p->name,p->courtesy[0]?p->courtesy:L"—",p->force_name,p->status_name,p->health_name,p->location,p->home_name,p->id,
        p->ability[0],p->ability[1],p->ability[2],p->ability[3],p->ability[4],p->tactics_text[0]?p->tactics_text:L"—");
    wchar_t ambition[16]=L"—",bond[16]=L"—",loyalty[16]=L"—",character[160];
    if (p->ambition>=0) swprintf(ambition,16,L"%d",p->ambition);
    if (p->bond>=0) swprintf(bond,16,L"%d",p->bond);
    if (p->loyalty>=0) swprintf(loyalty,16,L"%d",p->loyalty);
    swprintf(character,160,L"内心：野心 %ls / 情义 %ls / 忠诚 %ls（野心、情义为 1～5 级）\r\n",ambition,bond,loyalty);
    wcsncat(text,character,4095-wcslen(text));
    for (int j=0;j<9;j++) {
        int id=p->personalities[j];if (!id || id>=S14_PERSONALITY_MAX) continue;
        S14OfficerDefinition *d=&ui->snapshot->personalities[id];if (!d->name[0]) continue;
        wchar_t line[160];swprintf(line,160,L"%ls：%ls\r\n",d->name,d->description[0]?d->description:L"—");
        wcsncat(text,line,4095-wcslen(text));
    }
    wchar_t kills[32]=L"未采集",routs[32]=L"未采集",soldiers[32]=L"未采集",wounded[32]=L"未采集",career_line[256];
    if (p->career.valid_mask&S14_CAREER_KILLS) swprintf(kills,32,L"%llu",(unsigned long long)p->career.officer_kills);
    if (p->career.valid_mask&S14_CAREER_ROUTS) swprintf(routs,32,L"%llu",(unsigned long long)p->career.units_routed);
    if (p->career.valid_mask&S14_CAREER_SOLDIERS) swprintf(soldiers,32,L"%llu",(unsigned long long)p->career.soldiers_killed);
    if (p->career.valid_mask&S14_CAREER_WOUNDED) swprintf(wounded,32,L"%llu",(unsigned long long)p->career.soldiers_wounded);
    swprintf(career_line,256,L"战绩：击杀武将 %ls / 击溃部队 %ls / 击杀士兵 %ls / 伤兵 %ls",kills,routs,soldiers,wounded);
    wcsncat(text,p->career.valid_mask?career_line:L"击杀武将 / 实际阵亡士兵：未采集（尚未核实）",4095-wcslen(text));
    if(ui->snapshot->battle_connected) {
        wchar_t values[5][32];unsigned int flags[]={S14_STATS_DAMAGE,S14_STATS_ROUTS,S14_STATS_DEFEATS,S14_STATS_LOSSES,S14_STATS_INJURIES};
        uint64_t numbers[]={p->battle.enemy_loss,p->battle.units_routed,p->battle.units_defeated,p->battle.own_loss,p->battle.officers_injured};
        for(int i=0;i<5;i++) {if(p->battle.valid_mask&flags[i]) swprintf(values[i],32,L"%llu",(unsigned long long)numbers[i]);else wcscpy(values[i],L"未采集");}
        wchar_t battle_line[512];swprintf(battle_line,512,L"\r\n存档战绩：杀敌数（含伤兵）%ls / 击溃部队 %ls / 自身部队覆灭 %ls / 自身兵力损失 %ls / 击伤武将 %ls\r\n杀敌数为造成敌方兵力减少；仅归属明确的伤害计入，队伍计数按主将。%ls%ls",
            values[0],values[1],values[2],values[3],values[4],
            ui->snapshot->battle_bound?L"已建立存档检查点；新战绩请随游戏保存。":L"请保存游戏，以保留当前战绩。",ui->snapshot->battle_incomplete?L" 期间有采集缺口，数值为已记录部分。":L"");
        wcsncat(text,battle_line,4095-wcslen(text));
    }
    wchar_t previous[4096];GetWindowTextW(ui->details,previous,4096);
    if (wcscmp(previous,text)) {
        int first_line=same_person?(int)SendMessageW(ui->details,EM_GETFIRSTVISIBLELINE,0,0):0;
        SetWindowTextW(ui->details,text);if (first_line) SendMessageW(ui->details,EM_LINESCROLL,0,first_line);
    }
}
static void rebuild(S14OfficerUI *ui) {
    GetWindowTextW(ui->query,ui->filter.query,128);
    ui->filter.own_force=(int)SendMessageW(ui->scope,CB_GETCURSEL,0,0)==1;
    int place=(int)SendMessageW(ui->place,CB_GETCURSEL,0,0);ui->filter.place=place>0?place:0;
    ui->filter.include_history=(int)SendMessageW(ui->history,CB_GETCURSEL,0,0)==1;
    ui->visible_count=0;
    for (int i=0;i<ui->snapshot->count;i++) if (s14_officer_matches(&ui->snapshot->rows[i],ui->snapshot->player_force,&ui->filter))
        ui->indices[ui->visible_count++]=i;
    sorting_ui=ui;qsort(ui->indices,(size_t)ui->visible_count,sizeof(int),compare_indices);sorting_ui=NULL;
    ListView_SetItemCountEx(ui->list,ui->visible_count,LVSICF_NOSCROLL);
    int selected=-1;for (int i=0;i<ui->visible_count;i++) if (ui->snapshot->rows[ui->indices[i]].id==ui->selected_id) selected=i;
    if (selected>=0) { ListView_SetItemState(ui->list,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED); }
    else { ListView_SetItemState(ui->list,-1,0,LVIS_SELECTED|LVIS_FOCUSED); }
    detail(ui,selected);InvalidateRect(ui->list,NULL,FALSE);InvalidateRect(ui->window,NULL,FALSE);
}
static void capture_on_open(S14OfficerUI *ui) {
    if (!ui->window || !IsWindowVisible(ui->window)) return;
    LARGE_INTEGER start,finish,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
    if (!s14_officers_capture(read_memory,GetCurrentProcess(),ui->base,ui->candidate)) {
        ui->last_capture_ok=0;swprintf(ui->notice,160,L"本次未取得稳定数据，上次列表暂留；请回到地图后关闭并重新按 F 打开。");
        InvalidateRect(ui->window,NULL,FALSE);return;
    }
    s14_officers_attach_career(ui->candidate,&ui->career);
    S14BattleStatsSnapshot *stats=malloc(sizeof(*stats));int connected=stats && ui->battle.version==1 && ui->battle.read && ui->battle.read(ui->battle.context,ui->candidate->world,stats);
    s14_officers_attach_battle(ui->candidate,connected?stats:NULL);free(stats);
    S14OfficerSnapshot *old=ui->snapshot;ui->snapshot=ui->candidate;ui->candidate=old;
    ui->last_capture_ok=1;ui->captured_tick=GetTickCount64();
    QueryPerformanceCounter(&finish);double elapsed=(double)(finish.QuadPart-start.QuadPart)*1000.0/(double)frequency.QuadPart;
    swprintf(ui->notice,160,L"%d 位武将 · %.1f ms · %ls%ls · 按 F 重开刷新",ui->snapshot->count,elapsed,
        ui->snapshot->battle_connected?L"战绩从接入时起累计，随游戏保存":L"战绩未接入",ui->snapshot->battle_incomplete?L"（有采集缺口）":L"");
    rebuild(ui);
}
static void cell_text(const S14Officer *p,int column,wchar_t *out,size_t size) {
    const wchar_t *s=NULL;
    if (column==0) s=p->name;else if (column==1) s=p->force_name;else if (column==2) s=p->status_name;
    else if (column==3) s=p->location;else if (column==10) s=p->personality_text;
    if (s) { wcsncpy(out,s,size-1);out[size-1]=0;return; }
    if (column>=4 && column<=8) swprintf(out,size,L"%d",p->ability[column-4]);
    else if (column==9) swprintf(out,size,L"%d",p->total);
    else if (column==11) { if (p->troops>=0) swprintf(out,size,L"%d",p->troops);else wcscpy(out,L"—"); }
    else if (column==12 || column==13) {
        if(column==13 && (p->battle.valid_mask&S14_STATS_ROUTS)) {swprintf(out,size,L"%llu",(unsigned long long)p->battle.units_routed);return;}
        unsigned int flag=column==12?S14_CAREER_KILLS:S14_CAREER_ROUTS;
        if (p->career.valid_mask&flag) swprintf(out,size,L"%llu",(unsigned long long)(column==12?p->career.officer_kills:p->career.units_routed));
        else wcscpy(out,L"未采集");
    } else if (column>=14 && column<=16) {
        int value=column==14?p->ambition:column==15?p->bond:p->loyalty;
        if (value>=0) swprintf(out,size,L"%d",value);else wcscpy(out,L"—");
    } else if(column>=17 && column<=20) {
        unsigned int flag=column==17?S14_STATS_DAMAGE:column==18?S14_STATS_DEFEATS:column==19?S14_STATS_LOSSES:S14_STATS_INJURIES;
        uint64_t value=column==17?p->battle.enemy_loss:column==18?p->battle.units_defeated:column==19?p->battle.own_loss:p->battle.officers_injured;
        if(p->battle.valid_mask&flag) swprintf(out,size,L"%llu",(unsigned long long)value);else wcscpy(out,L"未采集");
    } else wcscpy(out,L"—");
}
static void hide(S14OfficerUI *ui) { ShowWindow(ui->window,SW_HIDE);if (ui->owner) SetForegroundWindow(ui->owner); }
int s14_owned_foreground(HWND owner,HWND panel,HWND foreground) {
    if (!foreground) return 0;
    /* An overlapped panel's GA_ROOTOWNER can be the panel itself, despite its
       GW_OWNER being the game. Match the actual owner chain for shortcuts. */
    if ((owner && (foreground==owner || IsChild(owner,foreground))) ||
        (panel && (foreground==panel || IsChild(panel,foreground)))) return 1;
    HWND root=GetAncestor(foreground,GA_ROOT);
    for (int i=0;root && i<32;i++) {
        if ((owner && root==owner) || (panel && root==panel)) return 1;
        root=GetWindow(root,GW_OWNER);
    }
    return 0;
}
int s14_officer_ui_sync_enabled(S14OfficerUI *ui,int enabled) {
    if (enabled || !ui->window || !IsWindowVisible(ui->window)) return 0;
    /* Turning off the feature is an explicit user action, not a focus test. */
    ShowWindow(ui->window,SW_HIDE);return 1;
}
static LRESULT CALLBACK officer_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    S14OfficerUI *ui=(S14OfficerUI*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if (message==WM_NCCREATE) { ui=((CREATESTRUCTW*)lparam)->lpCreateParams;ui->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui); }
    if (!ui) return DefWindowProcW(window,message,wparam,lparam);
    if (message==WM_ERASEBKGND) return 1;
    if (message==WM_SIZE) { if (ui->query) layout(ui);return 0; }
    if (message==WM_GETMINMAXINFO) { MINMAXINFO *m=(MINMAXINFO*)lparam;m->ptMinTrackSize=(POINT){px(ui,1020),px(ui,700)};return 0; }
    if (message==WM_DRAWITEM && (wparam==506 || wparam==509)) {
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT*)lparam;int pressed=(d->itemState&ODS_SELECTED)!=0;
        card(d->hDC,ui,d->rcItem,pressed?S14_ACCENT_SOFT:S14_CARD);
        wchar_t label[32];GetWindowTextW(d->hwndItem,label,32);
        paint_text(d->hDC,ui->font,label,d->rcItem,wparam==509?S14_ACCENT:S14_INK,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if (d->itemState&ODS_FOCUS) { RECT focus=d->rcItem;InflateRect(&focus,-4,-4);DrawFocusRect(d->hDC,&focus); }
        return TRUE;
    }
    if (message==WM_CLOSE) { hide(ui);return 0; }
    if (message==WM_KEYDOWN && wparam==VK_ESCAPE) { hide(ui);return 0; }
    if (message==WM_COMMAND) {
        int id=LOWORD(wparam),notification=HIWORD(wparam);
        if (id==IDCANCEL) { hide(ui);return 0; }
        if (id==509 && notification==BN_CLICKED) { hide(ui);return 0; }
        if ((id==501 && notification==EN_CHANGE) || ((id==502 || id==503 || id==504) && notification==CBN_SELCHANGE)) rebuild(ui);
        if (id==505 && notification==CBN_SELCHANGE) { ui->filter.sort=(int)SendMessageW(ui->sort,CB_GETCURSEL,0,0);rebuild(ui); }
        if (id==506 && notification==BN_CLICKED) { ui->filter.descending=!ui->filter.descending;SetWindowTextW(ui->direction,ui->filter.descending?L"降序 ↓":L"升序 ↑");rebuild(ui); }
        return 0;
    }
    if (message==WM_NOTIFY) {
        NMHDR *header=(NMHDR*)lparam;
        if (header->hwndFrom==ui->list && header->code==LVN_GETDISPINFOW) {
            NMLVDISPINFOW *info=(NMLVDISPINFOW*)lparam;
            if ((info->item.mask&LVIF_TEXT) && info->item.iItem>=0 && info->item.iItem<ui->visible_count)
                cell_text(&ui->snapshot->rows[ui->indices[info->item.iItem]],info->item.iSubItem,info->item.pszText,(size_t)info->item.cchTextMax);
            return 0;
        }
        if (header->hwndFrom==ui->list && header->code==LVN_COLUMNCLICK) {
            int column=((NMLISTVIEW*)lparam)->iSubItem;
            if (ui->filter.sort==column) ui->filter.descending=!ui->filter.descending;else { ui->filter.sort=column;ui->filter.descending=column>=4 && column!=10; }
            SendMessageW(ui->sort,CB_SETCURSEL,column,0);SetWindowTextW(ui->direction,ui->filter.descending?L"降序 ↓":L"升序 ↑");rebuild(ui);return 0;
        }
        if (header->hwndFrom==ui->list && header->code==LVN_ITEMCHANGED) {
            NMLISTVIEW *change=(NMLISTVIEW*)lparam;if (change->uNewState&LVIS_SELECTED) detail(ui,change->iItem);return 0;
        }
        if (header->hwndFrom==ui->list && header->code==NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW *draw=(NMLVCUSTOMDRAW*)lparam;
            if (draw->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) {
                int selected=(ListView_GetItemState(ui->list,(int)draw->nmcd.dwItemSpec,LVIS_SELECTED)&LVIS_SELECTED)!=0;
                draw->clrText=S14_INK;draw->clrTextBk=selected?S14_ACCENT_SOFT:(draw->nmcd.dwItemSpec%2)?S14_PAPER:S14_CARD;
                /* Native themed selection otherwise repaints the requested
                   color. The owner-data selection itself remains unchanged. */
                draw->nmcd.uItemState&=~CDIS_SELECTED;return CDRF_NEWFONT;
            }
        }
    }
    if (message==WM_CTLCOLOREDIT || message==WM_CTLCOLORSTATIC || message==WM_CTLCOLORLISTBOX) {
        SetTextColor((HDC)wparam,S14_INK);SetBkColor((HDC)wparam,S14_CARD);return (LRESULT)ui->brush;
    }
    if (message==WM_PAINT || message==WM_PRINTCLIENT) {
        PAINTSTRUCT p;HDC dc=message==WM_PAINT?BeginPaint(window,&p):(HDC)wparam;RECT r;GetClientRect(window,&r);HBRUSH bg=CreateSolidBrush(S14_PAPER);FillRect(dc,&r,bg);DeleteObject(bg);
        paint_text(dc,ui->title_font,L"武将一览",(RECT){px(ui,32),px(ui,16),r.right-px(ui,150),px(ui,60)},S14_INK,DT_SINGLELINE|DT_VCENTER);
        paint_text(dc,ui->small_font,L"天下归心  /  实时情报    ·    中文、拼音与首字母搜索    ·    手动关闭 / Esc",(RECT){px(ui,34),px(ui,66),r.right-px(ui,32),px(ui,90)},S14_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
        card(dc,ui,(RECT){px(ui,20),px(ui,102),r.right-px(ui,20),px(ui,200)},S14_CARD);
        card(dc,ui,(RECT){px(ui,20),px(ui,212),r.right-px(ui,20),r.bottom-px(ui,218)},S14_CARD);
        card(dc,ui,(RECT){px(ui,20),r.bottom-px(ui,206),r.right-px(ui,20),r.bottom-px(ui,16)},S14_CARD);
        paint_text(dc,ui->small_font,L"排序",(RECT){px(ui,36),px(ui,163),px(ui,72),px(ui,189)},S14_MUTED,DT_SINGLELINE);
        paint_text(dc,ui->font,L"武将档案",(RECT){px(ui,36),r.bottom-px(ui,200),r.right-px(ui,36),r.bottom-px(ui,182)},S14_ACCENT,DT_SINGLELINE);
        wchar_t count[200];swprintf(count,200,L"显示 %d / %d 位    %ls",ui->visible_count,ui->snapshot->count,ui->notice);
        paint_text(dc,ui->small_font,count,(RECT){px(ui,322),px(ui,163),r.right-px(ui,36),px(ui,189)},ui->last_capture_ok?S14_MUTED:S14_ERROR,DT_SINGLELINE|DT_END_ELLIPSIS);
        if (message==WM_PAINT) EndPaint(window,&p);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
static LRESULT CALLBACK control_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR id,DWORD_PTR data) {
    (void)id;S14OfficerUI *ui=(S14OfficerUI*)data;
    if (window==ui->list && message==WM_NOTIFY && ((NMHDR*)lparam)->code==NM_CUSTOMDRAW) {
        NMCUSTOMDRAW *draw=(NMCUSTOMDRAW*)lparam;
        if (draw->dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (draw->dwDrawStage==CDDS_ITEMPREPAINT && draw->dwItemSpec<S14_SORT_COUNT) {
            RECT box=draw->rc;HBRUSH brush=CreateSolidBrush(S14_PAPER);FillRect(draw->hdc,&box,brush);DeleteObject(brush);
            InflateRect(&box,-px(ui,8),0);int column=(int)draw->dwItemSpec;
            paint_text(draw->hdc,ui->small_font,columns[column],box,ui->filter.sort==column?S14_ACCENT:S14_MUTED,DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            return CDRF_SKIPDEFAULT;
        }
    }
    if (message==WM_KEYDOWN && wparam==VK_ESCAPE) { hide(ui);return 0; }
    if (message==WM_NCDESTROY) RemoveWindowSubclass(window,control_proc,1);
    return DefSubclassProc(window,message,wparam,lparam);
}
static HWND combo(S14OfficerUI *ui,int id,const wchar_t *const *items,int count,int selected) {
    HWND window=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,ui->window,(HMENU)(intptr_t)id,ui->instance,NULL);
    for (int i=0;i<count;i++) SendMessageW(window,CB_ADDSTRING,0,(LPARAM)items[i]);SendMessageW(window,CB_SETCURSEL,selected,0);return window;
}
int s14_officer_ui_create(S14OfficerUI *ui,HINSTANCE instance,HWND owner,uintptr_t base) {
    ui->instance=instance;ui->owner=owner;ui->base=base;ui->scale=96;ui->filter.sort=S14_SORT_TOTAL;ui->filter.descending=1;
    typedef UINT (WINAPI *Dpi)(HWND);Dpi dpi=(Dpi)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");if (owner && dpi) ui->scale=(int)dpi(owner);
    if (ui->scale<96 || ui->scale>288) ui->scale=96;
    HMONITOR monitor=MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST);MONITORINFO monitor_info={sizeof(monitor_info)};
    if (GetMonitorInfoW(monitor,&monitor_info)) {
        int fit_w=MulDiv(monitor_info.rcWork.right-monitor_info.rcWork.left,96,1020);
        int fit_h=MulDiv(monitor_info.rcWork.bottom-monitor_info.rcWork.top,96,640);
        if (ui->scale>fit_w) ui->scale=fit_w;if (ui->scale>fit_h) ui->scale=fit_h;
        if (ui->scale<96) ui->scale=96;
    }
    ACTCTXW activation={0};activation.cbSize=sizeof(activation);activation.dwFlags=ACTCTX_FLAG_HMODULE_VALID|ACTCTX_FLAG_RESOURCE_NAME_VALID;
    activation.hModule=instance;activation.lpResourceName=MAKEINTRESOURCEW(2);ui->activation=CreateActCtxW(&activation);
    ULONG_PTR activation_cookie=0;int activated=ui->activation!=INVALID_HANDLE_VALUE && ActivateActCtx(ui->activation,&activation_cookie);
    ui->snapshot=calloc(1,sizeof(S14OfficerSnapshot));ui->candidate=calloc(1,sizeof(S14OfficerSnapshot));
    if (!ui->snapshot || !ui->candidate) { if (activated) DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0; }
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    ui->font=CreateFontW(-px(ui,15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->small_font=CreateFontW(-px(ui,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->title_font=CreateFontW(-px(ui,34),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"SimSun");ui->brush=CreateSolidBrush(S14_CARD);
    WNDCLASSEXW cls={0};cls.cbSize=sizeof(cls);cls.hInstance=instance;cls.lpfnWndProc=officer_proc;cls.lpszClassName=officer_class;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    if ((!RegisterClassExW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) || !ui->font || !ui->small_font || !ui->title_font || !ui->brush) { if (activated) DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0; }
    ui->window=CreateWindowExW(WS_EX_TOOLWINDOW,officer_class,L"天下归心 · 武将一览",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,px(ui,1500),px(ui,880),owner,NULL,instance,ui);
    if (!ui->window) { if (activated) DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0; }
    ui->query=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,ui->window,(HMENU)501,instance,NULL);
    SendMessageW(ui->query,EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索姓名、字、个性  ·  曹操 / caocao / cc  ·  空格组合");SendMessageW(ui->query,EM_SETLIMITTEXT,127,0);
    const wchar_t *scopes[]={L"全部势力",L"自势力"},*places[]={L"全部位置",L"城市 / 关隘",L"出征部队",L"执行任务"},*histories[]={L"现存武将",L"全部记录 / 历史"};
    ui->scope=combo(ui,502,scopes,2,0);ui->place=combo(ui,503,places,4,0);ui->history=combo(ui,504,histories,2,0);ui->sort=combo(ui,505,columns,S14_SORT_COUNT,S14_SORT_TOTAL);
    ui->direction=CreateWindowExW(0,L"BUTTON",L"降序 ↓",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,ui->window,(HMENU)506,instance,NULL);
    ui->list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_OWNERDATA|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,ui->window,(HMENU)507,instance,NULL);
    ListView_SetExtendedListViewStyle(ui->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ListView_SetBkColor(ui->list,S14_CARD);ListView_SetTextBkColor(ui->list,S14_CARD);ListView_SetTextColor(ui->list,S14_INK);
    for (int i=0;i<S14_SORT_COUNT;i++) { LVCOLUMNW c={0};c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT;c.pszText=(wchar_t*)columns[i];c.cx=px(ui,column_width[i]);c.fmt=i>=4 && i!=10?LVCFMT_RIGHT:LVCFMT_LEFT;ListView_InsertColumn(ui->list,i,&c); }
    int order[]={0,1,17,13,18,19,20,2,3,4,5,6,7,8,9,14,15,16,10,11,12};ListView_SetColumnOrderArray(ui->list,S14_SORT_COUNT,order);
    ui->details=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,0,0,0,0,ui->window,(HMENU)508,instance,NULL);
    ui->close_button=CreateWindowExW(0,L"BUTTON",L"关闭",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,ui->window,(HMENU)509,instance,NULL);
    HWND controls[]={ui->query,ui->scope,ui->place,ui->history,ui->sort,ui->direction,ui->list,ui->details,ui->close_button};
    for (int i=0;i<9;i++) { if (!controls[i]) { if (activated) DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0; }SendMessageW(controls[i],WM_SETFONT,(WPARAM)ui->font,TRUE);SetWindowSubclass(controls[i],control_proc,1,(DWORD_PTR)ui); }
    if (activated) DeactivateActCtx(0,activation_cookie);
    wcscpy(ui->notice,L"载入存档后可查看武将。");layout(ui);detail(ui,-1);return 1;
}
void s14_officer_ui_toggle(S14OfficerUI *ui) {
    if (!ui->window) return;if (IsWindowVisible(ui->window)) { hide(ui);return; }
    HMONITOR monitor=MonitorFromWindow(ui->owner,MONITOR_DEFAULTTONEAREST);MONITORINFO info={sizeof(info)};GetMonitorInfoW(monitor,&info);
    int w=px(ui,1500),h=px(ui,880),available_w=info.rcWork.right-info.rcWork.left,available_h=info.rcWork.bottom-info.rcWork.top;
    if (w>available_w) w=available_w;if (h>available_h) h=available_h;
    SetWindowPos(ui->window,HWND_TOP,info.rcWork.left+(available_w-w)/2,info.rcWork.top+(available_h-h)/2,w,h,SWP_SHOWWINDOW);
    SetForegroundWindow(ui->window);capture_on_open(ui);SetFocus(ui->query);
}
void s14_officer_ui_destroy(S14OfficerUI *ui) {
    if (ui->window && IsWindow(ui->window)) DestroyWindow(ui->window);
    if (ui->font) DeleteObject(ui->font);if (ui->small_font) DeleteObject(ui->small_font);if (ui->title_font) DeleteObject(ui->title_font);if (ui->brush) DeleteObject(ui->brush);
    free(ui->snapshot);free(ui->candidate);ui->snapshot=ui->candidate=NULL;ui->window=NULL;
    if (ui->activation && ui->activation!=INVALID_HANDLE_VALUE) ReleaseActCtx(ui->activation);ui->activation=NULL;
}
