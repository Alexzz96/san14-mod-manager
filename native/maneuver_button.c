#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "MinHook.h"
#include "maneuver_button.h"
#include "person_entry.h"
#include "map_effects_model.h"
#include "map_render.h"
#include "tech_tree_ui.h"

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
typedef uintptr_t (*Dispatch)(void*,int,void*);
typedef void (*Scale)(void*,float,float);
typedef void (*Category)(void*,int);
typedef int (*CategoryReset)(void*,int,int);
typedef void (*Animation)(void*,const wchar_t*,int,float);
typedef int (*AnimationPlaying)(void*,const wchar_t*);
static Dispatch original_dispatch;
static Render original_button_render,visibility_result;
static Update native_button_update,native_text_update,native_text_render;
static MenuInit original_init;
static Update original_update;
static Cleanup original_cleanup,cleanup_button,cleanup_text;
static Update activate_button,deactivate_button,activate_text,deactivate_text;
static Context context;
static Allocate allocate;
static Construct construct;
static MenuButtonInit initialize_menu_button;
static State native_normal,native_hover,native_pressed,native_disabled,native_selected;
static Setting bind_input,opacity;
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
static Extent button_extent,mouse_offset;
static Scale layout_scale;
static Setting button_selected;
static Category native_category,original_populate,native_mask_position;
static Setting native_restore_map_controls;
static Update native_clear_submenu;
static Category native_submenu_background;
static CategoryReset original_category_reset;
static State original_main_tooltip,original_submenu_tooltip;
static Text native_tooltip;
static State link_tooltip_button;
static Construct construct_layout;
static Update construct_layout_resource,activate_layout,deactivate_layout;
static MenuInit initialize_cursor;
static Animation animate_cursor;
static AnimationPlaying animation_playing;
static Cleanup cleanup_layout;
static Destroy destroy_cursor;
static Render render_cursor;
static uintptr_t image;
/* Private tables retain native RTTI, rendering, hit testing and deletion. */
static struct {uintptr_t rtti,methods[65];} button_type;
static struct {uintptr_t rtti,methods[16];} behavior_type;
static struct {uintptr_t rtti,methods[46];} text_type;
static struct {uintptr_t rtti,methods[49];} cursor_type;
static void (*submenu_set_title)(void*,void*,const wchar_t*);
typedef struct {void *owner,*button,*anchor,*label,*original_title,*submenu_button,*submenu_behavior,*native_panel,*native_outer,*native_links,*cursor;uintptr_t *title_slot;int active,pending,submenu_open,submenu_update_pending,button_update_pending,text_update_pending,native_linked,borrowed_position,cursor_active,tooltip_linked,detail_return,image_caption;unsigned return_root_flags,return_owner_flags;int submenu_rect[4];float cursor_scale[2];} Entry;
static Entry *opening_entry;
static int anchor_rect(void*,void*,int[4],unsigned*);
static void set_submenu_visible(Entry*,int);
static Entry entries[16]; /* Only used by the native UI thread. */
enum {WAIT_MENU,WAIT_GEOMETRY,CREATED,CAPACITY,ALLOC_BEHAVIOR,ALLOC_BUTTON,INIT_BUTTON,ALLOC_LABEL,INIT_LABEL,BIND_INPUT,SCALE_BUTTON,CREATE_LABEL};
static volatile LONG phase,init_calls,update_calls,created_count,destroyed_count,activated_count;
static volatile LONG hover_count,press_count;
static volatile LONG draw_count,cursor_draw_count,image_caption_count,fallback_caption_count;
static volatile LONG main_click_count,submenu_click_count,submenu_open_count,detail_request_count;
static volatile LONG map_return_count;
static volatile LONG native_update_count,fallback_update_count;
static ULONGLONG retry_after; /* Allocation failure retries are bounded. UI thread only. */
static volatile LONG native_link_failure;
static volatile uintptr_t native_panel_vtable_seen,native_links_vtable_seen;
static volatile LONG anchor_geometry[4],parent_geometry[2];
static void progress(LONG value){InterlockedExchange(&phase,value);}
static const wchar_t caption[]=L"演武";
static const wchar_t submenu_caption[]=L"武将详情";
static const wchar_t officer_hint[]=L"查看自势力武将清单及武将详情。";
/* +0x200/+0x208 belong to the tooltip. The actual submenu is owned by
   CMainCmdLayout, above this base layout in the native parent chain. */
static uintptr_t pointer(uintptr_t at);
static int register_native_main_command(Entry *e){
    native_link_failure=1;
    if(!e||!e->owner||!e->button)return 0;
    void *outer=(void*)pointer((uintptr_t)e->owner+8);
    if(pointer((uintptr_t)outer)!=image+0x13580e8 || pointer((uintptr_t)outer+0x178)!=(uintptr_t)e->owner){native_link_failure=3;return 0;}
    void *panel=(void*)pointer((uintptr_t)outer+0x170);
    void *links=(void*)pointer((uintptr_t)outer+0x168);
    if(!panel||!links){native_link_failure=2;return 0;}
    native_panel_vtable_seen=pointer((uintptr_t)panel);
    native_links_vtable_seen=pointer((uintptr_t)links);
    if(native_panel_vtable_seen!=image+0x1357f58 || native_links_vtable_seen!=image+0x1357968){native_link_failure=4;return 0;}
    e->native_panel=panel;e->native_links=links;e->native_outer=outer;e->native_linked=1;
    native_link_failure=0;
    return 1;
}
static int game_memory(void *unused,uintptr_t at,void *out,size_t n){(void)unused;SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got)&&got==n;}
static void no_command_label(void *behavior,void *button,int command){(void)behavior;(void)button;(void)command;}
static int read_at(uintptr_t at,void *out,size_t n){SIZE_T got=0;return at && ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static uintptr_t pointer(uintptr_t at){uintptr_t n=0;read_at(at,&n,8);return n;}
static Entry *entry(void *owner){for(unsigned i=0;i<16;i++)if(entries[i].owner==owner)return &entries[i];return NULL;}
static Entry *button_entry(void *button){for(unsigned i=0;i<16;i++)if(entries[i].button==button)return &entries[i];return NULL;}
static void main_tooltip_hook(void *owner,void *button){
    Entry *e=entry(owner);
    if(e && e->button==button){native_tooltip((void*)pointer((uintptr_t)owner+0x200),officer_hint);return;}
    original_main_tooltip(owner,button);
}
static void submenu_tooltip_hook(void *links,void *button){
    for(unsigned i=0;i<16;i++)if(entries[i].native_links==links && entries[i].submenu_open && entries[i].submenu_button==button){
        native_tooltip((void*)pointer((uintptr_t)links+0x2b8),officer_hint);return;
    }
    original_submenu_tooltip(links,button);
}
static void sync_cursor(Entry *e){
    if(!e->cursor)return;
    void *stock=(void*)pointer((uintptr_t)e->owner+0x168+5*8);int r[4],a[4];float scale[2]={1,1};
    if(pointer((uintptr_t)stock)!=image+0x1357c38 || !read_at((uintptr_t)stock+0x20,r,sizeof(r)) || !read_at((uintptr_t)e->anchor+0x20,a,sizeof(a)))return;
    /* +0x18 is a pointer, not two floats. The native scale setter writes the
       transform at +0x108, offsets +0x10/+0x14 (verified in 0x78a880). */
    uintptr_t transform=pointer((uintptr_t)stock+0x108);
    if(transform&&read_at(transform+0x10,scale,sizeof(scale))&&scale[0]>0&&scale[0]<=8&&scale[1]>0&&scale[1]<=8&&
       (e->cursor_scale[0]!=scale[0]||e->cursor_scale[1]!=scale[1])){
        layout_scale(e->cursor,scale[0],scale[1]);memcpy(e->cursor_scale,scale,sizeof(scale));
    }
    int own[4]={0};read_at((uintptr_t)e->cursor+0x20,own,sizeof(own));
    if(own[0]!=r[0]||own[1]!=r[1]+a[3]+20)move(e->cursor,r[0],r[1]+a[3]+20);
    if(own[2]!=r[2])button_width(e->cursor,r[2]);if(own[3]!=r[3])button_height(e->cursor,r[3]);
}
static int create_cursor(Entry *e){
    if(e->cursor)return 1;
    void *stock=(void*)pointer((uintptr_t)e->owner+0x168+5*8);
    if(pointer((uintptr_t)stock)!=image+0x1357c38)return 0;
    void *p=allocate(0x168);if(!p)return 0;
    memset(p,0,0x168);construct_layout(p);construct_layout_resource((char*)p+0x128);*(uintptr_t*)p=(uintptr_t)cursor_type.methods;
    if(!initialize_cursor(p,0,0,e->owner)){
        cleanup_layout(p,(void*)pointer((uintptr_t)context()+0x210));destroy_cursor(p,1);return 0;
    }
    e->cursor=p;sync_cursor(e);visible(p,0);
    if(e->active){activate_layout(p);e->cursor_active=1;}
    return 1;
}
static void cursor_hover(Entry *e,int show){
    if(!e)return;
    if(!show){if(e->cursor)visible(e->cursor,0);return;}
    if(!create_cursor(e))return;
    sync_cursor(e);visible(e->cursor,1);
    /* Exact animation and arguments used by native 0x7a4e10. Its resource
       and native timeline supply the stock breathing halo. */
    animate_cursor(e->cursor,(const wchar_t*)pointer(image+0x190e860),0,1.f);
}
static void release_cursor(Entry *e,void *resources){
    if(!e->cursor)return;
    visible(e->cursor,0);if(e->cursor_active)deactivate_layout(e->cursor);
    cleanup_layout(e->cursor,resources);destroy_cursor(e->cursor,1);e->cursor=NULL;
}
/* Native behavior writes its title palette through layout node 20. Point that
   slot at the native CCmnText font, so the ORIGINAL behavior colors the caption.
   This is a weak binding only: restore the original image before cleanup. */
static void bind_caption(Entry *e,int bind){
    if(e->title_slot)*e->title_slot=bind?pointer((uintptr_t)e->label+0x128):(uintptr_t)e->original_title;
}
static void hide_atlas_caption(Entry *e){
    bind_caption(e,0);layout_visible((char*)e->button+0x128,20,0);bind_caption(e,1);
}
static void added_button_update(void *button){
    for(unsigned i=0;i<16;i++)if(entries[i].button==button){entries[i].button_update_pending=0;break;}
    native_button_update(button);
    InterlockedIncrement(&native_update_count);
}
static void added_text_update(void *label){
    for(unsigned i=0;i<16;i++)if(entries[i].label==label){entries[i].text_update_pending=0;break;}
    native_text_update(label);
}
/* The native menu's mask widgets delimit a draw interval. Display registration
   alone appends our controls after that interval. Queue their native graphics
   directly after the last original command, while the same mask is active.
   Their own UI-list render callbacks must not queue a second unmasked copy. */
static int deferred_button_render(void *button){return visibility_result(button);}
static void deferred_text_render(void *label){(void)label;}
/* Native hit testing sums +0x20 positions through +8 parents (up to 30).
   The PNG must follow that same animated menu carrier, not local row space. */
static int caption_position(Entry *e,float *x,float *y,float *alpha){
    uintptr_t at=(uintptr_t)e->button;int r[4];
    if(!read_at(at+0x20,r,sizeof(r))||r[2]<=0||r[3]<=0)return 0;
    *x=r[2]*.5f;*y=r[3]*.5f;*alpha=1;
    for(int depth=0;at&&depth<30;depth++){
        unsigned char data[0x44];int pos[2],opacity_value;unsigned flags;uintptr_t parent=0;
        if(!read_at(at,data,sizeof(data)))return 0;
        memcpy(pos,data+0x20,sizeof(pos));memcpy(&opacity_value,data+0x30,4);memcpy(&flags,data+0x40,4);memcpy(&parent,data+8,8);
        if((flags&3)!=3||opacity_value<0||opacity_value>255||parent==at)return 0;
        *x+=pos[0];*y+=pos[1];*alpha*=opacity_value/255.f;at=parent;
    }
    return !at&&*alpha>0&&*alpha<=1;
}
static int command_render_hook(void *button){
    int result=original_button_render(button);DWORD error=GetLastError();
    for(unsigned i=0;i<16;i++)if(entries[i].anchor==button&&entries[i].active){
        Entry *e=&entries[i];
        if(result && visibility_result(e->button)){
            /* The native menu updater advances its own animation. The added row
               must also receive the native 0x774A20 update before drawing. If
               the UI controller already updated it, do not advance it twice. */
            if(e->button_update_pending){added_button_update(e->button);InterlockedIncrement(&fallback_update_count);}
            if(e->text_update_pending)added_text_update(e->label);
            hide_atlas_caption(e);
            if(e->cursor && visibility_result(e->cursor)&&render_cursor(e->cursor))InterlockedIncrement(&cursor_draw_count);
            original_button_render(e->button);
            float x=0,y=0,alpha=0;ULONGLONG now=GetTickCount64();
            int position_ok=caption_position(e,&x,&y,&alpha);
            /* Visible native two-character glyphs occupy about 80x28 within
               the 160x64 title slot. Keep the PNG's transparent UV crop. */
            /* Shift only the image ink, preserving native input and glow geometry. */
            s14_map_caption_publish(x-6,y,80,28,alpha,position_ok,now);
            /* Keep the original native render callback/order. Suppress glyphs
               only after the GPU has actually painted the PNG. Failure or a
               stopped Present path restores native text automatically. */
            int image_caption=position_ok&&s14_map_caption_drawn(now);
            if(e->image_caption!=image_caption){text(e->label,image_caption?L"":caption);e->image_caption=image_caption;}
            native_text_render(e->label);
            InterlockedIncrement(image_caption?&image_caption_count:&fallback_caption_count);
            InterlockedIncrement(&draw_count);
        }else s14_map_caption_publish(0,0,0,0,0,0,GetTickCount64());
        break;
    }
    SetLastError(error);return result;
}
/* Native accepted click event, emitted by the original release/confirm path.
   Preserve native dispatch; defer opening to the next menu update. */
static uintptr_t click_dispatch_hook(void *container,int code,void *sender){
    if(s14_tech_ui_dispatch(container,code,sender))return 0;
    if(code==0x7ffffffc && (char*)container==(char*)sender+0x70)cursor_hover(button_entry(sender),0);
    /* Borrowed submenu controls already have a stock command listener.
       Consume their accepted click BEFORE dispatch, so 武将详情 can never
       accidentally execute the original 提案 command. */
    if(code==0x7ffffffe)for(unsigned i=0;i<16;i++){
        Entry *e=&entries[i];
        if(e->active && e->submenu_open && e->submenu_button==sender &&
           (char*)container==(char*)sender+0x70){
            unsigned flags=0;
            if(read_at((uintptr_t)sender+0x40,&flags,4) && (flags&7)==7 && !e->pending){e->pending=2;InterlockedIncrement(&submenu_click_count);}
            return 0;
        }
    }
    uintptr_t result=original_dispatch(container,code,sender);DWORD error=GetLastError();
    if(code==0x7ffffffe){
        for(unsigned i=0;i<16;i++){
            Entry *e=&entries[i];
            if(e->active && e->button==sender && (char*)container==(char*)e->button+0x70){
                unsigned flags=0;
                if(read_at((uintptr_t)e->button+0x40,&flags,4) && (flags&7)==7 && !e->pending){
                    e->pending=1;InterlockedIncrement(&main_click_count);
                }
                break;
            }
        }
    }
    SetLastError(error);return result;
}
/* These wrappers only record evidence. All visual behavior stays in the
   original five game callbacks; no custom tint, interpolation or animation. */
static void normal_state(void *behavior,void *button){native_normal(behavior,button);cursor_hover(button_entry(button),0);}
static void hover_state(void *behavior,void *button){native_hover(behavior,button);cursor_hover(button_entry(button),1);InterlockedIncrement(&hover_count);}
static void pressed_state(void *behavior,void *button){native_pressed(behavior,button);InterlockedIncrement(&press_count);}
static void disabled_state(void *behavior,void *button){native_disabled(behavior,button);cursor_hover(button_entry(button),0);}
static void selected_state(void *behavior,void *button){native_selected(behavior,button);}
static void display(Entry *e,int active){
    if(e->active==active)return;
    if(active){bind_caption(e,0);activate_button(e->button);activate_text(e->label);hide_atlas_caption(e);InterlockedIncrement(&activated_count);}
    else{e->pending=0;set_submenu_visible(e,0);s14_map_caption_publish(0,0,0,0,0,0,GetTickCount64());bind_caption(e,0);deactivate_text(e->label);deactivate_button(e->button);bind_caption(e,1);}
    if(e->cursor){visible(e->cursor,0);if(active&&!e->cursor_active){activate_layout(e->cursor);e->cursor_active=1;}else if(!active&&e->cursor_active){deactivate_layout(e->cursor);e->cursor_active=0;}}
    e->active=active;
}
static void release_controls(void *button,void *label,void *resources,int active){
    /* Native deleting destructors free object memory only. Unregister and run
       resource cleanup first; our extra row is outside the six layout slots. */
    if(label){if(active)deactivate_text(label);cleanup_text(label,resources);text_destroy(label,1);}
    if(button){if(active)deactivate_button(button);cleanup_button(button,resources);native_destroy(button,1);}
}
static void release_submenu(Entry *e,void *resources){
    /* This is a weak reference to a stock child. Its native owner alone
       unregisters, cleans and deletes it, including scene teardown. */
    (void)resources;e->submenu_button=NULL;e->submenu_open=e->borrowed_position=0;
}
static int create_submenu(Entry *e){
    if(!register_native_main_command(e))return 0;
    for(unsigned i=0;i<14;i++){
        void *button=(void*)pointer((uintptr_t)e->native_links+0x168+i*8);
        void *alert=(void*)pointer((uintptr_t)e->native_links+0x1d8+i*8);
        if(pointer((uintptr_t)button)!=image+0x134ccb8 || !pointer((uintptr_t)alert)){native_link_failure=5;return 0;}
    }
    /* Slot 10 is the game's first 裁决 submenu row. Reuse its resource-bound
       button, font, input registration, and render order, without changing
       the fourteen-element pointer array or creating another paper widget. */
    e->submenu_button=(void*)pointer((uintptr_t)e->native_links+0x168+10*8);
    return 1;
}
static void restore_submenu_position(Entry *e){
    if(e->borrowed_position){move(e->submenu_button,e->submenu_rect[0],e->submenu_rect[1]);e->borrowed_position=0;}
}
static void populate_hook(void *links,int category){
    for(unsigned i=0;i<16;i++)if(entries[i].native_links==links)restore_submenu_position(&entries[i]);
    Entry *e=opening_entry;
    if(!e || e->native_links!=links){original_populate(links,category);return;}
    /* Reuse only native visual resources. Do not populate 提案 or run its
       availability checks / command metadata when opening the extension. */
    native_clear_submenu(links);native_submenu_background(links,5);
    for(unsigned i=0;i<14;i++)visible((void*)pointer((uintptr_t)links+0x168+i*8),0);
    for(unsigned i=0;i<14;i++)visible((void*)pointer((uintptr_t)links+0x1d8+i*8),0);
    read_at((uintptr_t)e->submenu_button+0x20,e->submenu_rect,sizeof(e->submenu_rect));
    int r[4];unsigned flags=0;
    if(!anchor_rect(e->owner,e->anchor,r,&flags))return;
    /* Retain the stock row width and text styling. The native CSubCmd mask
       supplies its curved left edge; the row follows the seventh category. */
    move(e->submenu_button,e->submenu_rect[0]-40,e->submenu_rect[1]+r[3]+20);
    e->borrowed_position=1;
    submenu_set_title((void*)pointer((uintptr_t)e->submenu_button+0x178),e->submenu_button,submenu_caption);
    button_enabled(e->submenu_button,1);visible(e->submenu_button,1);
}
static int category_reset_hook(void *outer,int next,int immediate){
    int result=original_category_reset(outer,next,immediate);
    if(result)for(unsigned i=0;i<16;i++)if(entries[i].native_outer==outer && entries[i].submenu_open){
        entries[i].submenu_open=0;button_selected(entries[i].button,0);
    }
    return result;
}
static void set_submenu_visible(Entry *e,int show){
    if(e->submenu_open==show)return;
    if(!show){
        if(pointer((uintptr_t)e->native_outer)==image+0x13580e8 && pointer((uintptr_t)e->native_outer+0x168)==(uintptr_t)e->native_links)
            category_reset_hook(e->native_outer,-1,0);
        e->submenu_open=0;return;
    }
    if(!create_submenu(e))return;
    int current=-1;read_at((uintptr_t)e->native_outer+0x180,&current,4);
    if(current==5)category_reset_hook(e->native_outer,-1,1);
    /* The stock entry validates category <=5. Use its existing category-5
       transition with a scoped content override, then store the extension's
       index 6. Native reset explicitly skips original arrays for index >5. */
    opening_entry=e;native_category(e->native_outer,5);opening_entry=NULL;
    *(int*)((char*)e->native_outer+0x180)=6;
    native_mask_position(e->native_panel,6);
    e->submenu_open=1;
}
static void *button_destroy(void *button,unsigned int flags){
    /* Native deleting destructor resets its vtable before cleaning children;
       remove our weak owner / anchor references before that reset. */
    for(unsigned i=0;i<16;i++)if(entries[i].button==button){bind_caption(&entries[i],0);memset(&entries[i],0,sizeof(entries[i]));}
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
    if(out[0]<28||out[1]<32)return 0; /* Resource geometry is not bound yet. */
    /* Extend the original native paper beneath the curved mask on both sides.
       The title and mouse area retain their original center and row dimensions. */
    out[0]-=76;out[1]+=out[3]+20;out[2]+=96;
    return out[0]>=-48 && out[1]>=0 && out[2]>=176 && out[2]<=336 && out[3]>=32 && out[3]<=96 &&
           out[0]+out[2]<=width && out[1]+out[3]<=height;
}
static void sync(Entry *e){
    int r[4];unsigned flags=0;
    if(!anchor_rect(e->owner,e->anchor,r,&flags)){visible(e->button,0);return;}
    move(e->button,r[0],r[1]);visible(e->button,(flags&1)!=0);button_enabled(e->button,(flags&4)!=0);
    /* 0x77d670 sums parent positions. The 48px local margin retains the original
       click area while the native paper extends to the left beneath the mask. */
    mouse_offset(e->button,48);
    int alpha=255;read_at((uintptr_t)e->anchor+0x30,&alpha,4);
    opacity(e->button,alpha);
    sync_cursor(e);
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
    progress(BIND_INPUT);bind_input(button,1);
    *(int*)((char*)button+0x80)=6;*(int*)((char*)button+0x84)=1;
    progress(SCALE_BUTTON);
    /* Use CCmnSwitchButton vtable slot 43 (0x78A880). 0x78A8F0 is
       CCmnPaneControl-only: +0x88 is not a layout node in this button. */
    layout_scale(button,r[2]/160.f,1.f);
    button_width(button,r[2]);button_height(button,r[3]);
    button_extent(button,((uint64_t)(unsigned)r[3]<<32)|(unsigned)(r[2]-96));
    /* Replace the atlas image title with an original native CCmnText child. */
    progress(CREATE_LABEL);void *label=allocate(0x138);
    if(!label){release_controls(button,NULL,resources,0);progress(ALLOC_LABEL);return;}
    construct_text(label);
    *(uintptr_t*)label=(uintptr_t)text_type.methods;
    if(!initialize_text(label,48,0,r[2]-96,r[3],button)){
        release_controls(button,label,resources,0);progress(INIT_LABEL);return;
    }
    /* Draw and color the title entirely with native CCmnText. */
    text(label,caption);text_size(label,42);text_align(label,4);
    button_enabled(label,0);*(unsigned*)((char*)label+0x40)|=8;
    uintptr_t *slots=(uintptr_t*)pointer((uintptr_t)button+0x130);
    void *font=(void*)pointer((uintptr_t)label+0x128);unsigned char kind=0;int count=0;
    if(!read_at((uintptr_t)button+0x138,&count,4)||count!=21||!slots||!font||
       !read_at((uintptr_t)font+0x112,&kind,1)||kind!=2){
        release_controls(button,label,resources,0);progress(INIT_LABEL);return;
    }
    *free_entry=(Entry){.owner=owner,.button=button,.anchor=anchor,.label=label,
        .original_title=(void*)slots[20],.title_slot=&slots[20],.button_update_pending=1,.text_update_pending=1};
    (void)register_native_main_command(free_entry);
    void *tips=(void*)pointer((uintptr_t)owner+0x208);
    if(pointer((uintptr_t)tips)==image+0x12cc488 && pointer(pointer((uintptr_t)owner+0x200))==image+0x1296a28){link_tooltip_button(tips,button);free_entry->tooltip_linked=1;}
    hide_atlas_caption(free_entry);native_normal(behavior,button);sync(free_entry);
    /* Parent attachment is not native display registration. The resource
       layout activates only its bound six command rows, so activate our two
       independent native widgets explicitly, once, on the game's UI thread. */
    display(free_entry,1);
    retry_after=0;InterlockedIncrement(&created_count);progress(CREATED);
}
static void menu_cleanup_hook(void *owner,void *resources){
    DWORD error=GetLastError();Entry *e=entry(owner);
    if(e){Entry owned=*e;if(e->detail_return)s14_person_entry_cancel_return();s14_map_caption_publish(0,0,0,0,0,0,GetTickCount64());bind_caption(e,0);memset(e,0,sizeof(*e));
        release_submenu(&owned,resources);release_cursor(&owned,resources);release_controls(owned.button,owned.label,resources,owned.active);InterlockedIncrement(&destroyed_count);}
    SetLastError(error);original_cleanup(owner,resources);
}
static int menu_init_hook(void *owner,int x,int y,void *parent){
    int result=original_init(owner,x,y,parent);DWORD error=GetLastError();
    InterlockedIncrement(&init_calls);
    if(result && pointer((uintptr_t)owner)==image+0x1357dc8)add_button(owner);
    SetLastError(error);return result;
}
static void restore_map_menu(Entry *e){
    /* Restore the original native carrier, never synthesize replacement map
       controls. The parent hide unregisters its resource children; changing
       visibility alone cannot put those widgets back in the native UI list. */
    if(pointer((uintptr_t)e->native_outer)!=image+0x13580e8 ||
       pointer((uintptr_t)e->native_outer+0x178)!=(uintptr_t)e->owner ||
       pointer((uintptr_t)e->owner)!=image+0x1357dc8)return;
    unsigned flags=0;
    if((e->return_root_flags&3)==3){
        visible(e->native_outer,1);
        if(read_at((uintptr_t)e->native_outer+0x40,&flags,4)&&!(flags&2))activate_layout(e->native_outer);
    }
    if((e->return_owner_flags&3)==3){
        visible(e->owner,1);
        if(read_at((uintptr_t)e->owner+0x40,&flags,4)&&!(flags&2))original_show(e->owner);
        if(!e->active)display(e,1);
    }
    native_restore_map_controls(e->native_outer,1);
    button_selected(e->button,0);cursor_hover(e,0);sync(e);
    InterlockedIncrement(&map_return_count);
}
static void menu_update_hook(void *owner){
    original_update(owner);DWORD error=GetLastError();InterlockedIncrement(&update_calls);
    /* Resource layout binding / animation can finish after initialization.
       Retry on the native UI thread once the actual anchor bounds are ready,
       including menus that existed before hooks were enabled. */
    if(!entry(owner) && pointer((uintptr_t)owner)==image+0x1357dc8)add_button(owner);
    Entry *e=entry(owner);if(e){
        if(e->detail_return){int result=s14_person_entry_return_ready();if(result){e->detail_return=0;if(result>0)restore_map_menu(e);}}
        sync(e);e->button_update_pending=e->text_update_pending=1;if(e->submenu_open)e->submenu_update_pending=1;if(e->active && e->pending){
        int action=e->pending;e->pending=0;
        if(action==1){set_submenu_visible(e,!e->submenu_open);button_selected(e->button,e->submenu_open);if(e->submenu_open)InterlockedIncrement(&submenu_open_count);}
        else if(action==2){set_submenu_visible(e,0);button_selected(e->button,0);e->pending=3;}
        else if(action==3){
            /* Finish the stock close transition before pushing another native
               information page. A concurrent close would restore the map over
               the newly opened page, producing a one-frame flash. */
            if(animation_playing(e->native_links,(const wchar_t*)pointer(image+0x190d378)) ||
               animation_playing(e->owner,(const wchar_t*)pointer(image+0x190daa0)))e->pending=3;
            else if(s14_map_main_scene(game_memory,NULL,image)){
                read_at((uintptr_t)e->native_outer+0x40,&e->return_root_flags,4);
                read_at((uintptr_t)e->owner+0x40,&e->return_owner_flags,4);
                InterlockedIncrement(&detail_request_count);e->detail_return=s14_person_entry_open();
            }
        }
    }}SetLastError(error);
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
    static LONG last_cursor=-1,last_image=-1;
    static LONG last_phase=-1,last_init=-1,last_update=-1,last_created=-1,last_destroyed=-1,last_hover=-1,last_press=-1,last_draw=-1,last_main_click=-1,last_sub_click=-1,last_sub_open=-1,last_detail=-1,last_native_links=-1,last_link_failure=-1,last_returns=-1;
    LONG state=InterlockedCompareExchange(&phase,0,0),inits=InterlockedCompareExchange(&init_calls,0,0),
         updates=InterlockedCompareExchange(&update_calls,0,0),created=InterlockedCompareExchange(&created_count,0,0),
         destroyed=InterlockedCompareExchange(&destroyed_count,0,0);
    LONG hovers=InterlockedCompareExchange(&hover_count,0,0),presses=InterlockedCompareExchange(&press_count,0,0);
    LONG main_clicks=InterlockedCompareExchange(&main_click_count,0,0),sub_clicks=InterlockedCompareExchange(&submenu_click_count,0,0),sub_open=InterlockedCompareExchange(&submenu_open_count,0,0),detail_requests=InterlockedCompareExchange(&detail_request_count,0,0);
    LONG native_links=0;for(unsigned i=0;i<16;i++)if(entries[i].owner&&entries[i].native_linked)native_links++;
    LONG link_failure=InterlockedCompareExchange(&native_link_failure,0,0);
    uintptr_t panel_vtable=(uintptr_t)InterlockedCompareExchange64((volatile LONG64*)&native_panel_vtable_seen,0,0);
    uintptr_t links_vtable=(uintptr_t)InterlockedCompareExchange64((volatile LONG64*)&native_links_vtable_seen,0,0);
    LONG returns=InterlockedCompareExchange(&map_return_count,0,0);
    LONG draws=!!InterlockedCompareExchange(&draw_count,0,0);
    LONG cursor_draws=InterlockedCompareExchange(&cursor_draw_count,0,0),png_frames=InterlockedCompareExchange(&image_caption_count,0,0);
    if(state==last_phase && !!inits==last_init && !!updates==last_update && created==last_created && destroyed==last_destroyed && hovers==last_hover && presses==last_press && draws==last_draw && main_clicks==last_main_click && sub_clicks==last_sub_click && sub_open==last_sub_open && detail_requests==last_detail && native_links==last_native_links && link_failure==last_link_failure && returns==last_returns&&!!cursor_draws==last_cursor&&!!png_frames==last_image)return 0;
    static const char *names[]={"waiting_menu","waiting_geometry","created","capacity","behavior_allocation_failed","button_allocation_failed","button_init_failed","label_allocation_failed","label_init_failed","binding_input","scaling_button","creating_label"};
    int n=snprintf(out,capacity,"{\"event\":\"native_maneuver_button_state\",\"state\":\"%s\",\"init_calls\":%ld,\"update_calls\":%ld,\"created\":%ld,\"destroyed\":%ld,\"activated\":%ld,\"hover\":%ld,\"pressed\":%ld,\"draw_after_anchor\":%ld,\"main_clicks\":%ld,\"submenu_clicks\":%ld,\"submenu_opened\":%ld,\"detail_requests\":%ld,\"map_returns\":%ld,\"native_panel_links\":%ld,\"native_link_failure\":%ld,\"panel_vtable_seen\":\"0x%llx\",\"links_vtable_seen\":\"0x%llx\",\"native_cursor_draws\":%ld,\"png_caption_frames\":%ld,\"text_fallback_frames\":%ld,\"native_updates\":%ld,\"fallback_updates\":%ld,\"native_caption\":true,\"native_submenu\":true,\"anchor\":[%ld,%ld,%ld,%ld],\"parent_size\":[%ld,%ld]}\n",
        names[state],inits,updates,created,destroyed,activated_count,hovers,presses,draw_count,main_clicks,sub_clicks,sub_open,detail_requests,returns,native_links,link_failure,(unsigned long long)panel_vtable,(unsigned long long)links_vtable,cursor_draws,png_frames,fallback_caption_count,native_update_count,fallback_update_count,anchor_geometry[0],anchor_geometry[1],anchor_geometry[2],anchor_geometry[3],parent_geometry[0],parent_geometry[1]);
    if(n<=0 || (size_t)n>=capacity)return 0;
    last_phase=state;last_init=!!inits;last_update=!!updates;last_created=created;last_destroyed=destroyed;last_hover=hovers;last_press=presses;last_draw=draws;last_main_click=main_clicks;last_sub_click=sub_clicks;last_sub_open=sub_open;last_detail=detail_requests;last_native_links=native_links;last_link_failure=link_failure;last_returns=returns;last_cursor=!!cursor_draws;last_image=!!png_frames;return 1;
}
int s14_maneuver_button_install(uintptr_t base,uintptr_t end){
    static const struct {uintptr_t rva;unsigned char bytes[8];unsigned n;} anchors[]={
        {0x797890,{0x40,0x53,0x48,0x83,0xec,0x40,0x33,0xc0},8},
        {0x82beb0,{0x33,0xc0,0x48,0x89,0x01,0x48,0x89,0x41},8},
        {0x77a0b0,{0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83},8},
        {0x787a70,{0x48,0x81,0xc1,0xa0,0x00,0x00,0x00,0xe9},8},
        {0x7841c0,{0x48,0x83,0xec,0x28,0x48,0x81,0xc1,0xa0},8},
        {0x7b0c50,{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74},8},
        {0x793db0,{0x48,0x89,0x5c,0x24,0x18,0x56,0x48,0x83},8},
        {0x7940a0,{0x40,0x53,0x48,0x83,0xec,0x20,0x45,0x33},8},
        {0x7944b0,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c},8},
        {0x7a8400,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c},8},
        {0x79f950,{0x83,0xfa,0x05,0x0f,0x87,0x02,0x02,0x00},8},
        {0x7a5a10,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83},8},
        {0x794700,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74},8},
        {0x7a3fc0,{0x40,0x57,0x48,0x83,0xec,0x20,0x48,0x89},8},
        {0x7a2e00,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74},8},
        {0x77fd10,{0x48,0x83,0xec,0x28,0x48,0x8b,0x41,0x08},8},
        {0x77c550,{0x4c,0x89,0x44,0x24,0x18,0x55,0x56,0x57},8},
        {0x7ac980,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74},8},
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
        {0x770da0,{0x48,0x89,0x54,0x24,0x10,0x48,0x83,0xec},8},
        {0x78a880,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x774a20,{0x40,0x53,0x48,0x83,0xec,0x20},6},
        {0x771910,{0x89,0x91,0x88,0x01,0x00,0x00},6},
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
    if(pointer(base+0x13580e8+7*8)!=base+0x43bec0 ||
       pointer(base+0x13580e8+24*8)!=base+0x789820 ||
       pointer(base+0x1357dc8+7*8)!=base+0x793c90)return 0;
    if(!read_at(base+0x1357c30,&cursor_type,sizeof(cursor_type)) ||
       cursor_type.rtti<base || cursor_type.rtti>=end || cursor_type.methods[0]!=base+0x793560 ||
       cursor_type.methods[2]!=base+0x78de90 || cursor_type.methods[3]!=base+0x788350 ||
       cursor_type.methods[5]!=base+0x78d9b0 || cursor_type.methods[7]!=base+0x77c0e0 ||
       cursor_type.methods[8]!=base+0x77ebf0 || cursor_type.methods[48]!=base+0x797890)return 0;
    if(!read_at(base+0x12d9ee8,&button_type,sizeof(button_type)) ||
       !read_at(base+0x1357af0,&behavior_type,sizeof(behavior_type)) ||
       !read_at(base+0x13546a8,&text_type,sizeof(text_type)) ||
       button_type.rtti<base || button_type.rtti>=end || behavior_type.rtti<base || behavior_type.rtti>=end ||
       button_type.methods[0]!=base+0x74d480 || button_type.methods[50]!=base+0x767950 ||
       button_type.methods[5]!=base+0x7741f0 || button_type.methods[7]!=base+0x43bec0 || button_type.methods[8]!=base+0x542bb0 ||
       button_type.methods[3]!=base+0x76dfb0 || text_type.methods[3]!=base+0x788310 ||
       text_type.rtti<base || text_type.rtti>=end ||
       button_type.methods[43]!=base+0x78a880 || button_type.methods[51]!=base+0x767b00 || behavior_type.methods[0]!=base+0x536220 ||
       button_type.methods[25]!=base+0x125f0 || button_type.methods[52]!=base+0x770d30 ||
       behavior_type.methods[5]!=base+0x7a2fe0 || behavior_type.methods[6]!=base+0x7a2730 ||
       behavior_type.methods[7]!=base+0x7a3b70 || behavior_type.methods[9]!=base+0x7a1ed0 ||
       behavior_type.methods[10]!=base+0x7a2120 ||
       behavior_type.methods[4]!=base+0x795de0 || pointer(base+0x13546b0)!=base+0x77b240 ||
       pointer(base+0x13546b0+5*8)!=base+0x78d480 || pointer(base+0x13546b0+7*8)!=base+0x77c140 ||
       pointer(base+0x13546b0+8*8)!=base+0x77ec20)return 0;
    uintptr_t submenu_button_methods[64];
    uintptr_t *submenu_behavior_methods=(uintptr_t*)(base+0x13578c8);
    if(!read_at(base+0x134ccb8,submenu_button_methods,sizeof(submenu_button_methods)) ||
       submenu_button_methods[0]!=base+0x764530 || submenu_button_methods[2]!=base+0x774a20 ||
       submenu_button_methods[3]!=base+0x76dfb0 || submenu_button_methods[43]!=base+0x78a880 ||
       submenu_button_methods[51]!=base+0x767b00 ||
       submenu_behavior_methods[0]!=base+0x538290 || submenu_behavior_methods[2]!=base+0x7a4360 ||
       submenu_behavior_methods[4]!=base+0x795e10 || submenu_behavior_methods[5]!=base+0x7a30b0 ||
       submenu_behavior_methods[6]!=base+0x7a2800 || submenu_behavior_methods[7]!=base+0x7a3c40 ||
       submenu_behavior_methods[9]!=base+0x7a1fa0 || submenu_behavior_methods[10]!=base+0x7a21f0)return 0;
    if(!s14_person_entry_init(base,end))return 0;
    image=base;allocate=(Allocate)(base+0x3a54d0);construct=(Construct)(base+0x763cb0);
    construct_layout=(Construct)(base+0x77a0b0);construct_layout_resource=(Update)(base+0x82beb0);
    initialize_cursor=(MenuInit)(base+0x797890);destroy_cursor=(Destroy)cursor_type.methods[0];render_cursor=(Render)cursor_type.methods[3];
    cleanup_layout=(Cleanup)cursor_type.methods[5];activate_layout=(Update)cursor_type.methods[7];deactivate_layout=(Update)cursor_type.methods[8];
    cursor_type.methods[3]=(uintptr_t)deferred_button_render;
    animate_cursor=(Animation)(base+0x787a70);animation_playing=(AnimationPlaying)(base+0x7841c0);
    native_tooltip=(Text)(base+0x7b0c50);link_tooltip_button=(State)(base+0x7ac980);
    visibility_result=(Render)(base+0x784630);
    native_button_update=(Update)button_type.methods[2];native_text_update=(Update)text_type.methods[2];native_text_render=(Update)(base+0x788310);
    submenu_set_title=(void(*)(void*,void*,const wchar_t*))(base+0x7a4360);
    button_type.methods[2]=(uintptr_t)added_button_update;text_type.methods[2]=(uintptr_t)added_text_update;
    button_type.methods[3]=(uintptr_t)deferred_button_render;
    text_type.methods[3]=(uintptr_t)deferred_text_render;
    native_destroy=(Destroy)button_type.methods[0];
    initialize_menu_button=(MenuButtonInit)(base+0x767b00);bind_input=(Setting)(base+0x770d30);
    native_category=(Category)(base+0x79f950);native_mask_position=(Category)(base+0x7a2e00);
    native_restore_map_controls=(Setting)(base+0x7a3fc0);
    native_clear_submenu=(Update)(base+0x7944b0);native_submenu_background=(Category)(base+0x7a8400);
    opacity=(Setting)(base+0x788aa0);
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
    mouse_offset=(Extent)(base+0x770da0);layout_scale=(Scale)button_type.methods[43];button_selected=(Setting)(base+0x771910);
    activate_button=(Update)(base+0x43bec0);deactivate_button=(Update)(base+0x542bb0);
    activate_text=(Update)(base+0x77c140);deactivate_text=(Update)(base+0x77ec20);
    cleanup_button=(Cleanup)(base+0x7741f0);cleanup_text=(Cleanup)(base+0x78d480);context=(Context)(base+0x13ee90);
    behavior_type.methods[3]=(uintptr_t)no_command_label;
    struct {uintptr_t rva;void *hook;void **original;} hooks[]={
        {0x797b50,menu_init_hook,(void**)&original_init},{0x7a7520,menu_update_hook,(void**)&original_update},
        {0x7a6670,menu_cleanup_hook,(void**)&original_cleanup},{0x793c90,menu_show_hook,(void**)&original_show},
        {0x794a50,menu_hide_hook,(void**)&original_hide},
        {0x76dfb0,command_render_hook,(void**)&original_button_render},
        {0x77fd10,click_dispatch_hook,(void**)&original_dispatch},
        {0x7a5a10,populate_hook,(void**)&original_populate},
        {0x794700,category_reset_hook,(void**)&original_category_reset},
        {0x793db0,main_tooltip_hook,(void**)&original_main_tooltip},
        {0x7940a0,submenu_tooltip_hook,(void**)&original_submenu_tooltip}
    };
    for(unsigned i=0;i<sizeof(hooks)/sizeof(hooks[0]);i++)if(MH_CreateHook((void*)(base+hooks[i].rva),hooks[i].hook,hooks[i].original)!=MH_OK){
        while(i)MH_RemoveHook((void*)(base+hooks[--i].rva));return 0;
    }
    return 1;
}
