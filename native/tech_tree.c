#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "MinHook.h"
#include "tech_tree.h"
#include "tech_tree_ui.h"
#include "tech_tree_abi.h"

typedef uintptr_t (*TabInit)(void*,int,int,void*,const wchar_t*const*,void*const*,int);
typedef void *(*Allocate)(size_t);
typedef uintptr_t (*One)(void*);
typedef void (*LayoutVisible)(void*,int,int);
typedef void (*LayoutText)(void*,int,const wchar_t*,int);
typedef void (*Move)(void*,int,int);
typedef uintptr_t (*Two)(void*,void*);
static TabInit original_tab;
static Allocate allocate;
static One widget_construct,layout_construct;
static Two profile_init,profile_refresh;
static LayoutVisible layout_visible;
static LayoutText layout_text;
static Move move;
static void (*native_select)(void*,int);
static uintptr_t image;
/* Inherit the native profile page's complete ABI and destruction. Only this
   privately owned table changes; no original vtable or resource is patched. */
static struct {uintptr_t rtti_locator,methods[51];S14TechPageIdentity identity;} page_type;
static void (*native_update)(void*),(*native_show)(void*),(*native_hide)(void*),(*native_cleanup)(void*,void*);
static void *(*native_destroy_page)(void*,unsigned);
#define page_vtable page_type.methods
static int read_at(uintptr_t at,void *out,size_t n){SIZE_T got;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static uintptr_t pointer(uintptr_t at){uintptr_t n=0;read_at(at,&n,8);return n;}
static int integer(uintptr_t at){int n=0;read_at(at,&n,4);return n;}

static void hide_biography(void *page){
    /* Resource 0x950e4a5d: nodes 0..18 are the complete portrait/identity
       subtree. The right-side relations start at root 19 (children 20..98).
       Do not force left-side conditional health/portrait elements visible. */
    if(integer((uintptr_t)page+0x138)==99)
        for(int i=19;i<99;i++)layout_visible((char*)page+0x128,i,0);
}
static uintptr_t page_init(void *page,void *parent){
    uintptr_t result=profile_init(page,parent);
    hide_biography(page);
    if(integer((uintptr_t)page+0x138)==99 && !s14_tech_ui_create(page))
        layout_text((char*)page+0x128,98,L"科技树预览构建失败，请返回后重试。",1);
    return result;
}
static uintptr_t page_refresh(void *page,void *person){
    uintptr_t result=profile_refresh(page,person);
    hide_biography(page); /* Refresh can show conditional relationship nodes. */
    unsigned short id=0;unsigned permission=0;
    if(pointer((uintptr_t)person)==image+0x12a00d0 && read_at((uintptr_t)person+0x10,&id,2) && read_at((uintptr_t)person+0x164,&permission,4))
        s14_tech_ui_person(page,id,permission);
    return result;
}
static void page_update(void *p){native_update(p);s14_tech_ui_update(p);}
static void page_show(void *p){native_show(p);s14_tech_ui_display(p,1);}
static void page_hide(void *p){s14_tech_ui_display(p,0);native_hide(p);}
static void page_cleanup(void *p,void *resources){s14_tech_ui_release(p,resources);native_cleanup(p,resources);}
static void *page_destroy(void *p,unsigned flags){
    void *resources=(void*)pointer((uintptr_t)((void*(*)(void))(image+0x13ee90))()+0x210);
    s14_tech_ui_release(p,resources);return native_destroy_page(p,flags);
}
static void *new_page(void *parent){
    void *p=allocate(0x168);if(!p)return NULL;
    memset(p,0,0x168);
    widget_construct(p);
    *(uintptr_t*)p=image+0x123e360; /* native layout-widget intermediate type */
    layout_construct((char*)p+0x128);
    *(uintptr_t*)p=(uintptr_t)page_vtable;
    page_init(p,parent);
    *(unsigned int*)((char*)p+0x40)|=8;
    move(p,0,50); /* same content origin as the three native officer pages */
    return p;
}
static uintptr_t tab_hook(void *tab,int x,int y,void *parent,const wchar_t *const *labels,void *const *pages,int count){
    DWORD error=GetLastError();
    /* CCmnTab is shared by many game screens. Exact parent, container and
       original page types keep the extension confined to officer details. */
    if(count!=3 || !labels || !pages || pointer((uintptr_t)parent)!=image+0x13812f8 ||
       pointer((uintptr_t)tab)!=image+0x1351680 || pointer((uintptr_t)parent+0x2e0)!=(uintptr_t)tab ||
       pointer((uintptr_t)pages[0])!=image+0x1381700 || pointer((uintptr_t)pages[1])!=image+0x1304a30 ||
       pointer((uintptr_t)pages[2])!=image+0x1304be8){
        SetLastError(error);return original_tab(tab,x,y,parent,labels,pages,count);
    }
    /* Original init copies both arrays, generates all four native buttons and
       binds its own native selection callback. Stack arrays never escape. */
    void *extra=new_page(parent);
    if(!extra){SetLastError(error);return original_tab(tab,x,y,parent,labels,pages,count);}
    const wchar_t *extended_labels[]={labels[0],labels[1],labels[2],L"科技树"};
    void *extended_pages[]={pages[0],pages[1],pages[2],extra};
    SetLastError(error);return original_tab(tab,x,y,parent,extended_labels,extended_pages,4);
}
int s14_tech_tree_select_fourth(void *dialog){
    DWORD error=GetLastError();int selected=0;
    uintptr_t tab=pointer((uintptr_t)dialog+0x2e0);
    uintptr_t pages=pointer(tab+0x130),buttons=pointer(tab+0x128),selector=pointer(buttons+0x130);
    if(native_select && pointer((uintptr_t)dialog)==image+0x13812f8 && pointer(tab)==image+0x1351680 &&
       integer(tab+0x138)==4 && pages && pointer(pointer(pages+24))==(uintptr_t)page_vtable &&
       pointer(pages) && pointer(pages+8) && pointer(pages+16) &&
       pointer(buttons)==image+0x13514f0 && integer(buttons+0x138)==4 && selector){
        /* 0x76a6cc uses this same selector to choose the initial native tab.
           Its events update both the tab buttons and the displayed page. */
        native_select((void*)selector,3);
        selected=integer(selector+0x18)==3;
    }
    SetLastError(error);return selected;
}

int s14_tech_tree_install(uintptr_t base,uintptr_t end){
    static const struct {uintptr_t rva;unsigned char bytes[16];unsigned int n;} anchors[]={
        {0x76a440,{0x40,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57},13},
        {0x3a54d0,{0x48,0x89,0x5c,0x24,0x08},5},
        {0x77a0b0,{0x48,0x89,0x4c,0x24,0x08},5},
        {0x82beb0,{0x33,0xc0,0x48,0x89,0x01},5},
        {0x7e7430,{0x40,0x56,0x57,0x41,0x56},5},
        {0x8309d0,{0x85,0xd2,0x78,0x72},4},
        {0x831e70,{0x48,0x89,0x5c,0x24,0x08},5},
        {0x789ca0,{0x48,0x83,0xec,0x28},4},
        {0x4c0020,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c},8},
        {0x1304d80,{'1','1','_','0','1','_','0','2','_','0','2','_','i','n','f','o'},16}
    };
    unsigned char b[16];
    if(!base || end<=base || end-base<0x1381900)return 0;
    for(unsigned int i=0;i<sizeof(anchors)/sizeof(anchors[0]);i++)
        if(!read_at(base+anchors[i].rva,b,anchors[i].n) || memcmp(b,anchors[i].bytes,anchors[i].n))return 0;
    if(!read_at(base+0x1304be0,&page_type,8+sizeof(page_type.methods)) || page_type.rtti_locator<base || page_type.rtti_locator>=end ||
       page_vtable[0]!=base+0x8161c0 ||
       page_vtable[4]!=base+0x7e7430 || page_vtable[5]!=base+0x78d9b0 ||
       page_vtable[48]!=base+0x80ba40 || page_vtable[49]!=base+0x54b380 || !s14_tech_ui_install(base,end))return 0;
    image=base;allocate=(Allocate)(base+0x3a54d0);widget_construct=(One)(base+0x77a0b0);
    layout_construct=(One)(base+0x82beb0);profile_init=(Two)(base+0x7e7430);
    layout_visible=(LayoutVisible)(base+0x8309d0);layout_text=(LayoutText)(base+0x831e70);move=(Move)(base+0x789ca0);
    native_select=(void(*)(void*,int))(base+0x4c0020);
    profile_refresh=(Two)(base+0x80ba40);
    native_update=(void(*)(void*))page_vtable[2];native_show=(void(*)(void*))page_vtable[7];native_hide=(void(*)(void*))page_vtable[8];
    native_cleanup=(void(*)(void*,void*))page_vtable[5];native_destroy_page=(void*(*)(void*,unsigned))page_vtable[0];
    page_type.identity=(S14TechPageIdentity){S14_TECH_PAGE_MAGIC,1,base+0x1304be8};
    page_vtable[0]=(uintptr_t)page_destroy;page_vtable[2]=(uintptr_t)page_update;page_vtable[4]=(uintptr_t)page_init;
    page_vtable[5]=(uintptr_t)page_cleanup;page_vtable[7]=(uintptr_t)page_show;page_vtable[8]=(uintptr_t)page_hide;page_vtable[48]=(uintptr_t)page_refresh;
    return MH_CreateHook((void*)(base+0x76a440),tab_hook,(void**)&original_tab)==MH_OK;
}
