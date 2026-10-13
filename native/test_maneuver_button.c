#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "maneuver_button.c"
int s14_tech_ui_dispatch(void *c,int e,void *s){(void)c;(void)e;(void)s;return 0;}
int s14_person_entry_init(uintptr_t base,uintptr_t end){(void)base;(void)end;return 1;}
static int officer_opens,officer_return_ready,officer_cancelled,map_restores,map_parent_activations;
int s14_person_entry_open(void){officer_opens++;return 1;}
int s14_person_entry_return_ready(void){int ready=officer_return_ready;officer_return_ready=0;return ready;}
void s14_person_entry_cancel_return(void){officer_cancelled++;}
int s14_map_main_scene(S14OfficerRead read,void *context,uintptr_t base){(void)read;(void)context;(void)base;return 1;}
static unsigned char owner[0x2c0],anchor[0x190],unrelated_button[0x190];
static unsigned char native_outer[0x198],native_panel[0x178],native_links[0x2e0];
static unsigned char stock_subbuttons[14][0x188],stock_alerts[14][0x168],stock_behaviors[14][0x10];
static void *allocated_button,*allocated_behavior,*allocated_label;
static void *allocated_cursor;
static unsigned char stock_cursor[0x168],stock_cursor_transform[0x28],tip_controller[0x80],tip_balloon[0x170];
static int cursor_scale_calls;
static int cursor_active,cursor_animations,cursor_cleanups,cursor_fail_init,closing_animation,tooltip_calls,stock_tooltip_calls,tooltip_regs;
static int allocations,fail_at,fail_init,fail_text,destroyed,labels,updates,last_visible,last_enabled;
static int position[2],font_size,alignment;static wchar_t title[8];
static unsigned char fake_context[0x218];static int resource_context;
static int button_active,label_active,button_activations,label_activations,cleanups;
static int native_category_calls,native_reset_calls,native_populate_calls,native_mask_calls,dispatch_calls,detail_calls;
static wchar_t stock_title[16];
static int input_bound,label_mouse_disabled,last_alpha,native_states[5];
static unsigned char font_object[0x120];
static uintptr_t native_title_slots[21];
static int render_order[8],render_length,render_mask,render_visibility;
static int visibility_mock(void *p){if(p==allocated_cursor)return !!(*(unsigned*)((char*)p+0x40)&1);assert(p==allocated_button);return render_visibility;}
static void native_button_update_mock(void *p){assert(p==allocated_button);}
static void native_text_update_mock(void *p){assert(p==allocated_label);}
static int render_mock(void *p){
    assert(render_mask==1);
    if(p==anchor){render_order[render_length++]=1;SetLastError(241);return render_visibility;}
    if(p==unrelated_button){render_order[render_length++]=5;return 0;}
    assert(p==allocated_button);render_order[render_length++]=2;return 1;
}
static void text_render_mock(void *p){assert(p==allocated_label&&render_mask==1);render_order[render_length++]=3;}
static float caption_x,caption_y,caption_width,caption_height,caption_alpha;static int caption_published,caption_hidden,caption_gpu_active;
void s14_map_caption_publish(float x,float y,float width,float height,float alpha,int show,ULONGLONG now){(void)now;if(show){caption_x=x;caption_y=y;caption_width=width;caption_height=height;caption_alpha=alpha;caption_published++;}else{caption_hidden++;caption_gpu_active=0;}}
int s14_map_caption_drawn(ULONGLONG now){(void)now;return caption_gpu_active;}
static void putp(void *p,uintptr_t n){memcpy(p,&n,8);}
static void setrect(void *p,int x,int y,int w,int h){int r[]={x,y,w,h};memcpy((char*)p+0x20,r,16);}
static void *alloc_mock(size_t n){allocations++;if(allocations==fail_at)return NULL;void *p=calloc(1,n);assert(p);if(n==8)allocated_behavior=p;else if(n==0x190)allocated_button=p;else if(n==0x168)allocated_cursor=p;else{assert(n==0x138);allocated_label=p;}return p;}
static void *construct_mock(void *p){assert(p==allocated_button);return p;}
static void *construct_text_mock(void *p){assert(p==allocated_label);return p;}
static int init_mock(void *b,int x,int y,void *p,void *behavior){assert(b==allocated_button && p==owner && behavior==allocated_behavior);assert(x==-3 && y==781);assert(pointer((uintptr_t)b)==(uintptr_t)button_type.methods);setrect(b,x,y,256,64);putp((char*)b+0x178,(uintptr_t)behavior);putp((char*)b+0x130,(uintptr_t)native_title_slots);*(int*)((char*)b+0x138)=21;native_title_slots[20]=(uintptr_t)b;*(unsigned*)((char*)b+0x40)=7;return !fail_init;}
static int text_init_mock(void *l,int x,int y,int w,int h,void *parent){assert(l==allocated_label && parent==allocated_button && x==48 && !y && w==160 && h==64);putp((char*)l+0x128,(uintptr_t)&font_object);return !fail_text;}
static void *behavior_destroy_mock(void *p,unsigned f){assert(p==allocated_behavior && f==1);free(p);allocated_behavior=NULL;return NULL;}
static void *text_destroy_mock(void *p,unsigned f){assert(p==allocated_label && f==1);free(p);allocated_label=NULL;return NULL;}
static void *destroy_mock(void *p,unsigned f){assert(p==allocated_button && f==1 && !allocated_behavior && !allocated_label && !button_active && !label_active);destroyed++;free(p);allocated_button=NULL;return NULL;}
static void *context_mock(void){return fake_context;}
static void activate_button_mock(void *p){assert(p==allocated_button && entry(owner) && !button_active && labels>=1 && !wcscmp(title,L"演武"));button_active=1;button_activations++;}
static void activate_text_mock(void *p){assert(p==allocated_label && button_active && !label_active);label_active=1;label_activations++;}
static void deactivate_button_mock(void *p){assert(p==allocated_button && button_active && !label_active);button_active=0;}
static void deactivate_text_mock(void *p){assert(p==allocated_label && label_active && button_active);label_active=0;}
static void cleanup_button_mock(void *p,void *resources){assert(p==allocated_button && resources==&resource_context && !button_active && !allocated_label);if(allocated_behavior)behavior_destroy_mock(allocated_behavior,1);}
static void cleanup_text_mock(void *p,void *resources){assert(p==allocated_label && resources==&resource_context && !label_active);}
static void menu_cleanup_mock(void *p,void *resources){assert(p==owner && resources==&resource_context && !entry(owner) && !allocated_button && !allocated_label);cleanups++;SetLastError(219);}
static void close_menu(void){menu_cleanup_hook(owner,&resource_context);assert(GetLastError()==219);}
static void move_mock(void *p,int x,int y){if(p==stock_subbuttons[10]||p==allocated_cursor){*(int*)((char*)p+0x20)=x;*(int*)((char*)p+0x24)=y;return;}assert(p==allocated_button);position[0]=x;position[1]=y;*(int*)((char*)p+0x20)=x;*(int*)((char*)p+0x24)=y;}
static void visible_mock(void *p,int show){if(p==allocated_button){last_visible=show;return;}if(p==allocated_cursor||p==owner||p==native_outer){unsigned *f=(unsigned*)((char*)p+0x40);*f=(*f&~1u)|(show?1u:0u);return;}for(int i=0;i<14;i++)if(p==stock_subbuttons[i]||p==stock_alerts[i]){unsigned *f=(unsigned*)((char*)p+0x40);*f=(*f&~1u)|(show?1u:0u);return;}assert(0);}
static void enabled_mock(void *p,int enable){
    if(p==allocated_label){assert(!enable);label_mouse_disabled++;return;}
    assert(p==allocated_button||p==stock_subbuttons[10]);if(p==allocated_button)last_enabled=enable;
    unsigned *flags=(unsigned*)((char*)p+0x40);*flags=(*flags&~4u)|(enable?4u:0u);
}
static void input_mock(void *p,int node){assert(p==allocated_button && node==1);input_bound++;}
static void sub_title_mock(void *behavior,void *button,const wchar_t *value){assert(button==stock_subbuttons[10]&&behavior==stock_behaviors[10]);wcscpy(stock_title,value);}
static void selected_set_mock(void *p,int selected){assert(p==allocated_button);*(int*)((char*)p+0x188)=selected;}
static int reset_mock(void *root,int next,int immediate){assert(root==native_outer);(void)next;(void)immediate;native_reset_calls++;*(int*)(native_outer+0x180)=-1;return 1;}
static void populate_mock(void *links,int category){assert(links==native_links&&category>=0&&category<=5);native_populate_calls++;wcscpy(stock_title,L"提案");for(int i=0;i<14;i++){*(unsigned*)(stock_subbuttons[i]+0x40)=6;*(unsigned*)(stock_alerts[i]+0x40)=0;}}
static void clear_mock(void *links){assert(links==native_links);for(int i=0;i<14;i++){*(unsigned*)(stock_subbuttons[i]+0x40)=6;*(unsigned*)(stock_alerts[i]+0x40)=0;}stock_title[0]=0;}
static void background_mock(void *links,int category){assert(links==native_links&&category==5);}
static int animation_mock(void *p,const wchar_t *name){assert(p==owner||p==native_links);(void)name;return closing_animation;}
static void tooltip_mock(void *balloon,const wchar_t *value){assert(balloon==tip_balloon&&!wcscmp(value,officer_hint));tooltip_calls++;}
static void stock_tip_mock(void *owner_ptr,void *button){(void)owner_ptr;(void)button;stock_tooltip_calls++;}
static void tip_register_mock(void *controller,void *button){assert(controller==tip_controller&&button==allocated_button);tooltip_regs++;}
static void *cursor_construct_mock(void *p){assert(p==allocated_cursor);return p;}
static void cursor_resource_mock(void *p){assert(p==(char*)allocated_cursor+0x128);}
static int cursor_init_mock(void *p,int x,int y,void *parent){assert(p==allocated_cursor&&parent==owner&&!x&&!y&&pointer((uintptr_t)p)==(uintptr_t)cursor_type.methods);*(unsigned*)((char*)p+0x40)=8;return !cursor_fail_init;}
static void cursor_activate_mock(void *p){if(p==native_outer){assert((*(unsigned*)(native_outer+0x40)&3)==1);*(unsigned*)(native_outer+0x40)|=2;map_parent_activations++;return;}assert(p==allocated_cursor&&!cursor_active);cursor_active=1;}
static void cursor_deactivate_mock(void *p){assert(p==allocated_cursor&&cursor_active);cursor_active=0;}
static void cursor_cleanup_mock(void *p,void *resources){assert(p==allocated_cursor&&resources==&resource_context&&!cursor_active);cursor_cleanups++;}
static void *cursor_destroy_mock(void *p,unsigned flag){assert(p==allocated_cursor&&flag==1&&!cursor_active);free(p);allocated_cursor=NULL;return NULL;}
static void cursor_animate_mock(void *p,const wchar_t *name,int repeat,float rate){assert(p==allocated_cursor&&!repeat&&rate==1.f&&cursor_active&&visibility_mock(p));(void)name;cursor_animations++;}
static int cursor_render_mock(void *p){assert(p==allocated_cursor&&render_mask==1);render_order[render_length++]=4;return 1;}
static void category_mock(void *root,int category){assert(root==native_outer&&category==5);native_category_calls++;category_reset_hook(root,category,0);populate_hook(native_links,category);*(int*)(native_outer+0x180)=category;}
static void mask_mock(void *panel,int category){assert(panel==native_panel&&category==6);native_mask_calls++;}
static uintptr_t dispatch_mock(void *container,int code,void *sender){(void)container;(void)code;(void)sender;dispatch_calls++;return 77;}
static void opacity_mock(void *p,int value){assert(p==allocated_button);last_alpha=value;*(int*)((char*)p+0x30)=value;}
static void scale_mock(void *p,float x,float y){if(p==allocated_cursor){assert(x==1.f&&y==1.f);cursor_scale_calls++;return;}assert(p==allocated_button && x==1.6f && y==1.f);}
static void native_state_mock(void *behavior,void *button,int state){assert(behavior==allocated_behavior && button==allocated_button);native_states[state]++;}
static void normal_mock(void *behavior,void *button){native_state_mock(behavior,button,0);}
static void hover_mock(void *behavior,void *button){native_state_mock(behavior,button,1);}
static void pressed_mock(void *behavior,void *button){native_state_mock(behavior,button,2);}
static void disabled_mock(void *behavior,void *button){native_state_mock(behavior,button,3);}
static void selected_mock(void *behavior,void *button){native_state_mock(behavior,button,4);}
static void menu_display_mock(void *p){assert(p==owner);*(unsigned*)(owner+0x40)|=2;*(unsigned*)(anchor+0x40)|=2;}
static void menu_hide_mock(void *p){assert(p==owner);*(unsigned*)(owner+0x40)&=~2u;*(unsigned*)(anchor+0x40)&=~2u;}
static void map_restore_mock(void *p,int show){assert(p==native_outer&&show==1&&entry(owner)->active);map_restores++;}
static void text_mock(void *p,const wchar_t *s){assert(p==allocated_label);wcscpy(title,s);labels++;}
static void size_mock(void *p,int v){assert(p==allocated_label);font_size=v;}
static void align_mock(void *p,int v){assert(p==allocated_label);alignment=v;}
static void hide_mock(void *p,int i,int show){assert(p==(char*)allocated_button+0x128 && i==20 && !show);}
static void width_mock(void *p,int v){if(p==allocated_cursor){assert(v==332);*(int*)((char*)p+0x28)=v;return;}assert(p==allocated_button && v==256);}
static void height_mock(void *p,int v){if(p==allocated_cursor){assert(v==80);*(int*)((char*)p+0x2c)=v;return;}assert(p==allocated_button&&v==64);}
static void extent_mock(void *p,uint64_t v){assert(p==allocated_button && v==((uint64_t)64<<32|160));}
static void mouse_offset_mock(void *p,uint64_t v){assert(p==allocated_button && v==48);}
static int menu_mock(void *p,int x,int y,void *parent){assert(p==owner && !x && !y && !parent);SetLastError(157);return 1;}
static void update_mock(void *p){assert(p==owner);updates++;SetLastError(197);}
static void fixture(void){
    assert(!allocated_button && !allocated_behavior && !allocated_label&&!allocated_cursor);memset(owner,0,sizeof(owner));memset(anchor,0,sizeof(anchor));memset(entries,0,sizeof(entries));
    image=0x140000000;putp(owner,image+0x1357dc8);putp(anchor,image+0x12d9ef0);putp(owner+0x1c0,(uintptr_t)anchor);
    memset(native_outer,0,sizeof(native_outer));memset(native_panel,0,sizeof(native_panel));memset(native_links,0,sizeof(native_links));memset(native_title_slots,0,sizeof(native_title_slots));memset(font_object,0,sizeof(font_object));font_object[0x112]=2;
    putp(owner+8,(uintptr_t)native_outer);putp(native_outer,image+0x13580e8);putp(native_outer+0x178,(uintptr_t)owner);putp(native_outer+0x170,(uintptr_t)native_panel);putp(native_outer+0x168,(uintptr_t)native_links);*(int*)(native_outer+0x180)=-1;
    putp(native_panel,image+0x1357f58);putp(native_links,image+0x1357968);
    *(unsigned*)(native_outer+0x40)=*(unsigned*)(owner+0x40)=3;
    *(int*)(native_outer+0x30)=*(int*)(owner+0x30)=255;
    memset(stock_subbuttons,0,sizeof(stock_subbuttons));memset(stock_alerts,0,sizeof(stock_alerts));
    for(int i=0;i<14;i++){putp(stock_subbuttons[i],image+0x134ccb8);putp(stock_subbuttons[i]+0x178,(uintptr_t)stock_behaviors[i]);setrect(stock_subbuttons[i],300,697,176,40);putp(stock_alerts[i],image+0x1296898);putp(native_links+0x168+i*8,(uintptr_t)stock_subbuttons[i]);putp(native_links+0x1d8+i*8,(uintptr_t)stock_alerts[i]);}
    setrect(owner,0,0,960,1080);setrect(anchor,73,697,160,64);*(unsigned*)(anchor+0x40)=7;*(int*)(anchor+0x30)=255;
    allocate=alloc_mock;construct=construct_mock;initialize_menu_button=init_mock;native_destroy=destroy_mock;behavior_destroy=behavior_destroy_mock;
    construct_text=construct_text_mock;initialize_text=text_init_mock;text_destroy=text_destroy_mock;text=text_mock;
    move=move_mock;visible=visible_mock;button_enabled=enabled_mock;layout_visible=hide_mock;text_size=size_mock;text_align=align_mock;
    button_width=width_mock;button_height=height_mock;button_extent=extent_mock;original_init=menu_mock;original_update=update_mock;
    layout_scale=scale_mock;
    mouse_offset=mouse_offset_mock;
    context=context_mock;putp(fake_context+0x210,(uintptr_t)&resource_context);
    activate_button=activate_button_mock;activate_text=activate_text_mock;deactivate_button=deactivate_button_mock;deactivate_text=deactivate_text_mock;
    cleanup_button=cleanup_button_mock;cleanup_text=cleanup_text_mock;original_cleanup=menu_cleanup_mock;
    bind_input=input_mock;opacity=opacity_mock;original_show=menu_display_mock;original_hide=menu_hide_mock;native_restore_map_controls=map_restore_mock;
    native_category=category_mock;original_category_reset=reset_mock;original_populate=populate_mock;native_mask_position=mask_mock;original_dispatch=dispatch_mock;submenu_set_title=sub_title_mock;button_selected=selected_set_mock;
    native_clear_submenu=clear_mock;native_submenu_background=background_mock;animation_playing=animation_mock;native_tooltip=tooltip_mock;original_main_tooltip=original_submenu_tooltip=stock_tip_mock;link_tooltip_button=tip_register_mock;
    construct_layout=cursor_construct_mock;construct_layout_resource=cursor_resource_mock;initialize_cursor=cursor_init_mock;activate_layout=cursor_activate_mock;deactivate_layout=cursor_deactivate_mock;cleanup_layout=cursor_cleanup_mock;destroy_cursor=cursor_destroy_mock;animate_cursor=cursor_animate_mock;render_cursor=cursor_render_mock;
    cursor_active=cursor_animations=cursor_cleanups=cursor_fail_init=closing_animation=tooltip_calls=stock_tooltip_calls=tooltip_regs=officer_opens=cursor_scale_calls=0;
    officer_return_ready=officer_cancelled=map_restores=map_parent_activations=map_return_count=0;
    native_category_calls=native_reset_calls=native_populate_calls=native_mask_calls=dispatch_calls=detail_calls=0;opening_entry=NULL;
    native_normal=normal_mock;native_hover=hover_mock;native_pressed=pressed_mock;native_disabled=disabled_mock;native_selected=selected_mock;
    original_button_render=render_mock;visibility_result=visibility_mock;native_text_render=text_render_mock;
    native_button_update=native_button_update_mock;native_text_update=native_text_update_mock;
    render_length=render_mask=draw_count=caption_published=caption_hidden=caption_gpu_active=0;render_visibility=1;
    input_bound=label_mouse_disabled=last_alpha=hover_count=press_count=0;memset(native_states,0,sizeof(native_states));
    button_active=label_active=button_activations=label_activations=cleanups=0;
    allocations=fail_at=fail_init=fail_text=destroyed=labels=updates=0;
    retry_after=0;phase=init_calls=update_calls=created_count=destroyed_count=activated_count=0;
}
int main(void){
    fixture();unsigned char before[sizeof(owner)];memcpy(before,owner,sizeof(owner));
    assert(menu_init_hook(owner,0,0,NULL)==1 && GetLastError()==157 && allocations==3 && labels==1);
    assert(!memcmp(before,owner,sizeof(owner)) && !wcscmp(title,L"演武") && font_size==42 && alignment==4);
    assert(input_bound==1 && label_mouse_disabled==1);
    assert(last_visible && last_enabled && position[0]==-3 && position[1]==781);
    assert(button_active && label_active && button_activations==1 && label_activations==1 && activated_count==1);
    assert(entry(owner)->native_linked && entry(owner)->native_panel==native_panel&&entry(owner)->native_links==native_links);
    assert(native_link_failure==0 && native_panel_vtable_seen==image+0x1357f58 && native_links_vtable_seen==image+0x1357968);
    assert(menu_init_hook(owner,0,0,NULL)==1 && allocations==3);
    /* An opened native mask must bracket anchor, extra paper, and caption.
       The controls' later independent render slots produce no duplicate. */
    render_mask=1;assert(command_render_hook(anchor)==1 && GetLastError()==241);
    render_mask=0;assert(render_length==3 && render_order[0]==1 && render_order[1]==2 && render_order[2]==3 && draw_count==1);
    assert(caption_published==1 && caption_x==119 && caption_y==813 && caption_width==80 && caption_height==28);
    int hidden_before=caption_hidden;render_mask=1;assert(command_render_hook(unrelated_button)==0);render_mask=0;
    assert(caption_hidden==hidden_before&&caption_published==1);render_length=3;
    assert(deferred_button_render(allocated_button)==1);deferred_text_render(allocated_label);assert(render_length==3);
    render_mask=1;render_visibility=0;assert(command_render_hook(anchor)==0 && render_length==4 && draw_count==1);
    render_mask=0;render_visibility=1;render_length=0;
    /* Native input flags survive repeated layout updates: an accidental leave
       callback here used to erase both hover and press on every frame. */
    *(unsigned*)((char*)allocated_button+0x40)|=0x180;
    for(int i=0;i<12;i++)menu_update_hook(owner);
    assert((*(unsigned*)((char*)allocated_button+0x40)&0x180)==0x180);
    hover_state(allocated_behavior,allocated_button);assert(native_states[1]==1 && hover_count==1);
    pressed_state(allocated_behavior,allocated_button);assert(native_states[2]==1 && press_count==1);
    disabled_state(allocated_behavior,allocated_button);assert(native_states[3]==1);
    selected_state(allocated_behavior,allocated_button);assert(native_states[4]==1);
    normal_state(allocated_behavior,allocated_button);assert(native_states[0]==2);
    *(int*)(anchor+0x30)=128;menu_update_hook(owner);assert(last_alpha==128);
    menu_hide_hook(owner);menu_hide_hook(owner);assert(!button_active && !label_active && !entry(owner)->active);
    render_mask=1;command_render_hook(anchor);render_mask=0;assert(render_length==1 && draw_count==1);
    menu_show_hook(owner);menu_show_hook(owner);assert(button_active && label_active && button_activations==2 && label_activations==2);
    updates=0;
    setrect(anchor,83,707,160,64);*(unsigned*)(anchor+0x40)=6;menu_update_hook(owner);
    assert(GetLastError()==197 && updates==1 && !last_visible && last_enabled && position[0]==7 && position[1]==791 && allocations==3);
    *(unsigned*)(anchor+0x40)=3;menu_update_hook(owner);assert(last_visible && !last_enabled);
    setrect(anchor,73,1040,160,64);menu_update_hook(owner);assert(!last_visible);
    close_menu();assert(!entry(owner) && destroyed==1 && !button_active && !label_active && cleanups==1);
    fixture();add_button(owner);menu_hide_hook(owner);close_menu();assert(destroyed==1 && !button_active && !label_active);
    /* During native initialization the resource-bound anchor can still have
       its default origin. A later native update must create the missing row. */
    fixture();setrect(anchor,0,0,160,64);
    assert(menu_init_hook(owner,0,0,NULL)==1 && GetLastError()==157 && !allocations && phase==WAIT_GEOMETRY);
    char diagnostic[1024];assert(s14_maneuver_button_next_log(diagnostic,sizeof(diagnostic)) && strstr(diagnostic,"waiting_geometry"));
    setrect(anchor,73,697,160,64);menu_update_hook(owner);
    assert(GetLastError()==197 && entry(owner) && created_count==1 && allocations==3 && phase==CREATED);
    assert(s14_maneuver_button_next_log(diagnostic,sizeof(diagnostic)) && strstr(diagnostic,"\"created\":1"));
    menu_update_hook(owner);assert(allocations==3 && !s14_maneuver_button_next_log(diagnostic,sizeof(diagnostic)));
    close_menu();assert(destroyed_count==1 && !entry(owner));
    menu_update_hook(owner);assert(entry(owner) && created_count==2 && allocations==6);
    close_menu();assert(activated_count==2 && cleanups==2);
    /* No init callback is required for a menu created before hook activation. */
    fixture();menu_update_hook(owner);assert(entry(owner) && !init_calls && created_count==1);
    close_menu();
    fixture();setrect(owner,0,0,0,0);menu_update_hook(owner);assert(!entry(owner) && !allocations);
    setrect(owner,0,0,960,1080);menu_update_hook(owner);assert(entry(owner));close_menu();
    /* Foreign menus, missing anchors and invalid bounds leave all six original
       commands alone. All partial allocation / initialization failures clean. */
    fixture();putp(owner,image+0x13580e8);assert(menu_init_hook(owner,0,0,NULL)==1 && !allocations);
    fixture();putp(owner+0x1c0,0);add_button(owner);assert(!allocations);
    for(int i=1;i<=3;i++){fixture();fail_at=i;add_button(owner);assert(!entry(owner) && !allocated_button && !allocated_behavior && !allocated_label);}
    fixture();fail_init=1;add_button(owner);assert(!entry(owner) && destroyed==1 && !allocated_behavior);
    fixture();fail_text=1;add_button(owner);assert(!entry(owner) && destroyed==1 && !allocated_label);
    menu_update_hook(owner);assert(allocations==3 && phase==INIT_LABEL); /* no allocation loop each frame */
    /* A real menu carrier differs from the old tooltip fixture. Main click
       uses the native category transition; detail click never dispatches the
       borrowed row's original command. No new submenu widget is allocated. */
    fixture();add_button(owner);Entry *e=entry(owner);unsigned char original_links[sizeof(native_links)];memcpy(original_links,native_links,sizeof(native_links));
    assert(*(int*)((char*)allocated_button+0x80)==6&&*(int*)((char*)allocated_button+0x84)==1);
    assert(click_dispatch_hook((char*)allocated_button+0x70,0x7ffffffe,allocated_button)==77&&e->pending==1);
    menu_update_hook(owner);assert(e->submenu_open&&native_category_calls==1&&native_mask_calls==1&&allocations==3&&!native_populate_calls);
    assert(*(int*)(native_outer+0x180)==6&&!memcmp(original_links,native_links,sizeof(native_links)));
    assert(e->submenu_button==stock_subbuttons[10]&&!wcscmp(stock_title,L"武将详情"));
    assert(*(int*)(stock_subbuttons[10]+0x20)==260&&*(int*)(stock_subbuttons[10]+0x24)==781);
    for(int i=0;i<14;i++)assert((*(unsigned*)(stock_subbuttons[i]+0x40)&1)==(i==10));
    int dispatched=dispatch_calls;assert(click_dispatch_hook(stock_subbuttons[10]+0x70,0x7ffffffe,stock_subbuttons[10])==0&&e->pending==2&&dispatch_calls==dispatched);
    menu_update_hook(owner);assert(!e->submenu_open&&*(int*)(native_outer+0x180)==-1&&e->pending==3&&!officer_opens);
    closing_animation=1;menu_update_hook(owner);menu_update_hook(owner);assert(e->pending==3&&!officer_opens);
    closing_animation=0;menu_update_hook(owner);assert(!e->pending&&officer_opens==1);menu_update_hook(owner);assert(officer_opens==1);
    assert(e->detail_return&&e->return_root_flags==3&&e->return_owner_flags==3);
    menu_hide_hook(owner);*(unsigned*)(native_outer+0x40)=0;*(unsigned*)(owner+0x40)=0;
    menu_update_hook(owner);assert(!e->active&&!button_active&&!map_restores); /* detail still open */
    officer_return_ready=1;menu_update_hook(owner);
    assert(e->active&&button_active&&label_active&&last_visible&&!e->detail_return&&map_restores==1&&map_parent_activations==1&&map_return_count==1);
    menu_update_hook(owner);assert(map_restores==1&&map_parent_activations==1); /* exactly once */
    e->detail_return=1;officer_return_ready=1;menu_update_hook(owner);assert(map_restores==2&&map_parent_activations==1); /* already active parent */
    e->detail_return=1;officer_return_ready=-1;menu_update_hook(owner);assert(!e->detail_return&&map_restores==2); /* changed scene */
    populate_hook(native_links,2);assert(!e->borrowed_position&&*(int*)(stock_subbuttons[10]+0x20)==300&&*(int*)(stock_subbuttons[10]+0x24)==697&&!wcscmp(stock_title,L"提案"));
    assert(click_dispatch_hook(stock_subbuttons[10]+0x70,0x7ffffffe,stock_subbuttons[10])==77&&dispatch_calls==dispatched+1);
    set_submenu_visible(e,1);assert(e->submenu_open);category_reset_hook(native_outer,2,0);assert(!e->submenu_open&&!*(int*)((char*)allocated_button+0x188));populate_hook(native_links,2);
    *(int*)(native_outer+0x180)=5;int resets=native_reset_calls;set_submenu_visible(e,1);assert(e->submenu_open&&native_reset_calls==resets+2);set_submenu_visible(e,0);populate_hook(native_links,5);
    putp(native_outer+0x170,0);set_submenu_visible(e,1);assert(!e->submenu_open&&native_link_failure==2);putp(native_outer+0x170,(uintptr_t)native_panel);
    e->detail_return=1;close_menu();assert(destroyed==1&&officer_cancelled==1); /* no stale return target */
    /* Independent native cursor resource, exact native loop, native tooltip
       override, masked draw order, hide/reenter and separate resource cleanup. */
    fixture();putp(stock_cursor,image+0x1357c38);putp(owner+0x190,(uintptr_t)stock_cursor);setrect(stock_cursor,-25,689,332,80);
    /* Match the real CCmnControl: +0x18 contains a pointer. Scale floats live
       in the transform addressed by +0x108, at +0x10/+0x14. */
    putp(stock_cursor+0x18,image+0x1359788);putp(stock_cursor+0x108,(uintptr_t)stock_cursor_transform);float scale[2]={1,1};memcpy(stock_cursor_transform+0x10,scale,sizeof(scale));
    putp(tip_controller,image+0x12cc488);putp(tip_balloon,image+0x1296a28);putp(owner+0x208,(uintptr_t)tip_controller);putp(owner+0x200,(uintptr_t)tip_balloon);putp(native_links+0x2b8,(uintptr_t)tip_balloon);
    add_button(owner);e=entry(owner);assert(e->tooltip_linked&&tooltip_regs==1);
    hover_state(allocated_behavior,allocated_button);assert(allocated_cursor&&cursor_active&&cursor_animations==1&&visibility_mock(allocated_cursor));assert(*(int*)((char*)allocated_cursor+0x24)==773);
    assert(cursor_scale_calls==1&&*(int*)((char*)allocated_cursor+0x28)==332&&*(int*)((char*)allocated_cursor+0x2c)==80);
    for(int i=0;i<12;i++)sync_cursor(e);assert(cursor_scale_calls==1); /* Preserve the native breathing timeline. */
    render_mask=1;render_length=0;command_render_hook(anchor);render_mask=0;assert(render_length==4&&render_order[1]==4&&render_order[2]==2&&render_order[3]==3);assert(deferred_button_render(allocated_cursor));
    main_tooltip_hook(owner,allocated_button);set_submenu_visible(e,1);submenu_tooltip_hook(native_links,stock_subbuttons[10]);assert(tooltip_calls==2&&!stock_tooltip_calls&&!native_populate_calls);
    main_tooltip_hook(owner,anchor);assert(stock_tooltip_calls==1);
    click_dispatch_hook((char*)allocated_button+0x70,0x7ffffffc,allocated_button);assert(!visibility_mock(allocated_cursor));hover_state(allocated_behavior,allocated_button);assert(cursor_animations==2);
    menu_hide_hook(owner);assert(!cursor_active&&!visibility_mock(allocated_cursor));menu_show_hook(owner);assert(cursor_active&&!visibility_mock(allocated_cursor));close_menu();assert(!allocated_cursor&&cursor_cleanups==1);
    fixture();putp(owner+0x190,(uintptr_t)stock_cursor);add_button(owner);cursor_fail_init=1;hover_state(allocated_behavior,allocated_button);assert(!allocated_cursor&&cursor_cleanups==1);close_menu();
    fixture();for(int i=0;i<16;i++)entries[i].owner=(void*)(uintptr_t)(i+1);add_button(owner);assert(!allocations);
    /* Animated parent coordinates and actual GPU acknowledgment govern the
       image/fallback choice; the native text render callback remains ordered. */
    fixture();add_button(owner);e=entry(owner);putp((char*)allocated_button+8,(uintptr_t)owner);
    setrect(owner,20,30,960,1080);setrect(native_outer,5,10,960,1080);caption_gpu_active=1;
    render_mask=1;command_render_hook(anchor);render_mask=0;
    assert(caption_x==144&&caption_y==853&&caption_alpha==1&&e->image_caption&&!title[0]&&render_length==3);
    *(int*)(native_outer+0x30)=128;render_length=0;render_mask=1;command_render_hook(anchor);render_mask=0;
    assert(caption_alpha>0.50f&&caption_alpha<0.51f);*(int*)(native_outer+0x30)=255;
    caption_gpu_active=0;render_length=0;render_mask=1;command_render_hook(anchor);render_mask=0;
    assert(!e->image_caption&&!wcscmp(title,L"演武")&&render_length==3);
    int hidden_count=caption_hidden;menu_hide_hook(owner);assert(caption_hidden>hidden_count);close_menu();
    puts("{\"status\":\"passed\",\"native_button\":true,\"image_caption\":true,\"caption_anchor_and_size\":true,\"native_cursor_transform_scale\":true,\"cursor_sync_preserves_animation\":true,\"unrelated_hidden_button_preserves_caption\":true,\"animated_parent_coordinates_and_alpha\":true,\"native_text_fallback_on_gpu_failure\":true,\"original_command_arrays_preserved\":true,\"placeholder_only\":false,\"native_main_command_link\":true,\"native_stock_panel_parent\":true,\"no_duplicate\":true,\"visibility_and_position_follow_anchor\":true,\"partial_creation_cleanup\":true,\"destruction_forgets_owner\":true,\"bounds_guard\":true,\"deferred_layout_creation\":true,\"existing_menu_recovery\":true,\"creation_diagnostics\":true,\"allocation_retry_throttled\":true,\"native_display_registration\":true,\"unregister_before_resource_cleanup\":true,\"resource_cleanup_before_memory_delete\":true,\"reenter_registers_new_controls\":true,\"hover_press_survive_frame_updates\":true,\"native_state_callbacks_preserved\":true,\"caption_does_not_capture_mouse\":true,\"native_menu_mouse_area\":true,\"parent_show_hide_lifecycle\":true,\"native_map_return_registration\":true,\"return_restores_original_controls_once\":true,\"cleanup_cancels_return\":true,\"anchor_opacity\":true,\"in_game_acceptance\":\"pending\"}");return 0;
}

