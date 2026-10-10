#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <shellapi.h>
#include "MinHook.h"
#include "rule.h"
#include "interaction.h"
#include "toast.h"
#include "features.h"
#include "manager_ui.h"
#include "package.h"
#include "auto_search.h"
#include "battle_probe.h"
#include "turn_report.h"
#include "report_ui.h"
#include "search_model.h"
#include "officer_ui.h"
#include "detail_ui.h"
#include "army_ui.h"
#include "army_buff.h"
#include "career_affix.h"
#include "ai_affix.h"
#include "affix_names.h"
#include "map_effects_ui.h"
#include "map_render.h"
#include "troop_registry.h"
#include "troop_runtime.h"
#include "troop_ui.h"
#include "personality_edit.h"

#define COMMON_RVA 0x251610
#define CREATE_RVA 0x2a7e60
#define WRAPPER_RVA 0x251440
#define ENTER_RVA 0x702ed0
#define EXIT_RVA 0x701f00
#define CONSTRUCTION_VTABLE_RVA 0x133f170
#define FORCE_ID_RVA 0x20b060
#define MAP_WIDTH 220
#define MAP_CELLS 48400
#define RING_SIZE 256

static const unsigned char common_signature[15]={0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20};
static const unsigned char create_signature[15]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20};
static const unsigned char wrapper_signature[15]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x48,0x83,0xec,0x20};
static const unsigned char enter_signature[12]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
static const unsigned char exit_signature[14]={0x40,0x53,0x55,0x56,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0x48,0x8b,0xe9};

typedef int (*CheckFunction)(void*,void*,void*,int);
typedef int (*WrapperFunction)(void*,void*,void*);
typedef uintptr_t (*StateFunction)(void*,void*);
typedef void* (*CreateFunction)(const int*);
typedef int (*ForceIdFunction)(void*);
typedef HRESULT (WINAPI *InputCreate)(HINSTANCE,DWORD,const GUID*,void**,void*);
typedef HRESULT (WINAPI *SimpleCom)(void);
typedef HRESULT (WINAPI *GetClassObject)(const GUID*,const GUID*,void**);

static HMODULE own_module, real_input;
static INIT_ONCE input_once=INIT_ONCE_STATIC_INIT;
static volatile LONG worker_started, mode;
static volatile LONG effective_flags;
static int runtime_fault, hooks_ready;
static wchar_t game_folder[MAX_PATH];
static uintptr_t image_base, image_end;
static unsigned char **manager_slot;
static CheckFunction original_check;
static CreateFunction original_create;
static WrapperFunction original_wrapper;
static StateFunction original_enter, original_exit;
static PVOID volatile construction_state;
static volatile LONG construction_thread;
static wchar_t ini_path[MAX_PATH], log_folder[MAX_PATH];
static volatile LONG64 checks, original_rejected, would_reject, enforced, creations, decode_errors, dropped;
static volatile LONG next_event;
static volatile LONG active_checks, peak_active_checks;
static volatile LONG64 preview_allowed, toast_requests;

typedef struct {
    DWORD tid;
    uintptr_t caller;
    int event, tile, type, owner, original, final, reason, count, reads;
    int durability, flags, matched_validation, territory_owner, builder_owner;
    ULONGLONG validation_age_ms;
    uintptr_t wrapper_caller;
    int phase;
    POINT click;
} Event;
typedef struct { volatile LONG state; Event event; } EventSlot;
static EventSlot ring[RING_SIZE];
static EventSlot toast_slot;
typedef struct { int tile,type,owner,accepted; ULONGLONG time; int builder; } LastValidation;
static _Thread_local LastValidation last_validation;
static _Thread_local Event last_logged_check;
static _Thread_local int last_logged_valid;
static _Thread_local uintptr_t wrapper_caller;

static int is_wall(int type) { return type==9 || type==10; }
static int executable_address(uintptr_t address) { return address>=image_base+0x1000 && address<image_end; }

static void queue_event(Event event) {
    if (event.event==2) {
        // Reserve a slot for click feedback so preview log traffic cannot
        // consume its capacity. Repeated clicks replace a pending request.
        LONG state=InterlockedCompareExchange(&toast_slot.state,0,0);
        if ((state!=0 && state!=2) || InterlockedCompareExchange(&toast_slot.state,1,state)!=state) return;
        toast_slot.event=event; InterlockedExchange(&toast_slot.state,2); return;
    }
    if (!(InterlockedCompareExchange(&effective_flags,0,0)&S14_DIAGNOSTICS)) return;
    if (event.event==0 && last_logged_valid && event.tile==last_logged_check.tile &&
        event.type==last_logged_check.type && event.owner==last_logged_check.owner &&
        event.caller==last_logged_check.caller && event.original==last_logged_check.original &&
        event.final==last_logged_check.final && event.reason==last_logged_check.reason &&
        event.count==last_logged_check.count && event.reads==last_logged_check.reads &&
        event.wrapper_caller==last_logged_check.wrapper_caller && event.phase==last_logged_check.phase) return;
    if (event.event==0) { last_logged_check=event; last_logged_valid=1; }
    unsigned int index=(unsigned int)InterlockedIncrement(&next_event)%RING_SIZE;
    EventSlot *slot=&ring[index];
    if (InterlockedCompareExchange(&slot->state,1,0)!=0) { InterlockedIncrement64(&dropped); return; }
    slot->event=event;
    InterlockedExchange(&slot->state,2);
}

typedef struct { unsigned char *manager, *pool; } GameMap;
static int hex_force(const unsigned char *hex) {
    int owner=hex[0x14];
    return owner<=51?owner:0;
}
static S14Cell cell_from_records(const unsigned char *hex,const unsigned char *building) {
    if (!building) return (S14Cell){0,-1};
    int type=building[0x12];
    if (!type || type>30) return (S14Cell){0,-1};
    // Category-3 walls store neutral 0 in building+0x13. Ownership comes
    // from the hex, using the exact normalization in game RVA 0x20a0a0.
    return (S14Cell){type,hex_force(hex)};
}
static int decode_map(GameMap *map) {
    if (!manager_slot || !*manager_slot) return 0;
    map->manager=*manager_slot;
    unsigned char **pool_table=(unsigned char**)(map->manager+0xdfe0);
    map->pool=pool_table[0];
    return map->pool && pool_table[MAP_CELLS-1]==map->pool+(MAP_CELLS-1)*32;
}
static S14Cell game_lookup(void *context,int tile) {
    GameMap *map=context;
    unsigned char *hex=map->pool+(size_t)tile*32;
    unsigned short id=0;
    memcpy(&id,hex+0x1a,sizeof(id));
    if (!id || id>3000) return (S14Cell){0,-1};
    unsigned char *building=((unsigned char**)(map->manager+0x6d808))[id];
    return cell_from_records(hex,building);
}

static int decode_call(void *force,void *target,GameMap *map,int *tile,int *owner,int *builder) {
    if (!force || !target || !decode_map(map)) return 0;
    intptr_t delta=(intptr_t)((uintptr_t)target-(uintptr_t)map->pool);
    if (delta<0 || delta%32 || delta/32>=MAP_CELLS) return 0;
    *tile=(int)(delta/32);
    uintptr_t *vtable=*(uintptr_t**)force;
    // This is the same virtual getter called by the inspected original check.
    if (!vtable || vtable[12]!=image_base+FORCE_ID_RVA) return 0;
    *builder=((ForceIdFunction)vtable[12])(force);
    // Proposed wall belongs to its current tile too, including allied land.
    *owner=hex_force(target);
    return *builder>=0 && *builder<=51;
}

static int hooked_wrapper(void *troops,void *target,void *definition) {
    uintptr_t previous=wrapper_caller;
    wrapper_caller=(uintptr_t)__builtin_return_address(0)-image_base;
    int result=original_wrapper(troops,target,definition);
    wrapper_caller=previous;
    return result;
}

static uintptr_t hooked_enter(void *state,void *argument) {
    void *definition=*(void**)((unsigned char*)state+0x490);
    unsigned char *manager=manager_slot?*manager_slot:NULL;
    void **definitions=manager?(void**)(manager+0x736c8):NULL;
    int wall=definitions && (definition==definitions[9] || definition==definitions[10]);
    InterlockedExchange(&construction_thread,wall?(LONG)GetCurrentThreadId():0);
    InterlockedExchangePointer(&construction_state,wall?state:NULL);
    return original_enter(state,argument);
}

static uintptr_t hooked_exit(void *state,void *argument) {
    if (InterlockedCompareExchangePointer(&construction_state,NULL,state)==state)
        InterlockedExchange(&construction_thread,0);
    return original_exit(state,argument);
}

static int hooked_check(void *force,void *target,void *definition,int extra) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    int original=original_check(force,target,definition,extra);
    unsigned int current_flags=(unsigned int)InterlockedCompareExchange(&effective_flags,0,0);
    int current_mode=s14_runtime_mode(current_flags);
    if (!current_mode || !manager_slot || !*manager_slot) return original;
    unsigned char *manager=*manager_slot;
    void **definitions=(void**)(manager+0x736c8);
    int type=definition==definitions[9]?9:(definition==definitions[10]?10:0);
    if (!type) return original;
    InterlockedIncrement64(&checks);
    if (!original) { InterlockedIncrement64(&original_rejected); last_validation.accepted=0; return original; }
    GameMap map;
    int tile=-1,owner=-1,builder=-1;
    if (!decode_call(force,target,&map,&tile,&owner,&builder)) {
        InterlockedIncrement64(&decode_errors); InterlockedExchange(&mode,0); InterlockedExchange(&effective_flags,0); return original;
    }
    LONG active=InterlockedIncrement(&active_checks);
    LONG previous=InterlockedCompareExchange(&peak_active_checks,0,0);
    while (active>previous) {
        LONG observed=InterlockedCompareExchange(&peak_active_checks,active,previous);
        if (observed==previous) break;
        previous=observed;
    }
    S14Cell existing=game_lookup(&map,tile);
    int continuing=is_wall(existing.type) && existing.type==type && existing.owner==owner;
    S14Decision decision=s14_evaluate(game_lookup,&map,MAP_WIDTH,MAP_WIDTH,tile,type,owner,1,continuing);
    InterlockedDecrement(&active_checks);
    uintptr_t common_caller=caller-image_base;
    int on_construction_thread=InterlockedCompareExchangePointer(&construction_state,NULL,NULL)!=NULL &&
        (DWORD)InterlockedCompareExchange(&construction_thread,0,0)==GetCurrentThreadId();
    int phase=s14_check_phase(common_caller,wrapper_caller,on_construction_thread);
    int final=s14_check_result(original,decision.allowed,current_mode,phase);
    if (decision.reason==S14_INVALID) { final=original; InterlockedIncrement64(&decode_errors); InterlockedExchange(&mode,0); InterlockedExchange(&effective_flags,0); }
    else if (!decision.allowed) {
        InterlockedIncrement64(&would_reject);
        if (current_mode==2) {
            if (phase==S14_PHASE_PREVIEW) InterlockedIncrement64(&preview_allowed);
            else InterlockedIncrement64(&enforced);
        }
    }
    last_validation=(LastValidation){tile,type,owner,final!=0,GetTickCount64(),builder};
    Event event={0}; event.tid=GetCurrentThreadId(); event.caller=common_caller;
    event.wrapper_caller=wrapper_caller; event.phase=phase;
    event.tile=tile; event.type=type; event.owner=owner; event.original=original; event.final=final;
    event.territory_owner=owner;
    event.builder_owner=builder;
    event.reason=decision.reason; event.count=decision.count; event.reads=decision.reads;
    queue_event(event);
    if (current_mode==2 && (current_flags&S14_LIMIT_HINT) && phase==S14_PHASE_COMMIT && decision.reason==S14_LIMIT && !final) {
        // This call site is gated by the game's confirm/click input edge.
        // Hover enumeration and AI checks never request a popup.
        event.event=2;
        if (GetCursorPos(&event.click)) { InterlockedIncrement64(&toast_requests); queue_event(event); }
    }
    return final;
}

static int matches_validation(int tile,int type,int territory_owner) {
    return last_validation.accepted && last_validation.tile==tile &&
        last_validation.type==type && last_validation.owner==territory_owner;
}

static void *hooked_create(const int *parameters) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    int tile=parameters[0],type=parameters[1],owner=parameters[2];
    void *created=original_create(parameters);
    if (!s14_runtime_mode((unsigned int)InterlockedCompareExchange(&effective_flags,0,0)) || !is_wall(type)) return created;
    InterlockedIncrement64(&creations);
    Event event={0}; event.event=1; event.tid=GetCurrentThreadId(); event.caller=caller-image_base;
    event.tile=tile; event.type=type; event.owner=owner; event.original=created!=NULL; event.final=created!=NULL;
    event.count=-1; event.reason=-1;
    GameMap map;
    event.territory_owner=-1;
    if (tile>=0 && tile<MAP_CELLS && decode_map(&map)) {
        event.territory_owner=hex_force(map.pool+(size_t)tile*32);
    } else {
        InterlockedIncrement64(&decode_errors); InterlockedExchange(&mode,0); InterlockedExchange(&effective_flags,0);
    }
    event.matched_validation=matches_validation(tile,type,event.territory_owner);
    event.builder_owner=event.matched_validation?last_validation.builder:-1;
    event.validation_age_ms=last_validation.time?GetTickCount64()-last_validation.time:(ULONGLONG)-1;
    if (created) {
        unsigned char *data=created;
        unsigned short durability=0; memcpy(&durability,data+0x14,sizeof(durability));
        event.durability=durability; event.flags=data[0x16];
    }
    queue_event(event);
    return created;
}

// These checks concern the bytes about to be patched, not a version or
// executable fingerprint. Refuse unreadable/out-of-image addresses before
// dereferencing them, including when the host image uses a different layout.
static int mapped_bytes(uintptr_t address,size_t length,uintptr_t allocation) {
    if (!length || address>UINTPTR_MAX-length) return 0;
    uintptr_t end=address+length;
    while (address<end) {
        MEMORY_BASIC_INFORMATION region;
        if (!VirtualQuery((void*)address,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
            (uintptr_t)region.AllocationBase!=allocation || (region.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return 0;
        uintptr_t next=(uintptr_t)region.BaseAddress+region.RegionSize;
        if (next<=address) return 0;
        address=next<end?next:end;
    }
    return 1;
}
static int image_span(size_t rva,size_t length) {
    size_t size=image_end-image_base;
    return rva<=size && length<=size-rva && mapped_bytes(image_base+rva,length,image_base);
}
static int prepare_image(uintptr_t base) {
    image_base=base; image_end=base; manager_slot=NULL;
    if (!mapped_bytes(base,sizeof(IMAGE_DOS_HEADER),base)) return 0;
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)image_base;
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 ||
        (uintptr_t)dos->e_lfanew>UINTPTR_MAX-image_base ||
        !mapped_bytes(image_base+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS64),base)) return 0;
    IMAGE_NT_HEADERS64 *pe=(IMAGE_NT_HEADERS64*)(image_base+dos->e_lfanew);
    if (pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        pe->OptionalHeader.SizeOfImage>UINTPTR_MAX-image_base) return 0;
    image_end=image_base+pe->OptionalHeader.SizeOfImage;
    if (!image_span(COMMON_RVA,sizeof(common_signature)) || !image_span(CREATE_RVA,sizeof(create_signature)) ||
        !image_span(WRAPPER_RVA,sizeof(wrapper_signature)) || !image_span(ENTER_RVA,sizeof(enter_signature)) ||
        !image_span(EXIT_RVA,sizeof(exit_signature)) || !image_span(CONSTRUCTION_VTABLE_RVA,16*sizeof(uintptr_t)) ||
        !image_span(0x20cdcd,7) || !image_span(0x1fc91d0,sizeof(void*)) || !image_span(FORCE_ID_RVA,1)) return 0;
    if (memcmp((void*)(image_base+COMMON_RVA),common_signature,15) ||
        memcmp((void*)(image_base+CREATE_RVA),create_signature,15) ||
        memcmp((void*)(image_base+WRAPPER_RVA),wrapper_signature,sizeof(wrapper_signature)) ||
        memcmp((void*)(image_base+ENTER_RVA),enter_signature,sizeof(enter_signature)) ||
        memcmp((void*)(image_base+EXIT_RVA),exit_signature,sizeof(exit_signature))) return 0;
    uintptr_t *state_vtable=(uintptr_t*)(image_base+CONSTRUCTION_VTABLE_RVA);
    if (state_vtable[1]!=image_base+ENTER_RVA || state_vtable[2]!=image_base+EXIT_RVA ||
        state_vtable[5]!=image_base+0x707050 || state_vtable[15]!=image_base+0x722490) return 0;
    unsigned char *instruction=(unsigned char*)(image_base+0x20cdcd);
    if (memcmp(instruction,"\x48\x8b\x05",3)) return 0;
    int32_t relative=0; memcpy(&relative,instruction+3,4);
    uintptr_t slot=(uintptr_t)(instruction+7)+relative;
    if (slot!=image_base+0x1fc91d0 || !executable_address(image_base+FORCE_ID_RVA)) return 0;
    static const size_t returns[]={0x6eebe0,0x7089fd,0x70d9bc,0x722571,0x724406,
        0x733cd,0x7370a,0x74245,0x74605,0x70744c,0x26015d,0x70d9fa,0x37894d};
    for (size_t i=0;i<sizeof(returns)/sizeof(returns[0]);i++) {
        if (!image_span(returns[i]-5,5)) return 0;
        unsigned char *call=(unsigned char*)(image_base+returns[i]-5);
        int32_t displacement; memcpy(&displacement,call+1,4);
        size_t target=i<11?WRAPPER_RVA:COMMON_RVA;
        if (call[0]!=0xe8 || (int64_t)returns[i]+displacement!=(int64_t)target) return 0;
    }
    manager_slot=(unsigned char**)slot;
    return 1;
}
static int prepare_game(void) { return prepare_image((uintptr_t)GetModuleHandleW(NULL)); }

static void write_line(HANDLE file,const char *line) {
    static size_t written_bytes;
    if (file==INVALID_HANDLE_VALUE) return;
    if (written_bytes>=16*1024*1024) return;
    DWORD written=0; WriteFile(file,line,(DWORD)strlen(line),&written,NULL); written_bytes+=written;
}

static BOOL CALLBACK find_game_window(HWND window,LPARAM result) {
    DWORD pid=0; GetWindowThreadProcessId(window,&pid);
    if (pid!=GetCurrentProcessId()) return TRUE;
    wchar_t name[64]; if (!GetClassNameW(window,name,64) || wcscmp(name,L"KT_SANGOKUSHI14_WINDOW")) return TRUE;
    *(HWND*)result=window; return FALSE;
}

static int foreground_is(HWND owner) {
    return owner && s14_owned_foreground(owner,NULL,GetForegroundWindow());
}

static void drain_events(HANDLE file,S14Toast *toast,HWND owner) {
    for(int i=0;i<=RING_SIZE;i++) {
        EventSlot *slot=i==RING_SIZE?&toast_slot:&ring[i];
        if (InterlockedCompareExchange(&slot->state,3,2)!=2) continue;
        Event event=slot->event; InterlockedExchange(&slot->state,0);
        char line[896];
        snprintf(line,sizeof(line),"{\"event\":\"%s\",\"tid\":%lu,\"caller_rva\":\"0x%llx\",\"tile\":%d,\"type\":%d,\"owner\":%d,\"original\":%d,\"final\":%d,\"reason\":%d,\"count\":%d,\"reads\":%d,\"durability\":%d,\"flags\":%d,\"matched_validation\":%d,\"validation_age_ms\":%llu,\"territory_owner\":%d,\"builder_owner\":%d,\"wrapper_caller_rva\":\"0x%llx\",\"phase\":%d}\n",
            event.event==2?"toast_request":(event.event?"create":"check"),(unsigned long)event.tid,(unsigned long long)event.caller,event.tile,event.type,event.owner,
            event.original,event.final,event.reason,event.count,event.reads,event.durability,event.flags,event.matched_validation,
            (unsigned long long)event.validation_age_ms,event.territory_owner,event.builder_owner,(unsigned long long)event.wrapper_caller,event.phase);
        write_line(file,line);
        if (event.event==2) {
            int shown=(InterlockedCompareExchange(&effective_flags,0,0)&S14_LIMIT_HINT) && foreground_is(owner) &&
                s14_toast_show(toast,own_module,owner,event.click,GetTickCount64());
            snprintf(line,sizeof(line),"{\"event\":\"toast_shown\",\"tile\":%d,\"shown\":%d,\"duration_ms\":5000,\"window\":\"0x%llx\"}\n",
                event.tile,shown,(unsigned long long)(uintptr_t)toast->window);
            write_line(file,line);
        }
    }
}

static void apply_configuration(S14ManagerUI *ui) {
    ui->requested=s14_config_read(ini_path);
    ui->effective=hooks_ready && !runtime_fault && !InterlockedCompareExchange64(&decode_errors,0,0)?s14_effective_flags(ui->requested):0;
    ui->search_state=s14_search_state(&ui->search_force,&ui->search_group);
    if(ui->search_state==S14_SEARCH_WAITING)wcscpy(ui->search_status,L"等待游戏载入玩家势力；载入后自动匹配，不需要选择势力编号。");
    if(ui->search_state==S14_SEARCH_UNAVAILABLE)wcscpy(ui->search_status,L"探索入口未通过指令校验，自动搜索不可用。");
    if (!s14_search_is_ready()) ui->effective&=~S14_AUTO_SEARCH;
    if(!s14_army_buff_ready() || !s14_affix_names_ready())ui->effective&=~S14_CAO_REN_BUFF;
    s14_army_buff_configure(!!(ui->effective&S14_CAO_REN_BUFF));
    if(!s14_troop_ready())ui->effective&=~S14_PLUGIN_TROOPS;
    s14_troop_configure(!!(ui->effective&S14_PLUGIN_TROOPS));
    if(!s14_troop_ready() || !s14_army_buff_ready() || !s14_affix_names_ready())ui->effective&=~S14_AI_RANDOM_AFFIX;
    s14_ai_configure(!!(ui->effective&S14_AI_RANDOM_AFFIX));
    s14_personality_enabled(hooks_ready && !runtime_fault && !!(ui->effective&S14_MASTER) && s14_manager_view_enabled(ui,0));
    ui->search_settings=s14_search_settings_read(ini_path);
    ui->officers_enabled=GetPrivateProfileIntW(L"Views",L"Officers",1,ini_path)!=0;
    ui->views_enabled=s14_views_setting_read(ini_path);
    ui->native_stats_enabled=GetPrivateProfileIntW(L"Views",L"NativeOfficerStats",1,ini_path)!=0;
    ui->native_army_enabled=GetPrivateProfileIntW(L"Views",L"NativeArmyValues",1,ini_path)!=0;
    ui->visual_settings=s14_visual_settings_read(ini_path);
    ui->battle_enabled=s14_battle_setting_read(ini_path);
    s14_battle_configure(ui->effective,ui->battle_enabled);
    // Context matching must keep running even while its effective bit is off.
    s14_search_configure(hooks_ready && !runtime_fault?s14_effective_flags(ui->requested):0,ui->search_settings);
    InterlockedExchange(&effective_flags,(LONG)ui->effective);
    InterlockedExchange(&mode,s14_runtime_mode(ui->effective));
    ui->fault=runtime_fault; ui->attached=hooks_ready;
    wcscpy(ui->status,runtime_fault?s14_fault_message(runtime_fault):
        (ui->requested&S14_MASTER?L"游戏已接入 · 开关从下一次检查起生效":L"扩展功能已关闭 · F10 管理器仍可使用"));
    s14_manager_refresh(ui);
}
static void publish_manager_runtime(S14ManagerUI *ui) {
    s14_publish_search_runtime(game_folder,ui->search_state,ui->search_force,ui->search_group,ui->search_status);
    s14_publish_runtime(game_folder,ui->requested,ui->effective,hooks_ready,runtime_fault);
}
static void game_manager_action(S14ManagerUI *ui,int action,void *context) {
    (void)context;
    if (action==S14_ACTION_CONFIG) { apply_configuration(ui); publish_manager_runtime(ui); }
    if (action==S14_ACTION_LOGS) ShellExecuteW(ui->window,L"open",log_folder,NULL,NULL,SW_SHOWNORMAL);
    if(action==S14_ACTION_CHECK_UPDATE){
        wchar_t manager[MAX_PATH],args[MAX_PATH+64];
        if(s14_join(manager,game_folder,L"SAN14ModManager.exe")){
            swprintf(args,MAX_PATH+64,L"--updates \"%ls\"",game_folder);
            INT_PTR result=(INT_PTR)ShellExecuteW(ui->window,L"open",manager,args,game_folder,SW_SHOWNORMAL);
            wcscpy(ui->notice,result>32?L"已打开独立更新管理器。安装新版前请保存并退出游戏。":L"无法打开更新管理器，请从发布包运行新版安装器。");
            ui->notice_error=result<=32;
        }
    }
}

static void pump_battle_menu(void){s14_battle_worker(game_folder);}
static DWORD WINAPI plugin_worker(LPVOID unused) {
    (void)unused;
    wchar_t executable[MAX_PATH]; if (!GetModuleFileNameW(NULL,executable,MAX_PATH)) return 0;
    wchar_t *game_name=wcsrchr(executable,L'\\'); if (!game_name || _wcsicmp(game_name+1,L"SAN14PK_SC.exe")) return 0;
    wchar_t module_path[MAX_PATH];
    if (!GetModuleFileNameW(own_module,module_path,MAX_PATH)) return 0;
    wchar_t *slash=wcsrchr(module_path,L'\\'); if (!slash) return 0; *slash=0;
    if (wcslen(module_path)>MAX_PATH-90) return 0;
    swprintf(ini_path,MAX_PATH,L"%ls\\SAN14BuildLimit.ini",module_path);
    wcscpy(game_folder,module_path);
    wchar_t manager_folder[MAX_PATH]; swprintf(manager_folder,MAX_PATH,L"%ls\\SAN14ModManager",module_path); CreateDirectoryW(manager_folder,NULL);
    swprintf(log_folder,MAX_PATH,L"%ls\\SAN14ModManager\\logs",module_path);
    unsigned int requested_flags=s14_config_read(ini_path);
    int requested=s14_runtime_mode(requested_flags);
    CreateDirectoryW(log_folder,NULL);
    SYSTEMTIME now; GetLocalTime(&now);
    wchar_t log_path[MAX_PATH];
    swprintf(log_path,MAX_PATH,L"%ls\\plugin-%04d%02d%02d-%02d%02d%02d-%lu.jsonl",log_folder,
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,(unsigned long)GetCurrentProcessId());
    HANDLE log=CreateFileW(log_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (log==INVALID_HANDLE_VALUE) return 0;
    int prepared=prepare_game();
    if (!prepared) {
        write_line(log,"{\"event\":\"startup_failed\",\"reason\":\"hook_entry_unavailable\",\"game_version_check\":false}\n");
        runtime_fault=S14_FAULT_ENTRY;
    }
    MH_STATUS status=prepared?MH_Initialize():MH_ERROR_NOT_EXECUTABLE;
    if (status==MH_OK) status=MH_CreateHook((void*)(image_base+COMMON_RVA),hooked_check,(void**)&original_check);
    if (status==MH_OK) status=MH_CreateHook((void*)(image_base+CREATE_RVA),hooked_create,(void**)&original_create);
    if (status==MH_OK) status=MH_CreateHook((void*)(image_base+WRAPPER_RVA),hooked_wrapper,(void**)&original_wrapper);
    if (status==MH_OK) status=MH_CreateHook((void*)(image_base+ENTER_RVA),hooked_enter,(void**)&original_enter);
    if (status==MH_OK) status=MH_CreateHook((void*)(image_base+EXIT_RVA),hooked_exit,(void**)&original_exit);
    int search_ready=status==MH_OK && s14_search_install(image_base,image_end,manager_slot);
    int battle_ready=status==MH_OK && search_ready && s14_battle_install(image_base,image_end,manager_slot);
    int army_ready=status==MH_OK && s14_army_observer_install(image_base,image_end);
    int buff_ready=status==MH_OK && s14_army_buff_install(image_base,image_end);
    int name_ready=status==MH_OK && s14_affix_names_install(image_base,image_end);
    s14_troop_root(game_folder);
    int troop_ready=status==MH_OK && battle_ready && army_ready && buff_ready && name_ready && s14_troop_install(image_base,image_end);
    s14_ai_attach(image_base,game_folder,troop_ready && buff_ready && name_ready && battle_ready);
    int map_ready=status==MH_OK && s14_map_render_install();
    int personality_ready=battle_ready && search_ready && s14_personality_init(image_base,image_end);
    char personality_startup[192];snprintf(personality_startup,sizeof(personality_startup),"{\"event\":\"personality_editor_ready\",\"ready\":%d,\"slots\":9,\"preset_id\":6,\"native_setter\":true,\"in_game_acceptance\":\"pending\"}\n",personality_ready);write_line(log,personality_startup);
    if (status==MH_OK) status=MH_EnableHook(MH_ALL_HOOKS);
    char startup[384],version[32]={0}; WideCharToMultiByte(CP_UTF8,0,S14_MANAGER_VERSION,-1,version,sizeof(version),NULL,NULL);
    snprintf(startup,sizeof(startup),"{\"event\":\"startup\",\"requested_mode\":%d,\"hook_status\":%d,\"base\":\"0x%llx\",\"wall_owner_source\":\"tile_current_force\",\"interaction_version\":2,\"toast_duration_ms\":5000,\"manager_version\":\"%s\",\"game_version_check\":false,\"search_ready\":%d,\"requested_flags\":%u}\n",requested,(int)status,(unsigned long long)image_base,version,search_ready,requested_flags);
    write_line(log,startup);
    const S14TroopRegistry *troop_catalog=s14_troop_builtin_registry();
    char troop_startup[192];snprintf(troop_startup,sizeof(troop_startup),
        "{\"event\":\"troop_registry\",\"schema_version\":%d,\"definitions\":%u,\"engine_capabilities\":%u,\"gameplay_integrated\":%s}\n",
        S14_TROOP_SCHEMA,(unsigned)s14_troop_registry_count(troop_catalog),troop_ready?s14_troop_capabilities():0,troop_ready?"true":"false");write_line(log,troop_startup);
    char name_startup[256];snprintf(name_startup,sizeof(name_startup),"{\"event\":\"career_affix_names_ready\",\"ready\":%d,\"officer_id\":518,\"threshold\":5000,\"name\":\"神 曹仁\",\"native_person_fields_modified\":false}\n",name_ready);write_line(log,name_startup);
    char ai_startup[256];snprintf(ai_startup,sizeof(ai_startup),"{\"event\":\"ai_affix_hooks_ready\",\"ready\":%d,\"probability_percent\":10,\"city_guarantee_interval\":9,\"default_enabled\":false,\"native_rng_modified\":false,\"actual_ai_creation_path\":\"pending_in_game\"}\n",status==MH_OK && troop_ready && buff_ready && name_ready && battle_ready);write_line(log,ai_startup);
    char battle_startup[128];snprintf(battle_startup,sizeof(battle_startup),"{\"event\":\"battle_observer\",\"hooks_ready\":%d,\"schema_version\":7,\"gameplay_modified\":false}\n",battle_ready);write_line(log,battle_startup);
    char army_startup[160];snprintf(army_startup,sizeof(army_startup),"{\"event\":\"army_value_observer\",\"hooks_ready\":%d,\"sample_officer\":518,\"extra_native_calls\":0,\"gameplay_modified\":false}\n",army_ready);write_line(log,army_startup);
    char buff_startup[384];snprintf(buff_startup,sizeof(buff_startup),"{\"event\":\"army_buff_ready\",\"hooks_ready\":%d,\"officer_id\":518,\"attack_percent\":10,\"defense_percent\":10,\"default_enabled\":false,\"extra_native_calls\":0,\"install_code\":%d,\"failed_entry_rva\":\"0x%llx\"}\n",buff_ready,s14_army_buff_install_code(),(unsigned long long)s14_army_buff_failed_entry());write_line(log,buff_startup);
    char map_startup[160];snprintf(map_startup,sizeof(map_startup),"{\"event\":\"map_renderer_install\",\"ready\":%d,\"backend\":\"D3D11_Present\"}\n",map_ready);write_line(log,map_startup);
    if (status!=MH_OK && prepared) { MH_DisableHook(MH_ALL_HOOKS); runtime_fault=S14_FAULT_HOOK; }
    hooks_ready=status==MH_OK;
    ULONGLONG last_summary=0,last_config=0;
    HWND owner=NULL; S14Toast toast={0},search_toast={0},affix_toast={0};S14ReportUI turn_ui={0};
    S14AffixState last_affix={0};
    unsigned int report_epoch=s14_turn_report_epoch();
    S14OfficerUI officers={0};
    S14DetailUI native_detail={0};
    S14ArmyUI army_values={0};S14ArmyTrace last_army_trace={0};int last_army_visible=-1;
    S14TroopUI troop_ui={0};
    S14MapEffectsUI map_effects={0};int last_map_state=-1;
    int last_detail_visible=-1,last_detail_officer=-1,last_detail_ready=-1;
    S14BattleTotals last_detail_stats={0};
    officers.battle=(S14BattleStatsProvider){1,NULL,s14_stats_snapshot};
    officers.pump_events=pump_battle_menu;
    S14ManagerUI ui={0}; ui.action=game_manager_action; ui.game_found=1; ui.running=1; ui.installed=1;
    wcscpy(ui.root,game_folder); wcscpy(ui.ini,ini_path); apply_configuration(&ui);
    s14_search_report_read(game_folder,ui.search_summary,ui.search_details);
    ATOM hotkey_id=GlobalAddAtomW(L"SAN14ModManager.F10.0.2"); int hotkey_registered=0;
    ATOM officer_key=GlobalAddAtomW(L"SAN14ModManager.Officers.F.v1");int officer_registered=0;
    int last_officer_ready=-1,last_officer_registered=-1,last_officer_enabled=-1;
    DWORD officer_error=0,last_officer_error=(DWORD)-1;ULONGLONG last_officer_key_attempt=0,last_officer_create_attempt=0;
    int last_window=-1,last_panel=-1,last_foreground=-1,last_hotkey=-1;
    DWORD hotkey_error=0,last_hotkey_error=(DWORD)-1; ULONGLONG last_hotkey_attempt=0;
    for (;;) {
        if (!owner || !IsWindow(owner)) EnumWindows(find_game_window,(LPARAM)&owner);
        if (owner && !ui.window) { s14_manager_create(&ui,own_module,owner,1); apply_configuration(&ui); }
        if (owner && !officers.window && GetTickCount64()-last_officer_create_attempt>=500) {
            last_officer_create_attempt=GetTickCount64();s14_officer_ui_create(&officers,own_module,owner,image_base);
        }
        int foreground=owner && foreground_is(owner);
        ULONGLONG input_tick=GetTickCount64();
        if (foreground && !hotkey_registered && hotkey_id && input_tick-last_hotkey_attempt>=500) {
            last_hotkey_attempt=input_tick;
            hotkey_registered=RegisterHotKey(NULL,hotkey_id,MOD_NOREPEAT,VK_F10);
            hotkey_error=hotkey_registered?0:GetLastError();
        }
        if (!foreground && hotkey_registered) { UnregisterHotKey(NULL,hotkey_id); hotkey_registered=0; }
        /* Plain F is registered only while the actual game window is focused.
           Search/edit controls in the popup must receive ordinary text input. */
        int officer_enabled=s14_manager_view_enabled(&ui,0);
        int officer_input=owner && GetForegroundWindow()==owner && officer_enabled && officers.window;
        if (officer_input && officer_key && !officer_registered && input_tick-last_officer_key_attempt>=500) {
            last_officer_key_attempt=input_tick;officer_registered=RegisterHotKey(NULL,officer_key,MOD_NOREPEAT,'F');
            officer_error=officer_registered?0:GetLastError();
        }
        if (!officer_input && officer_registered) { UnregisterHotKey(NULL,officer_key);officer_registered=0; }
        int officer_ready=officers.window && IsWindow(officers.window);
        if (officer_ready!=last_officer_ready || officer_registered!=last_officer_registered || officer_enabled!=last_officer_enabled || officer_error!=last_officer_error) {
            char line[192];snprintf(line,sizeof(line),"{\"event\":\"officer_input\",\"panel_ready\":%d,\"enabled\":%d,\"hotkey_registered\":%d,\"hotkey_error\":%lu,\"key\":\"F\"}\n",
                officer_ready,officer_enabled,officer_registered,(unsigned long)officer_error);write_line(log,line);
            last_officer_ready=officer_ready;last_officer_registered=officer_registered;last_officer_enabled=officer_enabled;last_officer_error=officer_error;
        }
        int has_window=owner && IsWindow(owner),has_panel=ui.window && IsWindow(ui.window);
        if (has_window!=last_window || has_panel!=last_panel || foreground!=last_foreground ||
            hotkey_registered!=last_hotkey || hotkey_error!=last_hotkey_error) {
            char line[256];
            snprintf(line,sizeof(line),"{\"event\":\"manager_input\",\"window_found\":%d,\"panel_ready\":%d,\"foreground\":%d,\"hotkey_registered\":%d,\"hotkey_error\":%lu}\n",
                has_window,has_panel,foreground,hotkey_registered,(unsigned long)hotkey_error);
            write_line(log,line); last_window=has_window; last_panel=has_panel;
            last_foreground=foreground; last_hotkey=hotkey_registered; last_hotkey_error=hotkey_error;
        }
        if (!foreground && ui.window && IsWindowVisible(ui.window)) ShowWindow(ui.window,SW_HIDE);
        int officer_hidden=s14_officer_ui_sync_enabled(&officers,officer_enabled);
        if (officer_hidden) {
            write_line(log,"{\"event\":\"officer_hidden\",\"reason\":\"feature_disabled\"}\n");
        }
        MSG message; while (PeekMessageW(&message,NULL,0,0,PM_REMOVE)) {
            if (message.message==WM_HOTKEY && message.wParam==hotkey_id && foreground_is(owner)) {
                if (officers.window && IsWindowVisible(officers.window)) {s14_personality_ui_hide(&officers.personality);s14_history_hide(&officers.timeline);ShowWindow(officers.window,SW_HIDE);KillTimer(officers.window,1);}
                s14_manager_toggle(&ui); char line[96];
                snprintf(line,sizeof(line),"{\"event\":\"manager_toggle\",\"visible\":%d}\n",ui.window && IsWindowVisible(ui.window));
                write_line(log,line);
            }
            else if (message.message==WM_HOTKEY && message.wParam==officer_key && GetForegroundWindow()==owner && officer_enabled) {
                if (officer_registered) { UnregisterHotKey(NULL,officer_key);officer_registered=0; }
                if (ui.window && IsWindowVisible(ui.window)) ShowWindow(ui.window,SW_HIDE);
                s14_battle_worker(game_folder);
                s14_officer_ui_toggle(&officers);
                char line[384];snprintf(line,sizeof(line),"{\"event\":\"officer_toggle\",\"visible\":%d,\"capture_ok\":%d,\"records\":%d,\"filtered_records\":%d,\"read_only\":true,\"panel_hwnd\":\"0x%llx\",\"owner_hwnd\":\"0x%llx\",\"foreground_hwnd\":\"0x%llx\",\"root_owner_hwnd\":\"0x%llx\"}\n",
                    officers.window && IsWindowVisible(officers.window),officers.last_capture_ok,officers.snapshot?officers.snapshot->count:0,officers.visible_count,
                    (unsigned long long)(uintptr_t)officers.window,(unsigned long long)(uintptr_t)owner,(unsigned long long)(uintptr_t)GetForegroundWindow(),
                    (unsigned long long)(uintptr_t)GetAncestor(officers.window,GA_ROOTOWNER));write_line(log,line);
            }
            else if (officers.personality.window && IsWindowVisible(officers.personality.window) && IsDialogMessageW(officers.personality.window,&message)) { }
            else if (officers.timeline.window && IsWindowVisible(officers.timeline.window) && IsDialogMessageW(officers.timeline.window,&message)) { }
            else if (officers.window && IsWindowVisible(officers.window) && IsDialogMessageW(officers.window,&message)) { }
            else { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        drain_events(log,&toast,owner);
        s14_search_worker(&ui,&search_toast,own_module,owner,log);
        s14_battle_worker(game_folder);
        ULONGLONG tick=GetTickCount64();
        S14AffixState affix;s14_affix_snapshot(&affix);
        if(memcmp(&affix,&last_affix,sizeof(affix))){
            char line[512];snprintf(line,sizeof(line),"{\"event\":\"career_affix_state\",\"affix\":\"veteran_elite\",\"officer_id\":518,\"enemy_loss\":%llu,\"threshold\":5000,\"valid\":%d,\"active\":%d,\"suspended\":%d,\"epoch\":%u,\"day\":%d,\"bound_to_save\":%d,\"incomplete\":%d}\n",(unsigned long long)affix.enemy_loss,affix.valid,affix.active,affix.suspended,affix.epoch,affix.day,affix.bound,affix.incomplete);write_line(log,line);
            if(affix.active && last_affix.valid && !last_affix.active && !last_affix.suspended && last_affix.epoch==affix.epoch && last_affix.enabled && last_affix.enemy_loss<5000 && owner){POINT at={80,180};ClientToScreen(owner,&at);s14_toast_text(&affix_toast,own_module,owner,at,tick,L"获得词条 · 百战精锐",L"神 曹仁 · 已记录斩敌达到 5000（含伤兵）\n攻军 +10%，防御 +10%。",2000);}
            last_affix=affix;s14_manager_refresh(&ui);
        }
        s14_toast_tick(&affix_toast,tick,foreground_is(owner) && affix.active);
        s14_detail_tick(&native_detail,own_module,owner,image_base,s14_manager_view_enabled(&ui,1),tick);
        s14_army_ui_tick(&army_values,own_module,owner,image_base,s14_manager_view_enabled(&ui,2),tick);
        int map_blocked=(ui.window && IsWindowVisible(ui.window)) || (officers.window && IsWindowVisible(officers.window)) || turn_ui.visible || native_detail.shown || army_values.shown;
        s14_troop_ui_tick(&troop_ui,own_module,owner,map_blocked);
        s14_map_ui_tick(&map_effects,own_module,owner,image_base,(ui.effective&(S14_CAO_REN_BUFF|S14_AI_RANDOM_AFFIX)),ui.visual_settings,map_blocked,tick);
        int map_state=map_effects.ready|map_effects.halo_shown<<1|map_effects.card_shown<<2;
        HWND map_obstructions[4]={ui.window,officers.window,turn_ui.window,army_values.popup};
        s14_map_render_publish_all(owner,image_base,!map_blocked,ui.effective,ui.visual_settings,&map_effects.batch,map_effects.cache.dialogs,map_obstructions,tick);
        char map_line[512];if(s14_map_render_log(map_line,sizeof(map_line)))write_line(log,map_line);
        char personality_line[512];if(s14_personality_next_log(personality_line,sizeof(personality_line)))write_line(log,personality_line);
        if(last_map_state!=map_state){
            S14MapFrame *f=&map_effects.frame;char line[512];snprintf(line,sizeof(line),"{\"event\":\"cao_ren_map_effects\",\"ready\":%d,\"halo\":%d,\"tooltip\":%d,\"army_id\":%d,\"read_calls\":%u,\"read_bytes\":%u,\"portrait\":[%ld,%ld,%ld,%ld],\"card\":[%ld,%ld,%ld,%ld]}\n",map_effects.ready,map_effects.halo_shown,map_effects.card_shown,f->army_id,f->calls,f->bytes,f->portrait.left,f->portrait.top,f->portrait.right,f->portrait.bottom,f->card.left,f->card.top,f->card.right,f->card.bottom);write_line(log,line);last_map_state=map_state;
        }
        static ULONGLONG next_ai_sweep;if(tick>=next_ai_sweep){s14_ai_sweep();next_ai_sweep=tick+250;}char ai_line[768];for(int n=0;n<32 && s14_ai_next_log(ai_line,sizeof(ai_line));n++)write_line(log,ai_line);
        char troop_line[512];for(int n=0;n<16 && s14_troop_next_log(troop_line,sizeof(troop_line));n++)write_line(log,troop_line);
        char buff_line[768];for(int n=0;n<16 && s14_army_buff_next_log(buff_line,sizeof(buff_line));n++)write_line(log,buff_line);
        S14ArmyTrace stable_trace=army_values.frame.trace;stable_trace.tick=0;stable_trace.serial=0;
        if(last_army_visible!=army_values.shown || (army_values.shown && memcmp(&last_army_trace,&stable_trace,sizeof(stable_trace)))){
            char line[6144];
            if(s14_army_format_trace(&army_values.frame,army_values.shown,line,sizeof(line)))write_line(log,line);
            last_army_visible=army_values.shown;last_army_trace=stable_trace;
        }
        if(last_detail_visible!=native_detail.shown || last_detail_officer!=native_detail.frame.officer_id || last_detail_ready!=native_detail.ready || memcmp(&last_detail_stats,&native_detail.stats,sizeof(last_detail_stats))) {
            char line[512];snprintf(line,sizeof(line),"{\"event\":\"native_detail_view\",\"ready\":%d,\"enabled\":%d,\"visible\":%d,\"officer_id\":%d,\"stats_connected\":%d,\"enemy_loss\":%llu,\"units_routed\":%llu,\"units_defeated\":%llu,\"read_calls\":%u,\"read_bytes\":%u}\n",native_detail.ready,native_detail.enabled,native_detail.shown,native_detail.frame.officer_id,native_detail.connected,(unsigned long long)native_detail.stats.enemy_loss,(unsigned long long)native_detail.stats.units_routed,(unsigned long long)native_detail.stats.units_defeated,native_detail.frame.calls,native_detail.frame.bytes);
            write_line(log,line);last_detail_visible=native_detail.shown;last_detail_officer=native_detail.frame.officer_id;last_detail_ready=native_detail.ready;last_detail_stats=native_detail.stats;
        }
        if(report_epoch!=s14_turn_report_epoch()) {report_epoch=s14_turn_report_epoch();s14_report_ui_clear(&turn_ui);}
        const wchar_t *search_report,*battle_report;
        if(owner && IsWindowVisible(owner) && !IsIconic(owner) && s14_turn_report_take(tick,&search_report,&battle_report)) {
            const S14ReportSearch *search_data;const S14ReportBattle *battle_data;s14_turn_report_data(&search_data,&battle_data);
            int shown=s14_report_ui_show(&turn_ui,own_module,owner,image_base,game_folder,search_data,battle_data);
            char report_event[192];snprintf(report_event,sizeof(report_event),"{\"event\":\"turn_report_shown\",\"shown\":%d,\"tabs\":0,\"view\":\"portrait_cards\",\"actors\":%d,\"search_available\":%d,\"battle_available\":%d}\n",shown,turn_ui.battle.actor_count,turn_ui.search_data.available,turn_ui.battle.available);write_line(log,report_event);
        }
        if(ui.report_requested) {
            ui.report_requested=0;
            if(!s14_report_ui_reopen(&turn_ui)){wcscpy(ui.notice,L"当前游戏进程尚无完整回合报告；请开启探索或战斗记录后完成一个回合。");s14_manager_refresh(&ui);}
        }
        s14_report_ui_tick(&turn_ui,foreground_is(owner));
        if (tick-last_config>=250) {
            last_config=tick;
            if (!runtime_fault && InterlockedCompareExchange64(&decode_errors,0,0)) {
                runtime_fault=S14_FAULT_MAP; hooks_ready=0; InterlockedExchange(&effective_flags,0); InterlockedExchange(&mode,0);
                MH_DisableHook(MH_ALL_HOOKS); write_line(log,"{\"event\":\"rules_disabled_until_restart\",\"reason\":\"decode_error\"}\n");
            }
            unsigned int previous_flags=ui.requested; apply_configuration(&ui);
            if (previous_flags!=ui.requested) { char line[160]; snprintf(line,sizeof(line),"{\"event\":\"configuration\",\"requested_flags\":%u,\"effective_flags\":%u}\n",ui.requested,ui.effective); write_line(log,line); }
            if (!(ui.effective&S14_LIMIT_HINT) && toast.deadline) s14_toast_tick(&toast,tick,0);
        }
        int hidden=s14_toast_tick(&toast,tick,foreground_is(owner));
        if (hidden) {
            char line[160]; snprintf(line,sizeof(line),"{\"event\":\"toast_hidden\",\"reason\":%d,\"elapsed_ms\":%llu}\n",hidden,(unsigned long long)(tick-toast.shown_at));
            write_line(log,line);
        }
        if (tick-last_summary>=1000) {
            last_summary=tick;
            publish_manager_runtime(&ui);
            char line[640];
            snprintf(line,sizeof(line),"{\"event\":\"summary\",\"mode\":%ld,\"checks\":%lld,\"original_rejected\":%lld,\"would_reject\":%lld,\"enforced\":%lld,\"creations\":%lld,\"decode_errors\":%lld,\"dropped\":%lld,\"peak_active_checks\":%ld,\"preview_allowed\":%lld,\"toast_requests\":%lld}\n",
                (long)InterlockedCompareExchange(&mode,0,0),(long long)checks,(long long)original_rejected,
                (long long)would_reject,(long long)enforced,(long long)creations,(long long)decode_errors,(long long)dropped,
                (long)peak_active_checks,(long long)preview_allowed,(long long)toast_requests);
            write_line(log,line);
        }
        MsgWaitForMultipleObjects(0,NULL,FALSE,16,QS_ALLINPUT);
    }
}

static BOOL CALLBACK load_real_input(PINIT_ONCE once,PVOID parameter,PVOID *context) {
    (void)once; (void)parameter; (void)context;
    wchar_t path[MAX_PATH];
    UINT length=GetSystemDirectoryW(path,MAX_PATH);
    if (!length || length>MAX_PATH-14) return FALSE;
    wcscat(path,L"\\dinput8.dll");
    real_input=LoadLibraryW(path);
    return real_input!=NULL;
}
static FARPROC real_export(const char *name) {
    if (!InitOnceExecuteOnce(&input_once,load_real_input,NULL,NULL)) return NULL;
    return GetProcAddress(real_input,name);
}
HRESULT WINAPI DirectInput8Create(HINSTANCE instance,DWORD version,const GUID *iid,void **output,void *outer) {
    InputCreate real=(InputCreate)real_export("DirectInput8Create");
    if (!real) return E_FAIL;
    HRESULT result=real(instance,version,iid,output,outer);
    if (SUCCEEDED(result) && InterlockedCompareExchange(&worker_started,1,0)==0) {
        HANDLE thread=CreateThread(NULL,0,plugin_worker,NULL,0,NULL);
        if (thread) CloseHandle(thread);
    }
    return result;
}
HRESULT WINAPI DllCanUnloadNow(void) {
    if (InterlockedCompareExchange(&worker_started,0,0)) return S_FALSE;
    SimpleCom real=(SimpleCom)real_export("DllCanUnloadNow"); return real?real():S_FALSE;
}
HRESULT WINAPI DllGetClassObject(const GUID *clsid,const GUID *iid,void **output) {
    GetClassObject real=(GetClassObject)real_export("DllGetClassObject"); return real?real(clsid,iid,output):CLASS_E_CLASSNOTAVAILABLE;
}
HRESULT WINAPI DllRegisterServer(void) { SimpleCom real=(SimpleCom)real_export("DllRegisterServer"); return real?real():E_FAIL; }
HRESULT WINAPI DllUnregisterServer(void) { SimpleCom real=(SimpleCom)real_export("DllUnregisterServer"); return real?real():E_FAIL; }
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if (reason==DLL_PROCESS_ATTACH) { own_module=instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}

#ifdef S14_SELFTEST
static int test_original_check(void *force,void *target,void *definition,int extra) { (void)force; (void)target; (void)definition; (void)extra; return 1; }
static int test_force_id(void *force) { (void)force; return 1; }
__declspec(dllexport) int S14TestLiveSwitch(int *results) {
    unsigned char *manager=calloc(1,0x74000),*pool=calloc(MAP_CELLS,32),*buildings=calloc(6,32);
    if (!manager || !pool || !buildings) { free(manager); free(pool); free(buildings); return 0; }
    uintptr_t previous_base=image_base; unsigned char **previous_slot=manager_slot; CheckFunction previous_check=original_check;
    LONG previous_flags=InterlockedCompareExchange(&effective_flags,0,0);
    unsigned char **hexes=(unsigned char**)(manager+0xdfe0),**objects=(unsigned char**)(manager+0x6d808);
    for (int i=0;i<MAP_CELLS;i++) { hexes[i]=pool+i*32; hexes[i][0x14]=1; }
    for (int i=1;i<=5;i++) { int tile=(100-i)*220+100; memcpy(hexes[tile]+0x1a,&i,2); objects[i]=buildings+i*32; objects[i][0x12]=9; }
    void *definition=buildings; ((void**)(manager+0x736c8))[9]=definition;
    uintptr_t vtable[13]={0}; vtable[12]=(uintptr_t)test_force_id; uintptr_t *force=vtable;
    image_base=(uintptr_t)test_force_id-FORCE_ID_RVA; manager_slot=&manager; original_check=test_original_check;
    const unsigned int configurations[]={7,5,7,6};
    for (int i=0;i<4;i++) { InterlockedExchange(&effective_flags,s14_effective_flags(configurations[i])); results[i]=hooked_check(&force,hexes[100*220+100],definition,0); }
    memset(hexes[95*220+100]+0x1a,0,2); InterlockedExchange(&effective_flags,7); results[4]=hooked_check(&force,hexes[100*220+100],definition,0);
    image_base=previous_base; manager_slot=previous_slot; original_check=previous_check; InterlockedExchange(&effective_flags,previous_flags);
    free(buildings); free(pool); free(manager); return 1;
}
__declspec(dllexport) void S14TestMapCell(const unsigned char *hex,const unsigned char *building,S14Cell *output) {
    *output=cell_from_records(hex,building);
}
__declspec(dllexport) int S14TestCorrelation(int tile,int type,int owner,int actual_tile,int actual_type,int actual_owner) {
    last_validation=(LastValidation){tile,type,owner,1,GetTickCount64(),owner};
    return matches_validation(actual_tile,actual_type,actual_owner);
}
__declspec(dllexport) int S14TestPrepareImage(void *base) {
    uintptr_t previous_base=image_base,previous_end=image_end; unsigned char **previous_slot=manager_slot;
    int result=prepare_image((uintptr_t)base);
    image_base=previous_base; image_end=previous_end; manager_slot=previous_slot; return result;
}
__declspec(dllexport) int S14TestPrologue(int creation,unsigned char *output) {
    memcpy(output,creation?create_signature:common_signature,15);
    return creation?CREATE_RVA:COMMON_RVA;
}
__declspec(dllexport) int S14TestUIPrologue(int index,unsigned char *output,int *length) {
    const unsigned char *signatures[]={wrapper_signature,enter_signature,exit_signature};
    const int lengths[]={sizeof(wrapper_signature),sizeof(enter_signature),sizeof(exit_signature)};
    const int rvas[]={WRAPPER_RVA,ENTER_RVA,EXIT_RVA};
    if (index<0 || index>=3) return 0;
    *length=lengths[index]; memcpy(output,signatures[index],*length); return rvas[index];
}
__declspec(dllexport) int S14TestInteraction(uintptr_t common,uintptr_t wrapper,int construction,int original,int allowed,int current_mode,int *phase) {
    *phase=s14_check_phase(common,wrapper,construction);
    return s14_check_result(original,allowed,current_mode,*phase);
}
static uintptr_t test_hook_detour(void *a,void *b) { (void)a; (void)b; return 0; }
__declspec(dllexport) int S14TestHookBytes(const unsigned char *bytes,int length) {
    if (length<32 || length>128) return 0;
    void *memory=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!memory) return 0;
    memcpy(memory,bytes,(size_t)length); DWORD previous;
    int valid=VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&previous) && MH_Initialize()==MH_OK;
    if (valid) {
        void *trampoline=NULL;
        valid=MH_CreateHook(memory,test_hook_detour,&trampoline)==MH_OK && trampoline!=NULL;
        MH_RemoveHook(memory); MH_Uninitialize();
    }
    VirtualFree(memory,0,MEM_RELEASE); return valid;
}
__declspec(dllexport) int S14TestTLS(int marker) {
    last_validation.owner=marker;
    Sleep(0);
    return last_validation.owner;
}
__declspec(dllexport) int S14TestQueue(int count) {
    LONG previous_flags=InterlockedExchange(&effective_flags,S14_DIAGNOSTICS);
    Event event={0}; event.event=1; event.tid=GetCurrentThreadId();
    for(int i=0;i<count;i++) { event.tile=i; queue_event(event); }
    InterlockedExchange(&effective_flags,previous_flags);
    return (int)InterlockedCompareExchange64(&dropped,0,0);
}
#endif
