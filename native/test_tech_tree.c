#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "tech_tree.c"
static unsigned char parent[0x330],tab[0x148],pages_raw[3][0x180];
static unsigned char buttons[0x140],selector[0x20];
static void *bound_pages[4];
static int selects;
static unsigned char *allocated;
static int allocations,inits,visible_calls,text_calls,tab_calls,fail_allocate;
static int visible[99];static wchar_t caption[32];
static int ui_creates,ui_people,ui_shows,ui_hides,ui_releases,ui_updates,refreshes,cleanups;
int s14_tech_ui_install(uintptr_t b,uintptr_t e){(void)b;(void)e;return 1;}
int s14_tech_ui_create(void *p){assert(p==allocated);ui_creates++;return 1;}
void s14_tech_ui_person(void *p,int id,unsigned permission){assert(p==allocated&&id==254&&permission==2);ui_people++;}
void s14_tech_ui_display(void *p,int show){assert(p==allocated);if(show)ui_shows++;else ui_hides++;}
void s14_tech_ui_release(void *p,void *r){assert(p==allocated);(void)r;ui_releases++;}
void s14_tech_ui_update(void *p){assert(p==allocated);ui_updates++;}
static uintptr_t refresh_mock(void *p,void *person){assert(p==allocated&&person==pages_raw[0]);refreshes++;return 13;}
static void lifecycle_mock(void *p){assert(p==allocated);}
static void cleanup_mock(void *p,void *r){assert(p==allocated);(void)r;assert(ui_releases>0);cleanups++;}
static void putp(void *p,uintptr_t v){memcpy(p,&v,8);}
static void *alloc_mock(size_t n){assert(n==0x168);allocations++;if(fail_allocate)return NULL;allocated=calloc(1,n);assert(allocated);return allocated;}
static uintptr_t construct_mock(void *p){assert(p==allocated);putp(p,image+0x123e360);return (uintptr_t)p;}
static uintptr_t layout_mock(void *p){assert(p==allocated+0x128);memset(p,0,0x40);return (uintptr_t)p;}
static uintptr_t init_mock(void *p,void *owner){assert(p==allocated && owner==parent);inits++;*(int*)((char*)p+0x138)=99;return 1;}
static void visible_mock(void *p,int i,int v){assert(p==allocated+0x128 && i>=0 && i<99);visible[i]=v;visible_calls++;}
static void text_mock(void *p,int i,const wchar_t *s,int mode){assert(p==allocated+0x128 && i==98 && mode==1);wcscpy(caption,s);text_calls++;}
static void move_mock(void *p,int x,int y){assert(p==allocated && x==0 && y==50);}
static void select_mock(void *p,int index){assert(p==selector&&index==3);selects++;*(int*)(selector+0x18)=index;SetLastError(213);}
static const wchar_t *labels[]={L"基本",L"能力",L"列传"};
static void *pages[]={pages_raw[0],pages_raw[1],pages_raw[2]};
static int expected_count=4;
static uintptr_t tab_mock(void *p,int x,int y,void *owner,const wchar_t *const *names,void *const *content,int n){
    assert(p==tab && x==44 && y==0 && owner==parent && n==expected_count);tab_calls++;
    for(int i=0;i<3;i++)assert(names[i]==labels[i] && content[i]==pages[i]);
    if(n==4){assert(content[3]==allocated && !wcscmp(names[3],L"科技树"));assert(pointer((uintptr_t)allocated)==(uintptr_t)page_vtable);}
    memcpy(bound_pages,content,(size_t)n*sizeof(void*));putp(tab+0x130,(uintptr_t)bound_pages);*(int*)(tab+0x138)=n;
    putp(tab+0x128,(uintptr_t)buttons);putp(buttons,image+0x13514f0);*(int*)(buttons+0x138)=n;putp(buttons+0x130,(uintptr_t)selector);*(int*)(selector+0x18)=0;
    SetLastError(197);return 31;
}
static void fixture(void){
    memset(parent,0,sizeof(parent));memset(tab,0,sizeof(tab));image=0x140000000;
    putp(parent,image+0x13812f8);putp(parent+0x2e0,(uintptr_t)tab);putp(tab,image+0x1351680);
    putp(pages[0],image+0x1381700);putp(pages[1],image+0x1304a30);putp(pages[2],image+0x1304be8);
    allocate=alloc_mock;widget_construct=construct_mock;layout_construct=layout_mock;profile_init=init_mock;
    profile_refresh=refresh_mock;native_update=native_show=native_hide=lifecycle_mock;native_cleanup=cleanup_mock;
    layout_visible=visible_mock;layout_text=text_mock;move=move_mock;original_tab=tab_mock;native_select=select_mock;
}
int main(void){
    fixture();assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && GetLastError()==197);
    assert(allocations==1 && inits==1 && visible_calls==80 && text_calls==0&&ui_creates==1);
    for(int i=19;i<99;i++)assert(visible[i]==0);
    const wchar_t *four_labels[]={labels[0],labels[1],labels[2],L"科技树"};
    void *four_pages[]={pages[0],pages[1],pages[2],allocated};
    assert(tab_hook(tab,44,0,parent,four_labels,four_pages,4)==31 && allocations==1 && inits==1);
    assert(!selects&&integer((uintptr_t)selector+0x18)==0); /* ordinary init defaults stay native */
    SetLastError(229);assert(s14_tech_tree_select_fourth(parent)&&selects==1&&integer((uintptr_t)selector+0x18)==3&&GetLastError()==229);
    *(int*)(tab+0x138)=3;assert(!s14_tech_tree_select_fourth(parent)&&selects==1);*(int*)(tab+0x138)=4;
    putp(allocated,image+0x1304be8);assert(!s14_tech_tree_select_fourth(parent)&&selects==1);putp(allocated,(uintptr_t)page_vtable);
    putp(buttons+0x130,0);assert(!s14_tech_tree_select_fourth(parent)&&selects==1);putp(buttons+0x130,(uintptr_t)selector);
    putp(parent,image+0x1381300);assert(!s14_tech_tree_select_fourth(parent)&&selects==1);putp(parent,image+0x13812f8);
    putp(pages[0],image+0x12a00d0);*(unsigned short*)(pages_raw[0]+0x10)=254;*(unsigned*)(pages_raw[0]+0x164)=2;
    for(int i=0;i<100;i++)assert(page_refresh(allocated,pages[0])==13);
    assert(refreshes==100&&ui_people==100);page_show(allocated);page_update(allocated);page_hide(allocated);page_cleanup(allocated,NULL);
    assert(ui_shows==1&&ui_hides==1&&ui_updates==1&&ui_releases==1&&cleanups==1);
    assert(allocations==1 && text_calls==0);free(allocated);allocated=NULL;
    /* Shared native tabs, unexpected page types and a second init must never
       gain another page or alter the three supplied native labels. */
    expected_count=3;putp(parent,image+0x1381300);assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==1);
    fixture();putp(pages[1],0);assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==1);
    fixture();putp(parent+0x2e0,0);assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==1);
    fixture();fail_allocate=1;assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==2 && inits==1);
    puts("{\"status\":\"passed\",\"native_tab_count\":4,\"original_pages_preserved\":true,\"native_portrait_refresh\":true,\"owned_page_lifecycle\":true,\"officer_screen_only\":true,\"allocation_failure_fallback\":true,\"refresh_without_allocation\":true,\"native_fourth_tab_selection\":true,\"ordinary_tab_init_keeps_native_default\":true,\"absent_or_unexpected_page_not_selected\":true,\"in_game_acceptance\":\"pending\"}");return 0;
}
