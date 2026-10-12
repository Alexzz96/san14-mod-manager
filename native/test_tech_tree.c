#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "tech_tree.c"
static unsigned char parent[0x330],tab[0x148],pages_raw[3][0x180];
static unsigned char *allocated;
static int allocations,inits,visible_calls,text_calls,tab_calls,fail_allocate;
static int visible[99];static wchar_t caption[32];
static void putp(void *p,uintptr_t v){memcpy(p,&v,8);}
static void *alloc_mock(size_t n){assert(n==0x168);allocations++;if(fail_allocate)return NULL;allocated=calloc(1,n);assert(allocated);return allocated;}
static uintptr_t construct_mock(void *p){assert(p==allocated);putp(p,image+0x123e360);return (uintptr_t)p;}
static uintptr_t layout_mock(void *p){assert(p==allocated+0x128);memset(p,0,0x40);return (uintptr_t)p;}
static uintptr_t init_mock(void *p,void *owner){assert(p==allocated && owner==parent);inits++;*(int*)((char*)p+0x138)=99;return 1;}
static void visible_mock(void *p,int i,int v){assert(p==allocated+0x128 && i>=0 && i<99);visible[i]=v;visible_calls++;}
static void text_mock(void *p,int i,const wchar_t *s,int mode){assert(p==allocated+0x128 && i==98 && mode==1);wcscpy(caption,s);text_calls++;}
static void move_mock(void *p,int x,int y){assert(p==allocated && x==0 && y==50);}
static const wchar_t *labels[]={L"基本",L"能力",L"列传"};
static void *pages[]={pages_raw[0],pages_raw[1],pages_raw[2]};
static int expected_count=4;
static uintptr_t tab_mock(void *p,int x,int y,void *owner,const wchar_t *const *names,void *const *content,int n){
    assert(p==tab && x==44 && y==0 && owner==parent && n==expected_count);tab_calls++;
    for(int i=0;i<3;i++)assert(names[i]==labels[i] && content[i]==pages[i]);
    if(n==4){assert(content[3]==allocated && !wcscmp(names[3],L"科技树"));assert(pointer((uintptr_t)allocated)==(uintptr_t)page_vtable);}
    SetLastError(197);return 31;
}
static void fixture(void){
    memset(parent,0,sizeof(parent));memset(tab,0,sizeof(tab));image=0x140000000;
    putp(parent,image+0x13812f8);putp(parent+0x2e0,(uintptr_t)tab);putp(tab,image+0x1351680);
    putp(pages[0],image+0x1381700);putp(pages[1],image+0x1304a30);putp(pages[2],image+0x1304be8);
    allocate=alloc_mock;widget_construct=construct_mock;layout_construct=layout_mock;profile_init=init_mock;
    layout_visible=visible_mock;layout_text=text_mock;move=move_mock;original_tab=tab_mock;
}
int main(void){
    fixture();assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && GetLastError()==197);
    assert(allocations==1 && inits==1 && visible_calls==99 && text_calls==1 && !wcscmp(caption,L"科技树"));
    for(int i=0;i<99;i++)assert(visible[i]==(i==98));
    const wchar_t *four_labels[]={labels[0],labels[1],labels[2],L"科技树"};
    void *four_pages[]={pages[0],pages[1],pages[2],allocated};
    assert(tab_hook(tab,44,0,parent,four_labels,four_pages,4)==31 && allocations==1 && inits==1);
    for(int i=0;i<100;i++)assert(page_refresh(allocated,pages[i%3])==0);
    assert(allocations==1 && text_calls==1);free(allocated);allocated=NULL;
    /* Shared native tabs, unexpected page types and a second init must never
       gain another page or alter the three supplied native labels. */
    expected_count=3;putp(parent,image+0x1381300);assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==1);
    fixture();putp(pages[1],0);assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==1);
    fixture();putp(parent+0x2e0,0);assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==1);
    fixture();fail_allocate=1;assert(tab_hook(tab,44,0,parent,labels,pages,3)==31 && allocations==2 && inits==1);
    puts("{\"status\":\"passed\",\"native_tab_count\":4,\"original_pages_preserved\":true,\"placeholder_only\":true,\"officer_screen_only\":true,\"allocation_failure_fallback\":true,\"refresh_without_allocation\":true,\"in_game_acceptance\":\"pending\"}");return 0;
}
