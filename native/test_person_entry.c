#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include "person_entry.c"

static unsigned char *test_image,*world;
static unsigned char settings[0x140],districts[3][0x20],force_data[0x20],people[5][0x120];
static void *copied[8],*retained[8];
static uintptr_t counts[1];
static unsigned handle_id;
static unsigned char manager[0x58],frames[48],map_state[0x490],other_state[0x70];
static unsigned char opened_dialog[0x330];
static uintptr_t states[1];
static int scene_ok=1,copy_bad,change_scene,copy_count,opens,deleted;
static int fourth_available=1,fourth_calls;
static DWORD ui_thread;
int s14_map_main_scene(S14OfficerRead read,void *context,uintptr_t base){(void)read;(void)context;assert(base==entry_base);return scene_ok;}
static void putp(void *p,uintptr_t value){memcpy(p,&value,sizeof(value));}
int s14_tech_tree_select_fourth(void *dialog){assert(GetCurrentThreadId()==ui_thread&&dialog==opened_dialog&&opens>0&&ui_opening);fourth_calls++;return fourth_available;}
static void *info_mock(void){return &manager;}
static void *primary_mock(void *p){assert(p==settings);return districts[0];}
static void init_mock(void *list){((void**)list)[1]=&handle_id;}
static void copy_mock(void *list,void **begin,void **end){
    assert(((void**)list)[1]==&handle_id&&!(*end));copy_count=0;
    for(Node *node=*begin;node;node=node->next){assert(copy_count<8);copied[copy_count++]=node->person;}
    counts[0]=copy_bad?0:(uintptr_t)copy_count;
    if(change_scene)scene_ok=0;
}
static void delete_mock(void *list){if(((void**)list)[1]){assert(((void**)list)[1]==&handle_id);deleted++;((void**)list)[1]=NULL;}}
static void open_mock(void *ui,void *list,void *selected){
    assert(GetCurrentThreadId()==ui_thread&&ui==&manager&&selected==people[1]&&copy_count==3&&scene_ok);
    assert(copied[0]==people[0]&&copied[1]==people[1]&&copied[2]==people[3]);
    memcpy(retained,copied,(size_t)copy_count*sizeof(void*));opens++;
    putp(manager+8,(uintptr_t)frames+24);
    putp(frames+8,(uintptr_t)opened_dialog);
    /* Match the actual information UI's list-consumption contract. Its own
       page data remains valid after the temporary source nodes are freed. */
    delete_mock(list);assert(!s14_person_entry_open()); /* nested UI is blocked */
}
static void fixture(void){
    memset(world,0,0x85200);memset(settings,0,sizeof(settings));memset(districts,0,sizeof(districts));memset(people,0,sizeof(people));
    putp(test_image+0x1fc91d0,(uintptr_t)world);putp(world+0x85130,(uintptr_t)settings);settings[0x3a]=1;
    putp(force_data,entry_base+0x129fe58);*(unsigned short*)(force_data+0x10)=2;putp(world+0xdca0+8,(uintptr_t)force_data);
    for(int i=0;i<3;i++){putp(districts[i],entry_base+0x129fec8);districts[i][0x10]=i==1?2:1;districts[i][0x11]=i==0?1:0;putp(world+0xde40+(i+1)*8,(uintptr_t)districts[i]);}
    for(int i=0;i<5;i++){putp(people[i],entry_base+0x12a00d0);*(unsigned short*)(people[i]+0x10)=(unsigned short)(i+1);*(unsigned short*)(people[i]+0x12)=(unsigned short)(i+1);people[i][0x118]=i==2?2:i==3?3:1;people[i][0x11e]=i==4?1:i==3?4:2;putp(world+0x148+(i+1)*8,(uintptr_t)people[i]);}
    putp(test_image+0x201c378,(uintptr_t)counts);*(unsigned*)(test_image+0x201c390)=1;
    native_info=info_mock;native_primary=primary_mock;native_list_init=init_mock;native_list_copy=copy_mock;native_list_delete=delete_mock;native_open=open_mock;
    memset(manager,0,sizeof(manager));putp(manager,(uintptr_t)frames);putp(manager+8,(uintptr_t)frames);
    states[0]=(uintptr_t)map_state;putp(test_image+0x19e6330+0x10,1);putp(test_image+0x19e6330+0x20,(uintptr_t)states);
    s14_person_entry_cancel_return();scene_ok=1;copy_bad=change_scene=copy_count=opens=deleted=0;ui_opening=ui_requests=ui_returns=event_code=0;
    fourth_available=1;fourth_calls=entry_default_tab=0;
}
int main(void){
    test_image=VirtualAlloc(NULL,0x2020000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);world=calloc(1,0x85200);assert(test_image&&world);entry_base=(uintptr_t)test_image;ui_thread=GetCurrentThreadId();
    fixture();SetLastError(233);assert(s14_person_entry_open()&&GetLastError()==233&&opens==1&&deleted==1&&!ui_opening);
    assert(event_code==ENTRY_OPEN_REQUESTED&&event_count==3&&event_force==1&&event_selected==2&&ui_requests==1&&retained[2]==people[3]);
    assert(fourth_calls==1&&entry_default_tab==4);
    char log[256];assert(s14_person_entry_next_log(log,sizeof(log))&&strstr(log,"own_force_ui_submitted")&&strstr(log,"\"count\":3"));
    assert(!s14_person_entry_return_ready()&&!s14_person_entry_open()&&opens==1);
    /* Closing a nested page cannot restore the map; the native information
       stack and queued game-state transition must both have finished. */
    putp(manager+8,(uintptr_t)frames+48);assert(!s14_person_entry_return_ready());
    putp(manager+8,(uintptr_t)frames);states[0]=(uintptr_t)other_state;assert(!s14_person_entry_return_ready());
    states[0]=(uintptr_t)map_state;scene_ok=0;assert(!s14_person_entry_return_ready());
    scene_ok=1;SetLastError(244);assert(s14_person_entry_return_ready()==1&&GetLastError()==244&&ui_returns==1&&!returning.manager);
    assert(!s14_person_entry_return_ready());
    assert(s14_person_entry_next_log(log,sizeof(log))&&strstr(log,"map_return_ready"));
    assert(s14_person_entry_open()&&opens==2&&deleted==2&&ui_requests==2);assert(s14_person_entry_next_log(log,sizeof(log)));
    putp(world+0x85130,0);assert(s14_person_entry_return_ready()==-1&&event_code==ENTRY_RETURN_CANCELLED&&!returning.manager);
    fixture();assert(s14_person_entry_open());s14_person_entry_cancel_return();assert(!s14_person_entry_return_ready());
    fixture();fourth_available=0;assert(s14_person_entry_open()&&opens==1&&fourth_calls==1&&entry_default_tab==1); /* absent fourth page is safe */
    fixture();putp(manager+8,(uintptr_t)frames+24);assert(!s14_person_entry_open()&&!opens&&!deleted); /* existing information UI */
    fixture();copy_bad=1;assert(!s14_person_entry_open()&&!opens&&deleted==1&&event_code==ENTRY_LIST_FAILED);
    fixture();change_scene=1;assert(!s14_person_entry_open()&&!opens&&deleted==1);
    fixture();scene_ok=0;assert(!s14_person_entry_open()&&!opens&&!deleted);
    fixture();settings[0x3a]=0;assert(!s14_person_entry_open()&&!opens&&!deleted&&event_code==ENTRY_FILTER_FAILED);
    fixture();putp(world+0x85130,0);assert(!s14_person_entry_open()&&!opens&&event_code==ENTRY_CONTEXT_FAILED);
    free(world);VirtualFree(test_image,0,MEM_RELEASE);
    puts("{\"status\":\"passed\",\"own_force_districts\":true,\"foreign_and_unavailable_excluded\":true,\"ruler_selected\":true,\"native_list_count_validated\":true,\"information_ui_three_argument_abi\":true,\"same_ui_thread\":true,\"map_entry_defaults_to_fourth_tab\":true,\"fourth_selection_after_native_open\":true,\"missing_fourth_tab_safe_fallback\":true,\"native_list_consumption\":true,\"retained_page_data\":true,\"reentry_blocked\":true,\"scene_change_cancels\":true,\"repeat_requests_logged\":true,\"nested_information_return_waits\":true,\"native_state_return_waits\":true,\"single_return_ticket\":true,\"scene_replacement_cancels_return\":true,\"existing_information_ui_not_taken_over\":true,\"engine_boundaries_mocked\":true,\"in_game_acceptance\":\"pending\"}");return 0;
}
