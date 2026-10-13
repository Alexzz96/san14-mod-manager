#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "domestic_ui.h"
#include "battle_probe.h"
#include "features.h"
enum {CITY=710,AREAS,PEOPLE,APPLY,REFRESH,FILL,OPTIMIZE,LOCK,CLOSE,POLICY,SECURITY,AUTO,QUICK};
static int px(S14DomesticUI *u,int n){return MulDiv(n,u->dpi,96);}
static void place(S14DomesticUI *u,HWND h,int x,int y,int w,int height){MoveWindow(h,px(u,x),px(u,y),px(u,w),px(u,height),TRUE);}
static void layout(S14DomesticUI *u){
 RECT r;GetClientRect(u->window,&r);int w=MulDiv(r.right,96,u->dpi),h=MulDiv(r.bottom,96,u->dpi),side=280;
 place(u,u->city,20,20,220,280);place(u,u->fill,256,18,130,34);place(u,u->optimize,398,18,130,34);place(u,u->refresh,540,18,150,34);
 place(u,u->quick_button,706,18,170,34);
 place(u,u->policy,20,62,260,220);place(u,u->security_first,300,62,280,30);place(u,u->forecast_note,590,65,w-610,40);
 place(u,u->auto_fill,20,106,350,30);place(u,u->auto_status,390,108,w-410,30);
 place(u,u->list,20,144,w-side-52,h-422);place(u,u->people,w-side-20,144,side,h-422);
 place(u,u->details,20,h-260,w-40,106);
 place(u,u->lock_button,20,h-150,230,34);place(u,u->summary,270,h-145,w-290,30);
 place(u,u->outcome,20,h-111,w-40,25);place(u,u->status,20,h-82,w-40,30);place(u,u->apply,w-300,h-46,160,34);place(u,u->close,w-124,h-46,104,34);
}
static void fonts(S14DomesticUI *u){if(u->font)DeleteObject(u->font);u->font=CreateFontW(-px(u,16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");HWND controls[]={u->city,u->list,u->people,u->summary,u->status,u->apply,u->refresh,u->fill,u->optimize,u->lock_button,u->close,u->policy,u->security_first,u->forecast_note,u->details,u->auto_fill,u->auto_status,u->outcome,u->quick_button};for(size_t i=0;i<sizeof(controls)/sizeof(controls[0]);i++)SendMessageW(controls[i],WM_SETFONT,(WPARAM)u->font,TRUE);}
static void detail(S14DomesticUI *u,int row,int person){
 wchar_t text[4096]=L"",line[512];S14DSnapshot *s=&u->data->snapshot;
 if(row>=0&&row<u->data->plan.count){S14DRow *r=&u->data->plan.rows[row];S14DArea *a=&s->areas[r->area];S14DCity *c=&s->cities[r->city];person=r->after;
 const wchar_t *directions[]={L"未设置",L"商业",L"农业",L"兵营"};
 swprintf(text,4096,L"%ls · %ls　%ls → %ls；每旬开发 +%d → +%d；治安 %d → %d（原方案 %d）。\r\n驻城个性：%ls（驻城人员之间取最高；人物个性按原生计算，保护职务也可提供效果）\r\n",c->name,a->name,directions[a->project],directions[r->project],r->before_gain,r->after_gain,a->security,a->security+r->after_security,a->security+r->before_security,c->aura_description[0]?c->aura_description:L"无");
 }
 if(person>0&&person<6001){for(int j=0;j<9;j++){int id=s->people[person].traits[j];if(id<1||id>=356||!s->traits[id].domestic)continue;int duplicate=0;for(int k=0;k<j;k++)duplicate|=s->people[person].traits[k]==id;if(duplicate)continue;swprintf(line,512,L"%ls：%ls\r\n",s->traits[id].name,s->traits[id].description);if(wcslen(text)+wcslen(line)<4095)wcscat(text,line);}}
 if(!text[0])wcscpy(text,L"选择区域或武将查看内政个性。治安优先先最大化未满100区域的恢复，再优化每旬开发；关闭后开发优先。\r\n顺序预设在开发值达到1000后切换；黑名单与统率筛选应用于候选，白名单优先。锁定和原生受保护任命保留。预测按当前相邻任命与驻留状态，扣除1000开发/100治安上限。\r\n需足额开发金才能兑现增量；转任关系与下旬事件可能改变结果。驻军个性只作说明，不当作县长加成；开发金费用不代表收入。");
 SetWindowTextW(u->details,text);
}
static void summary(S14DomesticUI *u){
 int count=0,cost=0,left=0;int budget=s14_domestic_budget(&u->data->snapshot,&u->data->plan,&count,&cost,&left);wchar_t text[192],why[256];int available=s14_domestic_can_apply(why,256);
 int before=0,after=0,order=0;for(int i=0;i<u->data->plan.count;i++){S14DRow *r=&u->data->plan.rows[i];before+=r->before_gain;after+=r->selected||r->completed?r->after_gain:r->before_gain;order+=r->selected||r->completed?r->after_security:r->before_security;}
 swprintf(text,192,L"待调整 %d / 命令书费用 %d / 剩余 %d　每旬开发 +%d → +%d　治安合计 +%d",count,cost,left,before,after,order);SetWindowTextW(u->summary,text);
 EnableWindow(u->apply,!u->pending&&available&&budget&&(u->data->status==S14_D_PREVIEW||u->data->status==S14_D_STALE));
 if(u->pending)SetWindowTextW(u->status,L"等待游戏线程处理…");
 else if(u->data->status!=S14_D_PREVIEW&&u->data->status!=S14_D_STALE)SetWindowTextW(u->status,u->data->message);
 else if(!count){int empty=0,protected_count=0,locked_count=0;for(int i=0;i<u->data->plan.count;i++){int reason=u->data->plan.rows[i].reason;empty+=reason==S14_D_NO_CANDIDATE;protected_count+=reason==S14_D_PROTECTED;locked_count+=reason==S14_D_LOCKED;}
 swprintf(text,192,L"没有选中的实际变更：无合格候选 %d，受保护 %d，锁定 %d；其余任命已保留或调整已取消。",empty,protected_count,locked_count);SetWindowTextW(u->status,text);}
 else if(!budget)SetWindowTextW(u->status,u->data->status==S14_D_STALE?L"预览已过期，已重新生成；命令书不足，应用禁用。取消部分关联调整后可减少费用。":L"命令书不足，应用已禁用；取消部分关联调整后可减少费用。");
 else if(u->data->status==S14_D_STALE)SetWindowTextW(u->status,u->data->message);
 else SetWindowTextW(u->status,available?L"点击统一应用执行原生任命并扣除命令书；关联行整组选择。结果在本窗口显示。":why);
 EnableWindow(u->city,!u->pending);EnableWindow(u->fill,!u->pending);EnableWindow(u->optimize,!u->pending);EnableWindow(u->refresh,!u->pending);EnableWindow(u->list,!u->pending);EnableWindow(u->people,!u->pending);EnableWindow(u->lock_button,!u->pending);EnableWindow(u->policy,!u->pending);EnableWindow(u->security_first,!u->pending);
}
static void cell(HWND list,int row,int col,const wchar_t *text){LVITEMW item={.iSubItem=col,.pszText=(wchar_t*)text};SendMessageW(list,LVM_SETITEMTEXTW,row,(LPARAM)&item);}
static void render(S14DomesticUI *u,int repopulate){
 u->updating=1;SendMessageW(u->list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(u->list);
 const wchar_t *projects[]={L"未设置",L"商业 / 智力",L"农业 / 政治",L"兵营 / 武力"};const wchar_t *reasons[]={L"保留",L"已锁定",L"保护职务/状态/排除",L"无合格同城候选",L"调整"};
 for(int i=0;i<u->data->plan.count;i++){
  const S14DRow *r=&u->data->plan.rows[i];S14DSnapshot *s=&u->data->snapshot;LVITEMW item={.mask=LVIF_TEXT|LVIF_PARAM,.iItem=i,.pszText=s->cities[r->city].name,.lParam=r->area};ListView_InsertItem(u->list,&item);
  cell(u->list,i,1,s->areas[r->area].name);cell(u->list,i,2,r->before?s->people[r->before].name:L"空缺");cell(u->list,i,3,r->after?s->people[r->after].name:L"空缺");
  wchar_t text[256];int old_project=s->areas[r->area].project;if(old_project&&old_project!=r->project)swprintf(text,256,L"%ls → %ls",projects[old_project],projects[r->project]);else swprintf(text,256,L"%ls%ls",projects[r->project],r->first?L" · 首次设置":L"");cell(u->list,i,4,text);
  if(s->forecast_ready){swprintf(text,256,L"+%d → +%d (%+d)",r->before_gain,r->after_gain,r->after_gain-r->before_gain);cell(u->list,i,5,text);const S14DForecast *f=s14_domestic_forecast(s,r->area,r->after);swprintf(text,256,L"商 +%d / 农 +%d / 兵 +%d",f?f->development[0]:0,f?f->development[1]:0,f?f->development[2]:0);cell(u->list,i,7,text);
  S14DArea *a=&s->areas[r->area];swprintf(text,256,L"%d / %d / %d",a->development[0],a->development[1],a->development[2]);cell(u->list,i,8,text);swprintf(text,256,L"%d → %d（原 %d）",a->security,a->security+r->after_security,a->security+r->before_security);cell(u->list,i,6,text);
  const S14DForecast *old=s14_domestic_forecast(s,r->area,r->before);swprintf(text,256,L"%d → %d 金",old?old->gold_cost:0,f?f->gold_cost:0);cell(u->list,i,10,text);
  }else {cell(u->list,i,5,L"未核验");cell(u->list,i,6,L"未核验");cell(u->list,i,7,L"未核验");cell(u->list,i,8,L"未核验");cell(u->list,i,10,L"未核验");}
  s14_domestic_traits_text(s,r->after,text,256);cell(u->list,i,9,text[0]?text:L"无相关个性");swprintf(text,256,L"%d → %d",r->before_value,r->after_value);cell(u->list,i,11,text);
  swprintf(text,256,L"%d",r->reason==S14_D_CHANGE?s->unit_cost:0);cell(u->list,i,12,text);
  if(r->completed)cell(u->list,i,13,L"已完成");else if(r->reason==S14_D_CHANGE){swprintf(text,256,L"关联组 %d",r->group+1);cell(u->list,i,13,text);}else cell(u->list,i,13,reasons[r->reason]);
  ListView_SetCheckState(u->list,i,r->selected&&!r->completed);
 }
 SendMessageW(u->list,WM_SETREDRAW,TRUE,0);InvalidateRect(u->list,NULL,TRUE);
 if(repopulate){
  SendMessageW(u->city,CB_RESETCONTENT,0,0);SendMessageW(u->city,CB_ADDSTRING,0,(LPARAM)L"全部玩家直辖城市");SendMessageW(u->city,CB_SETITEMDATA,0,0);int selected=0;
  for(int c=1;c<52;c++)if(u->data->snapshot.cities[c].active){int i=(int)SendMessageW(u->city,CB_ADDSTRING,0,(LPARAM)u->data->snapshot.cities[c].name);SendMessageW(u->city,CB_SETITEMDATA,i,c);if(u->options.city==c)selected=i;}
  SendMessageW(u->city,CB_SETCURSEL,selected,0);if(!selected)u->options.city=0;
 }
 SendMessageW(u->people,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(u->people);int row=0;
 for(int id=1;id<6001;id++){S14DPerson *p=&u->data->snapshot.people[id];if(!p->active||(u->options.city&&p->city!=u->options.city))continue;wchar_t text[128];swprintf(text,128,L"%ls (#%d)%ls",p->name,id,p->protected_job||p->busy?L" · 受保护":L"");LVITEMW item={.mask=LVIF_TEXT|LVIF_PARAM,.iItem=row,.pszText=text,.lParam=id};ListView_InsertItem(u->people,&item);ListView_SetCheckState(u->people,row,u->options.excluded[id]);row++;}
 SendMessageW(u->people,WM_SETREDRAW,TRUE,0);InvalidateRect(u->people,NULL,TRUE);SendMessageW(u->policy,CB_SETCURSEL,u->options.policy-1,0);SendMessageW(u->security_first,BM_SETCHECK,u->options.security_first?BST_CHECKED:BST_UNCHECKED,0);u->updating=0;detail(u,-1,0);summary(u);
}
static void rebuild(S14DomesticUI *u){s14_domestic_preferences(&u->options);if(!u->data->snapshot.world||(u->data->status!=S14_D_PREVIEW&&u->data->status!=S14_D_STALE))return;s14_domestic_plan(&u->data->snapshot,&u->options,&u->data->plan);render(u,0);}
static void request(S14DomesticUI *u){if(u->pending)return;s14_domestic_preferences(&u->options);uint64_t id=s14_domestic_request(&u->options);if(id){u->request=id;u->pending=1;u->refresh_needed=0;u->revision=s14_domestic_revision();u->data->plan.valid=0;summary(u);}else {u->refresh_needed=1;u->data->plan.valid=0;summary(u);SetWindowTextW(u->status,L"已有请求正在处理，稍后自动刷新。");}}
static void poll(S14DomesticUI *u){
 wchar_t auto_text[192];s14_domestic_auto_status(auto_text,192);SetWindowTextW(u->auto_status,auto_text);SendMessageW(u->auto_fill,BM_SETCHECK,u->ini[0]&&(s14_config_read(u->ini)&S14_AUTO_DOMESTIC)?BST_CHECKED:BST_UNCHECKED,0);EnableWindow(u->auto_fill,u->ini[0]!=0);
 unsigned epoch=s14_battle_session_epoch();if(epoch!=u->epoch||s14_battle_is_loading()){
  s14_domestic_cancel();int mode=u->options.mode;memset(&u->options,0,sizeof(u->options));s14_domestic_settings_read(u->ini,&u->options);u->options.mode=mode;u->epoch=epoch;u->request=u->preview=0;u->pending=u->refresh_needed=0;u->revision=s14_domestic_revision();SetWindowTextW(u->outcome,L"");memset(u->data,0,sizeof(*u->data));u->data->status=S14_D_CANCELLED;wcscpy(u->data->message,L"存档会话变化，筛选、锁定和排除已清空；请回到战略地图后刷新。");render(u,1);return;
 }
 if(!u->pending&&(u->refresh_needed||u->revision!=s14_domestic_revision())&&IsWindowVisible(u->window)){if(!u->refresh_needed)SetWindowTextW(u->outcome,L"内政任命已变化，正在刷新原生快照与规划。");request(u);}
 if(!u->request||!u->pending||!s14_domestic_result(u->request,u->poll)||u->poll->status==S14_D_PENDING)return;
 *u->data=*u->poll;u->pending=0;u->preview=u->data->preview;u->revision=s14_domestic_revision();render(u,1);
 if(u->data->status==S14_D_APPLIED){SetWindowTextW(u->outcome,u->data->message);request(u);}
 else if(u->data->status==S14_D_FAILED)SetWindowTextW(u->outcome,u->data->message);
 if(!u->pending&&(u->data->snapshot.epoch!=u->epoch||(u->data->status==S14_D_STALE&&!u->data->plan.valid))){u->refresh_needed=1;u->data->plan.valid=0;summary(u);}
 if(GetForegroundWindow()==u->window&&!IsChild(u->window,GetFocus()))SetFocus(u->close);
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 S14DomesticUI *u=(S14DomesticUI*)GetWindowLongPtrW(w,GWLP_USERDATA);if(msg==WM_NCCREATE){u=((CREATESTRUCTW*)lp)->lpCreateParams;u->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)u);}if(!u)return DefWindowProcW(w,msg,wp,lp);
 if(msg==WM_SIZE){layout(u);return 0;}
 if(msg==WM_GETMINMAXINFO){MINMAXINFO *m=(MINMAXINFO*)lp;m->ptMinTrackSize=(POINT){px(u,1050),px(u,620)};return 0;}
 if(msg==WM_DPICHANGED){int before=u->dpi;u->dpi=HIWORD(wp);for(int i=0;i<14;i++)ListView_SetColumnWidth(u->list,i,MulDiv(ListView_GetColumnWidth(u->list,i),u->dpi,before));ListView_SetColumnWidth(u->people,0,MulDiv(ListView_GetColumnWidth(u->people,0),u->dpi,before));RECT *r=(RECT*)lp;fonts(u);SetWindowPos(w,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);layout(u);return 0;}
 if(msg==WM_TIMER){poll(u);return 0;}
 if(msg==WM_CLOSE){s14_domestic_cancel();u->pending=u->refresh_needed=0;int focused=GetForegroundWindow()==w;ShowWindow(w,SW_HIDE);HWND owner=GetWindow(w,GW_OWNER);if(owner&&focused)SetForegroundWindow(owner);return 0;}
 if(msg==WM_KEYDOWN&&wp==VK_ESCAPE){SendMessageW(w,WM_CLOSE,0,0);return 0;}
 if(msg==WM_COMMAND){int id=LOWORD(wp);if(id==CLOSE||id==IDCANCEL){SendMessageW(w,WM_CLOSE,0,0);return 0;}if(u->pending)return 0;
  if(id==AUTO&&u->ini[0]){int on=SendMessageW(u->auto_fill,BM_GETCHECK,0,0)==BST_CHECKED;if(!s14_config_set(u->ini,S14_AUTO_DOMESTIC,on))SetWindowTextW(u->outcome,L"自动补齐设置保存失败，请检查配置文件权限。");else{s14_domestic_preferences(&u->options);s14_domestic_auto_configure(on&&!!(s14_effective_flags(s14_config_read(u->ini))&S14_MASTER));SetWindowTextW(u->outcome,on?L"自动补齐全直辖城市；沿用当前开发模式、治安优先、锁定和排除。":L"自动补齐已关闭；仍可手动规划与应用。");}return 0;}
  if(id==FILL||id==OPTIMIZE){u->options.mode=id==FILL?S14_D_FILL:S14_D_OPTIMIZE;SetWindowTextW(w,id==FILL?L"区域官员配置 · 补齐空缺 · 原生任命 · 实机验收中":L"区域官员配置 · 重新优化 · 原生任命 · 实机验收中");request(u);}
  if(id==REFRESH)request(u);
  if(id==CITY&&HIWORD(wp)==CBN_SELCHANGE){int i=(int)SendMessageW(u->city,CB_GETCURSEL,0,0);u->options.city=(int)SendMessageW(u->city,CB_GETITEMDATA,i,0);request(u);}
  if(id==QUICK){SendMessageW(w,WM_CLOSE,0,0);s14_domestic_quick_show(&u->quick,u->instance,GetWindow(w,GW_OWNER),u->ini);return 0;}
  if(id==POLICY&&HIWORD(wp)==CBN_SELCHANGE){u->options.policy=1+(int)SendMessageW(u->policy,CB_GETCURSEL,0,0);if(u->ini[0])s14_domestic_settings_write(u->ini,&u->options);rebuild(u);}
  if(id==SECURITY){u->options.security_first=SendMessageW(u->security_first,BM_GETCHECK,0,0)==BST_CHECKED;if(u->ini[0])s14_domestic_settings_write(u->ini,&u->options);rebuild(u);}
  if(id==LOCK){int i=ListView_GetNextItem(u->list,-1,LVNI_SELECTED);if(i>=0&&i<u->data->plan.count){int area=u->data->plan.rows[i].area;u->options.locked[area]=!u->options.locked[area];rebuild(u);}}
  if(id==APPLY&&IsWindowEnabled(u->apply)){uint64_t id=s14_domestic_submit(u->preview,&u->options,&u->data->plan);if(id){u->request=id;u->pending=1;summary(u);}else SetWindowTextW(u->status,L"提交未接受，未执行；请刷新预览。");}return 0;
 }
 if(msg==WM_NOTIFY&&!u->updating&&!u->pending){NMHDR *h=(NMHDR*)lp;if(h->code==LVN_ITEMCHANGED){NMLISTVIEW *n=(NMLISTVIEW*)lp;if(n->iItem>=0&&(n->uNewState&LVIS_SELECTED)){if(h->hwndFrom==u->list)detail(u,n->iItem,0);else if(h->hwndFrom==u->people){LVITEMW item={.mask=LVIF_PARAM,.iItem=n->iItem};if(ListView_GetItem(u->people,&item))detail(u,-1,(int)item.lParam);}}if(n->iItem>=0&&(n->uChanged&LVIF_STATE)&&((n->uOldState^n->uNewState)&LVIS_STATEIMAGEMASK)){
   int checked=((n->uNewState&LVIS_STATEIMAGEMASK)>>12)==2;
   if(h->hwndFrom==u->list&&n->iItem<u->data->plan.count){s14_domestic_select(&u->data->plan,n->iItem,checked);u->updating=1;for(int i=0;i<u->data->plan.count;i++)ListView_SetCheckState(u->list,i,u->data->plan.rows[i].selected&&!u->data->plan.rows[i].completed);u->updating=0;summary(u);}
   if(h->hwndFrom==u->people){LVITEMW item={.mask=LVIF_PARAM,.iItem=n->iItem};if(ListView_GetItem(u->people,&item)&&item.lParam>0&&item.lParam<6001){u->options.excluded[item.lParam]=checked;rebuild(u);}}
  }}return 0;
 }
 return DefWindowProcW(w,msg,wp,lp);
}
static HWND control(S14DomesticUI *u,const wchar_t *class,const wchar_t *text,DWORD style,int id){return CreateWindowExW(0,class,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,1,1,u->window,(HMENU)(INT_PTR)id,u->instance,NULL);}
int s14_domestic_ui_show(S14DomesticUI *u,HINSTANCE instance,HWND owner,int mode){
 if(!u->window){u->instance=instance;u->dpi=96;u->options.policy=S14_D_MAX_DEVELOPMENT;u->options.security_first=1;HDC dc=GetDC(owner);if(dc){u->dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(owner,dc);}u->data=calloc(1,sizeof(*u->data));u->poll=calloc(1,sizeof(*u->poll));if(!u->data||!u->poll){s14_domestic_ui_destroy(u);return 0;}
  INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&common);WNDCLASSW wc={.lpfnWndProc=proc,.hInstance=instance,.hCursor=LoadCursorW(NULL,IDC_ARROW),.hbrBackground=(HBRUSH)(COLOR_WINDOW+1),.lpszClassName=L"S14.Domestic.Preview"};RegisterClassW(&wc);
  if(!CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_TOOLWINDOW,wc.lpszClassName,L"区域官员配置 · 测试预览",WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,CW_USEDEFAULT,px(u,1240),px(u,760),owner,NULL,instance,u)){s14_domestic_ui_destroy(u);return 0;}
  u->city=control(u,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,CITY);u->fill=control(u,L"BUTTON",L"补齐空缺",0,FILL);u->optimize=control(u,L"BUTTON",L"重新优化",0,OPTIMIZE);u->refresh=control(u,L"BUTTON",L"刷新原生快照",0,REFRESH);
  u->quick_button=control(u,L"BUTTON",L"快速设置 / 名单",0,QUICK);SendMessageW(u->quick_button,WM_SETFONT,(WPARAM)u->font,TRUE);
  u->policy=control(u,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,POLICY);const wchar_t *policies[]={L"全数值最高（预计净增）",L"全部商业（固定）",L"全部农业（固定）",L"商业满级 → 农业 → 兵营",L"农业满级 → 商业 → 兵营"};for(int i=0;i<5;i++)SendMessageW(u->policy,CB_ADDSTRING,0,(LPARAM)policies[i]);SendMessageW(u->policy,CB_SETCURSEL,0,0);
  u->security_first=control(u,L"BUTTON",L"治安未满100优先恢复",BS_AUTOCHECKBOX,SECURITY);SendMessageW(u->security_first,BM_SETCHECK,BST_CHECKED,0);u->forecast_note=control(u,L"STATIC",L"预计每旬净增，需足额开发金；关系按当前任命，已考虑上限。",0,0);u->details=control(u,L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,0);
  u->auto_fill=control(u,L"BUTTON",L"回合自动补齐（先内政，后探索）",BS_AUTOCHECKBOX,AUTO);u->auto_status=control(u,L"STATIC",L"",0,0);u->outcome=control(u,L"STATIC",L"",0,0);u->revision=s14_domestic_revision();
  u->list=control(u,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,AREAS);u->people=control(u,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,PEOPLE);
  ListView_SetExtendedListViewStyle(u->list,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);ListView_SetExtendedListViewStyle(u->people,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
  const wchar_t *columns[]={L"城市",L"区域",L"原官员",L"拟任官员",L"拟开发方向",L"每旬开发净增",L"治安预测",L"拟任三项净增",L"当前商/农/兵",L"相关内政个性",L"每旬开发金费用",L"开发有效能力",L"命令书",L"状态 / 关联组"};int widths[]={80,90,100,100,150,160,175,215,145,185,145,135,75,190};for(int i=0;i<14;i++){LVCOLUMNW col={.mask=LVCF_TEXT|LVCF_WIDTH,.pszText=(wchar_t*)columns[i],.cx=px(u,widths[i])};ListView_InsertColumn(u->list,i,&col);}
  LVCOLUMNW col={.mask=LVCF_TEXT|LVCF_WIDTH,.pszText=L"勾选排除武将（保护现职）",.cx=px(u,270)};ListView_InsertColumn(u->people,0,&col);
  u->lock_button=control(u,L"BUTTON",L"锁定 / 解锁选中区域",0,LOCK);u->summary=control(u,L"STATIC",L"",0,0);u->status=control(u,L"STATIC",L"",0,0);u->apply=control(u,L"BUTTON",L"统一应用",BS_DEFPUSHBUTTON,APPLY);u->close=control(u,L"BUTTON",L"关闭",0,CLOSE);fonts(u);layout(u);SetTimer(u->window,1,200,NULL);
 }
 if(u->epoch!=s14_battle_session_epoch()){memset(&u->options,0,sizeof(u->options));u->options.policy=S14_D_MAX_DEVELOPMENT;u->options.security_first=1;u->epoch=s14_battle_session_epoch();}
 if(!u->pending){S14DOptions saved;if(s14_domestic_settings_read(u->ini,&saved)){saved.mode=mode;saved.city=u->options.city;memcpy(saved.locked,u->options.locked,sizeof(saved.locked));memcpy(saved.excluded,u->options.excluded,sizeof(saved.excluded));u->options=saved;}u->options.mode=mode;}SetWindowTextW(u->window,u->options.mode==S14_D_OPTIMIZE?L"区域官员配置 · 重新优化 · 原生任命 · 实机验收中":L"区域官员配置 · 补齐空缺 · 原生任命 · 实机验收中");ShowWindow(u->window,SW_SHOW);SetForegroundWindow(u->window);SetFocus(u->city);request(u);return 1;
}
void s14_domestic_ui_destroy(S14DomesticUI *u){s14_domestic_quick_destroy(&u->quick);s14_domestic_cancel();if(u->window)DestroyWindow(u->window);if(u->font)DeleteObject(u->font);free(u->data);free(u->poll);memset(u,0,sizeof(*u));}
