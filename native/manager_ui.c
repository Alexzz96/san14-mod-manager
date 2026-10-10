#define WIN32_LEAN_AND_MEAN
#include <windowsx.h>
#include <wchar.h>
#include "manager_ui.h"
#include "ai_affix.h"
#include "package.h"
#include "theme.h"
#include "career_affix.h"

static const wchar_t manager_class[]=L"S14ModManager.Panel.v3";
static int px(S14ManagerUI *ui,int v){return MulDiv(v,ui->scale,96);}
static RECT box(S14ManagerUI *ui,int x,int y,int w,int h){return (RECT){px(ui,x),px(ui,y),px(ui,x+w),px(ui,y+h)};}
static void fill(HDC dc,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);}
static void rounded(HDC dc,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);HPEN p=CreatePen(PS_SOLID,1,S14_BORDER);HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,p);RoundRect(dc,r.left,r.top,r.right,r.bottom,12,12);SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(b);DeleteObject(p);}
static void expanded_frame(S14ManagerUI *ui,HDC dc,int y,int height){
    RECT r=box(ui,28,y,804,height-4);InflateRect(&r,-px(ui,1),-px(ui,1));
    HPEN edge=CreatePen(PS_SOLID,px(ui,2),S14_ACCENT),divider=CreatePen(PS_SOLID,px(ui,1),S14_BORDER);
    HGDIOBJ old_pen=SelectObject(dc,edge),old_brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    RoundRect(dc,r.left,r.top,r.right,r.bottom,px(ui,12),px(ui,12));
    SelectObject(dc,divider);MoveToEx(dc,px(ui,46),px(ui,y+72),NULL);LineTo(dc,px(ui,814),px(ui,y+72));
    SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(edge);DeleteObject(divider);
}
static void label(S14ManagerUI *ui,HDC dc,const wchar_t *s,RECT r,HFONT f,COLORREF c,UINT flags){(void)ui;HGDIOBJ old=SelectObject(dc,f);SetTextColor(dc,c);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s,-1,&r,flags|DT_NOPREFIX);SelectObject(dc,old);}
static void focus(S14ManagerUI *ui,HDC dc,RECT r,int id){if(ui->focus==id){HBRUSH b=CreateSolidBrush(S14_ACCENT);FrameRect(dc,&r,b);DeleteObject(b);}}
static void button(S14ManagerUI *ui,HDC dc,const wchar_t *s,int x,int y,int w,int id,int enabled){RECT r=box(ui,x,y,w,40);int primary=id==31;rounded(dc,r,enabled?(primary?S14_ACCENT:S14_CARD):RGB(238,236,230));focus(ui,dc,r,id);label(ui,dc,s,r,ui->body_font,enabled?(primary?RGB(255,252,248):id==34?S14_ERROR:S14_INK):RGB(137,133,124),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
static void toggle(S14ManagerUI *ui,HDC dc,int y,int enabled,int active,int id){rounded(dc,box(ui,744,y,64,30),enabled&&active?S14_ACCENT:RGB(193,189,179));rounded(dc,box(ui,enabled?779:748,y+4,22,22),RGB(255,254,251));focus(ui,dc,box(ui,739,y-5,74,40),id);}
int s14_manager_mod_enabled(const S14ManagerUI *ui,int mod){switch(mod){case S14_MOD_WALL:return !!(ui->requested&S14_WALL_LIMIT);case S14_MOD_SEARCH:return !!(ui->requested&S14_AUTO_SEARCH);case S14_MOD_BATTLE:return ui->battle_enabled;case S14_MOD_VIEWS:return ui->views_enabled;case S14_MOD_CAO_REN_BUFF:return !!(ui->requested&S14_CAO_REN_BUFF);case S14_MOD_DIAGNOSTICS:return !!(ui->requested&S14_DIAGNOSTICS);case S14_MOD_AI_AFFIX:return !!(ui->requested&S14_AI_RANDOM_AFFIX);case S14_MOD_TROOPS:return !!(ui->requested&S14_PLUGIN_TROOPS);default:return 0;}}
int s14_manager_view_enabled(const S14ManagerUI *ui,int native){return ui->views_enabled && (native==2?ui->native_army_enabled:native?ui->native_stats_enabled:ui->officers_enabled);}
int s14_manager_mod_index(const S14ManagerUI *ui,int position){if(position<0 || position>=S14_MOD_COUNT)return -1;int indices[S14_MOD_COUNT];for(int i=0;i<S14_MOD_COUNT;i++)indices[i]=i;for(int i=1;i<S14_MOD_COUNT;i++){int v=indices[i],j=i;while(j>0){int a=indices[j-1],cmp=0;if(ui->mod_sort==1)cmp=wcscmp(s14_mods[a].name,s14_mods[v].name);if(ui->mod_sort==2)cmp=s14_manager_mod_enabled(ui,v)-s14_manager_mod_enabled(ui,a);if(cmp<=0)break;indices[j]=a;j--;}indices[j]=v;}return indices[position];}
static int mod_height(const S14ManagerUI *ui,int mod){if(!(ui->expanded&(1u<<mod)))return 76;return 76+(mod==S14_MOD_WALL?70:mod==S14_MOD_SEARCH?304:mod==S14_MOD_VIEWS?210:(mod==S14_MOD_CAO_REN_BUFF || mod==S14_MOD_AI_AFFIX)?184:0);}
int s14_manager_mod_top(const S14ManagerUI *ui,int mod){int y=S14_MOD_LIST_TOP-ui->scroll;for(int i=0;i<S14_MOD_COUNT;i++){int at=s14_manager_mod_index(ui,i);if(at==mod)return y;y+=mod_height(ui,at);}return -1;}
static int content_height(const S14ManagerUI *ui){int height=0;for(int i=0;i<S14_MOD_COUNT;i++)height+=mod_height(ui,i);return height;}
static int max_scroll(const S14ManagerUI *ui){int n=content_height(ui)-(S14_MOD_LIST_BOTTOM-S14_MOD_LIST_TOP);return n>0?n:0;}
static int mod_active(const S14ManagerUI *ui,int mod){if(!s14_manager_mod_enabled(ui,mod))return 0;if(mod==S14_MOD_VIEWS)return ui->officers_enabled || ui->native_stats_enabled || ui->native_army_enabled;if(mod==S14_MOD_BATTLE)return !!(ui->effective&S14_MASTER);unsigned int flags[]={S14_WALL_LIMIT,S14_AUTO_SEARCH,0,0,S14_CAO_REN_BUFF,S14_DIAGNOSTICS,S14_PLUGIN_TROOPS,S14_AI_RANDOM_AFFIX};return !!(ui->effective&flags[mod]);}
const wchar_t *s14_manager_mod_status(const S14ManagerUI *ui,int mod) {
 if(!s14_manager_mod_enabled(ui,mod))return L"已关闭";
 if(!(ui->requested&S14_MASTER))return L"总开关关闭";
 if(mod==S14_MOD_AI_AFFIX && s14_ai_faulted())return L"异常停用";
 if(mod==S14_MOD_SEARCH){
  if(!ui->attached)return L"等待接入";
  if(ui->search_state==S14_SEARCH_STOPPED || ui->fault)return L"异常停用";
  if(ui->search_state==S14_SEARCH_UNAVAILABLE)return L"入口不兼容";
  if(ui->search_state==S14_SEARCH_CONTEXT_PAUSED)return L"等待匹配";
  if(ui->search_state==S14_SEARCH_WAITING)return L"等待载入";
 }
 if(mod_active(ui,mod))return L"已开启";
 return mod==S14_MOD_VIEWS?L"子功能全关":L"等待接入";
}
static void child(S14ManagerUI *ui,HDC dc,int y,const wchar_t *name,const wchar_t *description,int checked,int active,int id){fill(dc,box(ui,56,y,772,66),S14_CARD);fill(dc,box(ui,72,y+9,2,48),S14_BORDER);label(ui,dc,name,box(ui,88,y+9,580,25),ui->body_font,S14_INK,DT_SINGLELINE);label(ui,dc,description,box(ui,88,y+38,630,23),ui->small_font,S14_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);toggle(ui,dc,y+14,checked,active,id);}
static void search_options(S14ManagerUI *ui,HDC dc,int top){const wchar_t *titles[]={L"执行武将",L"返回天数",L"重视项目"};const wchar_t *options[3][4]={{L"全武将",L"各官员以外",L"县府官员以外",L"城市官员以外"},{L"10 天以内",L"20 天以内",L"无限制",NULL},{L"优先军师推荐",L"优先能力",NULL,NULL}};
 for(int g=0;g<3;g++){int y=top+g*62,n=g==0?4:g==1?3:2,w=736/n;label(ui,dc,titles[g],box(ui,72,y+2,700,22),ui->small_font,S14_MUTED,DT_SINGLELINE);for(int i=0;i<n;i++){int id=200+10*g+i,selected=(int)((ui->search_settings>>(g*4))&15)==i;RECT r=box(ui,72+i*w,y+27,w-8,29);rounded(dc,r,selected?S14_ACCENT_SOFT:S14_CARD);label(ui,dc,options[g][i],r,ui->small_font,selected?S14_ACCENT:S14_INK,DT_CENTER|DT_SINGLELINE|DT_VCENTER);focus(ui,dc,r,id);}}
 label(ui,dc,ui->search_status[0]?ui->search_status:L"条件将用于下一次自动派遣；关闭 Mod 后仍保留选择。",box(ui,72,top+190,736,22),ui->small_font,S14_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
 label(ui,dc,ui->search_summary[0]?ui->search_summary:L"回合完成后统计探索结果，完整明细在游戏内查看。",box(ui,72,top+218,736,38),ui->small_font,S14_INK,DT_WORDBREAK);
 RECT r=box(ui,72,top+262,230,30);rounded(dc,r,ui->in_game?S14_ACCENT_SOFT:S14_PAPER);label(ui,dc,L"查看上一回合报告",r,ui->small_font,ui->in_game?S14_ACCENT:S14_MUTED,DT_CENTER|DT_VCENTER|DT_SINGLELINE);focus(ui,dc,r,42);label(ui,dc,L"探索与战斗分标签查阅",box(ui,326,top+266,470,24),ui->small_font,S14_MUTED,DT_SINGLELINE);
}
static void paint_mods(S14ManagerUI *ui,HDC dc){int enabled=0;for(int i=0;i<S14_MOD_COUNT;i++)enabled+=s14_manager_mod_enabled(ui,i);wchar_t summary[64];swprintf(summary,64,L"Mod 列表    ·    已选择开启 %d / %d",enabled,S14_MOD_COUNT);label(ui,dc,summary,box(ui,30,155,610,30),ui->body_font,S14_INK,DT_SINGLELINE|DT_VCENTER);const wchar_t *sorts[]={L"默认排序",L"名称排序",L"已开启优先"};button(ui,dc,sorts[ui->mod_sort%3],658,150,174,61,1);
 label(ui,dc,L"Mod / 子功能",box(ui,48,192,560,22),ui->small_font,S14_MUTED,DT_SINGLELINE);label(ui,dc,L"状态",box(ui,646,192,76,22),ui->small_font,S14_MUTED,DT_CENTER|DT_SINGLELINE);label(ui,dc,L"开关",box(ui,744,192,64,22),ui->small_font,S14_MUTED,DT_CENTER|DT_SINGLELINE);
 int saved=SaveDC(dc);IntersectClipRect(dc,px(ui,28),px(ui,S14_MOD_LIST_TOP),px(ui,832),px(ui,S14_MOD_LIST_BOTTOM));
 for(int i=0;i<S14_MOD_COUNT;i++){int mod=s14_manager_mod_index(ui,i),y=s14_manager_mod_top(ui,mod),h=mod_height(ui,mod);if(y+h<S14_MOD_LIST_TOP || y>S14_MOD_LIST_BOTTOM)continue;const S14Mod *m=&s14_mods[mod];rounded(dc,box(ui,28,y,804,h-4),S14_CARD);
  if(m->children){POINT points[3];int open=!!(ui->expanded&(1u<<mod));if(open){points[0]=(POINT){px(ui,48),px(ui,y+29)};points[1]=(POINT){px(ui,60),px(ui,y+29)};points[2]=(POINT){px(ui,54),px(ui,y+36)};}else{points[0]=(POINT){px(ui,50),px(ui,y+25)};points[1]=(POINT){px(ui,50),px(ui,y+38)};points[2]=(POINT){px(ui,58),px(ui,y+32)};}HBRUSH b=CreateSolidBrush(S14_MUTED);HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,GetStockObject(NULL_PEN));Polygon(dc,points,3);SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(b);focus(ui,dc,box(ui,40,y+14,30,40),80+mod);}
  label(ui,dc,m->name,box(ui,82,y+11,455,26),ui->body_font,S14_INK,DT_SINGLELINE);label(ui,dc,m->category,box(ui,558,y+14,64,22),ui->small_font,S14_ACCENT,DT_CENTER|DT_SINGLELINE);label(ui,dc,m->description,box(ui,82,y+43,637,23),ui->small_font,S14_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
  int checked=s14_manager_mod_enabled(ui,mod),active=mod_active(ui,mod);const wchar_t *state=s14_manager_mod_status(ui,mod);label(ui,dc,state,box(ui,632,y+18,96,25),ui->small_font,active?S14_SUCCESS:S14_MUTED,DT_CENTER|DT_SINGLELINE);toggle(ui,dc,y+20,checked,active,m->action);
  if(ui->expanded&(1u<<mod)){int top=y+76;if(mod==S14_MOD_WALL)child(ui,dc,top,L"超限悬浮提示",L"点击超限格说明规则，5 秒后隐藏；需开启墙体限制。",!!(ui->requested&S14_LIMIT_HINT),!!(ui->effective&S14_LIMIT_HINT),11);
   if(mod==S14_MOD_SEARCH)search_options(ui,dc,top);
   if(mod==S14_MOD_VIEWS){child(ui,dc,top,L"武将一览 · 字母 F",L"搜索、排序、累计战绩与时间线；只在打开时刷新。",ui->officers_enabled,s14_manager_view_enabled(ui,0),40);child(ui,dc,top+70,L"原生武将详情 · 战绩条",L"在原生详情上方显示战绩；鼠标可穿透。",ui->native_stats_enabled,s14_manager_view_enabled(ui,1),43);child(ui,dc,top+140,L"曹仁部队详情 · 数值解析",L"原生基础值、最终值及修正层；点击悬浮条查看明细。",ui->native_army_enabled,s14_manager_view_enabled(ui,2),46);}
   if(mod==S14_MOD_AI_AFFIX){child(ui,dc,top,L"地图头像 · 红色光圈",L"随本次部队生效，多部队支持；只在主地图显示。",!!(ui->visual_settings&4),active && !!(ui->visual_settings&4),49);child(ui,dc,top+70,L"悬浮面板 · 禁军精锐",L"城市每 9 次必出，其余 10%；攻军防御 +10%，同类取最高。",!!(ui->visual_settings&8),active && !!(ui->visual_settings&8),50);wchar_t progress[160];if(ui->in_game)s14_ai_status(progress,160);else wcscpy(progress,L"本地测试版默认关闭 · 已有部队不补抽");label(ui,dc,progress,box(ui,72,top+145,736,30),ui->small_font,S14_ACCENT,DT_SINGLELINE|DT_END_ELLIPSIS);}
   if(mod==S14_MOD_CAO_REN_BUFF){child(ui,dc,top,L"地图头像 · 紫色光圈",L"曹仁获得百战精锐后显示；鼠标可穿透。",!!(ui->visual_settings&1),active && !!(ui->visual_settings&1),47);child(ui,dc,top+70,L"悬浮面板 · 特殊效果",L"达标后展示百战精锐、神 曹仁及实际增幅。",!!(ui->visual_settings&2),active && !!(ui->visual_settings&2),48);wchar_t progress[192];if(ui->in_game)s14_affix_status(0,progress,192);else wcscpy(progress,L"载入游戏后查看进度 · 曹仁已记录斩敌达到 5000（含伤兵）解锁");label(ui,dc,progress,box(ui,72,top+145,736,30),ui->small_font,S14_ACCENT,DT_SINGLELINE|DT_END_ELLIPSIS);}
   if(m->children)expanded_frame(ui,dc,y,h);}
 }
 RestoreDC(dc,saved);
}
static void paint_settings(S14ManagerUI *ui,HDC dc){label(ui,dc,L"游戏目录",box(ui,30,160,140,24),ui->body_font,S14_INK,DT_SINGLELINE);if(ui->in_game)label(ui,dc,ui->root,box(ui,30,196,800,49),ui->small_font,S14_MUTED,DT_WORDBREAK);else{rounded(dc,box(ui,30,186,678,40),S14_CARD);if(!ui->directory_edit)label(ui,dc,ui->root,box(ui,40,198,658,24),ui->small_font,S14_INK,DT_SINGLELINE|DT_END_ELLIPSIS);button(ui,dc,L"选择目录",730,186,102,30,1);}
 label(ui,dc,ui->game_found?L"游戏文件：已找到 SAN14PK_SC.exe，不检查版本":L"游戏文件：当前目录未找到 SAN14PK_SC.exe",box(ui,30,254,802,28),ui->body_font,ui->game_found?S14_SUCCESS:S14_ACCENT,DT_SINGLELINE);
 const wchar_t *installation=ui->installed?L"插件文件：已识别本项目插件，可更新或卸载":ui->detected&(S14_FOUND_MANAGER|S14_FOUND_DATA)?L"插件文件：未安装，已检测到管理器或残留记录":L"插件文件：未检测到本项目安装";if(ui->detected&S14_FOUND_UNKNOWN)installation=L"插件文件：发现未知同名文件，清理时会保留";label(ui,dc,installation,box(ui,30,294,802,28),ui->body_font,S14_INK,DT_SINGLELINE);
 label(ui,dc,ui->running?L"游戏正在运行，安装和卸载需要先退出游戏。":L"游戏已关闭，可以安装、更新或卸载。",box(ui,30,334,802,28),ui->small_font,S14_MUTED,DT_SINGLELINE);
 if(!ui->in_game){button(ui,dc,L"安装 / 更新",30,378,248,31,ui->game_found&&!ui->running);button(ui,dc,L"移除插件",294,378,248,32,ui->installed&&!ui->running);button(ui,dc,L"打开日志",558,378,274,33,ui->installed||!!(ui->detected&S14_FOUND_DATA));button(ui,dc,L"彻底卸载",30,434,248,34,!ui->running&&!!(ui->detected&(S14_FOUND_DLL|S14_FOUND_MANAGER|S14_FOUND_DATA)));label(ui,dc,L"移除插件保留设置；彻底卸载清理项目数据。\n彻底卸载请从游戏目录外执行。",box(ui,294,434,538,49),ui->small_font,S14_MUTED,DT_WORDBREAK);}else button(ui,dc,L"打开日志",30,378,248,33,1);
 button(ui,dc,ui->update_busy?L"正在连接…":L"检查 GitHub 更新",30,494,248,35,!ui->update_busy);if(!ui->in_game)button(ui,dc,L"下载并更新",294,494,248,36,ui->update_ready&&!ui->update_busy&&!ui->running&&ui->game_found);
 label(ui,dc,ui->update_status[0]?ui->update_status:ui->in_game?L"将打开独立管理器；安装更新需退出游戏。":L"检查发布版本；退出游戏后可下载更新。",box(ui,30,544,802,37),ui->small_font,S14_MUTED,DT_WORDBREAK);
 rounded(dc,box(ui,28,596,804,88),S14_CARD);label(ui,dc,L"规则与采集总开关",box(ui,46,609,630,28),ui->body_font,S14_INK,DT_SINGLELINE);label(ui,dc,L"控制墙体、探索、战斗记录、攻防增幅与诊断；武将情报独立启停。",box(ui,46,649,744,24),ui->small_font,S14_MUTED,DT_SINGLELINE);toggle(ui,dc,609,!!(ui->requested&S14_MASTER),1,1);
}
void s14_manager_paint(S14ManagerUI *ui,HDC dc,int width,int height){fill(dc,(RECT){0,0,width,height},S14_PAPER);label(ui,dc,L"天下归心 · Mod 管理器",box(ui,28,22,740,36),ui->title_font,S14_INK,DT_SINGLELINE|DT_VCENTER);wchar_t subtitle[96];swprintf(subtitle,96,L"三国志 14  |  v%ls  |  %ls",S14_MANAGER_VERSION,ui->in_game?L"F10 打开 / 收起":L"独立管理与安装");label(ui,dc,subtitle,box(ui,30,63,750,24),ui->small_font,S14_MUTED,DT_SINGLELINE);label(ui,dc,L"×",box(ui,810,18,30,32),ui->title_font,S14_MUTED,DT_CENTER|DT_VCENTER|DT_SINGLELINE);const wchar_t *tabs[]={L"Mod",L"设置"};for(int i=0;i<2;i++){RECT r=box(ui,28+i*130,102,118,36);rounded(dc,r,ui->tab==i?S14_ACCENT_SOFT:S14_PAPER);label(ui,dc,tabs[i],r,ui->body_font,ui->tab==i?S14_ACCENT:S14_MUTED,DT_CENTER|DT_SINGLELINE|DT_VCENTER);focus(ui,dc,r,100+i);}if(ui->tab==0)paint_mods(ui,dc);else paint_settings(ui,dc);
 fill(dc,box(ui,28,704,804,1),S14_BORDER);label(ui,dc,ui->status,box(ui,30,717,802,26),ui->small_font,ui->fault?S14_ERROR:S14_SUCCESS,DT_SINGLELINE|DT_END_ELLIPSIS);label(ui,dc,ui->notice[0]?ui->notice:ui->tab==0?L"点击右侧开关启停 Mod；点击箭头展开子功能。设置自动保存。":L"新增版本需退出游戏后安装；更新保留现有功能设置与战绩。",box(ui,30,746,802,37),ui->small_font,ui->notice_error?S14_ERROR:S14_MUTED,DT_WORDBREAK);
}
int s14_manager_hit(S14ManagerUI *ui,POINT point){int x=MulDiv(point.x,96,ui->scale),y=MulDiv(point.y,96,ui->scale);if(x>=804&&x<846&&y>=12&&y<56)return 90;if(y>=102&&y<138&&x>=28&&x<276){int t=(x-28)/130;return t<2&&(x-28)%130<118?100+t:0;}
 if(ui->tab==0){if(x>=658&&x<832&&y>=150&&y<190)return 61;if(y<S14_MOD_LIST_TOP || y>=S14_MOD_LIST_BOTTOM)return 0;for(int i=0;i<S14_MOD_COUNT;i++){int mod=s14_manager_mod_index(ui,i),top=s14_manager_mod_top(ui,mod);if(y>=top+12&&y<top+65){if(x>=732&&x<824)return s14_mods[mod].action;if(s14_mods[mod].children&&x>=40&&x<626)return 80+mod;}if(!(ui->expanded&(1u<<mod)))continue;int childtop=top+76;if(mod==S14_MOD_WALL&&x>=732&&x<824&&y>=childtop+9&&y<childtop+58)return 11;if(mod==S14_MOD_VIEWS&&x>=732&&x<824){if(y>=childtop+9&&y<childtop+58)return 40;if(y>=childtop+79&&y<childtop+128)return 43;if(y>=childtop+149&&y<childtop+198)return 46;}if(mod==S14_MOD_AI_AFFIX&&x>=732&&x<824){if(y>=childtop+9&&y<childtop+58)return 49;if(y>=childtop+79&&y<childtop+128)return 50;}if(mod==S14_MOD_CAO_REN_BUFF&&x>=732&&x<824){if(y>=childtop+9&&y<childtop+58)return 47;if(y>=childtop+79&&y<childtop+128)return 48;}if(mod==S14_MOD_SEARCH){for(int g=0;g<3;g++){int t=childtop+27+g*62,n=g==0?4:g==1?3:2,w=736/n;if(y>=t&&y<t+29&&x>=72&&x<808){int a=(x-72)/w;if(a<n&&(x-72)%w<w-8)return 200+g*10+a;}}if(ui->in_game&&x>=72&&x<302&&y>=childtop+262&&y<childtop+292)return 42;}}return 0;}
 if(ui->tab==1){if(y>=494&&y<534){if(x>=30&&x<278)return 35;if(!ui->in_game&&x>=294&&x<542)return 36;}if(!ui->in_game&&x>=730&&x<832&&y>=186&&y<226)return 30;if(y>=378&&y<418){if(x>=30&&x<278)return ui->in_game?33:31;if(!ui->in_game&&x>=294&&x<542)return 32;if(!ui->in_game&&x>=558&&x<832)return 33;}if(!ui->in_game&&x>=30&&x<278&&y>=434&&y<474)return 34;if(x>=732&&x<824&&y>=602&&y<653)return 1;}return 0;
}

int s14_manager_activate(S14ManagerUI *ui,int id) {
    if(id==61){ui->mod_sort=(ui->mod_sort+1)%3;ui->scroll=0;s14_manager_refresh(ui);return 1;}
    if(id>=80 && id<80+S14_MOD_COUNT){int mod=id-80;if(!s14_mods[mod].children)return 0;ui->expanded^=1u<<mod;s14_manager_refresh(ui);return 1;}
    if(id==44){
        if(!ui->in_game && !ui->game_found)return 0;
        if(!WritePrivateProfileStringW(L"Views",L"Enabled",ui->views_enabled?L"0":L"1",ui->ini))return 0;
        ui->views_enabled=!ui->views_enabled;
        if(ui->action)ui->action(ui,S14_ACTION_CONFIG,ui->context);
        ui->notice_error=0;wcscpy(ui->notice,L"武将情报开关已保存；关闭后暂停两个视图，保留子功能选择。");s14_manager_refresh(ui);return 1;
    }
    if(id==35 || id==36){
        if(!ui->action || ui->update_busy || (id==36 && (ui->in_game || !ui->update_ready || ui->running || !ui->game_found)))return 0;
        ui->action(ui,id==35?S14_ACTION_CHECK_UPDATE:S14_ACTION_DOWNLOAD_UPDATE,ui->context);s14_manager_refresh(ui);return 1;
    }
    if(id==43) {
        if(!ui->in_game && !ui->game_found) return 0;
        if(!WritePrivateProfileStringW(L"Views",L"NativeOfficerStats",ui->native_stats_enabled?L"0":L"1",ui->ini)) return 0;
        ui->native_stats_enabled=!ui->native_stats_enabled;
        if(ui->action) ui->action(ui,S14_ACTION_CONFIG,ui->context);
        ui->notice_error=0;wcscpy(ui->notice,L"原生详情战绩条选择已保存；需开启武将情报，不影响战斗采集。");
        s14_manager_refresh(ui);return 1;
    }
    if(id==46) {
        if(!ui->in_game && !ui->game_found)return 0;
        if(!WritePrivateProfileStringW(L"Views",L"NativeArmyValues",ui->native_army_enabled?L"0":L"1",ui->ini))return 0;
        ui->native_army_enabled=!ui->native_army_enabled;
        if(ui->action)ui->action(ui,S14_ACTION_CONFIG,ui->context);
        s14_manager_refresh(ui);return 1;
    }
    if(id==47 || id==48 || id==49 || id==50){
        if(!ui->in_game && !ui->game_found)return 0;
        unsigned int bit=1u<<(id-47);
        if(!WritePrivateProfileStringW(L"Visuals",id==47?L"CaoRenHalo":id==48?L"CaoRenTooltip":id==49?L"AiAffixHalo":L"AiAffixTooltip",ui->visual_settings&bit?L"0":L"1",ui->ini))return 0;
        ui->visual_settings=s14_visual_settings_read(ui->ini);
        if(ui->action)ui->action(ui,S14_ACTION_CONFIG,ui->context);
        ui->notice_error=0;wcscpy(ui->notice,id>=49?L"随机词条显示选择已保存；需开启 AI 随机词条并抽中，显示开关不改变攻防数值。":L"地图显示选择已保存；需开启百战精锐并达标，显示开关不改变攻防数值。");s14_manager_refresh(ui);return 1;
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
    if (id>=100 && id<=101) { ui->tab=id-100; ui->focus=id; s14_manager_refresh(ui); return 1; }
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


static int action_mod(int action){for(int i=0;i<S14_MOD_COUNT;i++)if(s14_mods[i].action==action || action==80+i)return i;if(action==49||action==50)return S14_MOD_AI_AFFIX;if(action==47||action==48)return S14_MOD_CAO_REN_BUFF;if(action==11)return S14_MOD_WALL;if(action==40||action==43||action==46)return S14_MOD_VIEWS;if(action==42 || (action>=200&&action<=221))return S14_MOD_SEARCH;return -1;}
static void reveal_focus(S14ManagerUI *ui){int mod=action_mod(ui->focus);if(ui->tab || mod<0)return;int y=s14_manager_mod_top(ui,mod),top=y,bottom=y+76;if(ui->focus==11){top=y+76;bottom=top+70;}if(ui->focus==40||ui->focus==43||ui->focus==46){top=y+76+(ui->focus==46?140:ui->focus==43?70:0);bottom=top+70;}if(ui->focus==49||ui->focus==50){top=y+76+(ui->focus==50?70:0);bottom=top+70;}if(ui->focus==47||ui->focus==48){top=y+76+(ui->focus==48?70:0);bottom=top+70;}if(ui->focus>=200&&ui->focus<=221){top=y+76+(ui->focus-200)/10*62;bottom=top+58;}if(ui->focus==42){top=y+76+262;bottom=top+32;}if(top<S14_MOD_LIST_TOP)ui->scroll-=S14_MOD_LIST_TOP-top;if(bottom>S14_MOD_LIST_BOTTOM)ui->scroll+=bottom-S14_MOD_LIST_BOTTOM;}
static LRESULT CALLBACK manager_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){S14ManagerUI *ui=(S14ManagerUI*)GetWindowLongPtrW(window,GWLP_USERDATA);if(message==WM_NCCREATE){ui=((CREATESTRUCTW*)lparam)->lpCreateParams;ui->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui);}if(!ui)return DefWindowProcW(window,message,wparam,lparam);if(message==WM_ERASEBKGND)return 1;if(message==WM_NCHITTEST){POINT p={GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};ScreenToClient(window,&p);return p.y<px(ui,90)&&!s14_manager_hit(ui,p)?HTCAPTION:HTCLIENT;}
 if(message==WM_PAINT){PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);RECT r;GetClientRect(window,&r);HDC back=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);HGDIOBJ old=SelectObject(back,bitmap);s14_manager_paint(ui,back,r.right,r.bottom);BitBlt(dc,0,0,r.right,r.bottom,back,0,0,SRCCOPY);SelectObject(back,old);DeleteObject(bitmap);DeleteDC(back);EndPaint(window,&paint);return 0;}
 if(message==WM_LBUTTONDOWN){ui->pressed=s14_manager_hit(ui,(POINT){GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});ui->focus=ui->pressed;SetCapture(window);s14_manager_refresh(ui);return 0;}if(message==WM_LBUTTONUP){int hit=s14_manager_hit(ui,(POINT){GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});ReleaseCapture();if(hit&&hit==ui->pressed)s14_manager_activate(ui,hit);ui->pressed=0;return 0;}
 if(message==WM_MOUSEWHEEL&&ui->tab==0){ui->scroll-=GET_WHEEL_DELTA_WPARAM(wparam)/WHEEL_DELTA*48;s14_manager_refresh(ui);return 0;}
 if(message==WM_VSCROLL&&ui->tab==0&&(HWND)lparam==ui->mod_scrollbar){SCROLLINFO s={sizeof(s),SIF_TRACKPOS};GetScrollInfo(ui->mod_scrollbar,SB_CTL,&s);switch(LOWORD(wparam)){case SB_LINEUP:ui->scroll-=32;break;case SB_LINEDOWN:ui->scroll+=32;break;case SB_PAGEUP:ui->scroll-=S14_MOD_LIST_BOTTOM-S14_MOD_LIST_TOP-48;break;case SB_PAGEDOWN:ui->scroll+=S14_MOD_LIST_BOTTOM-S14_MOD_LIST_TOP-48;break;case SB_THUMBTRACK:case SB_THUMBPOSITION:ui->scroll=s.nTrackPos;break;case SB_TOP:ui->scroll=0;break;case SB_BOTTOM:ui->scroll=max_scroll(ui);break;}s14_manager_refresh(ui);return 0;}
 if(message==WM_KEYDOWN){if(wparam==VK_ESCAPE){s14_manager_activate(ui,90);return 0;}if(wparam==VK_TAB){int items[64]={100,101},count=2,found=-1;if(ui->tab==0){items[count++]=61;for(int i=0;i<S14_MOD_COUNT;i++){int mod=s14_manager_mod_index(ui,i);if(s14_mods[mod].children)items[count++]=80+mod;items[count++]=s14_mods[mod].action;if(!(ui->expanded&(1u<<mod)))continue;if(mod==S14_MOD_WALL)items[count++]=11;if(mod==S14_MOD_AI_AFFIX){items[count++]=49;items[count++]=50;}if(mod==S14_MOD_CAO_REN_BUFF){items[count++]=47;items[count++]=48;}if(mod==S14_MOD_VIEWS){items[count++]=40;items[count++]=43;items[count++]=46;}if(mod==S14_MOD_SEARCH){for(int g=0;g<3;g++)for(int a=0;a<(g==0?4:g==1?3:2);a++)items[count++]=200+10*g+a;if(ui->in_game)items[count++]=42;}}}else{if(!ui->in_game){items[count++]=30;items[count++]=31;items[count++]=32;items[count++]=34;items[count++]=36;}items[count++]=33;items[count++]=35;items[count++]=1;}items[count++]=90;for(int i=0;i<count;i++)if(items[i]==ui->focus)found=i;int reverse=GetKeyState(VK_SHIFT)<0;ui->focus=items[found<0?0:(found+(reverse?count-1:1))%count];reveal_focus(ui);s14_manager_refresh(ui);return 0;}if(wparam==VK_SPACE||wparam==VK_RETURN){s14_manager_activate(ui,ui->focus);return 0;}}
 if(message==WM_CLOSE){s14_manager_activate(ui,90);return 0;}if(message==WM_CTLCOLOREDIT||message==WM_CTLCOLORSTATIC){SetTextColor((HDC)wparam,S14_INK);SetBkColor((HDC)wparam,S14_CARD);return (LRESULT)ui->edit_brush;}return DefWindowProcW(window,message,wparam,lparam);
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
    ui->window=CreateWindowExW(WS_EX_TOOLWINDOW,manager_class,L"天下归心 · 三国志14功能管理器",WS_POPUP|WS_CLIPCHILDREN,0,0,px(ui,S14_MANAGER_WIDTH),px(ui,S14_MANAGER_HEIGHT),owner,NULL,instance,ui);
    if (!ui->window) { s14_manager_destroy(ui); return 0; }
    ui->mod_scrollbar=CreateWindowExW(0,L"SCROLLBAR",L"",WS_CHILD|SBS_VERT,px(ui,838),px(ui,S14_MOD_LIST_TOP),px(ui,16),px(ui,S14_MOD_LIST_BOTTOM-S14_MOD_LIST_TOP),ui->window,(HMENU)402,instance,NULL);
    if(!ui->mod_scrollbar){s14_manager_destroy(ui);return 0;}
    if (!in_game) {
        ui->directory_edit=CreateWindowExW(0,L"EDIT",ui->root,WS_CHILD|ES_AUTOHSCROLL|ES_READONLY|WS_TABSTOP,px(ui,40),px(ui,198),px(ui,658),px(ui,24),ui->window,(HMENU)401,instance,NULL);
        SendMessageW(ui->directory_edit,WM_SETFONT,(WPARAM)ui->small_font,TRUE); SendMessageW(ui->directory_edit,EM_SETLIMITTEXT,MAX_PATH-1,0);
    }
    ui->views_enabled=s14_views_setting_read(ui->ini);
    ui->requested=s14_config_read(ui->ini); ui->effective=s14_effective_flags(ui->requested); ui->search_settings=s14_search_settings_read(ui->ini);
    ui->officers_enabled=GetPrivateProfileIntW(L"Views",L"Officers",1,ui->ini)!=0;
    ui->native_stats_enabled=GetPrivateProfileIntW(L"Views",L"NativeOfficerStats",1,ui->ini)!=0;
    ui->native_army_enabled=GetPrivateProfileIntW(L"Views",L"NativeArmyValues",1,ui->ini)!=0;
    ui->visual_settings=s14_visual_settings_read(ui->ini);
    ui->battle_enabled=s14_battle_setting_read(ui->ini);return 1;
}
void s14_manager_toggle(S14ManagerUI *ui) {
    if (!ui->window) return;
    if (IsWindowVisible(ui->window)) { ShowWindow(ui->window,SW_HIDE); if (ui->owner) SetForegroundWindow(ui->owner); return; }
    RECT region; if (ui->owner) { GetWindowRect(ui->owner,&region); } else SystemParametersInfoW(SPI_GETWORKAREA,0,&region,0);
    int w=px(ui,S14_MANAGER_WIDTH),h=px(ui,S14_MANAGER_HEIGHT); int x=region.left+(region.right-region.left-w)/2,y=region.top+(region.bottom-region.top-h)/2;
    SetWindowPos(ui->window,ui->in_game?HWND_TOPMOST:HWND_TOP,x,y,w,h,SWP_SHOWWINDOW); SetForegroundWindow(ui->window); s14_manager_refresh(ui);
}
void s14_manager_refresh(S14ManagerUI *ui) {
    if (!ui->window || !IsWindow(ui->window)) return;
    if (ui->directory_edit) ShowWindow(ui->directory_edit,ui->tab==1?SW_SHOW:SW_HIDE);
    if(ui->scroll<0)ui->scroll=0;if(ui->scroll>max_scroll(ui))ui->scroll=max_scroll(ui);
    if(ui->mod_scrollbar){SCROLLINFO si={sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,content_height(ui)-1,S14_MOD_LIST_BOTTOM-S14_MOD_LIST_TOP,ui->scroll,0};SetScrollInfo(ui->mod_scrollbar,SB_CTL,&si,TRUE);ShowWindow(ui->mod_scrollbar,ui->tab==0 && max_scroll(ui)>0?SW_SHOWNA:SW_HIDE);}
    InvalidateRect(ui->window,NULL,FALSE);
}
void s14_manager_destroy(S14ManagerUI *ui) {
    if (ui->window && IsWindow(ui->window)) DestroyWindow(ui->window);
    if (ui->title_font) DeleteObject(ui->title_font); if (ui->body_font) DeleteObject(ui->body_font); if (ui->small_font) DeleteObject(ui->small_font);
    if (ui->edit_brush) DeleteObject(ui->edit_brush); ui->edit_brush=NULL;
    ui->window=NULL; ui->title_font=ui->body_font=ui->small_font=NULL;
}
