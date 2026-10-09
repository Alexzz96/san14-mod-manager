#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include "officer_ui.h"
#include "theme.h"
static const wchar_t officer_class[]=L"SAN14ModManager.Officers.v1";
static const wchar_t *columns[]={L"武将姓名",L"效命势力",L"身份",L"驻军所在",L"统率",L"武力",L"智力",L"政治",L"魅力",L"总和",L"个性",L"兵力",L"斩杀敌将",L"破敌次数",L"野心",L"情义",L"忠诚",L"斩敌总数",L"所部覆灭",L"士卒折损",L"击伤敌将",L"单挑次数",L"单挑制胜",L"单挑败北",L"擒获次数",L"被俘次数",L"擒获人数",L"杀损比值"};
static const int column_width[]={110,100,80,200,64,64,64,64,64,70,210,80,100,100,70,70,70,144,118,118,104,100,100,100,100,100,140,80};
static const int pinned_keys[]={S14_SORT_NAME,S14_SORT_FORCE,S14_SORT_LOCATION};
static const int battle_keys[]={S14_SORT_ENEMY_LOSS,S14_SORT_OWN_LOSS,S14_SORT_KDA,S14_SORT_ROUTS,S14_SORT_DEFEATS,S14_SORT_KILLS,S14_SORT_INJURIES,S14_SORT_DUEL_WINS,S14_SORT_DUEL_LOSSES,S14_SORT_CAPTURES,S14_SORT_CAPTURED};
static const int raw_keys[]={S14_SORT_STATUS,S14_SORT_LEADERSHIP,S14_SORT_WAR,S14_SORT_INTELLIGENCE,S14_SORT_POLITICS,S14_SORT_CHARM,S14_SORT_TOTAL,S14_SORT_AMBITION,S14_SORT_BOND,S14_SORT_LOYALTY,S14_SORT_PERSONALITY,S14_SORT_TROOPS};
static int px(S14OfficerUI *ui,int n){return MulDiv(n,ui->scale,96);}
static int read_memory(void *context,uintptr_t address,void *out,size_t size){SIZE_T got=0;return ReadProcessMemory((HANDLE)context,(const void*)address,out,size,&got) && got==size;}
static void paint_text(HDC dc,HFONT font,const wchar_t *s,RECT box,COLORREF color,UINT flags){
    HGDIOBJ old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s,-1,&box,flags|DT_NOPREFIX);SelectObject(dc,old);
}
static void card(HDC dc,S14OfficerUI *ui,RECT r,COLORREF color){
    HBRUSH brush=CreateSolidBrush(color);HPEN pen=CreatePen(PS_SOLID,1,S14_BORDER);HGDIOBJ ob=SelectObject(dc,brush),op=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,px(ui,14),px(ui,14));SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(brush);DeleteObject(pen);
}
int s14_officer_ui_column(const S14OfficerUI *ui,int pinned,int column){
    if(column<0)return -1;if(pinned)return column<3?pinned_keys[column]:-1;
    return column<ui->dynamic_count?ui->dynamic_keys[column]:-1;
}
static void layout(S14OfficerUI *ui){
    if(ui->layouting || !ui->list || !ui->pinned)return;ui->layouting=1;
    RECT r;GetClientRect(ui->window,&r);int w=r.right,h=r.bottom;
    MoveWindow(ui->close_button,w-px(ui,128),px(ui,24),px(ui,96),px(ui,36),TRUE);
    MoveWindow(ui->query,px(ui,36),px(ui,116),w-px(ui,480),px(ui,32),TRUE);
    MoveWindow(ui->scope,w-px(ui,430),px(ui,116),px(ui,110),px(ui,200),TRUE);
    MoveWindow(ui->place,w-px(ui,308),px(ui,116),px(ui,122),px(ui,220),TRUE);
    MoveWindow(ui->history,w-px(ui,174),px(ui,116),px(ui,138),px(ui,200),TRUE);
    MoveWindow(ui->sort,px(ui,76),px(ui,158),px(ui,174),px(ui,470),TRUE);
    MoveWindow(ui->direction,px(ui,262),px(ui,157),px(ui,84),px(ui,32),TRUE);
    MoveWindow(ui->tabs[0],px(ui,30),px(ui,212),px(ui,120),px(ui,34),TRUE);
    MoveWindow(ui->tabs[1],px(ui,160),px(ui,212),px(ui,120),px(ui,34),TRUE);
    MoveWindow(ui->timeline_button,w-px(ui,154),px(ui,212),px(ui,124),px(ui,34),TRUE);
    int all=w-px(ui,60),frozen=all*29/100,height=h-px(ui,282);
    if(frozen>px(ui,350))frozen=px(ui,350);
    MoveWindow(ui->list,px(ui,30)+frozen,px(ui,258),w-px(ui,60)-frozen,height,TRUE);
    int fixed_used=0;for(int i=0;i<3;i++){int cw=i==2?frozen-fixed_used:frozen*(i?25:26)/100;fixed_used+=cw;ListView_SetColumnWidth(ui->pinned,i,cw);}
    int available=all-frozen-GetSystemMetrics(SM_CXVSCROLL)-2,used=0,weight_sum=0;
    for(int i=0;i<ui->dynamic_count;i++)weight_sum+=ui->active_tab && ui->dynamic_keys[i]==S14_SORT_PERSONALITY?30:ui->dynamic_keys[i]==S14_SORT_ENEMY_LOSS || ui->dynamic_keys[i]==S14_SORT_OWN_LOSS?12:10;
    for(int i=0;i<ui->dynamic_count;i++){int key=ui->dynamic_keys[i],weight=ui->active_tab && key==S14_SORT_PERSONALITY?30:key==S14_SORT_ENEMY_LOSS || key==S14_SORT_OWN_LOSS?12:10;int cw=i+1==ui->dynamic_count?available-used:available*weight/weight_sum;used+=cw;ListView_SetColumnWidth(ui->list,i,cw);}
    HFONT header_font=w<px(ui,1200)?ui->dense_font:ui->small_font;SendMessageW(ListView_GetHeader(ui->list),WM_SETFONT,(WPARAM)header_font,TRUE);SendMessageW(ListView_GetHeader(ui->pinned),WM_SETFONT,(WPARAM)header_font,TRUE);
    int bar=(GetWindowLongPtrW(ui->list,GWL_STYLE)&WS_HSCROLL)?GetSystemMetrics(SM_CYHSCROLL):0;
    /* Keep the native vertical range/page size; clipping its scrollbar avoids
       stale ListView page metrics caused by repeatedly removing scroll styles. */
    MoveWindow(ui->pinned,px(ui,30),px(ui,258),frozen+GetSystemMetrics(SM_CXVSCROLL),height-bar,TRUE);
    HRGN clip=CreateRectRgn(0,0,frozen,height-bar);if(!SetWindowRgn(ui->pinned,clip,TRUE))DeleteObject(clip);ui->layouting=0;
}
static void sync_scroll(S14OfficerUI *ui,HWND from){
    if(ui->syncing || ui->layouting || ui->visible_count<=0)return;
    ui->syncing=1;HWND to=from==ui->pinned?ui->list:ui->pinned;
    int a=ListView_GetTopIndex(from),b=ListView_GetTopIndex(to);RECT ar,br;
    if(a>=0 && b>=0 && ListView_GetItemRect(from,a,&ar,LVIR_BOUNDS) && ListView_GetItemRect(to,b,&br,LVIR_BOUNDS)){
        int delta=(a-b)*(br.bottom-br.top)+br.top-ar.top;if(delta)ListView_Scroll(to,0,delta);
    }
    ui->syncing=0;
}
static void select_row(S14OfficerUI *ui,HWND from,int index){
    if(ui->syncing || index<0 || index>=ui->visible_count)return;
    ui->selected_id=ui->snapshot->rows[ui->indices[index]].id;ui->syncing=1;
    EnableWindow(ui->timeline_button,TRUE);
    HWND to=from==ui->pinned?ui->list:ui->pinned;
    ListView_SetItemState(to,index,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ui->syncing=0;
}
static S14OfficerUI *sorting_ui;
static int compare_indices(const void *a,const void *b){S14OfficerUI *ui=sorting_ui;return s14_officer_compare(&ui->snapshot->rows[*(const int*)a],&ui->snapshot->rows[*(const int*)b],ui->filter.sort,ui->filter.descending);}
static void rebuild(S14OfficerUI *ui){
    GetWindowTextW(ui->query,ui->filter.query,128);ui->filter.own_force=(int)SendMessageW(ui->scope,CB_GETCURSEL,0,0)==1;
    int place=(int)SendMessageW(ui->place,CB_GETCURSEL,0,0);ui->filter.place=place>0?place:0;
    ui->filter.include_history=(int)SendMessageW(ui->history,CB_GETCURSEL,0,0)==1;ui->visible_count=0;
    for(int i=0;i<ui->snapshot->count;i++)if(s14_officer_matches(&ui->snapshot->rows[i],ui->snapshot->player_force,&ui->filter))ui->indices[ui->visible_count++]=i;
    sorting_ui=ui;qsort(ui->indices,(size_t)ui->visible_count,sizeof(int),compare_indices);sorting_ui=NULL;
    ui->syncing=1;
    ListView_SetItemState(ui->list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);ListView_SetItemState(ui->pinned,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_SetItemCountEx(ui->list,ui->visible_count,LVSICF_NOSCROLL);ListView_SetItemCountEx(ui->pinned,ui->visible_count,LVSICF_NOSCROLL);
    int selected=-1;for(int i=0;i<ui->visible_count;i++)if(ui->snapshot->rows[ui->indices[i]].id==ui->selected_id)selected=i;
    if(selected>=0){ListView_SetItemState(ui->list,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_SetItemState(ui->pinned,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(ui->list,selected,FALSE);}
    ui->syncing=0;layout(ui);sync_scroll(ui,ui->list);
    EnableWindow(ui->timeline_button,selected>=0);
    InvalidateRect(ui->list,NULL,FALSE);InvalidateRect(ui->pinned,NULL,FALSE);InvalidateRect(ui->window,NULL,FALSE);
}
static void update_sort_control(S14OfficerUI *ui){
    int selected=-1;for(int i=0;i<ui->sort_count;i++)if(ui->sort_keys[i]==ui->filter.sort)selected=i;
    SendMessageW(ui->sort,CB_SETCURSEL,selected,0);SetWindowTextW(ui->direction,ui->filter.descending?L"降序 ↓":L"升序 ↑");
}
void s14_officer_ui_set_tab(S14OfficerUI *ui,int tab){
    if(!ui->list || tab<0 || tab>1 || (ui->active_tab==tab && ui->dynamic_count))return;
    if(ui->dynamic_count){ui->remembered_sort[ui->active_tab]=ui->filter.sort;ui->remembered_desc[ui->active_tab]=ui->filter.descending;}
    HWND drops[]={ui->scope,ui->place,ui->history,ui->sort};for(int i=0;i<4;i++)SendMessageW(drops[i],CB_SHOWDROPDOWN,FALSE,0);
    ui->active_tab=tab;ui->dynamic_count=tab?12:11;const int *keys=tab?raw_keys:battle_keys;
    memcpy(ui->dynamic_keys,keys,(size_t)ui->dynamic_count*sizeof(int));ui->filter.sort=ui->remembered_sort[tab];ui->filter.descending=ui->remembered_desc[tab];
    SendMessageW(ui->list,WM_SETREDRAW,FALSE,0);ui->syncing=1;
    while(Header_GetItemCount(ListView_GetHeader(ui->list))>0)ListView_DeleteColumn(ui->list,0);
    for(int i=0;i<ui->dynamic_count;i++){int key=keys[i];LVCOLUMNW c={0};c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT;c.pszText=(wchar_t*)columns[key];c.cx=px(ui,column_width[key]);c.fmt=key!=S14_SORT_STATUS && key!=S14_SORT_PERSONALITY?LVCFMT_RIGHT:LVCFMT_LEFT;ListView_InsertColumn(ui->list,i,&c);}
    SendMessageW(ui->sort,CB_RESETCONTENT,0,0);ui->sort_count=0;
    for(int i=0;i<3;i++)ui->sort_keys[ui->sort_count++]=pinned_keys[i];for(int i=0;i<ui->dynamic_count;i++)ui->sort_keys[ui->sort_count++]=keys[i];
    for(int i=0;i<ui->sort_count;i++)SendMessageW(ui->sort,CB_ADDSTRING,0,(LPARAM)columns[ui->sort_keys[i]]);
    ui->syncing=0;update_sort_control(ui);rebuild(ui);SendMessageW(ui->list,WM_SETREDRAW,TRUE,0);
    InvalidateRect(ui->list,NULL,TRUE);InvalidateRect(ui->tabs[0],NULL,TRUE);InvalidateRect(ui->tabs[1],NULL,TRUE);
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
    if(!s14_special_snapshot(ui->candidate->world,ui->special))memset(ui->special,0,sizeof(*ui->special));
    for(int i=0;i<ui->candidate->count;i++){int id=ui->candidate->rows[i].id;
        memset(&ui->candidate->rows[i].special,0,sizeof(S14SpecialTotals));
        if(ui->special->version==1 && id>0 && id<S14_SPECIAL_OFFICERS)ui->candidate->rows[i].special=ui->special->rows[id];
    }
    S14OfficerSnapshot *old=ui->snapshot;ui->snapshot=ui->candidate;ui->candidate=old;
    if(!s14_timeline_snapshot(ui->snapshot->world,ui->timeline.snapshot))memset(ui->timeline.snapshot,0,sizeof(*ui->timeline.snapshot));
    ui->last_capture_ok=1;ui->captured_tick=GetTickCount64();
    QueryPerformanceCounter(&finish);double elapsed=(double)(finish.QuadPart-start.QuadPart)*1000.0/(double)frequency.QuadPart;
    swprintf(ui->notice,160,L"%d 位武将 · %.1f ms · %ls%ls · 按 F 重开刷新",ui->snapshot->count,elapsed,
        ui->snapshot->battle_connected?L"战绩从接入时起累计，随游戏保存":L"战绩未接入",ui->snapshot->battle_incomplete?L"（有采集缺口）":L"");
    rebuild(ui);
}
static void cell_text(const S14Officer *p,int column,wchar_t *out,size_t size){
    const wchar_t *s=NULL;
    if(column==S14_SORT_NAME)s=p->name;else if(column==S14_SORT_FORCE)s=p->force_name;else if(column==S14_SORT_STATUS)s=p->status_name;
    else if(column==S14_SORT_LOCATION)s=p->location;else if(column==S14_SORT_PERSONALITY)s=p->personality_text;
    if(s){wcsncpy(out,s,size-1);out[size-1]=0;return;}
    if(column==S14_SORT_KDA)s14_officer_kda_text(p,out,size);
    else if(s14_officer_count_key(column))swprintf(out,size,L"%llu",(unsigned long long)s14_officer_count_value(p,column));
    else if(column>=S14_SORT_LEADERSHIP && column<=S14_SORT_CHARM)swprintf(out,size,L"%d",p->ability[column-S14_SORT_LEADERSHIP]);
    else if(column==S14_SORT_TOTAL)swprintf(out,size,L"%d",p->total);
    else if(column==S14_SORT_TROOPS){if(p->troops>=0)swprintf(out,size,L"%d",p->troops);else wcscpy(out,L"—");}
    else if(column>=S14_SORT_AMBITION && column<=S14_SORT_LOYALTY){int v=column==S14_SORT_AMBITION?p->ambition:column==S14_SORT_BOND?p->bond:p->loyalty;if(v>=0)swprintf(out,size,L"%d",v);else wcscpy(out,L"—");}
    else wcscpy(out,L"—");
}
static void hide(S14OfficerUI *ui) {s14_history_hide(&ui->timeline);ShowWindow(ui->window,SW_HIDE);if (ui->owner) SetForegroundWindow(ui->owner); }
static void show_timeline(S14OfficerUI *ui){for(int i=0;i<ui->visible_count;i++){S14Officer *p=&ui->snapshot->rows[ui->indices[i]];if(p->id==ui->selected_id){s14_history_show(&ui->timeline,p);return;}}}
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
    s14_history_hide(&ui->timeline);ShowWindow(ui->window,SW_HIDE);return 1;
}
static int tab_key(S14OfficerUI *ui,UINT message,WPARAM key,LPARAM state){
    if(message==WM_CHAR && key==L'\t')return 1;
    if(message==WM_KEYDOWN && key==VK_TAB){if(!(state&0x40000000))s14_officer_ui_set_tab(ui,!ui->active_tab);return 1;}return 0;
}
static LRESULT CALLBACK officer_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
    S14OfficerUI *ui=(S14OfficerUI*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_NCCREATE){ui=((CREATESTRUCTW*)lparam)->lpCreateParams;ui->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui);}
    if(!ui)return DefWindowProcW(window,message,wparam,lparam);
    if(message==WM_GETDLGCODE)return DefWindowProcW(window,message,wparam,lparam)|DLGC_WANTTAB;
    if(tab_key(ui,message,wparam,lparam))return 0;
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_TIMER && wparam==538){if(ui->menu_active && ui->pump_events)ui->pump_events();return 0;}
    if(message==WM_SIZE){layout(ui);sync_scroll(ui,ui->list);return 0;}
    if(message==WM_GETMINMAXINFO){((MINMAXINFO*)lparam)->ptMinTrackSize=(POINT){px(ui,1020),px(ui,640)};return 0;}
    if(message==WM_DRAWITEM && (wparam==506 || wparam==509 || wparam==510 || wparam==511 || wparam==513)){
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT*)lparam;int active=wparam>=510 && ui->active_tab==(int)wparam-510;
        card(d->hDC,ui,d->rcItem,(d->itemState&ODS_SELECTED)||active?S14_ACCENT_SOFT:S14_CARD);
        wchar_t label[32];GetWindowTextW(d->hwndItem,label,32);paint_text(d->hDC,ui->font,label,d->rcItem,active || wparam==509?S14_ACCENT:S14_INK,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if(d->itemState&ODS_FOCUS){RECT f=d->rcItem;InflateRect(&f,-4,-4);DrawFocusRect(d->hDC,&f);}return TRUE;
    }
    if(message==WM_CLOSE || (message==WM_KEYDOWN && wparam==VK_ESCAPE)){hide(ui);return 0;}
    if(message==WM_COMMAND){
        int id=LOWORD(wparam),n=HIWORD(wparam);
        if(id==IDCANCEL || (id==509 && n==BN_CLICKED)){hide(ui);return 0;}
        if((id==510 || id==511) && n==BN_CLICKED){s14_officer_ui_set_tab(ui,id-510);return 0;}
        if(id==513 && n==BN_CLICKED){show_timeline(ui);return 0;}
        if((id==501 && n==EN_CHANGE) || ((id==502 || id==503 || id==504) && n==CBN_SELCHANGE))rebuild(ui);
        if(id==505 && n==CBN_SELCHANGE){int i=(int)SendMessageW(ui->sort,CB_GETCURSEL,0,0);if(i>=0 && i<ui->sort_count){ui->filter.sort=ui->sort_keys[i];rebuild(ui);}}
        if(id==506 && n==BN_CLICKED){ui->filter.descending=!ui->filter.descending;update_sort_control(ui);rebuild(ui);}return 0;
    }
    if(message==WM_NOTIFY){
        NMHDR *hdr=(NMHDR*)lparam;int fixed=hdr->hwndFrom==ui->pinned,table=fixed || hdr->hwndFrom==ui->list;
        if(table && hdr->code==LVN_GETDISPINFOW){NMLVDISPINFOW *info=(NMLVDISPINFOW*)lparam;
            if((info->item.mask&LVIF_TEXT) && info->item.iItem>=0 && info->item.iItem<ui->visible_count)cell_text(&ui->snapshot->rows[ui->indices[info->item.iItem]],s14_officer_ui_column(ui,fixed,info->item.iSubItem),info->item.pszText,(size_t)info->item.cchTextMax);return 0;
        }
        if(table && hdr->code==LVN_COLUMNCLICK){int key=s14_officer_ui_column(ui,fixed,((NMLISTVIEW*)lparam)->iSubItem);if(key<0)return 0;
            if(ui->filter.sort==key)ui->filter.descending=!ui->filter.descending;else{ui->filter.sort=key;ui->filter.descending=key>=4 && key!=S14_SORT_PERSONALITY;}update_sort_control(ui);rebuild(ui);return 0;
        }
        if(table && hdr->code==LVN_ITEMCHANGED){NMLISTVIEW *v=(NMLISTVIEW*)lparam;if(v->uNewState&LVIS_SELECTED)select_row(ui,hdr->hwndFrom,v->iItem);return 0;}
        if(table && (hdr->code==NM_DBLCLK || hdr->code==NM_RCLICK || (fixed && hdr->code==NM_CLICK && ((NMITEMACTIVATE*)lparam)->iSubItem==0))){
            NMITEMACTIVATE *v=(NMITEMACTIVATE*)lparam;if(v->iItem<0 || v->iItem>=ui->visible_count)return 0;select_row(ui,hdr->hwndFrom,v->iItem);ListView_SetItemState(hdr->hwndFrom,v->iItem,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);EnableWindow(ui->timeline_button,TRUE);
            if(hdr->code==NM_DBLCLK){show_timeline(ui);return 0;}
            HMENU menu=CreatePopupMenu(),child=CreatePopupMenu();AppendMenuW(child,MF_STRING,530,L"查看时间线");AppendMenuW(menu,MF_POPUP,(UINT_PTR)child,L"战斗详情");POINT at;GetCursorPos(&at);
            ui->menu_active=1;SetTimer(window,538,50,NULL);
            UINT command=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,at.x,at.y,window,NULL);
            KillTimer(window,538);ui->menu_active=0;DestroyMenu(menu);if(command==530)show_timeline(ui);return 0;
        }
        if(table && hdr->code==NM_CUSTOMDRAW){NMLVCUSTOMDRAW *draw=(NMLVCUSTOMDRAW*)lparam;
            if(draw->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
            if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){int selected=(ListView_GetItemState(hdr->hwndFrom,(int)draw->nmcd.dwItemSpec,LVIS_SELECTED)&LVIS_SELECTED)!=0;
                draw->clrText=S14_INK;draw->clrTextBk=selected?S14_ACCENT_SOFT:draw->nmcd.dwItemSpec%2?S14_PAPER:S14_CARD;draw->nmcd.uItemState&=~CDIS_SELECTED;return CDRF_NEWFONT;}
        }
    }
    if(message==WM_CTLCOLOREDIT || message==WM_CTLCOLORSTATIC || message==WM_CTLCOLORLISTBOX){SetTextColor((HDC)wparam,S14_INK);SetBkColor((HDC)wparam,S14_CARD);return (LRESULT)ui->brush;}
    if(message==WM_PAINT || message==WM_PRINTCLIENT){PAINTSTRUCT p;HDC dc=message==WM_PAINT?BeginPaint(window,&p):(HDC)wparam;RECT r;GetClientRect(window,&r);HBRUSH bg=CreateSolidBrush(S14_PAPER);FillRect(dc,&r,bg);DeleteObject(bg);
        paint_text(dc,ui->title_font,L"武将一览",(RECT){px(ui,32),px(ui,16),r.right-px(ui,150),px(ui,60)},S14_INK,DT_SINGLELINE|DT_VCENTER);
        paint_text(dc,ui->small_font,L"天下归心  /  中文、拼音与首字母搜索    ·    Tab 切换数据    ·    手动关闭 / Esc",(RECT){px(ui,34),px(ui,66),r.right-px(ui,32),px(ui,90)},S14_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
        card(dc,ui,(RECT){px(ui,20),px(ui,102),r.right-px(ui,20),px(ui,200)},S14_CARD);
        card(dc,ui,(RECT){px(ui,20),px(ui,248),r.right-px(ui,20),r.bottom-px(ui,16)},S14_CARD);
        paint_text(dc,ui->small_font,L"排序",(RECT){px(ui,36),px(ui,163),px(ui,72),px(ui,189)},S14_MUTED,DT_SINGLELINE);
        paint_text(dc,ui->small_font,ui->active_tab?L"基础能力、身份、个性与内心属性":L"斩敌含伤兵 · 杀损比值 = 斩敌 ÷ 折损 · 未记录计数为 0",(RECT){px(ui,304),px(ui,216),r.right-px(ui,166),px(ui,240)},S14_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
        wchar_t count[200];swprintf(count,200,L"显示 %d / %d 位    %ls",ui->visible_count,ui->snapshot->count,ui->notice);
        paint_text(dc,ui->small_font,count,(RECT){px(ui,362),px(ui,163),r.right-px(ui,36),px(ui,189)},ui->last_capture_ok?S14_MUTED:S14_ERROR,DT_SINGLELINE|DT_END_ELLIPSIS);
        if(message==WM_PAINT)EndPaint(window,&p);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
static LRESULT CALLBACK control_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR id,DWORD_PTR data){
    (void)id;S14OfficerUI *ui=(S14OfficerUI*)data;int table=window==ui->list || window==ui->pinned;
    if(message==WM_GETDLGCODE)return DefSubclassProc(window,message,wparam,lparam)|DLGC_WANTTAB;
    if(tab_key(ui,message,wparam,lparam))return 0;
    if(table && message==WM_NOTIFY && ((NMHDR*)lparam)->code==NM_CUSTOMDRAW){NMCUSTOMDRAW *draw=(NMCUSTOMDRAW*)lparam;
        if(draw->dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
        if(draw->dwDrawStage==CDDS_ITEMPREPAINT){int key=s14_officer_ui_column(ui,window==ui->pinned,(int)draw->dwItemSpec);if(key<0)return CDRF_DODEFAULT;
            RECT box=draw->rc;HBRUSH brush=CreateSolidBrush(S14_PAPER);FillRect(draw->hdc,&box,brush);DeleteObject(brush);InflateRect(&box,-px(ui,3),0);RECT client;GetClientRect(ui->window,&client);
            paint_text(draw->hdc,client.right<px(ui,1200)?ui->dense_font:ui->small_font,columns[key],box,ui->filter.sort==key?S14_ACCENT:S14_MUTED,DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);return CDRF_SKIPDEFAULT;}
    }
    if(message==WM_KEYDOWN && wparam==VK_ESCAPE){hide(ui);return 0;}
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,control_proc,1);
    LRESULT result=DefSubclassProc(window,message,wparam,lparam);
    if(table && window==ui->list && message==WM_NOTIFY){int code=((NMHDR*)lparam)->code;
        if(code==HDN_ITEMCHANGEDW || code==HDN_ENDTRACKW){layout(ui);sync_scroll(ui,window);}
    }
    if(table && (message==WM_VSCROLL || message==WM_MOUSEWHEEL || message==WM_KEYDOWN || message==LVM_ENSUREVISIBLE || message==LVM_SCROLL || message==LVM_SETCOLUMNWIDTH)){
        if(message==LVM_SETCOLUMNWIDTH && window==ui->list)layout(ui);sync_scroll(ui,window);
    }
    return result;
}
static HWND combo(S14OfficerUI *ui,int id,const wchar_t *const *items,int count,int selected) {
    HWND window=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,ui->window,(HMENU)(intptr_t)id,ui->instance,NULL);
    for (int i=0;i<count;i++) SendMessageW(window,CB_ADDSTRING,0,(LPARAM)items[i]);SendMessageW(window,CB_SETCURSEL,selected,0);return window;
}
int s14_officer_ui_create(S14OfficerUI *ui,HINSTANCE instance,HWND owner,uintptr_t base) {
    ui->instance=instance;ui->owner=owner;ui->base=base;ui->scale=96;ui->filter.sort=S14_SORT_ENEMY_LOSS;ui->filter.descending=1;ui->remembered_sort[0]=S14_SORT_ENEMY_LOSS;ui->remembered_sort[1]=S14_SORT_TOTAL;ui->remembered_desc[0]=ui->remembered_desc[1]=1;
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
    ui->special=calloc(1,sizeof(S14SpecialSnapshot));
    if (!ui->snapshot || !ui->candidate || !ui->special) { if (activated) DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0; }
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    ui->font=CreateFontW(-px(ui,15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->small_font=CreateFontW(-px(ui,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->dense_font=CreateFontW(-px(ui,12),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->title_font=CreateFontW(-px(ui,34),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"SimSun");ui->brush=CreateSolidBrush(S14_CARD);
    WNDCLASSEXW cls={0};cls.cbSize=sizeof(cls);cls.hInstance=instance;cls.lpfnWndProc=officer_proc;cls.lpszClassName=officer_class;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    if ((!RegisterClassExW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) || !ui->font || !ui->small_font || !ui->dense_font || !ui->title_font || !ui->brush) { if (activated) DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0; }
    ui->window=CreateWindowExW(WS_EX_TOOLWINDOW,officer_class,L"天下归心 · 武将一览",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,px(ui,1500),px(ui,880),owner,NULL,instance,ui);
    if (!ui->window) { if (activated) DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0; }
    ui->query=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,ui->window,(HMENU)501,instance,NULL);
    SendMessageW(ui->query,EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索姓名、字、个性  ·  曹操 / caocao / cc  ·  空格组合");SendMessageW(ui->query,EM_SETLIMITTEXT,127,0);
    const wchar_t *scopes[]={L"全部势力",L"自势力"},*places[]={L"全部位置",L"城市 / 关隘",L"出征部队",L"执行任务"},*histories[]={L"现存武将",L"全部记录 / 历史"};
    ui->scope=combo(ui,502,scopes,2,0);ui->place=combo(ui,503,places,4,0);ui->history=combo(ui,504,histories,2,0);ui->sort=combo(ui,505,NULL,0,-1);
    ui->direction=CreateWindowExW(0,L"BUTTON",L"降序 ↓",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,ui->window,(HMENU)506,instance,NULL);
    ui->list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_OWNERDATA|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,ui->window,(HMENU)507,instance,NULL);
    ui->pinned=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_OWNERDATA|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,ui->window,(HMENU)512,instance,NULL);
    HWND tables[]={ui->list,ui->pinned};
    for(int i=0;i<2;i++){if(!tables[i]){if(activated)DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0;}ListView_SetExtendedListViewStyle(tables[i],LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ListView_SetBkColor(tables[i],S14_CARD);ListView_SetTextBkColor(tables[i],S14_CARD);ListView_SetTextColor(tables[i],S14_INK);}
    for(int i=0;i<3;i++){int key=pinned_keys[i];LVCOLUMNW c={0};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(wchar_t*)columns[key];c.cx=px(ui,column_width[key]);ListView_InsertColumn(ui->pinned,i,&c);}
    HWND fixed_header=ListView_GetHeader(ui->pinned);SetWindowLongPtrW(fixed_header,GWL_STYLE,GetWindowLongPtrW(fixed_header,GWL_STYLE)|HDS_NOSIZING);
    HWND data_header=ListView_GetHeader(ui->list);SetWindowLongPtrW(data_header,GWL_STYLE,GetWindowLongPtrW(data_header,GWL_STYLE)|HDS_NOSIZING);
    ui->tabs[0]=CreateWindowExW(0,L"BUTTON",L"战斗统计",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,ui->window,(HMENU)510,instance,NULL);
    ui->tabs[1]=CreateWindowExW(0,L"BUTTON",L"原始数据",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,ui->window,(HMENU)511,instance,NULL);
    ui->close_button=CreateWindowExW(0,L"BUTTON",L"关闭",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,ui->window,(HMENU)509,instance,NULL);
    ui->timeline_button=CreateWindowExW(0,L"BUTTON",L"查看时间线",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,ui->window,(HMENU)513,instance,NULL);
    HWND controls[]={ui->query,ui->scope,ui->place,ui->history,ui->sort,ui->direction,ui->list,ui->pinned,ui->close_button,ui->tabs[0],ui->tabs[1],ui->timeline_button};
    for(int i=0;i<12;i++){if(!controls[i]){if(activated)DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0;}SendMessageW(controls[i],WM_SETFONT,(WPARAM)ui->font,TRUE);SetWindowSubclass(controls[i],control_proc,1,(DWORD_PTR)ui);}
    if(!s14_history_create(&ui->timeline,instance,ui->window,ui->scale)){if(activated)DeactivateActCtx(0,activation_cookie);s14_officer_ui_destroy(ui);return 0;}
    for(int i=0;i<2;i++)SetWindowSubclass(ListView_GetHeader(tables[i]),control_proc,1,(DWORD_PTR)ui);
    HWND combos[]={ui->scope,ui->place,ui->history,ui->sort};
    for(int i=0;i<4;i++){COMBOBOXINFO info={0};info.cbSize=sizeof(info);if(GetComboBoxInfo(combos[i],&info)){if(info.hwndList)SetWindowSubclass(info.hwndList,control_proc,1,(DWORD_PTR)ui);if(info.hwndItem && info.hwndItem!=combos[i])SetWindowSubclass(info.hwndItem,control_proc,1,(DWORD_PTR)ui);}}
    if (activated) DeactivateActCtx(0,activation_cookie);
    wcscpy(ui->notice,L"载入存档后可查看武将。");s14_officer_ui_set_tab(ui,0);layout(ui);return 1;
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
    s14_history_destroy(&ui->timeline);
    if (ui->window && IsWindow(ui->window)) DestroyWindow(ui->window);
    if (ui->font) DeleteObject(ui->font);if (ui->small_font) DeleteObject(ui->small_font);if (ui->title_font) DeleteObject(ui->title_font);if (ui->brush) DeleteObject(ui->brush);
    if(ui->dense_font)DeleteObject(ui->dense_font);
    free(ui->snapshot);free(ui->candidate);ui->snapshot=ui->candidate=NULL;ui->window=NULL;
    free(ui->special);ui->special=NULL;
    if (ui->activation && ui->activation!=INVALID_HANDLE_VALUE) ReleaseActCtx(ui->activation);ui->activation=NULL;
}
