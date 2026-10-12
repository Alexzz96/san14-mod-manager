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
static int choice(S14PersonalityUI *u){LRESULT n=SendMessageW(u->catalog,CB_GETCURSEL,0,0);return n==CB_ERR?0:(int)SendMessageW(u->catalog,CB_GETITEMDATA,n,0);}
static void catalog(S14PersonalityUI *u){wchar_t query[80],name[120],idtext[20];GetWindowTextW(u->search,query,80);int old=choice(u);SendMessageW(u->catalog,CB_RESETCONTENT,0,0);int select=-1;
    for(int i=1;i<356;i++){swprintf(idtext,20,L"%d",i);const wchar_t *q=query[0]==L'#'?query+1:query;if(q[0] && !wcsstr(u->definitions[i].name,q) && !wcsstr(idtext,q) && !wcsstr(u->definitions[i].description,q))continue;
        swprintf(name,120,L"#%d · %ls",i,u->definitions[i].name[0]?u->definitions[i].name:L"隐藏／未命名");LRESULT n=SendMessageW(u->catalog,CB_ADDSTRING,0,(LPARAM)name);if(n>=0){SendMessageW(u->catalog,CB_SETITEMDATA,n,i);if(i==old)select=(int)n;}}
    SendMessageW(u->catalog,CB_SETCURSEL,select>=0?select:0,0);
}
static void actions(S14PersonalityUI *u){int writable=u->connected && !u->pending,id=choice(u),selected=u->selected>=0 && u->selected<9;
    int duplicate=0,empty=0;for(int i=0;i<9;i++){if(id && u->staged[i]==id)duplicate=1;if(!u->staged[i])empty=1;}
    EnableWindow(u->add,writable && id && !duplicate && empty);EnableWindow(u->replace,writable && id && !duplicate && selected && u->staged[u->selected]);
    EnableWindow(u->remove,writable && selected && u->staged[u->selected]);EnableWindow(u->apply,writable && memcmp(u->staged,u->officer.personalities,18)!=0);EnableWindow(u->discard,!u->pending);EnableWindow(u->refresh,!u->pending);EnableWindow(u->catalog,!u->pending);EnableWindow(u->search,!u->pending);
}
static void populate(S14PersonalityUI *u){int selected=u->selected;
    ListView_DeleteAllItems(u->list);
    for(int i=0;i<9;i++){
        wchar_t slot[32],name[80],kind[40];unsigned id=u->staged[i];swprintf(slot,32,L"第 %d 格",i+1);
        swprintf(kind,40,L"%ls",i<5?L"常规槽":L"扩展槽");
        if(!id)wcscpy(name,L"空位");else if(id<356 && u->definitions[id].name[0])swprintf(name,80,L"%ls · #%u",u->definitions[id].name,id);else swprintf(name,80,L"隐藏／未命名 · #%u",id);
        LVITEMW item={.mask=LVIF_TEXT,.iItem=i,.pszText=slot};ListView_InsertItem(u->list,&item);
        ListView_SetItemText(u->list,i,1,kind);ListView_SetItemText(u->list,i,2,name);
        ListView_SetItemText(u->list,i,3,id && id<356 && u->definitions[id].description[0]?u->definitions[id].description:id?L"本机定义未提供文字说明，保留编号识别。":L"可暂存目录个性");
    }
    u->selected=selected;if(selected>=0 && selected<9)ListView_SetItemState(u->list,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);actions(u);
}
static void refresh(S14PersonalityUI *u){u->connected=s14_edit_capability(S14_EDIT_PERSONALITIES) && s14_edit_capture(u->world,u->officer.id,&u->before);if(u->connected){memcpy(u->officer.personalities,u->before.raw+0x150,18);wcscpy(u->notice,L"已读取 9 个槽位。添加、替换和删除先暂存；点击应用才修改游戏。");}else wcscpy(u->notice,L"未取得编辑快照，当前只读；请载入存档后重新打开。待实机验收。");memcpy(u->staged,u->officer.personalities,18);populate(u);InvalidateRect(u->window,NULL,FALSE);}
static void layout(S14PersonalityUI *u){RECT r;GetClientRect(u->window,&r);int w=r.right,h=r.bottom;
    MoveWindow(u->close,w-px(u,116),px(u,22),px(u,88),px(u,34),TRUE);
    MoveWindow(u->search,px(u,28),px(u,120),px(u,240),px(u,30),TRUE);MoveWindow(u->catalog,px(u,280),px(u,120),w-px(u,308),px(u,300),TRUE);
    MoveWindow(u->list,px(u,28),px(u,218),w-px(u,56),h-px(u,338),TRUE);
    MoveWindow(u->add,px(u,28),h-px(u,104),px(u,126),px(u,38),TRUE);MoveWindow(u->replace,px(u,166),h-px(u,104),px(u,150),px(u,38),TRUE);MoveWindow(u->refresh,px(u,752),h-px(u,104),px(u,112),px(u,38),TRUE);
    MoveWindow(u->remove,px(u,328),h-px(u,104),px(u,104),px(u,38),TRUE);MoveWindow(u->apply,px(u,444),h-px(u,104),px(u,142),px(u,38),TRUE);MoveWindow(u->discard,px(u,598),h-px(u,104),px(u,142),px(u,38),TRUE);
    int width=w-px(u,56)-GetSystemMetrics(SM_CXVSCROLL)-4;ListView_SetColumnWidth(u->list,0,px(u,76));ListView_SetColumnWidth(u->list,1,px(u,88));ListView_SetColumnWidth(u->list,2,px(u,200));ListView_SetColumnWidth(u->list,3,width-px(u,364));}
void s14_personality_ui_hide(S14PersonalityUI *u){if(u->window){ShowWindow(u->window,SW_HIDE);if(u->owner && IsWindowVisible(u->owner))SetForegroundWindow(u->owner);}}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM p,LPARAM l){S14PersonalityUI *u=(void*)GetWindowLongPtrW(w,GWLP_USERDATA);if(m==WM_NCCREATE){u=((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)u);}if(!u)return DefWindowProcW(w,m,p,l);
    if(m==WM_GETMINMAXINFO){((MINMAXINFO*)l)->ptMinTrackSize=(POINT){px(u,920),px(u,540)};return 0;}if(m==WM_SIZE){if(u->list)layout(u);return 0;}if(m==WM_CLOSE || (m==WM_KEYDOWN && p==VK_ESCAPE)){s14_personality_ui_hide(u);return 0;}if(m==WM_ERASEBKGND)return 1;
    if(m==WM_COMMAND){int id=LOWORD(p);
        if(id==604 || id==IDCANCEL){s14_personality_ui_hide(u);return 0;}
        if(id==611 && HIWORD(p)==EN_CHANGE){catalog(u);actions(u);InvalidateRect(w,NULL,FALSE);return 0;}
        if(id==612 && HIWORD(p)==CBN_SELCHANGE){actions(u);InvalidateRect(w,NULL,FALSE);return 0;}
        if(u->pending)return 0;
        if(id==603){refresh(u);return 0;}
        if(id==607){memcpy(u->staged,u->officer.personalities,18);wcscpy(u->notice,L"已撤销暂存，游戏数据未改动。");populate(u);InvalidateRect(w,NULL,FALSE);return 0;}
        if(id==601 || id==602 || id==605){unsigned short staged[9];memcpy(staged,u->staged,18);int at=u->selected;
            if(id==601){at=-1;for(int i=0;i<9;i++)if(!staged[i]){at=i;break;}}
            if(!u->connected || at<0 || at>=9 || (id!=601 && !staged[at]))return 0;
            int selected=choice(u);if(id!=605 && !selected)return 0;staged[at]=id==605?0:selected;
            if(s14_edit_personality_validate(u->staged,staged)!=S14_PE_PENDING)wcscpy(u->notice,L"编号或重复项校验失败；暂存数据未改动。");
            else{memcpy(u->staged,staged,18);u->selected=at;wcscpy(u->notice,L"修改已暂存。点击应用才提交；刷新或撤销暂存会放弃本次修改。");}
            populate(u);InvalidateRect(w,NULL,FALSE);return 0;
        }
        if(id==606 && u->connected && memcmp(u->staged,u->officer.personalities,18)){
            S14EditRequest request={.version=1,.operation=S14_EDIT_PERSONALITIES,.before=u->before};memcpy(request.personalities,u->staged,18);
            int status=s14_edit_submit(&request,&u->request_id);u->pending=status==S14_EDIT_QUEUED;
            wcscpy(u->notice,u->pending?L"已排队；等待游戏规划回调，读档或回合执行会取消请求。":status==S14_EDIT_EXPIRED?L"身份或快照已改变；请刷新，暂存尚未应用。":L"校验失败或接口不可用；暂存尚未应用。");
            if(!u->pending){S14EditResult rejected;if(s14_edit_result(u->request_id,&rejected))wcscpy(u->notice,rejected.detail);}
            if(u->pending)SetTimer(w,610,100,NULL);actions(u);InvalidateRect(w,NULL,FALSE);return 0;
        }
    }
    if(m==WM_TIMER && p==610){S14EditResult result;if(!s14_edit_result(u->request_id,&result)){KillTimer(w,610);u->pending=0;u->connected=0;wcscpy(u->notice,L"请求结果已不可用；请刷新检查当前状态。");actions(u);InvalidateRect(w,NULL,FALSE);return 0;}if(result.status!=S14_EDIT_QUEUED){KillTimer(w,610);u->pending=0;
        if(result.status==S14_EDIT_SUCCESS){refresh(u);if(u->connected)SendMessageW(u->owner,S14_PERSONALITY_CHANGED,(WPARAM)u->world,(LPARAM)&u->officer);}else u->connected=0;
        wcscpy(u->notice,result.detail);actions(u);InvalidateRect(w,NULL,FALSE);}return 0;}
    if(m==WM_NOTIFY){NMHDR *h=(void*)l;if(h->hwndFrom==u->list && h->code==LVN_ITEMCHANGED){u->selected=ListView_GetNextItem(u->list,-1,LVNI_SELECTED);actions(u);}
        if(h->hwndFrom==u->list && h->code==NM_CUSTOMDRAW){NMLVCUSTOMDRAW *d=(void*)l;if(d->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;if(d->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){d->clrText=S14_INK;d->clrTextBk=d->nmcd.dwItemSpec%2?S14_PAPER:S14_CARD;return CDRF_NEWFONT;}}}
    if(m==WM_DRAWITEM){DRAWITEMSTRUCT *d=(void*)l;int active=IsWindowEnabled(d->hwndItem);HBRUSH b=CreateSolidBrush(active?S14_ACCENT_SOFT:S14_PAPER);FillRect(d->hDC,&d->rcItem,b);DeleteObject(b);b=CreateSolidBrush(S14_BORDER);FrameRect(d->hDC,&d->rcItem,b);DeleteObject(b);wchar_t text[64];GetWindowTextW(d->hwndItem,text,64);label(d->hDC,u->font,text,d->rcItem,active?S14_ACCENT:S14_MUTED,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;}
    if(m==WM_PAINT || m==WM_PRINTCLIENT){PAINTSTRUCT ps;HDC dc=m==WM_PAINT?BeginPaint(w,&ps):(HDC)p;RECT r;GetClientRect(w,&r);FillRect(dc,&r,u->paper);wchar_t title[96];swprintf(title,96,L"%ls · 个性编辑（待验证）",u->officer.name);label(dc,u->title,title,(RECT){px(u,28),px(u,18),r.right-px(u,132),px(u,64)},S14_INK,DT_LEFT|DT_SINGLELINE);
        label(dc,u->small,L"内部 9 个槽位 · 原生最多展示 5 项 · 扩展槽不等于全部隐藏",(RECT){px(u,28),px(u,72),r.right-px(u,28),px(u,100)},S14_MUTED,DT_LEFT|DT_SINGLELINE);
        label(dc,u->font,L"目录搜索：名称、编号或效果说明",(RECT){px(u,28),px(u,94),r.right-px(u,28),px(u,118)},S14_ACCENT,DT_LEFT|DT_SINGLELINE);
        label(dc,u->small,choice(u)>0?u->definitions[choice(u)].description:L"输入关键词筛选目录。共享定义只读；隐藏项保留编号。",(RECT){px(u,28),px(u,166),r.right-px(u,28),px(u,212)},S14_INK,DT_LEFT|DT_WORDBREAK);
        label(dc,u->small,u->notice,(RECT){px(u,28),r.bottom-px(u,54),r.right-px(u,28),r.bottom-px(u,10)},S14_MUTED,DT_LEFT|DT_WORDBREAK);if(m==WM_PAINT)EndPaint(w,&ps);return 0;}
    return DefWindowProcW(w,m,p,l);
}
int s14_personality_ui_create(S14PersonalityUI *u,HINSTANCE instance,HWND owner,int scale){u->owner=owner;u->instance=instance;u->scale=scale;u->selected=-1;u->paper=CreateSolidBrush(S14_PAPER);
    u->font=CreateFontW(-px(u,15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");u->small=CreateFontW(-px(u,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");u->title=CreateFontW(-px(u,29),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"SimSun");
    WNDCLASSEXW c={.cbSize=sizeof(c),.hInstance=instance,.lpfnWndProc=proc,.lpszClassName=klass,.hCursor=LoadCursorW(NULL,IDC_ARROW)};if(!RegisterClassExW(&c) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    u->window=CreateWindowExW(WS_EX_TOOLWINDOW,klass,L"武将个性配置",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,px(u,1040),px(u,640),owner,NULL,instance,u);if(!u->window)return 0;
    u->list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,u->window,(HMENU)600,instance,NULL);
    HWND *buttons[]={&u->add,&u->replace,&u->refresh,&u->close};const wchar_t *names[]={L"暂存添加",L"暂存替换选中槽",L"刷新",L"返回清单"};for(int i=0;i<4;i++)*buttons[i]=CreateWindowExW(0,L"BUTTON",names[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,u->window,(HMENU)(intptr_t)(601+i),instance,NULL);
    u->search=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,u->window,(HMENU)611,instance,NULL);
    SendMessageW(u->search,EM_SETLIMITTEXT,79,0);SendMessageW(u->search,EM_SETCUEBANNER,0,(LPARAM)L"搜索个性 / #编号");
    u->catalog=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,u->window,(HMENU)612,instance,NULL);
    HWND *staging[]={&u->remove,&u->apply,&u->discard};const wchar_t *labels[]={L"暂存删除",L"应用",L"撤销暂存"};for(int i=0;i<3;i++)*staging[i]=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,u->window,(HMENU)(intptr_t)(605+i),instance,NULL);
    HWND controls[]={u->list,u->add,u->replace,u->refresh,u->close,u->search,u->catalog,u->remove,u->apply,u->discard};for(int i=0;i<10;i++){if(!controls[i])return 0;SendMessageW(controls[i],WM_SETFONT,(WPARAM)u->font,TRUE);}
    ListView_SetExtendedListViewStyle(u->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ListView_SetBkColor(u->list,S14_CARD);ListView_SetTextBkColor(u->list,S14_CARD);ListView_SetTextColor(u->list,S14_INK);
    const wchar_t *heads[]={L"槽位",L"分类",L"当前个性",L"本机效果说明"};for(int i=0;i<4;i++){LVCOLUMNW col={.mask=LVCF_TEXT|LVCF_WIDTH,.pszText=(wchar_t*)heads[i],.cx=px(u,150)};ListView_InsertColumn(u->list,i,&col);}layout(u);return u->font && u->small && u->title && u->paper;
}
void s14_personality_ui_show(S14PersonalityUI *u,const S14OfficerSnapshot *s,const S14Officer *p){if(!u->window || !s || !p || u->pending)return;u->world=s->world;u->officer=*p;memcpy(u->definitions,s->personalities,sizeof(u->definitions));u->selected=-1;SetWindowTextW(u->search,L"");catalog(u);refresh(u);
    RECT r;GetWindowRect(u->owner,&r);MONITORINFO monitor={sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(u->owner,MONITOR_DEFAULTTONEAREST),&monitor);int width=px(u,1040),height=px(u,640);if(width>monitor.rcWork.right-monitor.rcWork.left)width=monitor.rcWork.right-monitor.rcWork.left;if(height>monitor.rcWork.bottom-monitor.rcWork.top)height=monitor.rcWork.bottom-monitor.rcWork.top;
    int x=monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,y=monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2;
    int offscreen=r.right<monitor.rcWork.left || r.bottom<monitor.rcWork.top;if(offscreen){x=r.left;y=r.top;}
    SetWindowPos(u->window,HWND_TOP,x,y,width,height,SWP_SHOWWINDOW|(offscreen?SWP_NOACTIVATE:0));if(!offscreen){SetForegroundWindow(u->window);SetFocus(u->list);}InvalidateRect(u->window,NULL,FALSE);
}
void s14_personality_ui_destroy(S14PersonalityUI *u){if(u->window)DestroyWindow(u->window);if(u->font)DeleteObject(u->font);if(u->title)DeleteObject(u->title);if(u->small)DeleteObject(u->small);if(u->paper)DeleteObject(u->paper);memset(u,0,sizeof(*u));}
