#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "personality_ui.h"
#include "personality_edit.h"
#include "theme.h"
static const wchar_t klass[]=L"SAN14ModManager.PersonalityEditor.v1";
static int px(S14PersonalityUI *u,int n){return MulDiv(n,u->scale,96);}
static void label(HDC dc,HFONT font,const wchar_t *s,RECT r,COLORREF color,UINT flags){HGDIOBJ old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s,-1,&r,flags|DT_NOPREFIX);SelectObject(dc,old);}
static const wchar_t *message(int status){switch(status){case S14_PE_APPLIED:return L"已修改。请保存游戏以保留；部队效果在原生下一次刷新时重算。";case S14_PE_STALE:return L"武将数据或存档已改变，请刷新后重新选择。";case S14_PE_FULL:return L"9 个槽位已满，无法新增；请选择一个已有槽位替换。";case S14_PE_DUPLICATE:return L"该武将已有远矢，不能重复添加。";case S14_PE_PENDING:return L"等待游戏规划界面处理，请保持暂停；读档与回合执行期间不修改。";case S14_PE_UNAVAILABLE:return L"暂未接入或游戏未处理请求，请回到地图规划界面后重试。";default:return L"修改未完成，请刷新检查当前个性。";}}
static void actions(S14PersonalityUI *u){int target=0;EnableWindow(u->add,u->connected && !u->pending && s14_personality_plan(u->officer.personalities,1,0,&target)==S14_PE_PENDING);EnableWindow(u->replace,u->connected && !u->pending && s14_personality_plan(u->officer.personalities,0,u->selected,&target)==S14_PE_PENDING);EnableWindow(u->refresh,!u->pending);}
static void populate(S14PersonalityUI *u){
    ListView_DeleteAllItems(u->list);
    for(int i=0;i<9;i++){
        wchar_t slot[32],name[80],kind[40];unsigned id=u->officer.personalities[i];if(id>=356)id=0;swprintf(slot,32,L"第 %d 格",i+1);
        swprintf(kind,40,L"%ls",i<5?L"常规槽":L"扩展槽");
        if(!id)wcscpy(name,L"空位");else if(u->definitions[id].name[0])swprintf(name,80,L"%ls · #%u",u->definitions[id].name,id);else swprintf(name,80,L"隐藏／未命名 · #%u",id);
        LVITEMW item={.mask=LVIF_TEXT,.iItem=i,.pszText=slot};ListView_InsertItem(u->list,&item);
        ListView_SetItemText(u->list,i,1,kind);ListView_SetItemText(u->list,i,2,name);
        ListView_SetItemText(u->list,i,3,id && u->definitions[id].description[0]?u->definitions[id].description:id?L"本机定义未提供文字说明，保留编号识别。":L"可新增远矢");
    }
    if(u->selected>=0 && u->selected<9)ListView_SetItemState(u->list,u->selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);actions(u);
}
static void refresh(S14PersonalityUI *u){unsigned short slots[9];u->connected=s14_personality_capture(u->world,u->officer.id,slots);if(u->connected){memcpy(u->officer.personalities,slots,18);wcscpy(u->notice,L"已读取当前 9 个槽位。新增只使用空位；替换只修改选中格。隐藏项按编号保留。");}else wcscpy(u->notice,L"未取得当前武将数据，请载入存档后重新打开面板。");populate(u);InvalidateRect(u->window,NULL,FALSE);}
static void layout(S14PersonalityUI *u){RECT r;GetClientRect(u->window,&r);int w=r.right,h=r.bottom;
    MoveWindow(u->close,w-px(u,116),px(u,22),px(u,88),px(u,34),TRUE);
    MoveWindow(u->list,px(u,28),px(u,184),w-px(u,56),h-px(u,304),TRUE);
    MoveWindow(u->add,px(u,28),h-px(u,104),px(u,156),px(u,38),TRUE);MoveWindow(u->replace,px(u,198),h-px(u,104),px(u,192),px(u,38),TRUE);MoveWindow(u->refresh,px(u,404),h-px(u,104),px(u,112),px(u,38),TRUE);
    int width=w-px(u,56)-GetSystemMetrics(SM_CXVSCROLL)-4;ListView_SetColumnWidth(u->list,0,px(u,76));ListView_SetColumnWidth(u->list,1,px(u,88));ListView_SetColumnWidth(u->list,2,px(u,200));ListView_SetColumnWidth(u->list,3,width-px(u,364));}
void s14_personality_ui_hide(S14PersonalityUI *u){if(u->window){ShowWindow(u->window,SW_HIDE);if(u->owner && IsWindowVisible(u->owner))SetForegroundWindow(u->owner);}}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM p,LPARAM l){S14PersonalityUI *u=(void*)GetWindowLongPtrW(w,GWLP_USERDATA);if(m==WM_NCCREATE){u=((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)u);}if(!u)return DefWindowProcW(w,m,p,l);
    if(m==WM_SIZE){layout(u);return 0;}if(m==WM_CLOSE || (m==WM_KEYDOWN && p==VK_ESCAPE)){s14_personality_ui_hide(u);return 0;}if(m==WM_ERASEBKGND)return 1;
    if(m==WM_COMMAND){int id=LOWORD(p);if(id==604 || id==IDCANCEL){s14_personality_ui_hide(u);return 0;}if(id==603){refresh(u);return 0;}if(id==601 || id==602){int status=s14_personality_submit(u->world,u->officer.id,u->officer.personalities,id==601,u->selected);u->pending=status==S14_PE_PENDING;wcscpy(u->notice,message(status));if(u->pending)SetTimer(w,610,100,NULL);actions(u);InvalidateRect(w,NULL,FALSE);return 0;}}
    if(m==WM_TIMER && p==610){int status=s14_personality_result(NULL);if(status!=S14_PE_PENDING){KillTimer(w,610);u->pending=0;if(status==S14_PE_APPLIED){refresh(u);if(u->connected)SendMessageW(u->owner,S14_PERSONALITY_CHANGED,(WPARAM)u->world,(LPARAM)&u->officer);}wcscpy(u->notice,message(status));actions(u);InvalidateRect(w,NULL,FALSE);}return 0;}
    if(m==WM_NOTIFY){NMHDR *h=(void*)l;if(h->hwndFrom==u->list && h->code==LVN_ITEMCHANGED){u->selected=ListView_GetNextItem(u->list,-1,LVNI_SELECTED);actions(u);}
        if(h->hwndFrom==u->list && h->code==NM_CUSTOMDRAW){NMLVCUSTOMDRAW *d=(void*)l;if(d->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;if(d->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){d->clrText=S14_INK;d->clrTextBk=d->nmcd.dwItemSpec%2?S14_PAPER:S14_CARD;return CDRF_NEWFONT;}}}
    if(m==WM_DRAWITEM){DRAWITEMSTRUCT *d=(void*)l;int active=IsWindowEnabled(d->hwndItem);HBRUSH b=CreateSolidBrush(active?S14_ACCENT_SOFT:S14_PAPER);FillRect(d->hDC,&d->rcItem,b);DeleteObject(b);b=CreateSolidBrush(S14_BORDER);FrameRect(d->hDC,&d->rcItem,b);DeleteObject(b);wchar_t text[64];GetWindowTextW(d->hwndItem,text,64);label(d->hDC,u->font,text,d->rcItem,active?S14_ACCENT:S14_MUTED,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;}
    if(m==WM_PAINT || m==WM_PRINTCLIENT){PAINTSTRUCT ps;HDC dc=m==WM_PAINT?BeginPaint(w,&ps):(HDC)p;RECT r;GetClientRect(w,&r);FillRect(dc,&r,u->paper);wchar_t title[96];swprintf(title,96,L"%ls · 个性配置",u->officer.name);label(dc,u->title,title,(RECT){px(u,28),px(u,18),r.right-px(u,132),px(u,64)},S14_INK,DT_LEFT|DT_SINGLELINE);
        label(dc,u->small,L"内部 9 个槽位 · 原生最多展示 5 项 · 扩展槽不等于全部隐藏",(RECT){px(u,28),px(u,72),r.right-px(u,28),px(u,100)},S14_MUTED,DT_LEFT|DT_SINGLELINE);
        label(dc,u->font,L"预设个性：远矢",(RECT){px(u,28),px(u,111),r.right-px(u,28),px(u,140)},S14_ACCENT,DT_LEFT|DT_SINGLELINE);
        label(dc,u->small,u->definitions[6].description[0]?u->definitions[6].description:L"按本机远矢原生条件生效。",(RECT){px(u,28),px(u,145),r.right-px(u,28),px(u,178)},S14_INK,DT_LEFT|DT_WORDBREAK);
        label(dc,u->small,u->notice,(RECT){px(u,28),r.bottom-px(u,54),r.right-px(u,28),r.bottom-px(u,10)},S14_MUTED,DT_LEFT|DT_WORDBREAK);if(m==WM_PAINT)EndPaint(w,&ps);return 0;}
    return DefWindowProcW(w,m,p,l);
}
int s14_personality_ui_create(S14PersonalityUI *u,HINSTANCE instance,HWND owner,int scale){u->owner=owner;u->instance=instance;u->scale=scale;u->selected=-1;u->paper=CreateSolidBrush(S14_PAPER);
    u->font=CreateFontW(-px(u,15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");u->small=CreateFontW(-px(u,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");u->title=CreateFontW(-px(u,29),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"SimSun");
    WNDCLASSEXW c={.cbSize=sizeof(c),.hInstance=instance,.lpfnWndProc=proc,.lpszClassName=klass,.hCursor=LoadCursorW(NULL,IDC_ARROW)};if(!RegisterClassExW(&c) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    u->window=CreateWindowExW(WS_EX_TOOLWINDOW,klass,L"武将个性配置",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,px(u,1040),px(u,640),owner,NULL,instance,u);if(!u->window)return 0;
    u->list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,u->window,(HMENU)600,instance,NULL);
    HWND *buttons[]={&u->add,&u->replace,&u->refresh,&u->close};const wchar_t *names[]={L"新增远矢",L"替换选中槽",L"刷新",L"返回清单"};for(int i=0;i<4;i++)*buttons[i]=CreateWindowExW(0,L"BUTTON",names[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,u->window,(HMENU)(intptr_t)(601+i),instance,NULL);
    HWND controls[]={u->list,u->add,u->replace,u->refresh,u->close};for(int i=0;i<5;i++){if(!controls[i])return 0;SendMessageW(controls[i],WM_SETFONT,(WPARAM)u->font,TRUE);}
    ListView_SetExtendedListViewStyle(u->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ListView_SetBkColor(u->list,S14_CARD);ListView_SetTextBkColor(u->list,S14_CARD);ListView_SetTextColor(u->list,S14_INK);
    const wchar_t *heads[]={L"槽位",L"分类",L"当前个性",L"本机效果说明"};for(int i=0;i<4;i++){LVCOLUMNW col={.mask=LVCF_TEXT|LVCF_WIDTH,.pszText=(wchar_t*)heads[i],.cx=px(u,150)};ListView_InsertColumn(u->list,i,&col);}layout(u);return u->font && u->small && u->title && u->paper;
}
void s14_personality_ui_show(S14PersonalityUI *u,const S14OfficerSnapshot *s,const S14Officer *p){if(!u->window || !s || !p || u->pending)return;u->world=s->world;u->officer=*p;memcpy(u->definitions,s->personalities,sizeof(u->definitions));u->selected=-1;refresh(u);
    RECT r;GetWindowRect(u->owner,&r);MONITORINFO monitor={sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(u->owner,MONITOR_DEFAULTTONEAREST),&monitor);int width=px(u,1040),height=px(u,640);if(width>monitor.rcWork.right-monitor.rcWork.left)width=monitor.rcWork.right-monitor.rcWork.left;if(height>monitor.rcWork.bottom-monitor.rcWork.top)height=monitor.rcWork.bottom-monitor.rcWork.top;
    int x=monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,y=monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2;
    int offscreen=r.right<monitor.rcWork.left || r.bottom<monitor.rcWork.top;if(offscreen){x=r.left;y=r.top;}
    SetWindowPos(u->window,HWND_TOP,x,y,width,height,SWP_SHOWWINDOW|(offscreen?SWP_NOACTIVATE:0));if(!offscreen){SetForegroundWindow(u->window);SetFocus(u->list);}InvalidateRect(u->window,NULL,FALSE);
}
void s14_personality_ui_destroy(S14PersonalityUI *u){if(u->window)DestroyWindow(u->window);if(u->font)DeleteObject(u->font);if(u->title)DeleteObject(u->title);if(u->small)DeleteObject(u->small);if(u->paper)DeleteObject(u->paper);memset(u,0,sizeof(*u));}
