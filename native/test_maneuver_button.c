#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "maneuver_button.c"
static unsigned char owner[0x2c0],anchor[0x190];
static void *allocated_button,*allocated_behavior,*allocated_label;
static int allocations,fail_at,fail_init,fail_text,destroyed,labels,updates,last_visible,last_enabled;
static int position[2],font_size,alignment;static wchar_t title[8];
static unsigned char fake_context[0x218];static int resource_context;
static int button_active,label_active,button_activations,label_activations,cleanups;
static int input_bound,label_mouse_disabled,palette,last_alpha,native_states[5];
static int font_object;
static int render_order[8],render_length,render_mask,render_visibility;
static int visibility_mock(void *p){assert(p==allocated_button);return render_visibility;}
static int render_mock(void *p){
    assert(render_mask==1);
    if(p==anchor){render_order[render_length++]=1;SetLastError(241);return render_visibility;}
    assert(p==allocated_button);render_order[render_length++]=2;return 1;
}
static void text_render_mock(void *p){assert(p==allocated_label && render_mask==1);render_order[render_length++]=3;}
static void putp(void *p,uintptr_t n){memcpy(p,&n,8);}
static void setrect(void *p,int x,int y,int w,int h){int r[]={x,y,w,h};memcpy((char*)p+0x20,r,16);}
static void *alloc_mock(size_t n){allocations++;if(allocations==fail_at)return NULL;void *p=calloc(1,n);assert(p);if(n==8)allocated_behavior=p;else if(n==0x190)allocated_button=p;else{assert(n==0x138);allocated_label=p;}return p;}
static void *construct_mock(void *p){assert(p==allocated_button);return p;}
static void *construct_text_mock(void *p){assert(p==allocated_label);return p;}
static int init_mock(void *b,int x,int y,void *p,void *behavior){assert(b==allocated_button && p==owner && behavior==allocated_behavior);assert(x==45 && y==781);assert(pointer((uintptr_t)b)==(uintptr_t)button_type.methods);putp((char*)b+0x178,(uintptr_t)behavior);*(unsigned*)((char*)b+0x40)=7;return !fail_init;}
static int text_init_mock(void *l,int x,int y,int w,int h,void *parent){assert(l==allocated_label && parent==allocated_button && !x && !y && w==160 && h==64);putp((char*)l+0x128,(uintptr_t)&font_object);return !fail_text;}
static void *behavior_destroy_mock(void *p,unsigned f){assert(p==allocated_behavior && f==1);free(p);allocated_behavior=NULL;return NULL;}
static void *text_destroy_mock(void *p,unsigned f){assert(p==allocated_label && f==1);free(p);allocated_label=NULL;return NULL;}
static void *destroy_mock(void *p,unsigned f){assert(p==allocated_button && f==1 && !allocated_behavior && !allocated_label && !button_active && !label_active);destroyed++;free(p);allocated_button=NULL;return NULL;}
static void *context_mock(void){return fake_context;}
static void activate_button_mock(void *p){assert(p==allocated_button && entry(owner) && !button_active && labels>=1 && !wcscmp(title,L"操演"));button_active=1;button_activations++;}
static void activate_text_mock(void *p){assert(p==allocated_label && button_active && !label_active);label_active=1;label_activations++;}
static void deactivate_button_mock(void *p){assert(p==allocated_button && button_active && !label_active);button_active=0;}
static void deactivate_text_mock(void *p){assert(p==allocated_label && label_active && button_active);label_active=0;}
static void cleanup_button_mock(void *p,void *resources){assert(p==allocated_button && resources==&resource_context && !button_active && !allocated_label);if(allocated_behavior)behavior_destroy_mock(allocated_behavior,1);}
static void cleanup_text_mock(void *p,void *resources){assert(p==allocated_label && resources==&resource_context && !label_active);}
static void menu_cleanup_mock(void *p,void *resources){assert(p==owner && resources==&resource_context && !entry(owner) && !allocated_button && !allocated_label);cleanups++;SetLastError(219);}
static void close_menu(void){menu_cleanup_hook(owner,&resource_context);assert(GetLastError()==219);}
static void move_mock(void *p,int x,int y){assert(p==allocated_button);position[0]=x;position[1]=y;}
static void visible_mock(void *p,int show){assert(p==allocated_button);last_visible=show;}
static void enabled_mock(void *p,int enable){
    if(p==allocated_label){assert(!enable);label_mouse_disabled++;return;}
    assert(p==allocated_button);last_enabled=enable;
    unsigned *flags=(unsigned*)((char*)p+0x40);*flags=(*flags&~4u)|(enable?4u:0u);
}
static void input_mock(void *p,int node){assert(p==allocated_button && node==1);input_bound++;}
static void palette_mock(void *p,int value){assert(p==&font_object);palette=value;}
static void opacity_mock(void *p,int value){assert(p==allocated_button);last_alpha=value;}
static void native_state_mock(void *behavior,void *button,int state){assert(behavior==allocated_behavior && button==allocated_button);native_states[state]++;}
static void normal_mock(void *behavior,void *button){native_state_mock(behavior,button,0);}
static void hover_mock(void *behavior,void *button){native_state_mock(behavior,button,1);}
static void pressed_mock(void *behavior,void *button){native_state_mock(behavior,button,2);}
static void disabled_mock(void *behavior,void *button){native_state_mock(behavior,button,3);}
static void selected_mock(void *behavior,void *button){native_state_mock(behavior,button,4);}
static void menu_display_mock(void *p){assert(p==owner);}
static void text_mock(void *p,const wchar_t *s){assert(p==allocated_label);wcscpy(title,s);labels++;}
static void size_mock(void *p,int v){assert(p==allocated_label);font_size=v;}
static void align_mock(void *p,int v){assert(p==allocated_label);alignment=v;}
static void hide_mock(void *p,int i,int show){assert(p==(char*)allocated_button+0x128 && i==20 && !show);}
static void width_mock(void *p,int v){assert(p==allocated_button && v==160);}
static void height_mock(void *p,int v){assert(p==allocated_button && v==64);}
static void extent_mock(void *p,uint64_t v){assert(p==allocated_button && v==((uint64_t)64<<32|160));}
static int menu_mock(void *p,int x,int y,void *parent){assert(p==owner && !x && !y && !parent);SetLastError(157);return 1;}
static void update_mock(void *p){assert(p==owner);updates++;SetLastError(197);}
static void fixture(void){
    assert(!allocated_button && !allocated_behavior && !allocated_label);memset(owner,0,sizeof(owner));memset(anchor,0,sizeof(anchor));memset(entries,0,sizeof(entries));
    image=0x140000000;putp(owner,image+0x1357dc8);putp(anchor,image+0x12d9ef0);putp(owner+0x1c0,(uintptr_t)anchor);
    setrect(owner,0,0,960,1080);setrect(anchor,73,697,160,64);*(unsigned*)(anchor+0x40)=7;
    allocate=alloc_mock;construct=construct_mock;initialize_menu_button=init_mock;native_destroy=destroy_mock;behavior_destroy=behavior_destroy_mock;
    construct_text=construct_text_mock;initialize_text=text_init_mock;text_destroy=text_destroy_mock;text=text_mock;
    move=move_mock;visible=visible_mock;button_enabled=enabled_mock;layout_visible=hide_mock;text_size=size_mock;text_align=align_mock;
    button_width=width_mock;button_height=height_mock;button_extent=extent_mock;original_init=menu_mock;original_update=update_mock;
    context=context_mock;putp(fake_context+0x210,(uintptr_t)&resource_context);
    activate_button=activate_button_mock;activate_text=activate_text_mock;deactivate_button=deactivate_button_mock;deactivate_text=deactivate_text_mock;
    cleanup_button=cleanup_button_mock;cleanup_text=cleanup_text_mock;original_cleanup=menu_cleanup_mock;
    bind_input=input_mock;apply_palette=palette_mock;opacity=opacity_mock;original_show=original_hide=menu_display_mock;
    native_normal=normal_mock;native_hover=hover_mock;native_pressed=pressed_mock;native_disabled=disabled_mock;native_selected=selected_mock;
    original_button_render=render_mock;visibility_result=visibility_mock;native_text_render=text_render_mock;
    render_length=render_mask=draw_count=0;render_visibility=1;
    input_bound=label_mouse_disabled=palette=last_alpha=hover_count=press_count=0;memset(native_states,0,sizeof(native_states));
    button_active=label_active=button_activations=label_activations=cleanups=0;
    allocations=fail_at=fail_init=fail_text=destroyed=labels=updates=0;
    retry_after=0;phase=init_calls=update_calls=created_count=destroyed_count=activated_count=0;
}
int main(void){
    fixture();unsigned char before[sizeof(owner)];memcpy(before,owner,sizeof(owner));
    assert(menu_init_hook(owner,0,0,NULL)==1 && GetLastError()==157 && allocations==3 && labels==1);
    assert(!memcmp(before,owner,sizeof(owner)) && !wcscmp(title,L"操演") && font_size==42 && alignment==4 && palette==25);
    assert(input_bound==1 && label_mouse_disabled==1);
    assert(last_visible && last_enabled && position[0]==45 && position[1]==781);
    assert(button_active && label_active && button_activations==1 && label_activations==1 && activated_count==1);
    assert(menu_init_hook(owner,0,0,NULL)==1 && allocations==3);
    /* An opened native mask must bracket anchor, extra paper, and caption.
       The controls' later independent render slots produce no duplicate. */
    render_mask=1;assert(command_render_hook(anchor)==1 && GetLastError()==241);
    render_mask=0;assert(render_length==3 && render_order[0]==1 && render_order[1]==2 && render_order[2]==3 && draw_count==1);
    assert(deferred_button_render(allocated_button)==1);deferred_text_render(allocated_label);assert(render_length==3);
    render_mask=1;render_visibility=0;assert(command_render_hook(anchor)==0 && render_length==4 && draw_count==1);
    render_mask=0;render_visibility=1;render_length=0;
    /* Native input flags survive repeated layout updates: an accidental leave
       callback here used to erase both hover and press on every frame. */
    *(unsigned*)((char*)allocated_button+0x40)|=0x180;
    for(int i=0;i<12;i++)menu_update_hook(owner);
    assert((*(unsigned*)((char*)allocated_button+0x40)&0x180)==0x180);
    hover_state(allocated_behavior,allocated_button);assert(palette==26 && native_states[1]==1 && hover_count==1);
    pressed_state(allocated_behavior,allocated_button);assert(palette==27 && native_states[2]==1 && press_count==1);
    disabled_state(allocated_behavior,allocated_button);assert(palette==28 && native_states[3]==1);
    selected_state(allocated_behavior,allocated_button);assert(palette==29 && native_states[4]==1);
    normal_state(allocated_behavior,allocated_button);assert(palette==25 && native_states[0]==1);
    *(int*)(anchor+0x30)=128;menu_update_hook(owner);assert(last_alpha==128);
    menu_hide_hook(owner);menu_hide_hook(owner);assert(!button_active && !label_active && !entry(owner)->active);
    render_mask=1;command_render_hook(anchor);render_mask=0;assert(render_length==1 && draw_count==1);
    menu_show_hook(owner);menu_show_hook(owner);assert(button_active && label_active && button_activations==2 && label_activations==2);
    updates=0;
    setrect(anchor,83,707,160,64);*(unsigned*)(anchor+0x40)=6;menu_update_hook(owner);
    assert(GetLastError()==197 && updates==1 && !last_visible && last_enabled && position[0]==55 && position[1]==791 && allocations==3);
    *(unsigned*)(anchor+0x40)=3;menu_update_hook(owner);assert(last_visible && !last_enabled);
    setrect(anchor,73,1040,160,64);menu_update_hook(owner);assert(!last_visible);
    close_menu();assert(!entry(owner) && destroyed==1 && !button_active && !label_active && cleanups==1);
    fixture();add_button(owner);menu_hide_hook(owner);close_menu();assert(destroyed==1 && !button_active && !label_active);
    /* During native initialization the resource-bound anchor can still have
       its default origin. A later native update must create the missing row. */
    fixture();setrect(anchor,0,0,160,64);
    assert(menu_init_hook(owner,0,0,NULL)==1 && GetLastError()==157 && !allocations && phase==WAIT_GEOMETRY);
    char diagnostic[512];assert(s14_maneuver_button_next_log(diagnostic,sizeof(diagnostic)) && strstr(diagnostic,"waiting_geometry"));
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
    fixture();for(int i=0;i<16;i++)entries[i].owner=(void*)(uintptr_t)(i+1);add_button(owner);assert(!allocations);
    puts("{\"status\":\"passed\",\"native_button\":true,\"native_text\":true,\"original_command_arrays_preserved\":true,\"placeholder_only\":true,\"no_duplicate\":true,\"visibility_and_position_follow_anchor\":true,\"partial_creation_cleanup\":true,\"destruction_forgets_owner\":true,\"bounds_guard\":true,\"deferred_layout_creation\":true,\"existing_menu_recovery\":true,\"creation_diagnostics\":true,\"allocation_retry_throttled\":true,\"native_display_registration\":true,\"unregister_before_resource_cleanup\":true,\"resource_cleanup_before_memory_delete\":true,\"reenter_registers_new_controls\":true,\"hover_press_survive_frame_updates\":true,\"native_state_callbacks_preserved\":true,\"native_caption_palettes\":true,\"caption_does_not_capture_mouse\":true,\"native_menu_mouse_area\":true,\"parent_show_hide_lifecycle\":true,\"anchor_opacity\":true,\"in_game_acceptance\":\"pending\"}");return 0;
}
