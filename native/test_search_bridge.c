// Exercise the actual native dispatcher with isolated mock game callbacks.
// This executable never opens a game process or loads a game executable.
#include <assert.h>
#ifdef NDEBUG
#error "Native verification requires assertions enabled"
#endif
#include "auto_search.c"
static unsigned char *mock_image,*mock_world,*mock_settings;
static unsigned char mock_force[32],mock_data[0x80],mock_people[3][0x220],mock_cities[52][0x60],mock_area[0x60];
static void *mock_city_list[52],*mock_nodes[3][2],*mock_heads[8];
static uintptr_t mock_counts[8],mock_city_vt[23],mock_data_vt[13],mock_iterator_vt[2];
static unsigned int mock_handle,allocated_maps,freed_maps,lists_created,lists_destroyed,native_submissions;
static unsigned int people_handle=2;
static int mock_day=100,mock_cost=2,mock_candidates=3,seen_settings;
static int result_mode,log_calls,outcome_calls,progress_calls;
static unsigned char mock_foreign_group[32],mock_second_group[32];
static int primary_group=1;
static void* player_group(void *settings) {
    assert(settings==mock_settings);
    return ((void**)(mock_world+0xde40))[primary_group];
}
typedef void (*PublishTarget)(void*,void*);
static PublishTarget publish_target;
static void jump_to(size_t rva,void *function) {
    unsigned char code[]={0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};
    uintptr_t target=(uintptr_t)function; memcpy(code+2,&target,8); memcpy(mock_image+rva,code,sizeof(code));
}
static void *call_site(size_t call_rva,size_t relay,int stacked) {
    // Keep the real return address used by each hook's provenance filter.
    // Preserve stack arguments and Win64 alignment in the isolated fixture.
    size_t prefix=4+(size_t)stacked*13,entry=call_rva-prefix;
    unsigned char *p=mock_image+entry; unsigned int reserve=stacked>1?0x68:0x28;
    unsigned char prologue[]={0x48,0x83,0xec,0}; prologue[3]=(unsigned char)reserve; memcpy(p,prologue,4); p+=4;
    for (int i=0;i<stacked;i++) {
        unsigned char load[]={0x48,0x8b,0x84,0x24,0,0,0,0}; unsigned int offset=reserve+0x28+8*i;
        memcpy(load+4,&offset,4); memcpy(p,load,8); p+=8;
        unsigned char store[]={0x48,0x89,0x44,0x24,(unsigned char)(0x20+8*i)}; memcpy(p,store,5); p+=5;
    }
    assert(p==mock_image+call_rva); *p++=0xe8; int32_t relative=(int32_t)(relay-call_rva-5); memcpy(p,&relative,4); p+=4;
    unsigned char epilogue[]={0x48,0x83,0xc4,(unsigned char)reserve,0xc3}; memcpy(p,epilogue,5);
    return mock_image+entry;
}
static void* find_person(void *p,void *t,int *roll,int mode) { assert(p==mock_people[0] && t==mock_area && mode==1); *roll=1; return result_mode==4?NULL:mock_people[2]; }
static int find_treasure(void *p,void *t,void **item,void **book,int *roll) { assert(p==mock_people[0] && t==mock_area && roll); *item=result_mode==1?mock_people[2]:NULL; *book=result_mode==2?mock_people[2]:NULL; return 1; }
static int find_money(void *p,void *t,int *amount) { assert(p==mock_people[0] && t==mock_area); *amount=1234; return 1; }
static int finish_outcome(void *p,int g,int outcome,int option) { assert(p==mock_people[0] && g==3 && outcome==2 && option==0); outcome_calls++; return 23; }
static uintptr_t record_log(void *p,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,const wchar_t *short_text,const wchar_t *long_text,uintptr_t f,uintptr_t h,uintptr_t i,uintptr_t j) {
    assert(p==mock_area && a==0x1234567800000002ull && b==1 && c==9 && d==0 && e==100 && f==3 && h==0 && i==1 && j==0xfedcba9876543210ull);
    assert(!wcscmp(short_text,L"短报告") && !wcscmp(long_text,L"发现名品：青釭剑")); log_calls++; return 0x123456789abcdef0ull;
}
static int resolve_search(void *command) {
    (void)command; int roll=0; void *item=NULL,*book=NULL;
    if (result_mode==0 || result_mode==4) {
        PersonFunction call=call_site(0x1c9373,0x1040,0); assert(call(mock_people[0],mock_area,&roll,1)==(result_mode==4?NULL:mock_people[2]));
    } else if (result_mode==1 || result_mode==2) {
        TreasureFunction call=call_site(0x1c9a08,0x1060,1); assert(call(mock_people[0],mock_area,&item,&book,&roll)==1);
    } else if (result_mode==3) {
        MoneyFunction call=call_site(0x1c9e0f,0x1080,0); assert(call(mock_people[0],mock_area,&roll)==1 && roll==1234);
    }
    LogFunction log=call_site(0x1c9dec,0x10c0,8);
    assert(log(mock_area,0x1234567800000002ull,1,9,0,100,L"短报告",L"发现名品：青釭剑",3,0,1,0xfedcba9876543210ull)==0x123456789abcdef0ull);
    OutcomeFunction finish=call_site(0x1ca28b,0x10a0,0); assert(finish(mock_people[0],3,2,0)==23);
    return 29;
}
static void progress_original(int value) { assert(value==0); progress_calls++; }
static int date_value(void *arg) { assert(arg==mock_settings+0x34); return mock_day; }
static int cost_value(void) { return mock_cost; }
static int iterator_valid(CityIterator *it,void **slot) {
    assert(slot==it->current && slot>=mock_city_list && slot<mock_city_list+52);
    return ((unsigned char*)*slot)[0x20]==it->force;
}
static void* iterator_start(void *data,CityIterator *it) {
    assert(data==mock_data); *it=(CityIterator){mock_iterator_vt,mock_city_list,mock_city_list+52,mock_settings[0x3a],0};
    while (it->current!=it->end && !iterator_valid(it,it->current)) it->current++;
    return it;
}
static void* city_owner(void *city) { return city; }
static int city_force(unsigned char *city) { return city[0x20]; }
static int data_force(void *data) { assert(data==mock_data); return mock_settings[0x3a]; }
static int distance(void *a,void *b) { assert(a!=b); return 4; }
static int travel(int days,void *person,void *data) { assert(days==4 && !person && data==mock_data); return 3; }
static void* map_create(RouteMap *map) {
    unsigned char *head=calloc(1,0x30); assert(head); *(void**)head=head; *(void**)(head+8)=head; *(void**)(head+16)=head; head[0x19]=1;
    map->head=head; map->size=0; allocated_maps++; return map;
}
static void map_destroy(RouteMap *map) {
    unsigned char *head=map->head; if (map->size) free(*(void**)(head+8)); free(head); map->head=NULL; freed_maps++;
}
static void* map_node(void *map,void *unused,void **key,void *output) {
    (void)map;(void)unused;(void)output; unsigned char *node=calloc(1,0x30); assert(node); *(void**)(node+0x20)=**(void***)key; return node;
}
static void* map_insert(RouteMap *map,void **result,void *hint,void *key,unsigned char *node) {
    (void)hint;(void)key; assert(!map->size); unsigned char *head=map->head; *(void**)node=head; *(void**)(node+8)=head; *(void**)(node+16)=head;
    *(void**)(head+8)=node; map->size=1; *result=node; return result;
}
static void list_create(void **list) { list[1]=&mock_handle; lists_created++; }
static void list_destroy(void **list) { assert(list[1]==&mock_handle); list[1]=NULL; lists_destroyed++; }
static void publish_c(unsigned char *person,void **record) {
    void **targets=*(void***)(mock_image+0x1fc9520); assert(targets);
    targets[*(unsigned short*)(person+0x10)]=record[2];
}
static void candidates(void *state,void **list,int executor,int days,int priority) {
    assert(*(void**)((unsigned char*)state+0x470)==mock_force && list[1]==&mock_handle);
    seen_settings=executor|(days<<4)|(priority<<8);
    RouteMap *maps=*(RouteMap**)((unsigned char*)state+0x478);
    for (int i=0;i<2;i++) { assert(maps[i].size==1); unsigned char *node=*(unsigned char**)((unsigned char*)maps[i].head+8); assert(*(int*)(node+0x28)==3); }
    mock_counts[mock_handle]=mock_candidates; mock_heads[mock_handle]=mock_candidates?mock_nodes[0]:NULL;
    for (int i=0;i<3;i++) {
        void *record[]={mock_people[i],NULL,mock_cities[1]}; publish_target(mock_people[i],record);
        *(unsigned short*)(mock_people[i]+0x196)|=0x40;
        *(int*)(mock_people[i]+0x1f4)=100+i;
    }
}
static void* search_target(void *force,unsigned char *person,void *city) {
    assert(force==mock_force && city==mock_cities[1] && person>=mock_people[0]); return mock_area;
}
static int valid_data(void *arg) { return arg!=NULL; }
static int modifier(void *force) { assert(force==mock_force); return 1; }
static void submit(void **args,int option) {
    assert(args[0]==mock_area && option==1 && mock_force[0x14]>=mock_cost);
    unsigned char *person=args[1]; assert(!(*(unsigned short*)(person+0x196)&1));
    if (person==mock_people[1]) return; // simulate original game's rejected command
    *(unsigned short*)(person+0x196)|=1; mock_force[0x14]-=mock_cost; native_submissions++;
}
void s14_manager_refresh(S14ManagerUI *ui) { (void)ui; }
int s14_toast_text(S14Toast *t,HINSTANCE h,HWND w,POINT p,ULONGLONG n,const wchar_t *a,const wchar_t *b,unsigned int d) { (void)t;(void)h;(void)w;(void)p;(void)n;(void)a;(void)b;(void)d; return 1; }
int s14_toast_tick(S14Toast *t,ULONGLONG n,int f) { (void)t;(void)n;(void)f; return 0; }
int s14_toast_report(S14Toast *t,HINSTANCE h,HWND w,ULONGLONG n,const wchar_t *text) { (void)t;(void)h;(void)w;(void)n;(void)text; return 1; }
static LONG WINAPI reproduce_exception(EXCEPTION_POINTERS *exception) {
    // Negative test stays inside this disposable test process. Windows does not
    // display a crash dialog or create a game dump for the intentional fault.
    uintptr_t fault=(uintptr_t)exception->ExceptionRecord->ExceptionAddress;
    if (exception->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION) {
        if (fault==base_address+0x2f614b && exception->ExceptionRecord->ExceptionInformation[1]==0) ExitProcess(71);
        if (fault==base_address+0x65dcd0 && exception->ExceptionRecord->ExceptionInformation[0]==1 &&
            exception->ExceptionRecord->ExceptionInformation[1]==393*8) ExitProcess(74);
    }
    ExitProcess(72);
}
static S14SearchEvent latest_begin(void) {
    QueueSlot *latest=NULL;
    for (int i=0;i<QSIZE;i++) if (queue[i].state==2 && queue[i].event.kind==S14_SEARCH_BEGIN && (!latest || queue[i].sequence>latest->sequence)) latest=&queue[i];
    assert(latest); return latest->event;
}
static void scratch_restored(void) {
    for (int i=0;i<3;i++) {
        assert((*(unsigned short*)(mock_people[i]+0x196)&0x40)==(i==1?0x40:0));
        assert(*(int*)(mock_people[i]+0x1f4)==11*(i+1));
    }
}
int main(int argc,char **argv) {
    mock_image=VirtualAlloc(NULL,0x2020000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE); assert(mock_image);
    mock_world=calloc(1,0x85200); mock_settings=calloc(1,0x1800); assert(mock_world && mock_settings);
    base_address=(uintptr_t)mock_image; end_address=base_address+0x2020000; manager_address=&mock_world; ready=1;
    *(void**)(mock_world+0x85130)=mock_settings; mock_settings[0x3a]=1;
    *(void**)(mock_image+0x1fc91d0)=mock_world;
    ((void**)(mock_world+0xde40))[1]=mock_force; ((void**)(mock_world+0xdca0))[1]=mock_data; mock_force[0x10]=1; mock_force[0x14]=5;
    *(uintptr_t*)mock_force=base_address+0x129fec8;
    *(uintptr_t*)mock_foreign_group=*(uintptr_t*)mock_second_group=base_address+0x129fec8;
    mock_foreign_group[0x10]=2;mock_second_group[0x10]=1;
    *(void***)(mock_image+0x201c360)=mock_heads; *(uintptr_t**)(mock_image+0x201c378)=mock_counts; *(unsigned int*)(mock_image+0x201c390)=8;
    mock_city_vt[16]=(uintptr_t)city_owner; mock_iterator_vt[1]=(uintptr_t)iterator_valid;
    // These mock virtual methods must lie in the bounded synthetic image, too.
    jump_to(0x1000,city_owner); mock_city_vt[16]=base_address+0x1000;
    jump_to(0x1020,iterator_valid); mock_iterator_vt[1]=base_address+0x1020;
    jump_to(0x1100,city_force); mock_city_vt[22]=base_address+0x1100;
    jump_to(0x1120,data_force); mock_data_vt[12]=base_address+0x1120; *(uintptr_t**)mock_data=mock_data_vt;
    memcpy(mock_image+0x129fe58,mock_data_vt,sizeof(mock_data_vt));*(void**)mock_data=mock_image+0x129fe58;
    jump_to(0x2f1fc0,player_group);
    for (int i=0;i<52;i++) {
        *(uintptr_t**)mock_cities[i]=mock_city_vt; mock_cities[i][0x11]=1;
        mock_cities[i][0x20]=(i==1 || i==50)?1:2; mock_city_list[i]=mock_cities[i];
        ((void**)(mock_world+0xdaa8))[i]=mock_cities[i];
    }
    mock_handle=1;
    for (int i=0;i<3;i++) {
        *(uintptr_t*)mock_people[i]=base_address+0x12a00d0;
        *(unsigned short*)(mock_people[i]+0x10)=i+1; mock_nodes[i][0]=mock_people[i]; mock_nodes[i][1]=i==2?NULL:mock_nodes[i+1];
        *(unsigned short*)(mock_people[i]+0x196)=i==1?0x40:0; *(int*)(mock_people[i]+0x1f4)=11*(i+1);
    }
    wcscpy((wchar_t*)(mock_people[0]+0x12),L"曹"); wcscpy((wchar_t*)(mock_people[0]+0x24),L"操");
    *(uintptr_t*)mock_area=base_address+0x129ff98; wcscpy((wchar_t*)(mock_area+0x10),L"襄阳郡");
    *(void**)(mock_world+0x100)=&people_handle; mock_heads[people_handle]=mock_nodes[0]; mock_counts[people_handle]=3;
    publish_target=(PublishTarget)publish_c;
    jump_to(0x2eff90,date_value); jump_to(0x62ab00,cost_value); jump_to(0x2034e0,iterator_start);
    jump_to(0x159880,map_create); jump_to(0x159fa0,map_destroy); jump_to(0x1575b0,map_node); jump_to(0x158330,map_insert);
    jump_to(0x27e6d0,distance); jump_to(0x20e710,travel); jump_to(0x17fb60,list_create); jump_to(0x5d30f0,list_destroy);
    jump_to(0x65cf80,candidates); jump_to(0x631aa0,search_target); jump_to(0x2f29d0,valid_data); jump_to(0x210cd0,modifier); jump_to(0x1d6c30,submit);
    int native_fixture=argc>1;
    if (native_fixture) {
        FILE *fixture=fopen(argv[1],"rb"); assert(fixture);
        unsigned int sizes[3]; assert(fread(sizes,sizeof(sizes),1,fixture)==1 && sizes[0]==147 && sizes[1]==55 && sizes[2]==19);
        assert(fread(mock_image+0x2034e0,1,sizes[0],fixture)==sizes[0]);
        assert(fread(mock_image+0x2f6130,1,sizes[1],fixture)==sizes[1]);
        assert(fread(mock_image+0x65dcc1,1,sizes[2],fixture)==sizes[2]); fclose(fixture);
        mock_image[0x65dcd4]=0x5f;mock_image[0x65dcd5]=0xc3; // pop RDI; return after the unchanged native store
        unsigned char publish_prefix[]={0x57,0x48,0x89,0xcf,0x48,0x89,0xd1,0xe9,0,0,0,0};
        int32_t publish_jump=0x65dcc1-0x1140-sizeof(publish_prefix); memcpy(publish_prefix+8,&publish_jump,4);
        memcpy(mock_image+0x1140,publish_prefix,sizeof(publish_prefix));publish_target=(PublishTarget)(mock_image+0x1140);
        ((uintptr_t*)(mock_image+0x129fb40))[1]=base_address+0x2f6130;
        if (argc>2 && !strcmp(argv[2],"--reproduce-040")) {
            // The copied predicate saves RBX and reserves 32 bytes. Register
            // its unwind data so the OS can reach this fixture's error filter.
            static RUNTIME_FUNCTION unwind={0x2f6130,0x2f6167,0x1800000};
            const unsigned char unwind_bytes[]={1,6,2,0,6,0x32,2,0x30};
            memcpy(mock_image+unwind.UnwindData,unwind_bytes,sizeof(unwind_bytes));
            assert(RtlAddFunctionTable(&unwind,1,base_address));
            SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
            SetUnhandledExceptionFilter(reproduce_exception);
            CityIterator it={0}; ((void*(*)(void*,void*))(base_address+0x2034e0))(mock_data,&it);
            assert(it.force==1);
            int legacy_result=((CityPredicate)(base_address+0x2f6130))(&it,NULL); // exact null RDX from the 0.4.0 crash dump
            fprintf(stderr,"Unexpected legacy return: %d\n",legacy_result);
            return 73;
        }
        if (argc>2 && !strcmp(argv[2],"--reproduce-041")) {
            static RUNTIME_FUNCTION unwind={0x65dcc1,0x65dcd6,0x1800020};
            const unsigned char unwind_bytes[]={1,0,1,0,0,0x70,0,0};
            memcpy(mock_image+unwind.UnwindData,unwind_bytes,sizeof(unwind_bytes));assert(RtlAddFunctionTable(&unwind,1,base_address));
            SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(reproduce_exception);
            *(unsigned short*)(mock_people[0]+0x10)=393;
            void *record[]={mock_people[0],NULL,mock_cities[1]}; publish_target(mock_people[0],record);
            return 73;
        }
    }
    void *owned[52]; assert(cities(mock_data,owned)==2 && owned[0]==mock_cities[1] && owned[1]==mock_cities[50]);
    mock_cities[1][0x20]=mock_cities[50][0x20]=2; assert(cities(mock_data,owned)==0);
    mock_cities[1][0x20]=mock_cities[50][0x20]=1;
    SearchWorkspace workspace={0}; void ***table_slot=(void***)(mock_image+0x1fc9520);
    assert(workspace_begin(&workspace,mock_world) && *table_slot==workspace.targets && !workspace.previous);
    unsigned short old_id=*(unsigned short*)(mock_people[2]+0x10);*(unsigned short*)(mock_people[2]+0x10)=6000;
    void *boundary_record[]={mock_people[2],NULL,mock_cities[50]};publish_target(mock_people[2],boundary_record);
    assert(workspace.targets[6000]==mock_cities[50]);workspace_end(&workspace);assert(!*table_slot);
    *(unsigned short*)(mock_people[2]+0x10)=old_id;
    void **existing=calloc(SEARCH_PEOPLE,sizeof(void*));assert(existing);existing[1]=mock_area;*table_slot=existing;
    assert(workspace_begin(&workspace,mock_world) && workspace.previous==existing && !workspace.targets[1]);
    void *record[]={mock_people[0],NULL,mock_cities[1]};publish_target(mock_people[0],record);
    assert(workspace.targets[1]==mock_cities[1]);workspace_end(&workspace);assert(*table_slot==existing && existing[1]==mock_area);
    *table_slot=NULL;free(existing);
    DWORD protection;assert(VirtualProtect(table_slot,8,PAGE_READONLY,&protection));
    assert(!workspace_begin(&workspace,mock_world));workspace_end(&workspace);assert(VirtualProtect(table_slot,8,protection,&protection));
    s14_search_configure(S14_MASTER|S14_AUTO_SEARCH,0x123); dispatch_search();
    assert(!search_fault && seen_settings==0x123 && native_submissions==1 && mock_force[0x14]==3);
    assert(allocated_maps==2 && freed_maps==2 && lists_created==1 && lists_destroyed==1);
    assert(latest_begin().dispatched==1 && !*table_slot);scratch_restored(); // native rejection and scratch restoration
    dispatch_search(); assert(native_submissions==1 && lists_created==1); // repeated same turn
    mock_day=110; mock_force[0x14]=1; dispatch_search(); assert(lists_created==1 && latest_begin().dispatched==0); // insufficient orders
    mock_day=120; mock_force[0x14]=5; mock_candidates=0; dispatch_search(); assert(native_submissions==1 && allocated_maps==4 && freed_maps==4);
    s14_search_configure(S14_MASTER,0); mock_day=130; dispatch_search(); assert(lists_created==2); // toggle off
    s14_search_configure(S14_MASTER|S14_AUTO_SEARCH,0); mock_cost=0; dispatch_search(); assert(search_fault && native_submissions==1);
    original_person=find_person; original_treasure=find_treasure; original_money=find_money; original_log=record_log; original_outcome=finish_outcome; original_execute=resolve_search;
    jump_to(0x1040,hooked_person); jump_to(0x1060,hooked_treasure); jump_to(0x1080,hooked_money); jump_to(0x10a0,hooked_outcome); jump_to(0x10c0,hooked_log);
    last_force=1; last_world=(uintptr_t)mock_settings; unsigned char command[0x80]={0}; command[0x28]=1;
    LONG first=queue_sequence;
    for (int i=0;i<5;i++) {
        result_mode=i; assert(hooked_execute(command)==29 && !current_result);
        QueueSlot *slot=&queue[(unsigned int)(first+i+1)%QSIZE]; assert(slot->state==2);
        assert(slot->event.type==(i==4?S14_SEARCH_NOTHING:i+1) && slot->event.force==1);
        assert(!wcscmp(slot->event.detail,L"发现名品：青釭剑"));
        assert(!wcscmp(slot->event.actor,L"曹操") && !wcscmp(slot->event.location,L"襄阳郡"));
        if (i==3) assert(slot->event.amount==1234);
    }
    LONG before=queue_sequence; command[0x28]=2; assert(hooked_execute(command)==29 && queue_sequence==before); // AI faction excluded
    command[0x28]=1; s14_search_configure(S14_MASTER,0); assert(hooked_execute(command)==29 && queue_sequence==before);
    assert(log_calls==7 && outcome_calls==7); // all original callbacks and return values preserved
    original_progress=progress_original; jump_to(0x10e0,hooked_progress); search_fault=0; mock_cost=2; mock_candidates=3; mock_day=140;
    memset(&guard,0,sizeof(guard)); current_user_state=mock_area; s14_search_configure(S14_MASTER|S14_AUTO_SEARCH,0);
    ProgressFunction other=call_site(0x3f9800,0x10e0,0),accepted=call_site(0x3f9441,0x10e0,0);
    unsigned int before_lists=lists_created; other(0); assert(lists_created==before_lists);
    accepted(0); assert(lists_created==before_lists+1); accepted(0); assert(lists_created==before_lists+1);
    current_user_state=NULL; mock_day=150; accepted(0); assert(lists_created==before_lists+1 && progress_calls==4);
    // New campaigns may assign completely different force and group IDs.
    // The original player selector, not array-index equality, owns selection.
    unsigned char strategy[0x480]={0};
    memset(&guard,0,sizeof(guard));mock_day=160;mock_settings[0x3a]=3;
    ((void**)(mock_world+0xde40))[1]=NULL;
    ((void**)(mock_world+0xde40))[3]=mock_foreign_group;
    ((void**)(mock_world+0xde40))[4]=mock_force;
    ((void**)(mock_world+0xde40))[5]=mock_second_group;
    ((void**)(mock_world+0xdca0))[3]=mock_data;
    mock_force[0x10]=mock_second_group[0x10]=3;primary_group=4;
    mock_cities[1][0x20]=mock_cities[50][0x20]=3;
    assert(((unsigned char**)(mock_world+0xde40))[3][0x10]!=mock_settings[0x3a]);
    planning(strategy);int force_id=0,group_id=0;
    assert(s14_search_state(&force_id,&group_id)==S14_SEARCH_MATCHED && force_id==3 && group_id==4);
    unsigned int previous_lists=lists_created;dispatch_search();assert(lists_created==previous_lists+1);
    assert(latest_begin().force==3);dispatch_search();assert(lists_created==previous_lists+1);
    unsigned char command_manager[32]={0},pending_commands[3][64]={{0}};
    unsigned int commands_handle=3;void *pending_nodes[3][2];
    *(void**)(mock_world+0x85128)=command_manager;*(void**)(command_manager+0x18)=&commands_handle;
    mock_heads[commands_handle]=pending_nodes[0];mock_counts[commands_handle]=3;
    for(int i=0;i<3;i++){
        *(uintptr_t*)pending_commands[i]=base_address+0x129bf20;pending_commands[i][0x28]=(unsigned char)(3+i);
        pending_nodes[i][0]=pending_commands[i];pending_nodes[i][1]=i==2?NULL:pending_nodes[i+1];
    }
    assert(pending_searches(mock_world,3)==2 && pending_searches(mock_world,2)==1);
    // Results from our other group are included; another force is excluded.
    command[0x28]=5;result_mode=3;before=queue_sequence;
    assert(hooked_execute(command)==29 && queue_sequence==before+1);
    assert(queue[(unsigned int)queue_sequence%QSIZE].event.force==3);
    command[0x28]=3;before=queue_sequence;assert(hooked_execute(command)==29 && queue_sequence==before);
    // Same-force, same-day loads can reuse both pointers. A successful native
    // load epoch resets de-duplication even without pointer/date changes.
    last_session_epoch=s14_battle_session_epoch()-1;
    planning(strategy);previous_lists=lists_created;dispatch_search();assert(lists_created==previous_lists+1);
    dispatch_search();assert(lists_created==previous_lists+1);
    // Recover a context mismatch after validation, without toggling/restarting.
    primary_group=3;mock_day=170;planning(strategy);
    assert(search_fault==2 && s14_search_state(NULL,NULL)==S14_SEARCH_CONTEXT_PAUSED);
    previous_lists=lists_created;dispatch_search();assert(lists_created==previous_lists);
    primary_group=4;planning(strategy);assert(!search_fault && s14_search_is_ready());
    dispatch_search();assert(lists_created==previous_lists+1);
    // Every supported force ID can use a different group-table index.
    for(int id=1;id<=51;id++){
        int selected=(id+17)%51+1;mock_settings[0x3a]=(unsigned char)id;
        mock_force[0x10]=(unsigned char)id;((void**)(mock_world+0xdca0))[id]=mock_data;
        memset(mock_world+0xde40,0,52*8);((void**)(mock_world+0xde40))[selected]=mock_force;primary_group=selected;
        unsigned char *matched=NULL;void *owned=NULL;int group=-1;
        assert(player_binding(mock_world,mock_settings,id,&matched,&owned,&group));
        assert(matched==mock_force && owned==mock_data && group==selected && group!=id);
    }
    unsigned char *matched=NULL;void *invalid_data=NULL;int group=-1;
    assert(!player_binding(mock_world,mock_settings,0,&matched,&invalid_data,&group));
    assert(!player_binding(mock_world,mock_settings,52,&matched,&invalid_data,&group));
    // A new save must never clear a fatal native/scratch/queue fault.
    fail(L"synthetic fatal fault");last_session_epoch=s14_battle_session_epoch()-1;planning(strategy);
    assert(search_fault==1 && s14_search_state(NULL,NULL)==S14_SEARCH_STOPPED);
    assert(allocated_maps==freed_maps && lists_created==lists_destroyed && !*table_slot);scratch_restored();
    VirtualFree(mock_image,0,MEM_RELEASE); free(mock_world); free(mock_settings);
    printf("{\"supported_force_ids_tested\":51,\"pending_search_group_mapping\":true,\"adaptive_player_force\":true,\"distinct_force_group_ids\":true,\"multi_group_result_ownership\":true,\"same_day_load_resets_guard\":true,\"context_recovers_after_validation\":true,\"fatal_fault_not_cleared\":true,\"native_dispatch_mock\":true,\"confirmed_progress_provenance\":true,\"native_route_cache\":true,\"settings_abi\":true,\"native_rejection\":true,\"remaining_orders\":true,\"no_candidate\":true,\"cleanup_balanced\":true,\"feature_off\":true,\"invalid_cost_stops_search\":true,\"four_result_callbacks\":true,\"native_log_12_arguments\":true,\"native_log_full_width_return\":true,\"original_returns_preserved\":true,\"ai_faction_excluded\":true,\"city_predicate_two_arguments\":true,\"foreign_city_filter\":true,\"empty_city_iterator\":true,\"target_table_indirection\":true,\"target_table_last_person\":true,\"existing_table_preserved\":true,\"unwritable_table_rejected\":true,\"person_scratch_restored\":true,\"private_native_iterator_fixture\":%s,\"private_native_target_store\":%s,\"game_process_touched\":false}\n",native_fixture?"true":"false",native_fixture?"true":"false"); return 0;
}
