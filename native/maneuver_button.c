#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "MinHook.h"
#include "maneuver_button.h"

typedef int (*MenuInit)(void*,int,int,void*);
typedef void (*Update)(void*);
typedef void *(*Allocate)(size_t);
typedef void *(*Construct)(void*);
typedef int (*MenuButtonInit)(void*,int,int,void*,void*);
typedef void (*State)(void*,void*);
typedef void *(*Destroy)(void*,unsigned int);
typedef void (*Move)(void*,int,int);
typedef void (*Visible)(void*,int);
typedef void (*Text)(void*,const wchar_t*);
typedef int (*TextInit)(void*,int,int,int,int,void*);
typedef void (*Setting)(void*,int);
typedef void (*LayoutVisible)(void*,int,int);
typedef void (*Extent)(void*,uint64_t);
typedef void (*Cleanup)(void*,void*);
typedef void *(*Context)(void);
typedef int (*Render)(void*);
static Render original_button_render,visibility_result;
static Update native_text_render;
static MenuInit original_init;
static Update original_update;
static Cleanup original_cleanup,cleanup_button,cleanup_text;
static Update activate_button,deactivate_button,activate_text,deactivate_text;
static Context context;
static Allocate allocate;
static Construct construct;
static MenuButtonInit initialize_menu_button;
static State native_normal,native_hover,native_pressed,native_disabled,native_selected;
static Setting bind_input,apply_palette,opacity;
static Update original_show,original_hide;
static Destroy native_destroy,behavior_destroy;
static Move move;
static Visible visible;
static Text text;
static Construct construct_text;
static TextInit initialize_text;
static Setting text_size,text_align;
static LayoutVisible layout_visible;
static Destroy text_destroy;
static Setting button_width,button_height,button_enabled;
static Extent button_extent;
static uintptr_t image;
/* Private tables retain native RTTI, rendering, hit testing and deletion. */
static struct {uintptr_t rtti,methods[65];} button_type;
static struct {uintptr_t rtti,methods[16];} behavior_type;
static struct {uintptr_t rtti,methods[46];} text_type;
typedef struct {void *owner,*button,*anchor,*label;int active;} Entry;
static Entry entries[16]; /* Only used by the native UI thread. */
enum {WAIT_MENU,WAIT_GEOMETRY,CREATED,CAPACITY,ALLOC_BEHAVIOR,ALLOC_BUTTON,INIT_BUTTON,ALLOC_LABEL,INIT_LABEL};
static volatile LONG phase,init_calls,update_calls,created_count,destroyed_count,activated_count;
static volatile LONG hover_count,press_count;
static volatile LONG draw_count;
static ULONGLONG retry_after; /* Allocation failure retries are bounded. UI thread only. */
static volatile LONG anchor_geometry[4],parent_geometry[2];
static void progress(LONG value){InterlockedExchange(&phase,value);}
static const wchar_t caption[]=L"操演";
static void no_command_label(void *behavior,void *button,int command){(void)behavior;(void)button;(void)command;}
static int read_at(uintptr_t at,void *out,size_t n){SIZE_T got=0;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static uintptr_t pointer(uintptr_t at){uintptr_t n=0;read_at(at,&n,8);return n;}
static Entry *entry(void *owner){for(unsigned i=0;i<16;i++)if(entries[i].owner==owner)return &entries[i];return NULL;}
/* The native menu's mask widgets delimit a draw interval. Display registration
   alone appends our controls after that interval. Queue their native graphics
   directly after the last original command, while the same mask is active.
   Their own UI-list render callbacks must not queue a second unmasked copy. */
static int deferred_button_render(void *button){return visibility_result(button);}
static void deferred_text_render(void *label){(void)label;}
static int command_render_hook(void *button){
    int result=original_button_render(button);DWORD error=GetLastError();
    for(unsigned i=0;i<16;i++)if(entries[i].anchor==button && entries[i].active){
        Entry *e=&entries[i];
        if(result && visibility_result(e->button)){
            original_button_render(e->button);native_text_render(e->label);
            InterlockedIncrement(&draw_count);
        }
        break;
    }
    SetLastError(error);return result;
}
static void caption_palette(void *button,int palette){
    for(unsigned i=0;i<16;i++)if(entries[i].button==button && entries[i].label){
        void *font=(void*)pointer((uintptr_t)entries[i].label+0x128);
        if(font)apply_palette(font,palette);
        return;
    }
}
/* Keep the game's state animation callbacks intact. The replacement caption
   receives the same four-corner palette as the original atlas label. */
static void normal_state(void *behavior,void *button){native_normal(behavior,button);caption_palette(button,25);}
static void hover_state(void *behavior,void *button){native_hover(behavior,button);caption_palette(button,26);InterlockedIncrement(&hover_count);}
static void pressed_state(void *behavior,void *button){native_pressed(behavior,button);caption_palette(button,27);InterlockedIncrement(&press_count);}
static void disabled_state(void *behavior,void *button){native_disabled(behavior,button);caption_palette(button,28);}
static void selected_state(void *behavior,void *button){native_selected(behavior,button);caption_palette(button,29);}
static void display(Entry *e,int active){
    if(e->active==active)return;
    if(active){activate_button(e->button);activate_text(e->label);InterlockedIncrement(&activated_count);}
    else{deactivate_text(e->label);deactivate_button(e->button);}
    e->active=active;
}
static void release_controls(void *button,void *label,void *resources,int active){
    /* Native deleting destructors free object memory only. Unregister and run
       resource cleanup first; our extra row is outside the six layout slots. */
    if(label){if(active)deactivate_text(label);cleanup_text(label,resources);text_destroy(label,1);}
    if(button){if(active)deactivate_button(button);cleanup_button(button,resources);native_destroy(button,1);}
}
static void *button_destroy(void *button,unsigned int flags){
    /* Native deleting destructor resets its vtable before cleaning children;
       remove our weak owner / anchor references before that reset. */
    for(unsigned i=0;i<16;i++)if(entries[i].button==button)memset(&entries[i],0,sizeof(entries[i]));
    InterlockedIncrement(&destroyed_count);
    return native_destroy(button,flags);
}
static int anchor_rect(void *owner,void *anchor,int out[4],unsigned *flags){
    unsigned char a[0x48],p[0x48];
    if(!read_at((uintptr_t)owner,p,sizeof(p)) || pointer((uintptr_t)owner)!=image+0x1357dc8 ||
       !read_at((uintptr_t)anchor,a,sizeof(a)) || pointer((uintptr_t)anchor)!=image+0x12d9ef0)return 0;
    memcpy(out,a+0x20,16);memcpy(flags,a+0x40,4);
    int width,height;memcpy(&width,p+0x28,4);memcpy(&height,p+0x2c,4);
    for(int i=0;i<4;i++)InterlockedExchange(&anchor_geometry[i],out[i]);
    InterlockedExchange(&parent_geometry[0],width);InterlockedExchange(&parent_geometry[1],height);
    /* One native row below 裁决; follow the lower arc of the sidebar. */
    out[0]-=28;out[1]+=out[3]+20;
    return out[0]>=0 && out[1]>=0 && out[2]>=80 && out[2]<=240 && out[3]>=32 && out[3]<=96 &&
           out[0]+out[2]<=width && out[1]+out[3]<=height;
}
static void sync(Entry *e){
    int r[4];unsigned flags=0;
    if(!anchor_rect(e->owner,e->anchor,r,&flags)){visible(e->button,0);return;}
    move(e->button,r[0],r[1]);visible(e->button,(flags&1)!=0);button_enabled(e->button,(flags&4)!=0);
    int alpha=255;read_at((uintptr_t)e->anchor+0x30,&alpha,4);
    opacity(e->button,alpha);
}
static void add_button(void *owner){
    if(entry(owner))return;
    Entry *free_entry=NULL;for(unsigned i=0;i<16;i++)if(!entries[i].owner){free_entry=&entries[i];break;}
    void *anchor=(void*)pointer((uintptr_t)owner+0x1c0);int r[4];unsigned flags;
    if(!free_entry){progress(CAPACITY);return;}
    if(!anchor_rect(owner,anchor,r,&flags)){progress(WAIT_GEOMETRY);return;}
    void *resources=(void*)pointer((uintptr_t)context()+0x210);
    if(!resources)return;
    if(GetTickCount64()<retry_after)return;
    retry_after=GetTickCount64()+1000;
    void *behavior=allocate(8);if(!behavior){progress(ALLOC_BEHAVIOR);return;}
    *(uintptr_t*)behavior=(uintptr_t)behavior_type.methods;
    void *button=allocate(0x190);
    if(!button){behavior_destroy(behavior,1);progress(ALLOC_BUTTON);return;}
    memset(button,0,0x190);construct(button);
    *(uintptr_t*)button=(uintptr_t)button_type.methods;
    /* The native initializer registers the child with its parent, owns the
       behavior and creates the native button's hit-test and text controls. */
    if(!initialize_menu_button(button,r[0],r[1],owner,behavior)){
        if(pointer((uintptr_t)button+0x178)!=(uintptr_t)behavior)behavior_destroy(behavior,1);
        release_controls(button,NULL,resources,0);progress(INIT_BUTTON);return;
    }
    /* Match MainCmdLayoutBase: short native initialization, then use resource
       node 1 as the mouse area, then apply the command row's exact extent. */
    bind_input(button,1);
    button_width(button,r[2]);button_height(button,r[3]);
    button_extent(button,((uint64_t)(unsigned)r[3]<<32)|(unsigned)r[2]);
    /* Main-command labels are atlas images, not wchar_t text. Suppress that
       image and add a real native CCmnText child over the same native skin. */
    void *label=allocate(0x138);
    if(!label){release_controls(button,NULL,resources,0);progress(ALLOC_LABEL);return;}
    construct_text(label);
    *(uintptr_t*)label=(uintptr_t)text_type.methods;
    if(!initialize_text(label,0,0,r[2],r[3],button)){
        release_controls(button,label,resources,0);progress(INIT_LABEL);return;
    }
    /* Match the atlas titles' visible cap height; the 32-point body font was
       visibly smaller in the user's screenshot. Glyphs remain native text. */
    text(label,caption);text_size(label,42);text_align(label,4);
    /* A caption renders but must not win mouse hit testing over its button. */
    button_enabled(label,0);
    layout_visible((char*)button+0x128,20,0);
    /* No command index, command callbacks or original six-row arrays change.
       Clicking this placeholder only uses the native button's visual states. */
    *free_entry=(Entry){owner,button,anchor,label,0};caption_palette(button,25);sync(free_entry);
    /* Parent attachment is not native display registration. The resource
       layout activates only its bound six command rows, so activate our two
       independent native widgets explicitly, once, on the game's UI thread. */
    display(free_entry,1);
    retry_after=0;InterlockedIncrement(&created_count);progress(CREATED);
}
static void menu_cleanup_hook(void *owner,void *resources){
    DWORD error=GetLastError();Entry *e=entry(owner);
    if(e){Entry owned=*e;memset(e,0,sizeof(*e));
        release_controls(owned.button,owned.label,resources,owned.active);InterlockedIncrement(&destroyed_count);}
    SetLastError(error);original_cleanup(owner,resources);
}
static int menu_init_hook(void *owner,int x,int y,void *parent){
    int result=original_init(owner,x,y,parent);DWORD error=GetLastError();
    InterlockedIncrement(&init_calls);
    if(result && pointer((uintptr_t)owner)==image+0x1357dc8)add_button(owner);
    SetLastError(error);return result;
}
static void menu_update_hook(void *owner){
    original_update(owner);DWORD error=GetLastError();InterlockedIncrement(&update_calls);
    /* Resource layout binding / animation can finish after initialization.
       Retry on the native UI thread once the actual anchor bounds are ready,
       including menus that existed before hooks were enabled. */
    if(!entry(owner) && pointer((uintptr_t)owner)==image+0x1357dc8)add_button(owner);
    Entry *e=entry(owner);if(e)sync(e);SetLastError(error);
}
static void menu_show_hook(void *owner){
    original_show(owner);DWORD error=GetLastError();Entry *e=entry(owner);
    if(e){sync(e);display(e,1);}SetLastError(error);
}
static void menu_hide_hook(void *owner){
    DWORD error=GetLastError();Entry *e=entry(owner);if(e)display(e,0);
    SetLastError(error);original_hide(owner);
}
int s14_maneuver_button_next_log(char *out,size_t capacity){
    static LONG last_phase=-1,last_init=-1,last_update=-1,last_created=-1,last_destroyed=-1,last_hover=-1,last_press=-1,last_draw=-1;
    LONG state=InterlockedCompareExchange(&phase,0,0),inits=InterlockedCompareExchange(&init_calls,0,0),
         updates=InterlockedCompareExchange(&update_calls,0,0),created=InterlockedCompareExchange(&created_count,0,0),
         destroyed=InterlockedCompareExchange(&destroyed_count,0,0);
    LONG hovers=InterlockedCompareExchange(&hover_count,0,0),presses=InterlockedCompareExchange(&press_count,0,0);
    LONG draws=!!InterlockedCompareExchange(&draw_count,0,0);
    if(state==last_phase && !!inits==last_init && !!updates==last_update && created==last_created && destroyed==last_destroyed && hovers==last_hover && presses==last_press && draws==last_draw)return 0;
    static const char *names[]={"waiting_menu","waiting_geometry","created","capacity","behavior_allocation_failed","button_allocation_failed","button_init_failed","label_allocation_failed","label_init_failed"};
    int n=snprintf(out,capacity,"{\"event\":\"native_maneuver_button_state\",\"state\":\"%s\",\"init_calls\":%ld,\"update_calls\":%ld,\"created\":%ld,\"destroyed\":%ld,\"activated\":%ld,\"hover\":%ld,\"pressed\":%ld,\"draw_after_anchor\":%ld,\"anchor\":[%ld,%ld,%ld,%ld],\"parent_size\":[%ld,%ld]}\n",
        names[state],inits,updates,created,destroyed,activated_count,hovers,presses,draw_count,anchor_geometry[0],anchor_geometry[1],anchor_geometry[2],anchor_geometry[3],parent_geometry[0],parent_geometry[1]);
    if(n<=0 || (size_t)n>=capacity)return 0;
    last_phase=state;last_init=!!inits;last_update=!!updates;last_created=created;last_destroyed=destroyed;last_hover=hovers;last_press=presses;last_draw=draws;return 1;
}
int s14_maneuver_button_install(uintptr_t base,uintptr_t end){
    static const struct {uintptr_t rva;unsigned char bytes[8];unsigned n;} anchors[]={
        {0x76dfb0,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b},8},
        {0x784630,{0x40,0x53,0x48,0x83,0xec,0x20,0x8b,0x41},8},
        {0x788310,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b},8},
        {0x797b50,{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41},8},
        {0x7a7520,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x7a6670,{0x48,0x89,0x5c,0x24,0x08},5},
        {0x793c90,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x794a50,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x13ee90,{0x48,0x8b,0x05,0xe9,0x4e,0x8e,0x01,0xc3},8},
        {0x43bec0,{0xe9,0x1b,0x02,0x34,0x00},5},
        {0x542bb0,{0xe9,0x3b,0xc0,0x23,0x00},5},
        {0x7741f0,{0x48,0x89,0x5c,0x24,0x08},5},
        {0x77c140,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x77ec20,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x78d480,{0x48,0xc7,0x81,0x28,0x01,0x00,0x00,0x00},8},
        {0x3a54d0,{0x48,0x89,0x5c,0x24,0x08},5},
        {0x763cb0,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x767950,{0x48,0x89,0x5c,0x24,0x18},5},
        {0x767b00,{0x40,0x57,0x48,0x83,0xec,0x50},6},
        {0x770d30,{0x48,0x89,0x5c,0x24,0x10},5},
        {0x830fb0,{0x48,0x85,0xc9},3},
        {0x788aa0,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x125f0,{0x83,0x61,0x40,0xfb,0xf7,0xda},6},
        {0x77a300,{0x48,0x89,0x4c,0x24,0x08},5},
        {0x7822a0,{0x48,0x89,0x5c,0x24,0x08},5},
        {0x77b240,{0x40,0x57,0x48,0x83,0xec,0x30},6},
        {0x78aff0,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x789700,{0x48,0x89,0x5c,0x24,0x08},5},
        {0x78b030,{0x48,0x8b,0x89,0x28,0x01,0x00,0x00},7},
        {0x788fb0,{0x48,0x8b,0x89,0x28,0x01,0x00,0x00},7},
        {0x8309d0,{0x85,0xd2,0x78,0x72},4},
        {0x78b550,{0x48,0x83,0xb9,0x98,0x00,0x00,0x00,0x00},8},
        {0x789740,{0x48,0x83,0xb9,0x98,0x00,0x00,0x00,0x00},8},
        {0x770dd0,{0x48,0x8b,0x81,0x70,0x01,0x00,0x00},7},
        {0x76c800,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x789ca0,{0x48,0x83,0xec,0x28},4},
        {0x789820,{0x40,0x53,0x48,0x83,0xec,0x20},6}
    };
    unsigned char b[8];if(!base || end<=base || end-base<0x1358800)return 0;
    for(unsigned i=0;i<sizeof(anchors)/sizeof(anchors[0]);i++)
        if(!read_at(base+anchors[i].rva,b,anchors[i].n) || memcmp(b,anchors[i].bytes,anchors[i].n))return 0;
    if(!read_at(base+0x12d9ee8,&button_type,sizeof(button_type)) ||
       !read_at(base+0x1357af0,&behavior_type,sizeof(behavior_type)) ||
       !read_at(base+0x13546a8,&text_type,sizeof(text_type)) ||
       button_type.rtti<base || button_type.rtti>=end || behavior_type.rtti<base || behavior_type.rtti>=end ||
       button_type.methods[0]!=base+0x74d480 || button_type.methods[50]!=base+0x767950 ||
       button_type.methods[5]!=base+0x7741f0 || button_type.methods[7]!=base+0x43bec0 || button_type.methods[8]!=base+0x542bb0 ||
       button_type.methods[3]!=base+0x76dfb0 || text_type.methods[3]!=base+0x788310 ||
       text_type.rtti<base || text_type.rtti>=end ||
       button_type.methods[51]!=base+0x767b00 || behavior_type.methods[0]!=base+0x536220 ||
       button_type.methods[25]!=base+0x125f0 || button_type.methods[52]!=base+0x770d30 ||
       behavior_type.methods[5]!=base+0x7a2fe0 || behavior_type.methods[6]!=base+0x7a2730 ||
       behavior_type.methods[7]!=base+0x7a3b70 || behavior_type.methods[9]!=base+0x7a1ed0 ||
       behavior_type.methods[10]!=base+0x7a2120 ||
       behavior_type.methods[4]!=base+0x795de0 || pointer(base+0x13546b0)!=base+0x77b240 ||
       pointer(base+0x13546b0+5*8)!=base+0x78d480 || pointer(base+0x13546b0+7*8)!=base+0x77c140 ||
       pointer(base+0x13546b0+8*8)!=base+0x77ec20)return 0;
    image=base;allocate=(Allocate)(base+0x3a54d0);construct=(Construct)(base+0x763cb0);
    visibility_result=(Render)(base+0x784630);native_text_render=(Update)(base+0x788310);
    button_type.methods[3]=(uintptr_t)deferred_button_render;
    text_type.methods[3]=(uintptr_t)deferred_text_render;
    native_destroy=(Destroy)button_type.methods[0];
    initialize_menu_button=(MenuButtonInit)(base+0x767b00);bind_input=(Setting)(base+0x770d30);
    apply_palette=(Setting)(base+0x830fb0);opacity=(Setting)(base+0x788aa0);
    native_normal=(State)behavior_type.methods[5];native_hover=(State)behavior_type.methods[6];
    native_pressed=(State)behavior_type.methods[7];native_disabled=(State)behavior_type.methods[9];native_selected=(State)behavior_type.methods[10];
    behavior_type.methods[5]=(uintptr_t)normal_state;behavior_type.methods[6]=(uintptr_t)hover_state;
    behavior_type.methods[7]=(uintptr_t)pressed_state;behavior_type.methods[9]=(uintptr_t)disabled_state;behavior_type.methods[10]=(uintptr_t)selected_state;
    behavior_destroy=(Destroy)behavior_type.methods[0];move=(Move)(base+0x789ca0);
    visible=(Visible)(base+0x789820);text=(Text)(base+0x78aff0);button_type.methods[0]=(uintptr_t)button_destroy;
    construct_text=(Construct)(base+0x77a300);initialize_text=(TextInit)(base+0x7822a0);
    text_size=(Setting)(base+0x789700);text_align=(Setting)(base+0x78b030);
    layout_visible=(LayoutVisible)(base+0x8309d0);text_destroy=(Destroy)(base+0x77b240);
    button_width=(Setting)(base+0x78b550);button_height=(Setting)(base+0x789740);
    button_extent=(Extent)(base+0x770dd0);button_enabled=(Setting)(base+0x125f0);
    activate_button=(Update)(base+0x43bec0);deactivate_button=(Update)(base+0x542bb0);
    activate_text=(Update)(base+0x77c140);deactivate_text=(Update)(base+0x77ec20);
    cleanup_button=(Cleanup)(base+0x7741f0);cleanup_text=(Cleanup)(base+0x78d480);context=(Context)(base+0x13ee90);
    behavior_type.methods[3]=(uintptr_t)no_command_label;
    struct {uintptr_t rva;void *hook;void **original;} hooks[]={
        {0x797b50,menu_init_hook,(void**)&original_init},{0x7a7520,menu_update_hook,(void**)&original_update},
        {0x7a6670,menu_cleanup_hook,(void**)&original_cleanup},{0x793c90,menu_show_hook,(void**)&original_show},
        {0x794a50,menu_hide_hook,(void**)&original_hide},
        {0x76dfb0,command_render_hook,(void**)&original_button_render}
    };
    for(unsigned i=0;i<sizeof(hooks)/sizeof(hooks[0]);i++)if(MH_CreateHook((void*)(base+hooks[i].rva),hooks[i].hook,hooks[i].original)!=MH_OK){
        while(i)MH_RemoveHook((void*)(base+hooks[--i].rva));return 0;
    }
    return 1;
}
