#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "tech_tree_ui.h"
#include "tech_tree_model.h"
#include "archer_tech.h"
#include "archer_probe.h"
typedef void *(*Alloc)(size_t);
typedef void *(*Ctor)(void*);
typedef void (*Void)(void*);
typedef void (*Setting)(void*,int);
typedef void (*State)(void*,void*);
typedef void (*Cleanup)(void*,void*);
typedef void *(*Destroy)(void*,unsigned);
typedef int (*RectInit)(void*,int,int,int,int,void*);
typedef int (*ImageInit)(void*,int,int,int,int,int,void*);
typedef int (*BaseInit)(void*,int,int,int,int,void*,int);
typedef int (*ButtonInit)(void*,int,int,void*,void*);
typedef struct {void *object;int kind,registered;} Widget;
typedef struct {void *object,*rim[32],*face[32],*glyph[128];int rim_count,face_count,glyph_count;} Badge;
typedef struct {void *button,*label,*icon,*ring,*status,*fill,*edge,*skin;Badge badge;uintptr_t *slot,original;int action,paint;} Button;
typedef struct {
 void *page,*container,*popup;Widget widgets[256];int used,active,built,pending,offset,modal,modal_first;
 Button buttons[28];int button_count;
 void *heading,*points,*detail,*detail_icon,*condition,*notice,*hint,*summary,*metrics[5],*point_marks[8],*lines[17],*branches[5];
 S14TechSession model;
} Pane;
static Pane panes[8];
static uintptr_t image;
static Alloc alloc;
static void *(*context)(void);
static Ctor layout_ctor,button_ctor,text_ctor,image_ctor;
static BaseInit base_init;
static ButtonInit button_init;
static RectInit text_init;
static ImageInit image_init;
static Setting enabled,visible,width,height,text_size,text_align,text_color,bind_input,selected,alpha;
static Setting font_palette;
static void *(*rect_graphic)(void*,int,int,int,int);
static void (*graphic_color)(void*,const unsigned*);
/* Native palette entries use 0xRRGGBBAA, rather than Win32 COLORREF. */
#define RGBA(r,g,b,a) (((unsigned)(r)<<24)|((unsigned)(g)<<16)|((unsigned)(b)<<8)|(unsigned)(a))
#define GOLD RGBA(224,187,111,255)
#define INK RGBA(32,27,25,255)
#define CREAM RGBA(244,229,200,255)
#define MUTED RGBA(147,133,117,255)
#define EDGE RGBA(87,70,51,255)
static const unsigned branch_ink[]={RGBA(181,108,81,255),RGBA(104,158,152,255),RGBA(136,166,117,255),RGBA(199,158,84,255),RGBA(113,153,190,255)};
static uintptr_t ptr(uintptr_t p);
static void tint(void *graphic,unsigned color){unsigned colors[4]={color,color,color,color};if(graphic)graphic_color(graphic,colors);}
static void tint_text(void *widget,unsigned color){if(widget)tint((void*)ptr((uintptr_t)widget+0x128),color);}
static void (*scale)(void*,float,float);
static void (*extent)(void*,uint64_t);
static void (*text)(void*,const wchar_t*);
static void (*move)(void*,int,int);
static void (*image_set)(void*,int,void*,int,int,int);
static void (*layout_visible)(void*,int,int);
static struct {uintptr_t rtti,methods[16];} behavior_type;
static State native_normal,native_hover,native_press,native_disabled,native_selected;
static void refresh(Pane *p);
static int read_at(uintptr_t p,void *out,size_t n){SIZE_T got=0;return p&&ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got)&&got==n;}
static uintptr_t ptr(uintptr_t p){uintptr_t v=0;read_at(p,&v,8);return v;}
/* CCmnText::SetColor takes raw RGBA. Use the native graphic's palette API,
   as the stock button behavior does, instead of passing an index as RGBA. */
static void native_text_palette(void *t,int palette){font_palette((void*)ptr((uintptr_t)t+0x128),palette);}
static void virtual_void(void *p,int slot){((Void)(*(uintptr_t**)p)[slot])(p);}
static Pane *pane(void *page){for(int i=0;i<8;i++)if(panes[i].page==page)return &panes[i];return NULL;}
static Button *find_button(void *button,Pane **owner){for(int i=0;i<8;i++)for(int j=0;j<panes[i].button_count;j++)if(panes[i].buttons[j].button==button){*owner=&panes[i];return &panes[i].buttons[j];}return NULL;}
static int node_action(int a){return a>=10&&a<10+S14_TECH_NODES;}
static int archer(Pane *p){return p->model.selected>=0&&p->model.selected<p->model.count&&!p->model.choices[p->model.selected].is_mod&&p->model.choices[p->model.selected].carrier==3;}
static int locked(Button *b,Pane *p){return node_action(b->action)&&(!archer(p)||s14_tech_status(p->model.record,b->action-10)==0);}
static void node_style(Button *b,Pane *p,int hover){
 if(!b)return;for(int i=0;i<20;i++)layout_visible((char*)b->button+0x128,i,0);
 int status=node_action(b->action)&&archer(p)?s14_tech_status(p->model.record,b->action-10):0;
 int mode=b->badge.object?(locked(b,p)?0:status==2?3:hover?2:1):!(*(unsigned*)((char*)b->button+0x40)&4)?0:hover?2:1;
 tint_text(b->label,mode==0?MUTED:mode>=2?GOLD:CREAM);
 if(b->paint==mode)return;b->paint=mode;
 unsigned accent=b->badge.object?branch_ink[(b->action-11)/3]:GOLD;
 tint(b->edge,mode>=2?GOLD:EDGE);tint(b->fill,mode>=2?RGBA(94,48,31,255):RGBA(49,36,28,255));
 if(b->badge.object){
  for(int i=0;i<b->badge.rim_count;i++)tint(b->badge.rim[i],mode==0?EDGE:mode>=2?GOLD:accent);
  for(int i=0;i<b->badge.face_count;i++)tint(b->badge.face[i],mode==0?INK:mode>=2?RGBA(80,43,29,255):RGBA(45,37,30,255));
  for(int i=0;i<b->badge.glyph_count;i++)tint(b->badge.glyph[i],mode==0?MUTED:CREAM);
 }
 tint_text(b->label,mode==0?MUTED:mode>=2?GOLD:CREAM);
 tint_text(b->status,mode==3?GOLD:mode==0?MUTED:accent);
}
static void normal(void *behavior,void *button){Pane *p=NULL;Button *b=find_button(button,&p);if(b&&locked(b,p))native_disabled(behavior,button);else native_normal(behavior,button);if(b)b->paint=-1;node_style(b,p,0);}
static void hover(void *behavior,void *button){Pane *p=NULL;Button *b=find_button(button,&p);if(b&&locked(b,p))native_disabled(behavior,button);else native_hover(behavior,button);
 node_style(b,p,1);if(b&&p->hint&&node_action(b->action)&&!p->modal)text(p->hint,s14_tech_nodes[b->action-10].description);
}
static void press(void *behavior,void *button){Pane *p=NULL;Button *b=find_button(button,&p);if(b&&locked(b,p))native_disabled(behavior,button);else native_press(behavior,button);node_style(b,p,1);}
static void select_state(void *behavior,void *button){Pane *p=NULL;Button *b=find_button(button,&p);if(b&&locked(b,p))native_disabled(behavior,button);else native_selected(behavior,button);node_style(b,p,1);}
static void disabled_state(void *behavior,void *button){Pane *p=NULL;Button *b=find_button(button,&p);native_disabled(behavior,button);if(b)b->paint=-1;node_style(b,p,0);}
static void no_title(void *a,void *b,int c){(void)a;(void)b;(void)c;}
static void adopt(Pane *p,void *object,int kind){p->widgets[p->used++]=(Widget){object,kind,0};}
static void inert(void *object){enabled(object,0);*(unsigned*)((char*)object+0x40)|=8;}
static void *make_text(Pane *p,void *parent,int x,int y,int w,int h,int size,const wchar_t *caption){
 void *t=alloc(0x138);if(!t)return NULL;memset(t,0,0x138);text_ctor(t);adopt(p,t,1);
 if(!text_init(t,x,y,w,h,parent))return NULL;
 inert(t);text_size(t,size);text_align(t,0);text_color(t,16);text(t,caption);return t;
}
/* Rectangle graphics live in the widget's native scene. CCmnImage's native
   cleanup releases the entire scene, including multi-quad circular badges. */
static void *make_canvas(Pane *p,void *parent,int x,int y,int w,int h){
 void *o=alloc(0x130);if(!o)return NULL;memset(o,0,0x130);image_ctor(o);adopt(p,o,2);
 if(!base_init(o,x,y,w,h,parent,0))return NULL;inert(o);return o;
}
static void *quad(void *canvas,int x,int y,int w,int h,unsigned color){
 void *g=rect_graphic(canvas,x,y,w,h);if(!g)return NULL;
 if(!ptr((uintptr_t)canvas+0x128))*(uintptr_t*)((char*)canvas+0x128)=(uintptr_t)g;
 tint(g,color);return g;
}
static void *panel(Pane *p,void *parent,int x,int y,int w,int h,unsigned color,unsigned edge){
 void *c=make_canvas(p,parent,x,y,w,h);if(!c)return NULL;
 if(!quad(c,0,0,w,h,edge))return NULL;
 void *fill=quad(c,1,1,w-2,h-2,color);if(!fill)return NULL;
 unsigned shade=((color>>24)>8?(color>>24)-8:0)<<24;
 shade|=((color>>16&255)>6?(color>>16&255)-6:0)<<16;
 shade|=((color>>8&255)>4?(color>>8&255)-4:0)<<8;shade|=color&255;
 unsigned corners[4]={color,color,shade,shade};graphic_color(fill,corners);return c;
}
static void *rule(Pane *p,int x,int y,int w,int h){void *c=make_canvas(p,p->container,x,y,w,h);if(!c)return NULL;return quad(c,0,0,w,h,EDGE);}
static int disk(Badge *b,int radius,void **parts,int *count,unsigned color){
 for(int y=-radius;y<radius;y+=2){float middle=y+1.f;int half=(int)sqrtf((float)radius*radius-middle*middle);if(half<1)half=1;
  void *g=quad(b->object,76-half,32+y,half*2,2,color);if(!g||*count>=32)return 0;parts[(*count)++]=g;
 }return 1;
}
static int stroke(Badge *b,int x1,int y1,int x2,int y2){
 int dx=x2-x1,dy=y2-y1,steps=abs(dx)>abs(dy)?abs(dx):abs(dy);
 for(int i=0;i<=steps;i+=2){int x=x1+(steps?dx*i/steps:0),y=y1+(steps?dy*i/steps:0);
  void *g=quad(b->object,x,y,2,2,CREAM);if(!g||b->glyph_count>=128)return 0;b->glyph[b->glyph_count++]=g;
 }return 1;
}
static int emblem(Pane *p,Button *button,int x,int y,int branch){
 Badge *b=&button->badge;b->object=make_canvas(p,p->container,x,y,152,66);if(!b->object)return 0;
 if(!disk(b,30,b->rim,&b->rim_count,GOLD)||!disk(b,27,b->face,&b->face_count,INK))return 0;
 /* Bow, shield, boot, quiver and loading arrows: vector marks made of native
    scene quads, with no external texture or font-dependent symbol fallback. */
 if(branch==0){int points[][2]={{66,14},{75,19},{82,25},{85,32},{82,39},{75,45},{66,50}};
  for(int i=0;i<6;i++)if(!stroke(b,points[i][0],points[i][1],points[i+1][0],points[i+1][1]))return 0;
  return stroke(b,66,14,66,50)&&stroke(b,60,32,93,32)&&stroke(b,88,27,93,32)&&stroke(b,88,37,93,32);
 }
 if(branch==1)return stroke(b,62,19,76,15)&&stroke(b,76,15,90,19)&&stroke(b,90,19,88,37)&&stroke(b,88,37,76,49)&&stroke(b,76,49,64,37)&&stroke(b,64,37,62,19)&&stroke(b,76,19,76,43);
 if(branch==2)return stroke(b,67,15,83,15)&&stroke(b,83,15,80,35)&&stroke(b,80,35,93,41)&&stroke(b,93,41,91,48)&&stroke(b,91,48,62,48)&&stroke(b,62,48,63,39)&&stroke(b,63,39,67,15)&&stroke(b,67,23,81,23);
 if(branch==3)return stroke(b,63,28,89,28)&&stroke(b,89,28,86,49)&&stroke(b,86,49,66,49)&&stroke(b,66,49,63,28)&&stroke(b,69,15,69,37)&&stroke(b,77,12,77,37)&&stroke(b,84,17,84,37)&&stroke(b,73,17,77,12)&&stroke(b,81,17,77,12);
 return stroke(b,64,24,76,16)&&stroke(b,76,16,88,24)&&stroke(b,88,24,91,33)&&stroke(b,91,33,85,28)&&stroke(b,91,33,94,26)&&stroke(b,88,41,76,49)&&stroke(b,76,49,64,41)&&stroke(b,64,41,61,32)&&stroke(b,61,32,67,37)&&stroke(b,61,32,58,39);
}
static int graphic(int carrier){static const int ids[21]={0x1a2,0x1a8,0x1a9,0x1aa,0x1ac,0x1ad,0x1ab,0x1ae,0x1af,0x1a3,0x1a4,0x1a5,0x1a6,0x1a7,0x35b,0x1a2,0x1a2,0x1a2,0x1a2,0x1a2,0x1a2};return carrier>=1&&carrier<=20?ids[carrier]:0x1a2;}
static void *make_icon(Pane *p,void *parent,int x,int y,int size,int id){
 void *t=alloc(0x130);if(!t)return NULL;memset(t,0,0x130);image_ctor(t);adopt(p,t,2);
 if(!image_init(t,x,y,size,size,id,parent)||!ptr((uintptr_t)t+0x128))return NULL;
 inert(t);return t;
}
static void set_icon(void *icon,int id){image_set((void*)ptr((uintptr_t)icon+0x128),id,(void*)ptr((uintptr_t)icon+0x88),0,1,0);}
static Button *make_button_in(Pane *p,void *parent,int x,int y,int w,int h,int action,const wchar_t *caption,int icon_id){
 Button *b=&p->buttons[p->button_count++];b->action=action;b->paint=-1;
 void *behavior=alloc(8);if(!behavior)return NULL;*(uintptr_t*)behavior=(uintptr_t)behavior_type.methods;
 b->button=alloc(0x190);if(!b->button){((Destroy)behavior_type.methods[0])(behavior,1);return NULL;}
 memset(b->button,0,0x190);button_ctor(b->button);adopt(p,b->button,3);
 if(!button_init(b->button,x,y,parent,behavior)){
  if(ptr((uintptr_t)b->button+0x178)!=(uintptr_t)behavior)((Destroy)behavior_type.methods[0])(behavior,1);
  return NULL;
 }
 bind_input(b->button,1);scale(b->button,1.f,1.f);width(b->button,w);height(b->button,h);extent(b->button,((uint64_t)(unsigned)h<<32)|(unsigned)w);
 /* Decorative objects are siblings: a switch skin's scale must never stretch
    its labels, hit areas or icons. Native input and behavior remain unchanged. */
 if(!node_action(action)||action==10){
  void *c=make_canvas(p,parent,x,y,w,h);if(!c)return NULL;b->skin=c;
  b->edge=quad(c,0,0,w,h,EDGE);b->fill=quad(c,1,1,w-2,h-2,INK);if(!b->edge||!b->fill)return NULL;
 }
 b->label=make_text(p,parent,x+(icon_id?36:0),y+(h-26)/2,icon_id?w-40:w,28,22,caption);if(!b->label)return NULL;text_align(b->label,4);
 uintptr_t slots=ptr((uintptr_t)b->button+0x130);int count=0;unsigned char kind=0;uintptr_t font=ptr((uintptr_t)b->label+0x128);
 if(!read_at((uintptr_t)b->button+0x138,&count,4)||count!=21||!slots||!font||!read_at(font+0x112,&kind,1)||kind!=2)return NULL;
 b->slot=(uintptr_t*)slots+20;b->original=*b->slot;layout_visible((char*)b->button+0x128,20,0);*b->slot=font;
 if(icon_id){b->icon=make_icon(p,parent,x+8,y+(h-24)/2,24,icon_id);if(!b->icon)return NULL;}
 native_normal(behavior,b->button);node_style(b,p,0);return b;
}
static Button *make_button(Pane *p,int x,int y,int w,int h,int action,const wchar_t *caption,int icon_id){return make_button_in(p,p->container,x,y,w,h,action,caption,icon_id);}
static void dispose(Pane *p,void *resources){
 /* Weak title references must be restored before any font or layout is freed. */
 p->pending=0;p->active=0;p->built=0;
 for(int i=0;i<p->button_count;i++)if(p->buttons[i].slot)*p->buttons[i].slot=p->buttons[i].original;
 for(int i=p->used-1;i>=0;i--){Widget *w=&p->widgets[i];uintptr_t *vt=*(uintptr_t**)w->object;
  if(w->registered){visible(w->object,0);((Void)vt[8])(w->object);w->registered=0;}
  ((Cleanup)vt[5])(w->object,resources);((Destroy)vt[0])(w->object,1);
 }
 s14_tech_reset(&p->model);memset(p,0,sizeof(*p));
}
void s14_tech_ui_release(void *page,void *resources){Pane *p=pane(page);if(p)dispose(p,resources);}
void s14_tech_ui_display(void *page,int show){Pane *p=pane(page);if(!p||!p->built||p->active==show)return;
 p->active=show;if(!show){p->pending=p->modal=0;visible(p->popup,0);}
 /* Native parent membership and global input/draw registration are separate. */
 for(int i=0;i<p->used;i++){Widget *w=&p->widgets[i];if(show){virtual_void(w->object,7);w->registered=1;}else if(w->registered){virtual_void(w->object,8);w->registered=0;}}
 if(show)refresh(p);visible(p->container,show);
}
static int build(Pane *p){
 p->container=alloc(0x130);if(!p->container)return 0;memset(p->container,0,0x130);layout_ctor(p->container);adopt(p,p->container,0);
 if(!base_init(p->container,698,12,840,650,p->page,0))return 0;*(unsigned*)((char*)p->container+0x40)|=8;
 if(!panel(p,p->container,0,0,840,650,RGBA(29,25,23,249),RGBA(134,103,59,255))||
    !panel(p,p->container,8,8,824,94,RGBA(54,34,25,255),EDGE))return 0;
 p->heading=make_text(p,p->container,24,16,500,38,29,L"弓弩 · 武备研习");
 p->points=make_text(p,p->container,592,17,222,32,22,L"基础点数  0 / 8");
 if(!p->heading||!p->points)return 0;tint_text(p->heading,GOLD);tint_text(p->points,CREAM);
 if(!make_button(p,16,60,34,34,1,L"<",0)||!make_button(p,790,60,34,34,2,L">",0))return 0;
 for(int i=0;i<5;i++)if(!make_button(p,58+i*146,60,138,34,30+i,L"兵种",graphic(1)))return 0;
 if(!make_button(p,300,112,240,36,10,L"弓兵整备 · 已解锁",0))return 0;
 const wchar_t *names[]={L"攻军",L"守备",L"机动",L"容量",L"装填"};
 const wchar_t *roles[]={L"火力压制",L"阵地生存",L"迅捷转进",L"持久作战",L"灵活补给"};
 if(!rule(p,420,148,1,11)||!rule(p,92,158,656,1))return 0;
 for(int b=0;b<5;b++){
  int x=16+b*162;
  if(!panel(p,p->container,x,166,160,346,RGBA(38,32,28,255),EDGE))return 0;
  p->branches[b]=make_text(p,p->container,x,170,160,29,23,names[b]);if(!p->branches[b])return 0;text_align(p->branches[b],4);tint_text(p->branches[b],branch_ink[b]);
  void *role=make_text(p,p->container,x,198,160,23,15,roles[b]);if(!role)return 0;text_align(role,4);tint_text(role,MUTED);
  for(int tier=0;tier<3;tier++){
   int node=1+b*3+tier,y=226+tier*94;
   Button *button=make_button(p,x+4,y,152,91,10+node,L"",0);if(!button)return 0;
   if(!emblem(p,button,x+4,y,b))return 0;
   text(button->label,s14_tech_nodes[node].name);move(button->label,x+4,y+62);width(button->label,152);height(button->label,27);text_size(button->label,22);
   button->status=make_text(p,p->container,x+112,y+22,43,23,15,L"一阶");if(!button->status)return 0;tint_text(button->status,MUTED);
   if(tier<2){p->lines[b*3+tier]=rule(p,x+80,y+88,1,6);if(!p->lines[b*3+tier])return 0;}
   button->paint=-1;node_style(button,p,0);
  }
  p->lines[b*3+2]=rule(p,x+80,159,1,7);if(!p->lines[b*3+2])return 0;
 }
 p->hint=make_text(p,p->container,22,518,798,27,19,L"点击技能查看收益与代价，在浮窗中确认研习。可跨列学习。");if(!p->hint)return 0;tint_text(p->hint,CREAM);
 if(!panel(p,p->container,16,551,808,70,RGBA(47,36,28,255),EDGE))return 0;
 const wchar_t *stats[]={L"攻军强化",L"防御强化",L"机动强化",L"弹药上限",L"每日装填"};
 for(int i=0;i<5;i++){
  void *title=make_text(p,p->container,22+i*160,557,148,23,16,stats[i]);if(!title)return 0;text_align(title,4);tint_text(title,MUTED);
  p->metrics[i]=make_text(p,p->container,22+i*160,584,148,31,25,L"0");if(!p->metrics[i])return 0;text_align(p->metrics[i],4);tint_text(p->metrics[i],GOLD);
  if(i<4&&!rule(p,176+i*160,564,1,42))return 0;
 }
 p->notice=make_text(p,p->container,22,628,798,21,15,L"基础免费 · 最多8点 · 随游戏存档保存 · 进阶尚未接入");if(!p->notice)return 0;tint_text(p->notice,MUTED);
 /* An opaque native child panel, with native switch controls for confirmation. */
 p->modal_first=p->used;p->popup=alloc(0x130);if(!p->popup)return 0;memset(p->popup,0,0x130);layout_ctor(p->popup);adopt(p,p->popup,0);
 if(!base_init(p->popup,132,160,576,334,p->container,0))return 0;*(unsigned*)((char*)p->popup+0x40)|=8;
 Button *backdrop=make_button_in(p,p->popup,0,0,576,334,4,L"",0);if(!backdrop)return 0;enabled(backdrop->button,0);
 if(!panel(p,p->popup,4,4,568,326,RGBA(35,28,24,255),GOLD)||!panel(p,p->popup,12,12,552,66,RGBA(80,43,29,255),EDGE))return 0;
 p->detail_icon=make_icon(p,p->popup,28,29,32,0x1aa);if(!p->detail_icon)return 0;
 p->detail=make_text(p,p->popup,76,28,468,37,28,L"技能说明");
 p->condition=make_text(p,p->popup,28,100,520,146,22,L"");
 if(!make_button_in(p,p->popup,66,262,180,44,5,L"返回",0)||!make_button_in(p,p->popup,330,262,180,44,3,L"确认研习",0))return 0;
 if(!p->detail||!p->condition)return 0;tint_text(p->detail,GOLD);tint_text(p->condition,CREAM);
 visible(p->popup,0);visible(p->container,0);p->built=1;return 1;
}
int s14_tech_ui_create(void *page){
 if(pane(page))return 1;Pane *p=NULL;for(int i=0;i<8;i++)if(!panes[i].page){p=&panes[i];break;}if(!p)return 0;
 p->page=page;p->model.selected=-1;
 if(build(p))return 1;
 void *resources=(void*)ptr((uintptr_t)context()+0x210);dispose(p,resources);return 0;
}
static void utf8(const char *source,wchar_t *out,int count){if(!MultiByteToWideChar(CP_UTF8,0,source,-1,out,count))out[0]=0;}
static void refresh(Pane *p){
 wchar_t name[64],caption[512];S14TechSession *s=&p->model;S14TechRecord *r=s->record;int supported=archer(p);
 if(r&&supported&&s->current)r->learned=s14_archer_tech_mask(s->current->id);
 if(s->selected>=0&&s->selected<s->count)utf8(s->choices[s->selected].name,name,64);else wcscpy(name,L"无可用兵种");
 swprintf(caption,512,L"%ls · 武备研习",name);text(p->heading,caption);
 swprintf(caption,512,L"基础点数  %d / 8",s14_tech_points(r?r->learned:1));text(p->points,caption);
 int can=supported&&s->current&&s14_archer_tech_can_learn(s->current->id);
 visible(p->popup,p->modal);
 for(int i=0;i<p->button_count;i++){
  Button *b=&p->buttons[i];int action=b->action;
  if(action>=30&&action<35){int index=p->offset+action-30;int show=index<s->count;visible(b->button,show);visible(b->label,show);if(b->icon)visible(b->icon,show);if(b->skin)visible(b->skin,show);enabled(b->button,!p->modal);if(show){utf8(s->choices[index].name,caption,512);text_size(b->label,wcslen(caption)>6?16:22);text(b->label,caption);set_icon(b->icon,graphic(s->choices[index].carrier));selected(b->button,index==s->selected);b->paint=-1;node_style(b,p,index==s->selected);}}
  else if(action==1||action==2)enabled(b->button,!p->modal&&(action==1?p->offset>0:p->offset+5<s->count));
  else if(action==3){int status=s14_tech_status(r,r?r->node:-1);visible(b->button,p->modal);enabled(b->button,p->modal&&can&&status==1);text(b->label,status==2?L"已学习":!can?L"仅限自势力弓弩":s14_tech_points(r?r->learned:1)>=8?L"点数已用完":status==1?L"确认学习":L"前置未满足");}
  else if(action==5){visible(b->button,p->modal);enabled(b->button,p->modal);}
  else if(node_action(action)){int n=action-10,status=s14_tech_status(r,n);enabled(b->button,!p->modal&&r!=NULL);selected(b->button,r&&status==2);if(b->icon)alpha(b->icon,!supported||status==0?90:255);if(!supported||status==0)native_disabled((void*)ptr((uintptr_t)b->button+0x178),b->button);b->paint=-1;node_style(b,p,status==2);if(b->status){const wchar_t *rank[]={L"一阶",L"二阶",L"三阶"};text(b->status,status==2?L"已学":rank[(n-1)%3]);}}
 }
 for(int b=0;b<5;b++)for(int t=0;t<3;t++)tint(p->lines[b*3+t],supported&&s14_tech_status(r,1+b*3+t)==2?GOLD:EDGE);
 S14TechBonus bonus=s14_tech_bonus(r&&supported?r->learned:1);int cap,rate;s14_archer_ammo_parameters(s->current&&supported?s->current->id:0,&cap,&rate);
 int values[]={bonus.attack_bp,bonus.defense_bp,bonus.mobility_bp,cap,rate};
 for(int i=0;i<5;i++){
  if(i<3)swprintf(caption,512,L"%+.1f%%",values[i]/100.);
  else if(i==3)swprintf(caption,512,L"%d",cap);else swprintf(caption,512,L"%.2f / 日",rate/100.);
  text(p->metrics[i],caption);tint_text(p->metrics[i],i<3&&values[i]<0?RGBA(218,133,105,255):GOLD);
 }
 if(!supported)text(p->notice,L"本次仅接入原生弓弩。其它原生与MOD兵种暂未强化。");
 else if(!can)text(p->notice,L"仅自势力可学习；请确认MOD总开关、弓弩弹药功能已启用。");
 else text(p->notice,L"基础免费 · 最多8点 · 关闭保留，需保存游戏 · 进阶尚未接入");
 if(r&&p->modal){int n=r->node,status=s14_tech_status(r,n);int icons[]={0x1aa,0x1ac,0x1a9,0x1aa,0x1ab};set_icon(p->detail_icon,icons[s14_tech_nodes[n].icon]);
  swprintf(caption,512,L"%ls · %ls",s14_tech_nodes[n].name,name);text(p->detail,caption);
  swprintf(caption,512,L"%ls\n\n%ls\n基础点数 %d / 8",n==0?L"基础最多8点，可跨列学习。\n任意上一阶开放下一阶。\n进阶战术尚未接入。":s14_tech_nodes[n].description,status==2?L"已学习，不重复消耗点数。":s14_tech_points(r->learned)>=8?L"基础点数已用完。":status==1?L"前置已满足，可跨列学习。":L"请先学习任意列的上一阶节点。",s14_tech_points(r->learned));text(p->condition,caption);
 }
 /* Registration is global. Hide every popup widget explicitly instead of
    assuming that a parent's invisible bit suppresses independent draw/input. */
 for(int i=p->modal_first;i<p->used;i++)visible(p->widgets[i].object,p->modal);
}
void s14_tech_ui_person(void *page,int officer,unsigned permission){Pane *p=pane(page);if(!p||!p->built)return;p->pending=p->modal=0;
 if(!s14_tech_officer(&p->model,officer,permission,s14_troop_builtin_registry())){text(p->notice,L"状态无法创建，请关闭后重新打开。");return;}
 /* First entry prefers the only implemented troop. Remember subsequent choices. */
 if(p->model.record&&p->model.record->learned==1)for(int i=0;i<p->model.count;i++)if(p->model.choices[i].carrier==3&&!p->model.choices[i].is_mod){s14_tech_troop(&p->model,i);break;}
 p->offset=p->model.selected<0?0:(p->model.selected/5)*5;refresh(p);
}
int s14_tech_ui_dispatch(void *container,int event,void *sender){
 if(event!=0x7ffffffe||(char*)container!=(char*)sender+0x70)return 0;
 Pane *p=NULL;Button *b=find_button(sender,&p);if(!b)return 0;
 unsigned flags=0;if(p->active&&p->built&&read_at((uintptr_t)sender+0x40,&flags,4)&&(flags&7)==7&&!p->pending&&
   (p->modal?(b->action==3||b->action==5):b->action!=3&&b->action!=4&&b->action!=5))p->pending=b->action;return 1;
}
void s14_tech_ui_update(void *page){Pane *p=pane(page);if(!p||!p->active)return;
 /* A load replaces saved masks; never retain a learned mask only in the UI. */
 int action=p->pending;p->pending=0;
 if(action==1&&p->offset>0)p->offset-=5;
 else if(action==2&&p->offset+5<p->model.count)p->offset+=5;
 else if(action==5)p->modal=0;
 else if(action==3&&p->modal&&archer(p)&&p->model.current&&p->model.record){
  int ok=s14_archer_tech_learn(p->model.current->id,p->model.record->node);p->modal=0;
  if(!ok)text(p->hint,L"学习未执行：点数、前置或当前势力条件已变化。");else text(p->hint,L"学习成功，部队属性已生效；请保存游戏以保留强化。");
 }
 else if(node_action(action)&&p->model.record){p->model.record->node=action-10;p->modal=1;}
 else if(action>=30&&action<35){p->modal=0;s14_tech_troop(&p->model,p->offset+action-30);}
 /* Refresh only after an event, avoiding per-frame native layout mutations. */
 if(action)refresh(p);
}
int s14_tech_ui_install(uintptr_t base,uintptr_t end){
 static const struct {uintptr_t rva;unsigned char bytes[8];unsigned count;} anchors[]={
  {0x77a050,{0x48,0x89,0x4c,0x24,0x08},5},
  {0x781540,{0x48,0x89,0x5c,0x24,0x08},5},
  {0x7815c0,{0x40,0x53,0x55,0x56,0x57,0x41,0x54},7},
  {0x830fb0,{0x48,0x85,0xc9},3},
  {0x830750,{0x48,0x85,0xc9},3},
  {0x784b90,{0x48,0x89,0x5c,0x24,0x10},5},
  {0x830f10,{0x48,0x85,0xc9},3}
 };
 unsigned char bytes[8];
 if(!base||end<=base||end-base<0x1358000)return 0;
 for(unsigned i=0;i<sizeof(anchors)/sizeof(anchors[0]);i++)if(!read_at(base+anchors[i].rva,bytes,anchors[i].count)||memcmp(bytes,anchors[i].bytes,anchors[i].count))return 0;
 if(!read_at(base+0x1357af0,&behavior_type,sizeof(behavior_type))||behavior_type.rtti<base||behavior_type.rtti>=end||
  behavior_type.methods[0]!=base+0x536220||behavior_type.methods[5]!=base+0x7a2fe0||behavior_type.methods[6]!=base+0x7a2730||behavior_type.methods[7]!=base+0x7a3b70||behavior_type.methods[9]!=base+0x7a1ed0||behavior_type.methods[10]!=base+0x7a2120||
  ptr(base+0x1354538)!=base+0x77b0d0||ptr(base+0x1354250)!=base+0x77b150||ptr(base+0x13546b0)!=base+0x77b240)return 0;
 rect_graphic=(void*(*)(void*,int,int,int,int))(base+0x784b90);graphic_color=(void(*)(void*,const unsigned*))(base+0x830f10);
 image=base;context=(void*(*)(void))(base+0x13ee90);move=(void(*)(void*,int,int))(base+0x789ca0);alloc=(Alloc)(base+0x3a54d0);layout_ctor=(Ctor)(base+0x77a0b0);button_ctor=(Ctor)(base+0x763cb0);text_ctor=(Ctor)(base+0x77a300);image_ctor=(Ctor)(base+0x77a050);
 base_init=(BaseInit)(base+0x7815c0);button_init=(ButtonInit)(base+0x767b00);text_init=(RectInit)(base+0x7822a0);image_init=(ImageInit)(base+0x781540);
 enabled=(Setting)(base+0x125f0);visible=(Setting)(base+0x789820);width=(Setting)(base+0x78b550);height=(Setting)(base+0x789740);alpha=(Setting)(base+0x788aa0);
 text_size=(Setting)(base+0x789700);text_align=(Setting)(base+0x78b030);font_palette=(Setting)(base+0x830fb0);text_color=native_text_palette;bind_input=(Setting)(base+0x770d30);selected=(Setting)(base+0x771910);
 scale=(void(*)(void*,float,float))(base+0x78a880);extent=(void(*)(void*,uint64_t))(base+0x770dd0);text=(void(*)(void*,const wchar_t*))(base+0x78aff0);
 image_set=(void(*)(void*,int,void*,int,int,int))(base+0x830750);layout_visible=(void(*)(void*,int,int))(base+0x8309d0);
 native_normal=(State)behavior_type.methods[5];native_hover=(State)behavior_type.methods[6];native_press=(State)behavior_type.methods[7];native_disabled=(State)behavior_type.methods[9];native_selected=(State)behavior_type.methods[10];
 behavior_type.methods[3]=(uintptr_t)no_title;behavior_type.methods[5]=(uintptr_t)normal;behavior_type.methods[6]=(uintptr_t)hover;behavior_type.methods[7]=(uintptr_t)press;behavior_type.methods[10]=(uintptr_t)select_state;behavior_type.methods[9]=(uintptr_t)disabled_state;return 1;
}
