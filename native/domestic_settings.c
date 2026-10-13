#define WIN32_LEAN_AND_MEAN
#include "domestic_settings.h"
#include "battle_probe.h"
#include "features.h"
#include <commctrl.h>
#include <windowsx.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define BITS_BYTES ((S14_D_PEOPLE+7)/8)
#define HEX_COUNT (BITS_BYTES*2)
enum {Q_POLICY=760,Q_SECURITY,Q_THRESHOLD_ON,Q_THRESHOLD,Q_SEARCH,Q_BLACK,Q_WHITE,Q_AUTO,Q_SAVE,Q_CLOSE};
static void defaults(S14DOptions *o){memset(o,0,sizeof(*o));o->policy=S14_D_MAX_DEVELOPMENT;o->security_first=1;o->leadership_min=80;}
static int digit(wchar_t c){return c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:-1;}
static void encode(const unsigned char *bits,wchar_t out[HEX_COUNT+1]){
 const wchar_t *hex=L"0123456789abcdef";for(int b=0;b<BITS_BYTES;b++){int v=0;for(int k=0;k<8;k++)if(b*8+k<S14_D_PEOPLE&&bits[b*8+k])v|=1<<k;out[b*2]=hex[v>>4];out[b*2+1]=hex[v&15];}out[HEX_COUNT]=0;
}
static int decode(const wchar_t *hex,unsigned char *bits){
 for(int b=0;b<BITS_BYTES;b++){int hi=digit(hex[b*2]),lo=digit(hex[b*2+1]);if(hi<0||lo<0)return 0;int v=hi*16+lo;for(int k=0;k<8;k++){int id=b*8+k;if(id<S14_D_PEOPLE)bits[id]=!!(v&(1<<k));else if(v&(1<<k))return 0;}}
 return !bits[0];
}
static int number(const wchar_t **p,int max,int *out){const wchar_t *start=*p;if(*start<L'0'||*start>L'9')return 0;wchar_t *end;long v=wcstol(start,&end,10);if(*end!=L'|'||v<0||v>max)return 0;*out=(int)v;*p=end+1;return 1;}
int s14_domestic_settings_read(const wchar_t *ini,S14DOptions *o){
 if(!o)return 0;defaults(o);if(!ini||!ini[0])return 1;wchar_t text[3300];DWORD n=GetPrivateProfileStringW(L"Domestic",L"Preferences",L"",text,3300,ini);if(!n)return 1;if(n>=3299)return 0;
 const wchar_t *p=text;int version;S14DOptions parsed;defaults(&parsed);
 if(!number(&p,1,&version)||version!=1||!number(&p,5,&parsed.policy)||!parsed.policy||!number(&p,1,&parsed.security_first)||!number(&p,1,&parsed.leadership_enabled)||!number(&p,1000,&parsed.leadership_min)||wcslen(p)!=HEX_COUNT*2+1||p[HEX_COUNT]!=L'|'||!decode(p,parsed.blacklist)||!decode(p+HEX_COUNT+1,parsed.whitelist))return 0;
 *o=parsed;return 1;
}
int s14_domestic_settings_write(const wchar_t *ini,const S14DOptions *o){
 if(!ini||!ini[0]||!o||o->policy<1||o->policy>5||o->security_first<0||o->security_first>1||o->leadership_enabled<0||o->leadership_enabled>1||o->leadership_min<0||o->leadership_min>1000||o->blacklist[0]||o->whitelist[0])return 0;
 wchar_t black[HEX_COUNT+1],white[HEX_COUNT+1],text[3300];encode(o->blacklist,black);encode(o->whitelist,white);
 swprintf(text,3300,L"1|%d|%d|%d|%d|%ls|%ls",o->policy,o->security_first,o->leadership_enabled,o->leadership_min,black,white);
 return WritePrivateProfileStringW(L"Domestic",L"Preferences",text,ini);
}
int s14_domestic_settings_seen(const wchar_t *ini){return ini&&ini[0]&&GetPrivateProfileIntW(L"Domestic",L"QuickSetupShown",0,ini)==1;}
int s14_domestic_settings_mark_seen(const wchar_t *ini){return ini&&ini[0]&&WritePrivateProfileStringW(L"Domestic",L"QuickSetupShown",L"1",ini);}
static int px(S14DomesticQuickUI *u,int n){return MulDiv(n,u->dpi,96);}
static void place(S14DomesticQuickUI *u,HWND h,int x,int y,int w,int height){MoveWindow(h,px(u,x),px(u,y),px(u,w),px(u,height),TRUE);}
static void layout(S14DomesticQuickUI *u){RECT r;GetClientRect(u->window,&r);int w=MulDiv(r.right,96,u->dpi),h=MulDiv(r.bottom,96,u->dpi),half=(w-52)/2;
 place(u,u->policy,20,22,w-40,220);place(u,u->security,20,66,260,28);place(u,u->auto_fill,300,66,w-320,28);
 place(u,u->threshold_on,20,108,400,28);place(u,u->threshold,430,108,90,30);place(u,u->search,20,154,w-40,32);
 place(u,u->black,20,200,half,h-290);place(u,u->white,32+half,200,half,h-290);place(u,u->status,20,h-78,w-40,30);place(u,u->save,w-300,h-40,170,30);place(u,u->close,w-118,h-40,98,30);
}
static void column(HWND h,int i,const wchar_t *title,int width){LVCOLUMNW c={.mask=LVCF_TEXT|LVCF_WIDTH,.pszText=(wchar_t*)title,.cx=width};ListView_InsertColumn(h,i,&c);}
static void cell(HWND h,int row,int col,const wchar_t *text){ListView_SetItemText(h,row,col,(wchar_t*)text);}
static void render(S14DomesticQuickUI *u){wchar_t query[80];GetWindowTextW(u->search,query,80);u->updating=1;ListView_DeleteAllItems(u->black);ListView_DeleteAllItems(u->white);int row=0;
 for(int id=1;id<S14_D_PEOPLE;id++){const S14DPerson *p=&u->data->snapshot.people[id];if(!p->active||!p->owned||(query[0]&&!wcsstr(p->name,query)))continue;
  HWND lists[]={u->black,u->white};for(int j=0;j<2;j++){LVITEMW item={.mask=LVIF_TEXT|LVIF_PARAM,.iItem=row,.pszText=(wchar_t*)p->name,.lParam=id};ListView_InsertItem(lists[j],&item);wchar_t stat[16];swprintf(stat,16,L"%d",p->ability[0]);cell(lists[j],row,1,stat);cell(lists[j],row,2,p->city>0&&p->city<52&&u->data->snapshot.cities[p->city].active?u->data->snapshot.cities[p->city].name:L"外征 / 委任 / 其他");cell(lists[j],row,3,!p->eligible||p->busy||p->protected_job?L"原生限制":L"可参与");ListView_SetCheckState(lists[j],row,j?u->options.whitelist[id]:u->options.blacklist[id]);}row++;
 }
 u->updating=0;SetWindowTextW(u->status,u->corrupt?L"保存的快速设置损坏，自动内政已暂停；请重新设置并保存。":L"黑名单优先；白名单豁免统率筛选并优先任命。出征/任务/锁定等限制仍保留。");
}
static void request(S14DomesticQuickUI *u){if(u->pending)return;S14DOptions read=u->options;read.mode=S14_D_FILL;read.city=0;u->request=s14_domestic_request(&read);u->pending=u->request!=0;if(u->pending)SetWindowTextW(u->status,L"等待游戏规划线程读取自势力武将名单…");}
static void fonts(S14DomesticQuickUI *u){if(u->font)DeleteObject(u->font);u->font=CreateFontW(-px(u,15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");HWND cs[]={u->policy,u->security,u->threshold_on,u->threshold,u->search,u->black,u->white,u->auto_fill,u->status,u->save,u->close};for(size_t i=0;i<sizeof(cs)/sizeof(cs[0]);i++)SendMessageW(cs[i],WM_SETFONT,(WPARAM)u->font,TRUE);}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
 S14DomesticQuickUI *u=(S14DomesticQuickUI*)GetWindowLongPtrW(w,GWLP_USERDATA);if(m==WM_NCCREATE){u=((CREATESTRUCTW*)lp)->lpCreateParams;u->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)u);}if(!u)return DefWindowProcW(w,m,wp,lp);
 if(m==WM_SIZE){layout(u);return 0;}if(m==WM_GETMINMAXINFO){MINMAXINFO *v=(MINMAXINFO*)lp;v->ptMinTrackSize=(POINT){px(u,900),px(u,630)};return 0;}
 if(m==WM_DPICHANGED){int old=u->dpi;u->dpi=HIWORD(wp);HWND lists[]={u->black,u->white};for(int j=0;j<2;j++)for(int i=0;i<4;i++)ListView_SetColumnWidth(lists[j],i,MulDiv(ListView_GetColumnWidth(lists[j],i),u->dpi,old));fonts(u);RECT *r=(RECT*)lp;SetWindowPos(w,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);layout(u);return 0;}
 if(m==WM_CLOSE){int focused=GetForegroundWindow()==w;s14_domestic_cancel();u->pending=0;ShowWindow(w,SW_HIDE);HWND owner=GetWindow(w,GW_OWNER);if(owner&&focused)SetForegroundWindow(owner);return 0;}
 if(m==WM_TIMER&&IsWindowVisible(w)){
  if(u->epoch!=s14_battle_session_epoch()||s14_battle_is_loading()){SendMessageW(w,WM_CLOSE,0,0);return 0;}
  if(!u->pending&&!u->data->snapshot.world)request(u);
  if(u->pending&&s14_domestic_result(u->request,u->data)&&u->data->status!=S14_D_PENDING){u->pending=0;if((u->data->status==S14_D_PREVIEW||u->data->status==S14_D_STALE)&&u->data->snapshot.epoch==u->epoch){if(!u->data->plan.valid){memset(u->data,0,sizeof(*u->data));request(u);}else render(u);}else SetWindowTextW(u->status,u->data->message);}return 0;
 }
 if(m==WM_COMMAND){int id=LOWORD(wp);if(id==Q_CLOSE||id==IDCANCEL){SendMessageW(w,WM_CLOSE,0,0);return 0;}
  if(id==Q_SEARCH&&HIWORD(wp)==EN_CHANGE&&!u->updating)render(u);
  if(id==Q_SAVE){wchar_t num[24];GetWindowTextW(u->threshold,num,24);wchar_t *end;long min=wcstol(num,&end,10);if(!num[0]||*end||min<0||min>1000){SetWindowTextW(u->status,L"统率阈值请输入 0 至 1000 的整数，达到阈值即排除。");return 0;}
   static const int policies[]={S14_D_MAX_DEVELOPMENT,S14_D_COMMERCIAL_FIRST,S14_D_AGRICULTURAL_FIRST};int index=(int)SendMessageW(u->policy,CB_GETCURSEL,0,0);if(index<0||index>2)return 0;
   u->options.policy=policies[index];u->options.security_first=Button_GetCheck(u->security)==BST_CHECKED;u->options.leadership_enabled=Button_GetCheck(u->threshold_on)==BST_CHECKED;u->options.leadership_min=(int)min;
   int on=Button_GetCheck(u->auto_fill)==BST_CHECKED;if(!s14_domestic_settings_write(u->ini,&u->options)||!s14_config_set(u->ini,S14_AUTO_DOMESTIC,on)){SetWindowTextW(u->status,L"设置保存失败，请检查文件权限；未提交任命。");return 0;}
   s14_domestic_preferences(&u->options);s14_domestic_auto_configure(on&&!!(s14_config_read(u->ini)&S14_MASTER));if(!s14_domestic_settings_mark_seen(u->ini)){SetWindowTextW(u->status,L"设置已保存，但首次提示标记未保存；请检查文件权限。");return 0;}SendMessageW(w,WM_CLOSE,0,0);return 0;
  }return 0;
 }
 if(m==WM_NOTIFY&&!u->updating){NMLISTVIEW *v=(NMLISTVIEW*)lp;if(v->hdr.code==LVN_ITEMCHANGED&&v->iItem>=0&&(v->uChanged&LVIF_STATE)&&((v->uOldState^v->uNewState)&LVIS_STATEIMAGEMASK)){
  LVITEMW item={.mask=LVIF_PARAM,.iItem=v->iItem};if(ListView_GetItem(v->hdr.hwndFrom,&item)&&item.lParam>0&&item.lParam<S14_D_PEOPLE){int on=((v->uNewState&LVIS_STATEIMAGEMASK)>>12)==2;if(v->hdr.hwndFrom==u->black)u->options.blacklist[item.lParam]=on;else if(v->hdr.hwndFrom==u->white)u->options.whitelist[item.lParam]=on;}}
  return 0;
 }return DefWindowProcW(w,m,wp,lp);
}
static HWND control(S14DomesticQuickUI *u,const wchar_t *class,const wchar_t *text,DWORD style,int id){return CreateWindowExW(0,class,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,1,1,u->window,(HMENU)(INT_PTR)id,u->instance,NULL);}
int s14_domestic_quick_show(S14DomesticQuickUI *u,HINSTANCE instance,HWND owner,const wchar_t *ini){
 if(!u||!ini||!ini[0]||wcslen(ini)>=MAX_PATH)return 0;
 if(u->window&&IsWindowVisible(u->window)){SetForegroundWindow(u->window);return 1;}
 if(!u->window){u->instance=instance;u->dpi=96;HDC dc=GetDC(owner);if(dc){u->dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(owner,dc);}u->data=calloc(1,sizeof(*u->data));if(!u->data)return 0;
  INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&common);WNDCLASSW wc={.lpfnWndProc=proc,.hInstance=instance,.hCursor=LoadCursorW(NULL,IDC_ARROW),.hbrBackground=(HBRUSH)(COLOR_WINDOW+1),.lpszClassName=L"S14.Domestic.Quick"};RegisterClassW(&wc);
  if(!CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_TOOLWINDOW,wc.lpszClassName,L"内政快速设置 · 仅首次回合自动提示",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,px(u,1040),px(u,720),owner,NULL,instance,u)){s14_domestic_quick_destroy(u);return 0;}
  u->policy=control(u,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,Q_POLICY);const wchar_t *choices[]={L"全数值最高 · 按每旬预计净增选方向",L"商业优先 · 商业1000 → 农业1000 → 兵营",L"农业优先 · 农业1000 → 商业1000 → 兵营"};for(int i=0;i<3;i++)SendMessageW(u->policy,CB_ADDSTRING,0,(LPARAM)choices[i]);
  u->security=control(u,L"BUTTON",L"治安未满100优先恢复",BS_AUTOCHECKBOX,Q_SECURITY);u->auto_fill=control(u,L"BUTTON",L"开启回合自动内政（先内政，后探索）",BS_AUTOCHECKBOX,Q_AUTO);
  u->threshold_on=control(u,L"BUTTON",L"统率达到以下数值的武将不参与内政",BS_AUTOCHECKBOX,Q_THRESHOLD_ON);u->threshold=control(u,L"EDIT",L"80",WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,Q_THRESHOLD);SendMessageW(u->threshold,EM_SETLIMITTEXT,4,0);
  u->search=control(u,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,Q_SEARCH);SendMessageW(u->search,EM_SETCUEBANNER,0,(LPARAM)L"搜索自势力武将姓名；名单同时保留未显示的选择");SendMessageW(u->search,EM_SETLIMITTEXT,79,0);
  u->black=control(u,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS,Q_BLACK);u->white=control(u,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS,Q_WHITE);HWND lists[]={u->black,u->white};for(int j=0;j<2;j++){ListView_SetExtendedListViewStyle(lists[j],LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);column(lists[j],0,j?L"白名单 · 豁免/优先":L"黑名单 · 必不选",px(u,165));column(lists[j],1,L"统率",px(u,55));column(lists[j],2,L"驻城/所在",px(u,145));column(lists[j],3,L"任命状态",px(u,90));}
  u->status=control(u,L"STATIC",L"",0,0);u->save=control(u,L"BUTTON",L"保存设置",BS_DEFPUSHBUTTON,Q_SAVE);u->close=control(u,L"BUTTON",L"稍后设置",0,Q_CLOSE);fonts(u);layout(u);SetTimer(u->window,1,200,NULL);
 }
 wcscpy(u->ini,ini);int valid=s14_domestic_settings_read(ini,&u->options);u->corrupt=!valid;S14DOptions session;if(s14_domestic_preferences_read(&session)){memcpy(u->options.locked,session.locked,sizeof(session.locked));memcpy(u->options.excluded,session.excluded,sizeof(session.excluded));}u->epoch=s14_battle_session_epoch();u->request=0;u->pending=0;memset(u->data,0,sizeof(*u->data));u->updating=1;
 SendMessageW(u->policy,CB_SETCURSEL,u->options.policy==S14_D_COMMERCIAL_FIRST||u->options.policy==S14_D_COMMERCIAL?1:u->options.policy==S14_D_AGRICULTURAL_FIRST||u->options.policy==S14_D_AGRICULTURAL?2:0,0);Button_SetCheck(u->security,u->options.security_first?BST_CHECKED:BST_UNCHECKED);Button_SetCheck(u->threshold_on,u->options.leadership_enabled?BST_CHECKED:BST_UNCHECKED);Button_SetCheck(u->auto_fill,(s14_config_read(ini)&S14_AUTO_DOMESTIC)?BST_CHECKED:BST_UNCHECKED);wchar_t num[16];swprintf(num,16,L"%d",u->options.leadership_min);SetWindowTextW(u->threshold,num);SetWindowTextW(u->search,L"");ListView_DeleteAllItems(u->black);ListView_DeleteAllItems(u->white);u->updating=0;
 ShowWindow(u->window,SW_SHOW);SetForegroundWindow(u->window);SetFocus(u->policy);request(u);if(!valid)SetWindowTextW(u->status,L"保存的快速设置损坏，自动内政已暂停；请重新设置并保存。");return 1;
}
void s14_domestic_quick_destroy(S14DomesticQuickUI *u){if(!u)return;if(u->window)DestroyWindow(u->window);if(u->font)DeleteObject(u->font);free(u->data);memset(u,0,sizeof(*u));}
