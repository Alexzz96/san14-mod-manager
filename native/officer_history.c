#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "officer_history.h"
#include "theme.h"
static const wchar_t history_class[]=L"SAN14ModManager.OfficerHistory.v1";
static const wchar_t *labels[]={L"发生时间",L"战场地区",L"事件类别",L"交战对象",L"结算结果"};
static int px(S14OfficerHistory *h,int n){return MulDiv(n,h->scale,96);}
static void text(HDC dc,HFONT font,const wchar_t *s,RECT r,COLORREF color){HGDIOBJ old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,old);}
static void layout(S14OfficerHistory *h){if(!h->list)return;RECT r;GetClientRect(h->window,&r);MoveWindow(h->close,r.right-px(h,122),px(h,22),px(h,94),px(h,34),TRUE);MoveWindow(h->list,px(h,26),px(h,158),r.right-px(h,52),r.bottom-px(h,186),TRUE);
    int width=r.right-px(h,52)-GetSystemMetrics(SM_CXVSCROLL)-2,sum=0;const int weights[]={16,25,11,12,36};for(int i=0;i<5;i++){int w=i==4?width-sum:width*weights[i]/100;sum+=w;ListView_SetColumnWidth(h->list,i,w);}}
void s14_history_hide(S14OfficerHistory *h){if(h->window)ShowWindow(h->window,SW_HIDE);if(h->owner && IsWindowVisible(h->owner))SetFocus(h->owner);}
static LRESULT CALLBACK control(HWND w,UINT m,WPARAM p,LPARAM l,UINT_PTR id,DWORD_PTR data){(void)id;S14OfficerHistory *h=(S14OfficerHistory*)data;if(m==WM_KEYDOWN && p==VK_ESCAPE){s14_history_hide(h);return 0;}if(m==WM_NCDESTROY)RemoveWindowSubclass(w,control,1);return DefSubclassProc(w,m,p,l);}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM p,LPARAM l){
    S14OfficerHistory *h=(S14OfficerHistory*)GetWindowLongPtrW(w,GWLP_USERDATA);if(m==WM_NCCREATE){h=((CREATESTRUCTW*)l)->lpCreateParams;h->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)h);}if(!h)return DefWindowProcW(w,m,p,l);
    if(m==WM_SIZE){layout(h);return 0;}if(m==WM_GETMINMAXINFO){((MINMAXINFO*)l)->ptMinTrackSize=(POINT){px(h,900),px(h,520)};return 0;}
    if(m==WM_CLOSE || (m==WM_KEYDOWN && p==VK_ESCAPE) || (m==WM_COMMAND && (LOWORD(p)==IDCANCEL || LOWORD(p)==521))){s14_history_hide(h);return 0;}
    if(m==WM_ERASEBKGND)return 1;
    if(m==WM_NOTIFY){NMHDR *hdr=(NMHDR*)l;if(hdr->hwndFrom==h->list && hdr->code==LVN_GETDISPINFOW){NMLVDISPINFOW *v=(NMLVDISPINFOW*)l;
        if((v->item.mask&LVIF_TEXT) && v->item.iItem>=0 && v->item.iItem<h->count){const S14TimelineEvent *e=&h->snapshot->events[h->indices[v->item.iItem]];wchar_t *out=v->item.pszText;size_t size=(size_t)v->item.cchTextMax;
            if(v->item.iSubItem==0)s14_timeline_date(e,out,size);else if(v->item.iSubItem==1)s14_place_text(&e->place,out,size);
            else if(v->item.iSubItem==2)swprintf(out,size,e->kind==S14_TIMELINE_ROUT?L"部队击溃":e->kind==S14_TIMELINE_DUEL?L"武将单挑":e->kind==S14_TIMELINE_CAPTURE?L"武将被俘":L"武将负伤");
            else if(v->item.iSubItem==3)swprintf(out,size,L"%ls",e->actor==h->officer.id?e->target_name:e->actor_name[0]?e->actor_name:L"来源未确认");
            else s14_timeline_describe(e,h->officer.id,out,size);}return 0;}
        if(hdr->hwndFrom==h->list && hdr->code==NM_CUSTOMDRAW){NMLVCUSTOMDRAW *d=(NMLVCUSTOMDRAW*)l;if(d->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;if(d->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){d->clrText=S14_INK;d->clrTextBk=d->nmcd.dwItemSpec%2?S14_PAPER:S14_CARD;return CDRF_NEWFONT;}}
    }
    if(m==WM_PAINT || m==WM_PRINTCLIENT){PAINTSTRUCT ps;HDC dc=m==WM_PAINT?BeginPaint(w,&ps):(HDC)p;RECT r;GetClientRect(w,&r);FillRect(dc,&r,h->brush);
        wchar_t title[128],summary[256],kda[32];swprintf(title,128,L"%ls · 战斗时间线",h->officer.name);text(dc,h->title,title,(RECT){px(h,28),px(h,16),r.right-px(h,130),px(h,59)},S14_INK);
        swprintf(summary,256,L"%ls · %ls · 共 %d 条 · 最近记录在上方",h->officer.force_name,h->officer.location,h->count);text(dc,h->small,summary,(RECT){px(h,30),px(h,65),r.right-px(h,28),px(h,90)},S14_MUTED);
        s14_officer_kda_text(&h->officer,kda,32);swprintf(summary,256,L"斩敌总数 %llu   士卒折损 %llu   杀损比值 %ls   破敌次数 %llu   擒获次数 %llu",(unsigned long long)s14_officer_count_value(&h->officer,S14_SORT_ENEMY_LOSS),(unsigned long long)s14_officer_count_value(&h->officer,S14_SORT_OWN_LOSS),kda,(unsigned long long)s14_officer_count_value(&h->officer,S14_SORT_ROUTS),(unsigned long long)s14_officer_count_value(&h->officer,S14_SORT_CAPTURES));text(dc,h->font,summary,(RECT){px(h,30),px(h,94),r.right-px(h,28),px(h,122)},S14_INK);
        text(dc,h->small,L"记录发生时的地区 · 旧记录不补填地点 · 待核实条目未计入累计",(RECT){px(h,30),px(h,124),r.right-px(h,28),px(h,149)},S14_MUTED);
        const wchar_t *note=!h->snapshot->version?L"尚未取得本存档时间线；更新前未保留的战斗不能从累计数还原。":!h->count?L"暂无该武将的时间线；从新版本接入后记录。旧累计战绩仍保留。":h->snapshot->truncated?L"时间线保留最近 16,384 条全局记录，较早明细已超出容量；累计数仍独立保存。":h->snapshot->incomplete?L"存在采集缺口，本页显示已记录事件；请随游戏保存以保留时间线。":L"时间线随游戏保存、读档恢复；当前页面使用按 F 打开时的快照。";
        text(dc,h->small,note,(RECT){px(h,28),r.bottom-px(h,26),r.right-px(h,28),r.bottom-px(h,4)},S14_MUTED);if(m==WM_PAINT)EndPaint(w,&ps);return 0;}
    return DefWindowProcW(w,m,p,l);
}
int s14_history_create(S14OfficerHistory *h,HINSTANCE instance,HWND owner,int scale){
    h->owner=owner;h->scale=scale;h->snapshot=calloc(1,sizeof(*h->snapshot));h->font=CreateFontW(-px(h,15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");h->small=CreateFontW(-px(h,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");h->title=CreateFontW(-px(h,29),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"SimSun");h->brush=CreateSolidBrush(S14_PAPER);
    if(!h->snapshot || !h->font || !h->small || !h->title || !h->brush)return 0;
    WNDCLASSEXW cls={0};cls.cbSize=sizeof(cls);cls.hInstance=instance;cls.lpfnWndProc=procedure;cls.lpszClassName=history_class;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);if(!RegisterClassExW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    h->window=CreateWindowExW(WS_EX_TOOLWINDOW,history_class,L"武将战斗时间线",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,px(h,1260),px(h,720),owner,NULL,instance,h);if(!h->window)return 0;
    h->close=CreateWindowExW(0,L"BUTTON",L"返回清单",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,h->window,(HMENU)521,instance,NULL);
    h->list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_OWNERDATA|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,h->window,(HMENU)520,instance,NULL);if(!h->list || !h->close)return 0;
    HWND items[]={h->list,h->close};for(int i=0;i<2;i++){SendMessageW(items[i],WM_SETFONT,(WPARAM)h->font,TRUE);SetWindowSubclass(items[i],control,1,(DWORD_PTR)h);}
    ListView_SetExtendedListViewStyle(h->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ListView_SetBkColor(h->list,S14_CARD);ListView_SetTextBkColor(h->list,S14_CARD);ListView_SetTextColor(h->list,S14_INK);
    for(int i=0;i<5;i++){LVCOLUMNW c={0};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(wchar_t*)labels[i];c.cx=px(h,150);ListView_InsertColumn(h->list,i,&c);}HWND header=ListView_GetHeader(h->list);SetWindowLongPtrW(header,GWL_STYLE,GetWindowLongPtrW(header,GWL_STYLE)|HDS_NOSIZING);layout(h);return 1;
}
void s14_history_show(S14OfficerHistory *h,const S14Officer *officer){
    if(!h->window || !officer)return;h->officer=*officer;h->count=0;
    if(h->snapshot->version==1)for(unsigned int i=h->snapshot->count;i>0;i--){int at=(h->snapshot->first+i-1)%S14_TIMELINE_MAX;if(s14_timeline_matches(&h->snapshot->events[at],officer->id))h->indices[h->count++]=at;}
    ListView_SetItemCountEx(h->list,h->count,0);if(h->count)ListView_EnsureVisible(h->list,0,FALSE);
    RECT owner;GetWindowRect(h->owner,&owner);MONITORINFO m={sizeof(m)};GetMonitorInfoW(MonitorFromWindow(h->owner,MONITOR_DEFAULTTONEAREST),&m);int width=px(h,1260),height=px(h,720);if(width>m.rcWork.right-m.rcWork.left)width=m.rcWork.right-m.rcWork.left;if(height>m.rcWork.bottom-m.rcWork.top)height=m.rcWork.bottom-m.rcWork.top;
    int x=(owner.left+owner.right-width)/2,y=(owner.top+owner.bottom-height)/2;if(x<m.rcWork.left || x+width>m.rcWork.right)x=m.rcWork.left+(m.rcWork.right-m.rcWork.left-width)/2;if(y<m.rcWork.top || y+height>m.rcWork.bottom)y=m.rcWork.top+(m.rcWork.bottom-m.rcWork.top-height)/2;
    int offscreen=owner.right<m.rcWork.left || owner.bottom<m.rcWork.top;if(offscreen){x=owner.left;y=owner.top;}
    SetWindowPos(h->window,HWND_TOP,x,y,width,height,SWP_SHOWWINDOW|(offscreen?SWP_NOACTIVATE:0));if(!offscreen){SetForegroundWindow(h->window);SetFocus(h->list);}InvalidateRect(h->window,NULL,TRUE);
}
void s14_history_destroy(S14OfficerHistory *h){if(h->window && IsWindow(h->window))DestroyWindow(h->window);if(h->font)DeleteObject(h->font);if(h->small)DeleteObject(h->small);if(h->title)DeleteObject(h->title);if(h->brush)DeleteObject(h->brush);free(h->snapshot);memset(h,0,sizeof(*h));}
