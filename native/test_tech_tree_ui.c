#include <assert.h>
#include "tech_tree_ui.c"
typedef struct {void *object;int live,registered,kind;void *behavior;uintptr_t slots[21];unsigned char font[0x120];int x,y,w,h,size,align;unsigned color;void *parent;wchar_t caption[512];} Mock;
static Mock mocks[256];static uintptr_t mock_vt[46];static int allocations,fail_at,live,registers,unregisters,destroyed;
static unsigned char ctx[0x218];static int resource;
static unsigned char *tech_game,*tech_test_world,*tech_settings;static unsigned char tech_person[512],tech_other[512],tech_group[32];
typedef struct {void *owner;unsigned color[4];int live,x,y,w,h;} Graphic;
static Graphic graphics[8192];static int quad_calls,quad_fail,quad_total;
static Mock *mock(void *p){for(int i=0;i<256;i++)if(mocks[i].object==p&&mocks[i].live)return &mocks[i];assert(0);return NULL;}
static void *a_mock(size_t n){allocations++;if(allocations==fail_at)return NULL;void *p=calloc(1,n);assert(p);if(n==8)return p;
 for(int i=0;i<256;i++)if(!mocks[i].live){mocks[i]=(Mock){.object=p,.live=1};live++;return p;}assert(0);return NULL;}
static void *ctor_mock(void *p){*(uintptr_t**)p=mock_vt;*(unsigned*)((char*)p+0x40)=5;return p;}
static void *ctx_mock(void){return ctx;}
static void *rect_mock(void *p,int x,int y,int w,int h){
 Mock *m=mock(p);assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=m->w&&y+h<=m->h);quad_calls++;if(quad_calls==quad_fail)return NULL;
 for(int i=0;i<8192;i++)if(!graphics[i].live){graphics[i]=(Graphic){.owner=p,.live=1,.x=x,.y=y,.w=w,.h=h};quad_total++;return &graphics[i];}assert(0);return NULL;
}
static void rgba_mock(void *p,const unsigned *rgba){assert(p&&rgba);for(int i=0;i<8192;i++)if(p==&graphics[i]&&graphics[i].live){memcpy(graphics[i].color,rgba,16);return;}
 for(int i=0;i<256;i++)if(mocks[i].live&&p==mocks[i].font){mocks[i].color=rgba[0];return;}assert(0);
}
static int base_mock(void *p,int x,int y,int w,int h,void *owner,int option){assert(!option&&x>=0&&y>=0&&w>0&&h>0);Mock *m=mock(p);m->x=x;m->y=y;m->w=w;m->h=h;m->parent=owner;return 1;}
static void setting_mock(void *p,int v){(void)p;(void)v;}
static void palette_mock(void *font,int palette){assert(font&&palette>=0&&palette<=69);int found=0;for(int i=0;i<256;i++)if(mocks[i].live&&mocks[i].font==font)found=1;assert(found);}
static void enabled_mock(void *p,int v){unsigned *f=(unsigned*)((char*)p+0x40);*f=(*f&~4u)|(v?4:0);}
static void visible_mock(void *p,int v){unsigned *f=(unsigned*)((char*)p+0x40);*f=(*f&~1u)|(v?1:0);}
static void move_mock(void *p,int x,int y){Mock *m=mock(p);m->x=x;m->y=y;}
static void width_mock(void *p,int w){mock(p)->w=w;}
static void height_mock(void *p,int h){mock(p)->h=h;}
static void size_mock(void *p,int size){mock(p)->size=size;}
static void align_mock(void *p,int align){mock(p)->align=align;}
static void scale_mock(void *p,float x,float y){(void)p;assert(x==1&&y==1);}
static void extent_mock(void *p,uint64_t wh){(void)p;assert((unsigned)wh>0&&(wh>>32)>0);}
static void text_mock(void *p,const wchar_t *s){assert(mock(p));assert(s);wcsncpy(mock(p)->caption,s,511);}
static int text_init_mock(void *p,int x,int y,int w,int h,void *owner){assert(w>0&&h>0);Mock *m=mock(p);m->x=x;m->y=y;m->w=w;m->h=h;m->parent=owner;m->kind=1;m->font[0x112]=2;*(uintptr_t*)((char*)p+0x128)=(uintptr_t)m->font;return 1;}
static int image_init_mock(void *p,int x,int y,int w,int h,int id,void *owner){assert(w==h&&id>0);Mock *m=mock(p);m->x=x;m->y=y;m->w=w;m->h=h;m->parent=owner;m->kind=2;*(uintptr_t*)((char*)p+0x128)=(uintptr_t)m->font;return 1;}
static int button_init_mock(void *p,int x,int y,void *owner,void *b){assert(owner);Mock *m=mock(p);m->x=x;m->y=y;m->parent=owner;m->kind=3;m->behavior=b;*(uintptr_t*)((char*)p+0x178)=(uintptr_t)b;*(uintptr_t*)((char*)p+0x130)=(uintptr_t)m->slots;*(int*)((char*)p+0x138)=21;m->slots[20]=123;return 1;}
static void activate_mock(void *p){Mock *m=mock(p);assert(!m->registered);m->registered=1;*(unsigned*)((char*)p+0x40)|=2;registers++;}
static void deactivate_mock(void *p){Mock *m=mock(p);assert(m->registered);m->registered=0;*(unsigned*)((char*)p+0x40)&=~2u;unregisters++;}
static void cleanup_mock(void *p,void *r){assert(r==&resource);Mock *m=mock(p);assert(!m->registered);if(m->kind==3)assert(m->slots[20]==123);for(int i=0;i<8192;i++)if(graphics[i].live&&graphics[i].owner==p){graphics[i].live=0;quad_total--;}}
static void *destroy_mock(void *p,unsigned flags){assert(flags==1);Mock *m=mock(p);assert(!m->registered);if(m->behavior)free(m->behavior);m->live=0;live--;destroyed++;free(p);return NULL;}
static void *behavior_destroy_mock(void *p,unsigned flags){assert(flags==1);free(p);return NULL;}
static void state_mock(void *behavior,void *button){assert(behavior&&mock(button));}
static void image_set_mock(void *a,int id,void *b,int opt,int one,int zero){(void)a;(void)b;assert(id>0&&!opt&&one==1&&!zero);}
static void layout_visible_mock(void *p,int node,int v){(void)p;assert(node>=0&&node<=20&&!v);}
static void fixture(void){
 memset(mocks,0,sizeof(mocks));memset(graphics,0,sizeof(graphics));quad_calls=quad_fail=quad_total=0;allocations=live=registers=unregisters=destroyed=fail_at=0;
 mock_vt[0]=(uintptr_t)destroy_mock;mock_vt[5]=(uintptr_t)cleanup_mock;mock_vt[7]=(uintptr_t)activate_mock;mock_vt[8]=(uintptr_t)deactivate_mock;
 *(uintptr_t*)(ctx+0x210)=(uintptr_t)&resource;context=ctx_mock;alloc=a_mock;
 layout_ctor=button_ctor=text_ctor=image_ctor=ctor_mock;base_init=base_mock;button_init=button_init_mock;text_init=text_init_mock;image_init=image_init_mock;
 enabled=enabled_mock;visible=visible_mock;bind_input=selected=alpha=setting_mock;text_align=align_mock;width=width_mock;height=height_mock;text_size=size_mock;rect_graphic=rect_mock;graphic_color=rgba_mock;text_color=native_text_palette;font_palette=palette_mock;
 scale=scale_mock;move=move_mock;extent=extent_mock;text=text_mock;image_set=image_set_mock;layout_visible=layout_visible_mock;
 native_normal=native_hover=native_press=native_disabled=native_selected=state_mock;behavior_type.methods[0]=(uintptr_t)behavior_destroy_mock;
 s14_archer_tech_attach((uintptr_t)tech_game,NULL,1);s14_archer_tech_configure(1);s14_archer_tech_new_game((uintptr_t)tech_test_world);
}
static void click(Pane *p,int action){Button *b=NULL;for(int i=0;i<p->button_count;i++)if(p->buttons[i].action==action)b=&p->buttons[i];assert(b);assert(s14_tech_ui_dispatch((char*)b->button+0x70,0x7ffffffe,b->button));s14_tech_ui_update(p->page);}
/* Geometry proof exported from the actual mock native calls, not a second
   hand-maintained layout. Icons are the exact scene primitives used in-game. */
static void svg_escape(FILE *out,const wchar_t *s){
 for(;*s;s++){if(*s==L'&')fputs("&amp;",out);else if(*s==L'<')fputs("&lt;",out);else if(*s==L'>')fputs("&gt;",out);else {char utf[8]={0};WideCharToMultiByte(CP_UTF8,0,s,1,utf,8,NULL,NULL);fputs(utf,out);}}
}
static int position(void *object,Pane *p,int *x,int *y){
 if(object==p->container)return 1;Mock *m=mock(object);if(!(*(unsigned*)((char*)object+0x40)&1))return 0;
 *x+=m->x;*y+=m->y;return m->parent?position(m->parent,p,x,y):0;
}
static void export_svg(Pane *p,const char *name){
 FILE *out=fopen(name,"wb");assert(out);fputs("<svg xmlns='http://www.w3.org/2000/svg' width='840' height='650' viewBox='0 0 840 650'><rect width='840' height='650' fill='#191613'/>",out);
 for(int i=0;i<p->used;i++){
  Mock *m=mock(p->widgets[i].object);int x=0,y=0;if(!position(m->object,p,&x,&y))continue;
  for(int g=0;g<8192;g++)if(graphics[g].live&&graphics[g].owner==m->object){Graphic *q=&graphics[g];fprintf(out,"<rect x='%d' y='%d' width='%d' height='%d' fill='#%06x' fill-opacity='%.3f'/>",x+q->x,y+q->y,q->w,q->h,q->color[0]>>8,(q->color[0]&255)/255.);}
  if(m->kind==1){
   int line=0;const wchar_t *start=m->caption,*end=start;
   while(*start){while(*end&&*end!=L'\n')end++;wchar_t part[512]={0};wmemcpy(part,start,(size_t)(end-start));
    fprintf(out,"<text x='%d' y='%d' text-anchor='%s' font-family='KaiTi,STKaiti,serif' font-size='%d' fill='#%06x'>",x+(m->align==4?m->w/2:0),y+m->size+line*(m->size+3),m->align==4?"middle":"start",m->size,m->color?m->color>>8:0xf4e5c8);svg_escape(out,part);fputs("</text>",out);if(!*end)break;start=++end;line++;
   }
  }
 }
 fputs("</svg>",out);fclose(out);
}
int main(int argc,char **argv){

 tech_game=calloc(1,0x2020000);tech_test_world=calloc(1,0x86000);tech_settings=calloc(1,128);assert(tech_game&&tech_test_world&&tech_settings);
 *(uintptr_t*)(tech_game+0x1fc91d0)=(uintptr_t)tech_test_world;*(uintptr_t*)(tech_test_world+0x85130)=(uintptr_t)tech_settings;tech_settings[0x3a]=1;
 *(uintptr_t*)tech_person=(uintptr_t)tech_game+0x12a00d0;*(unsigned short*)(tech_person+0x10)=254;tech_person[0x118]=1;*(unsigned*)(tech_person+0x164)=(1u<<1)|(1u<<2)|(1u<<3);
 memcpy(tech_other,tech_person,512);*(unsigned short*)(tech_other+0x10)=253;
 *(uintptr_t*)(tech_test_world+0x148+254*8)=(uintptr_t)tech_person;*(uintptr_t*)(tech_test_world+0x148+253*8)=(uintptr_t)tech_other;
 *(uintptr_t*)tech_group=(uintptr_t)tech_game+0x129fec8;tech_group[0x10]=1;*(uintptr_t*)(tech_test_world+0xde48)=(uintptr_t)tech_group;
 int page;fixture();s14_archer_ammo_settings(NULL,5,2);assert(s14_tech_ui_create(&page));Pane *p=pane(&page);assert(p&&p->built&&p->used<=256&&p->button_count==26);int complete_allocations=allocations,complete_quads=quad_calls;
 /* Catch the original clipping and inherited-scale regressions with the actual
    layout: text fits its box, and every child stays inside its native parent. */
 for(int i=0;i<p->used;i++){
  Mock *m=mock(p->widgets[i].object);if(m->kind==1){assert(m->size<=m->h);if(m->parent==p->container){assert(m->x>=0&&m->y>=0&&m->x+m->w<=840&&m->y+m->h<=650);}}
 }
 for(int i=0;i<p->button_count;i++){Button *b=&p->buttons[i];if(b->badge.object){assert(mock(b->label)->parent==p->container);assert(b->badge.rim_count&&b->badge.face_count&&b->badge.glyph_count);}}

 s14_tech_ui_person(&page,254,(1u<<1)|(1u<<2)|(1u<<3));assert(p->model.selected==2);s14_tech_ui_display(&page,1);s14_tech_ui_display(&page,1);assert(registers==p->used);if(argc>1)export_svg(p,argv[1]);
 for(int i=0;i<p->button_count;i++){Button *b=&p->buttons[i];if(b->action>=30+p->model.count&&b->action<=34){assert(!(*(unsigned*)((char*)b->label+0x40)&1));assert(!(*(unsigned*)((char*)b->skin+0x40)&1));assert(!(*(unsigned*)((char*)b->icon+0x40)&1));}}
 for(int i=p->modal_first;i<p->used;i++)assert(!(*(unsigned*)((char*)p->widgets[i].object+0x40)&1));
 click(p,11);assert(p->model.record->node==1);if(argc>2)export_svg(p,argv[2]);click(p,3);assert(p->model.record->learned==3);click(p,3);assert(p->model.record->learned==3);
 click(p,13);assert(p->model.record->node==3);click(p,3);assert(p->model.record->learned==3); /* locked remains inspectable */
 click(p,5);click(p,30);assert(p->model.selected==0&&p->model.record->learned==1);click(p,11);click(p,3);assert(p->model.record->learned==1);click(p,5);click(p,32);assert(p->model.selected==2&&p->model.record->learned==3);
 s14_tech_ui_display(&page,0);assert(registers==unregisters);click(p,11);assert(!p->pending&&!p->modal);
 s14_tech_ui_display(&page,1);click(p,15);click(p,3);assert(p->model.record->learned==(1|2|32));
 s14_tech_ui_person(&page,253,1u<<3);assert(p->model.record->learned==1);s14_tech_ui_person(&page,254,(1u<<1)|(1u<<2)|(1u<<3));assert(p->model.selected==2&&p->model.record->learned==(1|2|32));
 s14_tech_ui_release(&page,&resource);s14_tech_ui_release(&page,&resource);assert(!live&&!quad_total&&!pane(&page)&&registers==unregisters);
 assert(s14_tech_ui_create(&page));s14_tech_ui_person(&page,254,1u<<3);assert(pane(&page)->model.record->learned==(1|2|32));s14_tech_ui_release(&page,&resource);
 for(int fail=1;fail<=complete_allocations;fail++){fixture();fail_at=fail;assert(!s14_tech_ui_create(&page));assert(!live&&!quad_total&&!pane(&page)&&!registers);}
 for(int fail=1;fail<=complete_quads;fail++){fixture();quad_fail=fail;assert(!s14_tech_ui_create(&page));assert(!live&&!quad_total&&!pane(&page)&&!registers);}
 for(int i=0;i<30;i++){fixture();assert(s14_tech_ui_create(&page));s14_tech_ui_person(&page,254,1u<<1);s14_tech_ui_display(&page,1);click(pane(&page),11);click(pane(&page),3);s14_tech_ui_release(&page,&resource);assert(!live&&!quad_total&&registers==unregisters);}
 puts("{\"status\":\"passed\",\"native_controls\":true,\"unit_scale_text_layout\":true,\"native_scene_cleanup\":true,\"native_vector_badges\":true,\"hidden_click_ignored\":true,\"deferred_click_processing\":true,\"partial_allocation_cleanup\":true,\"unregister_before_free\":true,\"repeated_window_release\":true,\"manual_visual_acceptance\":\"pending\"}");
}
