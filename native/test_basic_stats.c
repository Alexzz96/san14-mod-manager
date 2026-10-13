#include <assert.h>
#include <stdlib.h>
#include "basic_stats_ui.c"
#ifdef NDEBUG
#error assertions required
#endif
static uintptr_t extra_vt[50];static unsigned char ctx[0x218];
static int alive,registered,shown,fail_alloc,fail_resource,hidden[105],rendered;
static int registration_clock,parent_order,extra_order;
static void *original_page,*extra_page;
static void widget_visible_mock(void *p,int v){assert(p==extra_page);shown=v;}
static wchar_t captions[2][105][40];
static void *alloc_mock(size_t n){if(fail_alloc)return NULL;alive++;return calloc(1,n);}
static void *ctor_mock(void *p){return p;}
static void resource_ctor_mock(void *p){(void)p;}
static void *context_mock(void){return ctx;}
static void show_mock(void *p){(void)p;assert(!registered);registered++;extra_order=++registration_clock;}
static void parent_show_mock(void *p){(void)p;parent_order=++registration_clock;}
static void hide_mock(void *p){(void)p;assert(registered>0&&!shown);registered--;}
static void cleanup_mock(void *p,void *r){(void)p;(void)r;assert(!registered);}
static void *delete_mock(void *p,unsigned f){assert(f==1);assert(!registered);alive--;free(p);return NULL;}
static int init_mock(void *p,int x,int y,void *parent,const char *name,int z,void *q){
 assert(!x&&y==120&&parent==original_page&&!strcmp(name,"11_01_02_00_info_busho0")&&!z&&!q);
 *(uintptr_t*)p=(uintptr_t)extra_vt;*(int*)((char*)p+0x138)=fail_resource?0:105;extra_page=p;return !fail_resource;
}
static void visible_mock(void *p,int n,int v){assert(p==(char*)extra_page+0x128);hidden[n]=!v;}
static void text_mock(void *p,int n,const wchar_t *s,int v){assert(v==1&&n>=91&&n<=102);int extra=p==(char*)extra_page+0x128;wcscpy(captions[extra][n],s);rendered++;}
static void style_mock(void *p,int n,int style){assert(p==(char*)extra_page+0x128&&(n==91||n==96||n==101)&&style==13);}
static uintptr_t refresh_mock(void *p,void *person){*(uintptr_t*)((char*)p+0x168)=(uintptr_t)person;return 123;}
static void noop(void *p){(void)p;}
static void base_cleanup(void *p,void *r){(void)p;(void)r;}
int main(void){
 unsigned char *memory=calloc(1,0x1fc91e0),page[0x180]={0};assert(memory);image=(uintptr_t)memory;
 *(uintptr_t*)(memory+0x1fc91d0)=900;*(uintptr_t*)page=image+0x1381700;*(int*)(page+0x138)=105;*(int*)(page+0x40)=3;original_page=page;
 extra_vt[0]=(uintptr_t)delete_mock;extra_vt[5]=(uintptr_t)cleanup_mock;extra_vt[7]=(uintptr_t)show_mock;extra_vt[8]=(uintptr_t)hide_mock;
 allocate=alloc_mock;widget_ctor=ctor_mock;resource_ctor=resource_ctor_mock;resource_init=init_mock;native_context=context_mock;
 layout_visible=visible_mock;widget_visible=widget_visible_mock;layout_text=text_mock;layout_style=style_mock;native_refresh=refresh_mock;native_show=parent_show_mock;native_hide=noop;native_update=noop;native_cleanup=base_cleanup;
 fail_alloc=1;assert(refresh_hook(page,(void*)11)==123);assert(!find(page)&&!alive&&!s14_basic_stats_embedded((uintptr_t)page,11));fail_alloc=0;
 fail_resource=1;refresh_hook(page,(void*)11);assert(!find(page)&&!alive);fail_resource=0;
 for(int cycle=0;cycle<30;cycle++){
  assert(refresh_hook(page,(void*)11)==123);assert(alive==1&&registered==1&&shown);Pane *p=find(page);assert(p);
  /* Minimap entry: refresh activates extra before the later parent show. */
  show_hook(page);assert(registered==1&&shown&&extra_order>parent_order);
  show_hook(page);assert(registered==1&&shown&&extra_order>parent_order);
  for(int i=0;i<105;i++)assert(hidden[i]==!keep(i));
  S14BattleTotals t={.valid_mask=31,.enemy_loss=123456789012ULL,.units_routed=2,.units_defeated=3,.own_loss=44,.officers_injured=5};
  s14_basic_stats_publish((uintptr_t)page,11,900,&t,1,0,1,250);p->next=0;update_hook(page);
  assert(!wcscmp(captions[0][92],L"123456789012"));assert(!wcscmp(captions[1][92],L"44"));assert(!wcscmp(captions[1][97],L"5"));
  assert(s14_basic_stats_embedded((uintptr_t)page,11));assert(!wcscmp(captions[1][101],L"军功")&&!wcscmp(captions[1][102],L"250"));
  refresh_hook(page,(void*)12);assert(!wcscmp(captions[0][92],L"未采集"));assert(!s14_basic_stats_embedded((uintptr_t)page,11));
  t.valid_mask=1;t.enemy_loss=0;s14_basic_stats_publish((uintptr_t)page,12,900,&t,1,1,0,175);render(p);
  assert(!wcscmp(captions[0][92],L"0")&&!wcscmp(captions[0][97],L"未采集")&&!wcscmp(captions[1][102],L"175"));
  s14_basic_stats_publish((uintptr_t)page,12,901,&t,1,0,0,175);render(p);assert(!wcscmp(captions[0][92],L"未采集"));
  hide_hook(page);hide_hook(page);assert(!registered&&!shown&&!s14_basic_stats_embedded((uintptr_t)page,12));show_hook(page);show_hook(page);assert(registered==1&&shown);
  cleanup_hook(page,NULL);cleanup_hook(page,NULL);assert(!registered&&!alive&&!find(page));
 }
 free(memory);printf("{\"status\":\"passed\",\"cycles\":30,\"failed_build_fallback\":true,\"officer_and_world_isolation\":true,\"zero_vs_missing\":true,\"native_row_visibility\":true,\"unregister_before_free\":true}\n");return 0;
}
