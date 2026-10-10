// Experimental recorder. No game-state writes, replacement outcomes, or counters
// presented as career statistics. All native arguments and scalar returns pass through.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>
#include "MinHook.h"
#include "battle_probe.h"
#include "battle_offsets.h"
#include "features.h"
#include "battle_report.h"
#include "turn_report.h"
#include <stdlib.h>
#include <bcrypt.h>
#include "battle_stats.h"
#include "special_stats.h"
#include "battle_place.h"
#include "battle_timeline.h"
#include "career_affix.h"
#include "troop_runtime.h"
#include "ai_affix.h"

#define BATTLE_QSIZE 2048
#define BATTLE_JSON_SIZE 16384
#define BATTLE_LOG_LIMIT (256ull*1024*1024)
enum { B_TROOPS=1,B_REMOVE,B_INJURY,B_STATUS,B_LOG,B_PLANNING,B_PROGRESS,B_ACTION,B_EFFECT,B_FIRE,B_ARMY_SNAPSHOT,B_CATEGORY,B_WOUND_RATE,B_ABNORMAL,B_SAVE,B_LOAD_BEGIN,B_LOAD_END,B_NEW_GAME,B_DUEL,B_CAPTURE };
typedef struct {
    uintptr_t address,vtable;
    int kind,id,leader,raw_force,raw_status,health,troops,field18,tile,active,force_id;
    wchar_t name[32];
    unsigned char raw[512];int raw_size;
} BattleObject;
typedef struct {
    LONG64 id,parent;
    ULONGLONG tick,elapsed;
    uintptr_t caller,world,native_return,args[10];
    DWORD tid;
    int kind,planning_day,force,text_truncated,post_identity_matches,player_force_id;
    unsigned char clock_raw[6];
    BattleObject target,source,other,target_after,source_after;
    wchar_t text[512];
    LONG64 action_id,effect_id;
    int entity_type,entity_id,tactic_id,effect_count,effects[10],fire_before,fire_after,hex_id,scope_matches_target;
    int parameter_count,parameters[10],effect_category,effect_slot,source_context_verified,target_from_hex_occupant;
    wchar_t tactic_name[12];
    int wound_rate_count,attrition_read,attrition_percent,abnormal_mode,abnormal_requested,abnormal_before,abnormal_after;
    unsigned int wound_rate_bits,attrition_divisor_bits;
    int save_hash_valid;unsigned char save_hash[32];unsigned int affix_epoch;
    int capture_report_matches,capture_status_matches;S14BattlePlace place;
} BattleEvent;
static int same_object(const BattleObject *a,const BattleObject *b);
typedef struct { volatile LONG state; BattleEvent e; } BattleSlot;
static BattleSlot battle_queue[BATTLE_QSIZE];
static uintptr_t battle_base,battle_end;
static unsigned char **battle_manager;
static volatile LONG battle_ready,battle_active,battle_requested,battle_fault,battle_day=-1,battle_force=-1,battle_running;
static volatile LONG64 battle_serial,battle_dropped,battle_events,battle_read_errors;
static volatile LONG battle_queue_cursor;
static _Thread_local LONG64 battle_parent;
static _Thread_local BattleEvent *battle_action,*battle_effect,*battle_damage;
static _Thread_local BattleEvent *battle_capture;
static _Thread_local int battle_category=-1,battle_category_slot=-1;
static volatile LONG64 planning_world;
static volatile LONG battle_snapshot_pending;
static HANDLE battle_file=INVALID_HANDLE_VALUE;
static ULONGLONG battle_file_bytes,battle_last_status;
static wchar_t battle_file_path[MAX_PATH];
static volatile LONG battle_loading;
static volatile LONG battle_session_epoch;
unsigned int s14_battle_session_epoch(void) { return (unsigned int)InterlockedCompareExchange(&battle_session_epoch,0,0); }
int s14_battle_is_loading(void) { return InterlockedCompareExchange(&battle_loading,0,0)!=0; }
static int special_hooks;
static int duel_semantics_ready;
static int capture_semantics_ready;
typedef uintptr_t (*DuelCall)(void*,void*,int,int);
typedef uintptr_t (*CaptureCall)(void*,void*,int,int,void*);
static DuelCall original_duel;
static CaptureCall original_capture;
typedef uintptr_t (*TroopCall)(int,int,int,int,int);
typedef uintptr_t (*RemoveCall)(void*,void*,void*,int,int);
typedef uintptr_t (*InjuryCall)(void*,int,void*,int);
typedef uintptr_t (*StatusCall)(void*,int);
static TroopCall original_troops;
static RemoveCall original_remove;
static InjuryCall original_injury;
static StatusCall original_status;
typedef uintptr_t (*ActionCall)(void*,void*,void*);
typedef uintptr_t (*FireCall)(void*,int);
static ActionCall original_action,original_effect;
static FireCall original_fire;
typedef uintptr_t (*CategoryCall)(void*,int);
static CategoryCall original_category;
// The native rate getter returns in XMM0, not RAX. Preserve its float bits.
typedef float (*WoundRateCall)(void*,int,int,void*,void*,int);
typedef uintptr_t (*AbnormalCall)(void*,void*,int,int,void*,const wchar_t*,int);
static WoundRateCall original_wound_rate;
static AbnormalCall original_abnormal;

static int battle_read(const void *source,void *out,size_t length) {
    SIZE_T got=0;
    if (!source || (uintptr_t)source+length<(uintptr_t)source) return 0;
    return ReadProcessMemory(GetCurrentProcess(),source,out,length,&got) && got==length;
}
static uintptr_t battle_world(void) {
    uintptr_t g=0; if (battle_manager) battle_read(battle_manager,&g,sizeof(g)); return g;
}
static void *battle_army_at(uintptr_t world,int id) {
    void *p=NULL; if (world && id>=0 && id<=500) battle_read((void*)(world+0x7df60+(size_t)id*8),&p,8); return p;
}
static int battle_army_id(uintptr_t world,uintptr_t pointer) {
    uintptr_t pool=(uintptr_t)battle_army_at(world,0);
    if (!pool || pointer<pool || (pointer-pool)%512 || (pointer-pool)/512>500) return -1;
    int id=(int)((pointer-pool)/512);
    return (uintptr_t)battle_army_at(world,id)==pointer?id:-1;
}
static unsigned short battle_word(const unsigned char *p) { unsigned short v; memcpy(&v,p,2); return v; }
static void battle_name(wchar_t out[32],const unsigned char *p) {
    size_t at=0;
    for (int part=0;part<2;part++) for (int i=0;i<9 && at<31;i++) {
        wchar_t c; memcpy(&c,p+(part?0x24:0x12)+i*2,2);
        if (!c || c<L' ') break; out[at++]=c;
    }
    out[at]=0;
}
static int battle_actual_force(uintptr_t world,int group) {
    unsigned char raw[17];uintptr_t p=0;
    if(!world || group<1 || group>51 || !battle_read((void*)(world+0xde40+(size_t)group*8),&p,8) ||
       !battle_read((void*)p,raw,sizeof(raw)) || *(uintptr_t*)raw!=battle_base+0x129fec8 || raw[0x10]>51) return 0;
    return raw[0x10];
}
static BattleObject battle_object(uintptr_t world,const void *pointer) {
    BattleObject o={0}; o.address=(uintptr_t)pointer;
    o.id=o.leader=o.raw_force=o.raw_status=o.health=o.troops=o.field18=o.tile=-1;
    if (!pointer || !battle_read(pointer,&o.vtable,8)) return o;
    if (o.vtable==battle_base+0x12a00d0) {
        unsigned char raw[512];
        if (!battle_read(pointer,raw,sizeof(raw))) { InterlockedIncrement64(&battle_read_errors); return o; }
        o.kind=1; o.id=battle_word(raw+0x10); o.leader=o.id;
        o.raw_force=raw[0x118]; o.raw_status=raw[0x11e]; o.health=raw[0x198]; o.active=o.id>0 && o.id<=6000;
        o.force_id=battle_actual_force(world,o.raw_force);
        battle_name(o.name,raw);
        memcpy(o.raw,raw,sizeof(raw));o.raw_size=sizeof(raw);
    } else if (o.vtable==battle_base+0x123e288) {
        unsigned char raw[512];
        if (!battle_read(pointer,raw,sizeof(raw))) { InterlockedIncrement64(&battle_read_errors); return o; }
        o.kind=2; o.id=battle_army_id(world,(uintptr_t)pointer); o.leader=battle_word(raw+0x12);
        o.raw_force=raw[0x11]; o.raw_status=raw[0x10]; o.troops=battle_word(raw+0x16);
        o.field18=battle_word(raw+0x18); o.tile=battle_word(raw+0x2a);
        o.active=o.id>0 && o.leader>0 && o.leader<=6000 && raw[0x10]!=0;
        memcpy(o.raw,raw,sizeof(raw));o.raw_size=sizeof(raw);
        void *leader=NULL;
        if (world && o.leader>0 && o.leader<=6000 && battle_read((void*)(world+0x148+(size_t)o.leader*8),&leader,8)) {
            unsigned char person[0x119];
            if (battle_read(leader,person,sizeof(person)) && *(uintptr_t*)person==battle_base+0x12a00d0) {
                battle_name(o.name,person);o.force_id=battle_actual_force(world,person[0x118]);
            }
        }
    }
    return o;
}
static void *battle_hex_at(uintptr_t world,int id) {
    void *p=NULL;if (world && id>=0 && id<48400) battle_read((void*)(world+0xdfe0+(size_t)id*8),&p,8);return p;
}
static int battle_hex_id(uintptr_t world,const void *pointer) {
    uintptr_t pool=(uintptr_t)battle_hex_at(world,0),at=(uintptr_t)pointer;
    if (!pool || at<pool || (at-pool)%32 || (at-pool)/32>=48400) return -1;
    int id=(int)((at-pool)/32);return battle_hex_at(world,id)==pointer?id:-1;
}
static int battle_fire_value(const void *hex) {
    unsigned short value;if (!hex || !battle_read((const unsigned char*)hex+0x16,&value,2)) return -1;return value;
}
static int place_read(void *context,uintptr_t at,void *out,size_t size){(void)context;return battle_read((void*)at,out,size);}
static int object_tile(uintptr_t world,const BattleObject *o){
    if(o->kind==2 && o->active)return o->tile;
    if(o->kind==1 && o->raw_size>=0x11e){int current=battle_word(o->raw+0x11c);
        if(current>=62 && current<=561){BattleObject army=battle_object(world,battle_army_at(world,current-61));if(army.kind==2 && army.active)return army.tile;}}
    return -1;
}
static void event_place(BattleEvent *e){
    int tile=object_tile(e->world,&e->target),basis=1;if(tile<0){tile=object_tile(e->world,&e->source);basis=2;}
    s14_place_capture(place_read,NULL,battle_base,e->world,tile,&e->place);e->place.basis=tile>=0?basis:0;
}
static void battle_skill(BattleEvent *e,const void *definition) {
    e->tactic_id=-1;unsigned char raw[136];
    if (!definition || !battle_read(definition,raw,sizeof(raw)) || *(uintptr_t*)raw!=battle_base+0x12a0298) return;
    memcpy(e->tactic_name,raw+0x10,10);e->tactic_name[5]=0;
    uintptr_t table[201];if (e->world && battle_read((void*)(e->world+0x76c00),table,sizeof(table)))
        for (int i=0;i<201;i++) if (table[i]==(uintptr_t)definition) { e->tactic_id=i;break; }
    uintptr_t begin=0,end=0;memcpy(&begin,raw+0x70,8);memcpy(&end,raw+0x78,8);
    // The native getter treats the 40-byte vector as ten parameters, not an
    // effect list. Actual categories are captured from its existing calls.
    if (begin && end>=begin && end-begin==40 && battle_read((void*)begin,e->parameters,40)) e->parameter_count=10;
}
static void battle_enqueue(BattleEvent *event) {
    // IDs also belong to snapshots that may be filtered after the native call.
    // They cannot choose queue slots: gaps can collide while most slots are free.
    unsigned int start=(unsigned int)InterlockedIncrement(&battle_queue_cursor);
    for (unsigned int offset=0;offset<BATTLE_QSIZE;offset++) {
        unsigned int index=(start+offset)&(BATTLE_QSIZE-1);
        if (InterlockedCompareExchange(&battle_queue[index].state,1,0)!=0) continue;
        battle_queue[index].e=*event; InterlockedExchange(&battle_queue[index].state,2); return;
    }
    InterlockedIncrement64(&battle_dropped); InterlockedExchange(&battle_fault,1); InterlockedExchange(&battle_active,0);
}
static BattleEvent battle_begin(int kind,uintptr_t caller) {
    BattleEvent e={0}; e.id=InterlockedIncrement64(&battle_serial); e.parent=battle_parent;
    e.kind=kind; e.caller=caller; e.tid=GetCurrentThreadId(); e.tick=GetTickCount64();e.affix_epoch=s14_affix_epoch();
    e.world=battle_world(); e.planning_day=InterlockedCompareExchange(&battle_day,0,0);
    e.tactic_id=e.fire_before=e.fire_after=e.hex_id=-1;
    e.effect_category=e.effect_slot=-1;e.abnormal_mode=e.abnormal_before=e.abnormal_after=-1;
    if (battle_action) e.action_id=battle_action->id;
    if (battle_effect) {
        e.effect_id=battle_effect->id;e.tactic_id=battle_effect->tactic_id;
        memcpy(e.tactic_name,battle_effect->tactic_name,sizeof(e.tactic_name));
        e.effect_category=battle_category;e.effect_slot=battle_category_slot;
        e.source_context_verified=battle_action && same_object(&battle_effect->source,&battle_action->source);
    }
    e.force=InterlockedCompareExchange(&battle_force,0,0);
    // The strategy hook supplies settings +0x3A: an actual force ID. Only
    // unit/person raw ownership fields need group -> force conversion.
    e.player_force_id=e.force>=1 && e.force<=51?e.force:0;
    uintptr_t settings=0;
    if (e.world && battle_read((void*)(e.world+0x85130),&settings,8) && settings) battle_read((void*)(settings+0x34),e.clock_raw,6);
    return e;
}
static int same_object(const BattleObject *a,const BattleObject *b) {
    return a->kind && a->kind==b->kind && a->id==b->id && a->address==b->address && a->vtable==b->vtable && a->leader==b->leader;
}
static float hooked_battle_wound_rate(void *person,int morale,int type,void *context,void *source,int valid) {
    if(!s14_battle_enabled() || !battle_damage) return original_wound_rate(person,morale,type,context,source,valid);
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_WOUND_RATE,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)person;e.args[1]=(uintptr_t)(intptr_t)morale;e.args[2]=(uintptr_t)(intptr_t)type;
    e.args[3]=(uintptr_t)context;e.args[4]=(uintptr_t)source;e.args[5]=(uintptr_t)(intptr_t)valid;
    e.target=battle_damage->target;e.source=battle_damage->source;
    SetLastError(error);float result=original_wound_rate(person,morale,type,context,source,valid);error=GetLastError();
    memcpy(&e.wound_rate_bits,&result,4);e.wound_rate_count=1;
    // Read parameters at the existing getter return, before native arithmetic.
    // Optional reads are bounded by the module; no native helpers are called.
    if(battle_end>=battle_base && battle_end-battle_base>=0x18ebb90 &&
       battle_read((void*)(battle_base+0x18ebb8c),&e.attrition_percent,4) &&
       battle_read((void*)(battle_base+0x123ea5c),&e.attrition_divisor_bits,4)) e.attrition_read=1;
    battle_damage->wound_rate_count++;
    if(battle_damage->wound_rate_count==1) {
        battle_damage->wound_rate_bits=e.wound_rate_bits;battle_damage->attrition_read=e.attrition_read;
        battle_damage->attrition_percent=e.attrition_percent;battle_damage->attrition_divisor_bits=e.attrition_divisor_bits;
    }
    e.elapsed=GetTickCount64()-e.tick;battle_enqueue(&e);SetLastError(error);return result;
}
static uintptr_t hooked_battle_abnormal(void *target,void *source,int mode,int duration,void *hex,const wchar_t *name,int option) {
    DWORD before=GetLastError();int refusal=s14_troop_confusion_reject(target,mode,duration);SetLastError(before);
    if(refusal)return (uintptr_t)refusal;
    if(!s14_battle_enabled() || !battle_effect) return original_abnormal(target,source,mode,duration,hex,name,option);
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_ABNORMAL,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)target;e.args[1]=(uintptr_t)source;e.args[2]=(uintptr_t)(intptr_t)mode;
    e.args[3]=(uintptr_t)(intptr_t)duration;e.args[4]=(uintptr_t)hex;e.args[5]=(uintptr_t)name;e.args[6]=(uintptr_t)(intptr_t)option;
    e.target=battle_object(e.world,target);e.source=battle_object(e.world,source);
    e.abnormal_mode=mode;e.abnormal_requested=duration;
    e.source_context_verified=battle_action && same_object(&e.source,&battle_action->source) && same_object(&e.source,&battle_effect->source);
    if(e.target.kind==2 && mode>=0 && mode<3 && e.target.raw_size==512) e.abnormal_before=e.target.raw[0x23+mode];
    SetLastError(error);uintptr_t result=original_abnormal(target,source,mode,duration,hex,name,option);error=GetLastError();
    e.native_return=result;e.target_after=battle_object(e.world,target);e.source_after=battle_object(e.world,source);
    e.post_identity_matches=same_object(&e.target,&e.target_after);
    if(e.post_identity_matches && e.target_after.kind==2 && mode>=0 && mode<3 && e.target_after.raw_size==512) e.abnormal_after=e.target_after.raw[0x23+mode];
    e.elapsed=GetTickCount64()-e.tick;battle_enqueue(&e);SetLastError(error);return result;
}
static uintptr_t hooked_battle_category(void *definition,int slot) {
    if(!s14_battle_enabled() || !battle_effect || battle_effect->args[3]!=(uintptr_t)definition)
        return original_category(definition,slot);
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_CATEGORY,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)definition;e.args[1]=(uintptr_t)(intptr_t)slot;
    SetLastError(error);uintptr_t result=original_category(definition,slot);error=GetLastError();
    e.native_return=result;e.effect_category=(int)(result&255);e.effect_slot=slot;
    if(slot>=0 && slot<2) {
        battle_category=e.effect_category;battle_category_slot=slot;
        battle_effect->effects[slot]=e.effect_category;
        if(battle_effect->effect_count<=slot) battle_effect->effect_count=slot+1;
    }
    e.source=battle_effect->source;e.target=battle_effect->target;
    battle_enqueue(&e);SetLastError(error);return result;
}
int s14_battle_enabled(void) { return InterlockedCompareExchange(&battle_active,0,0)!=0 && !InterlockedCompareExchange(&battle_loading,0,0); }
static uintptr_t hooked_battle_action(void *manager,void *source_entry,void *target_entry) {
    if (!s14_battle_enabled()) return original_action(manager,source_entry,target_entry);
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_ACTION,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)manager;e.args[1]=(uintptr_t)source_entry;e.args[2]=(uintptr_t)target_entry;
    unsigned char raw[16];void *source=NULL,*target=NULL;
    if (source_entry && battle_read(source_entry,raw,sizeof(raw))) {
        e.entity_id=battle_word(raw);memcpy(&e.entity_type,raw+4,4);
        if (e.entity_type==27) source=battle_army_at(e.world,e.entity_id);
    }
    if (target_entry && battle_read(target_entry,raw,sizeof(raw))) {
        int target_type=0;memcpy(&target_type,raw+4,4);
        if(target_type==27) target=battle_army_at(e.world,battle_word(raw));
    }
    e.source=battle_object(e.world,source);e.target=battle_object(e.world,target);LONG64 previous=battle_parent;BattleEvent *old=battle_action;
    e.action_id=e.id;battle_parent=e.id;battle_action=&e;SetLastError(error);
    uintptr_t result=original_action(manager,source_entry,target_entry);error=GetLastError();battle_action=old;battle_parent=previous;
    e.native_return=result;e.source_after=battle_object(e.world,source);e.target_after=battle_object(e.world,target);
    e.post_identity_matches=same_object(&e.target,&e.target_after);e.elapsed=GetTickCount64()-e.tick;
    battle_enqueue(&e);SetLastError(error);return result;
}
static uintptr_t hooked_battle_effect(void *manager,void *entry,void *values) {
    if (!s14_battle_enabled()) return original_effect(manager,entry,values);
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_EFFECT,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)manager;e.args[1]=(uintptr_t)entry;e.args[2]=(uintptr_t)values;
    void *source=NULL,*target=NULL,*hex=NULL,*definition=NULL;unsigned char raw[16];
    if (manager) battle_read((unsigned char*)manager+0x98,&source,8);
    if (entry && battle_read(entry,raw,sizeof(raw))) {
        memcpy(&e.entity_type,raw,4);e.entity_id=battle_word(raw+4);memcpy(&definition,raw+8,8);
        if (e.entity_type==27) target=battle_army_at(e.world,e.entity_id);
    }
    e.source=battle_object(e.world,source);e.target=battle_object(e.world,target);battle_skill(&e,definition);
    e.args[3]=(uintptr_t)definition;for(int i=0;i<10;i++) e.effects[i]=-1;
    if (e.target.kind==2 && e.target.active) { e.hex_id=e.target.tile;hex=battle_hex_at(e.world,e.hex_id);e.fire_before=battle_fire_value(hex); }
    LONG64 previous=battle_parent;BattleEvent *old=battle_effect;
    int old_category=battle_category,old_slot=battle_category_slot;
    battle_parent=e.id;battle_effect=&e;battle_category=battle_category_slot=-1;SetLastError(error);
    uintptr_t result=original_effect(manager,entry,values);error=GetLastError();battle_effect=old;battle_parent=previous;
    battle_category=old_category;battle_category_slot=old_slot;
    e.native_return=result;e.target_after=battle_object(e.world,target);e.source_after=battle_object(e.world,source);
    e.post_identity_matches=same_object(&e.target,&e.target_after);e.fire_after=battle_fire_value(hex);e.elapsed=GetTickCount64()-e.tick;
    battle_enqueue(&e);SetLastError(error);return result;
}
static uintptr_t hooked_battle_fire(void *hex,int value) {
    if (!s14_battle_enabled()) return original_fire(hex,value);
    DWORD error=GetLastError();
    // Most global calls only decrement an existing fire timer, or write zero
    // to a non-burning tile. Bypass identity/clock snapshots for those calls.
    if(!battle_effect) {
        int before=battle_fire_value(hex);
        if(before>=0 && ((before==0 && value<=0) || (value>0 && value<=before))) {
            SetLastError(error);return original_fire(hex,value);
        }
    }
    BattleEvent e=battle_begin(B_FIRE,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)hex;e.args[1]=(uintptr_t)(intptr_t)value;e.hex_id=battle_hex_id(e.world,hex);
    if (e.hex_id>=0) {
        e.fire_before=battle_fire_value(hex);short occupant=-1;
        if (battle_read((unsigned char*)hex+0x1c,&occupant,2)) e.target=battle_object(e.world,battle_army_at(e.world,occupant));
        e.target_from_hex_occupant=e.target.kind==2 && e.target.active && e.target.tile==e.hex_id;
        if (battle_effect) {
            e.source=battle_effect->source;
            e.scope_matches_target=same_object(&e.target,&battle_effect->target) && e.target.tile==e.hex_id;
        }
    }
    SetLastError(error);uintptr_t result=original_fire(hex,value);error=GetLastError();e.native_return=result;
    if (e.hex_id>=0) e.fire_after=battle_fire_value(hex);
    e.target_after=battle_object(e.world,(void*)e.target.address);
    e.post_identity_matches=same_object(&e.target,&e.target_after);
    /* Outside a skill, preserve only start/clear/extension transitions, not
       per-tick countdown noise. A zero/negative request is still forwarded. */
    if (e.hex_id>=0 && (battle_effect || e.fire_after>e.fire_before || (e.fire_before>0 && e.fire_after==0))) {
        e.elapsed=GetTickCount64()-e.tick;battle_enqueue(&e);
    }
    SetLastError(error);return result;
}
static uintptr_t hooked_battle_troops(int type,int id,int amount,int source_type,int source_id) {
    DWORD troop_error=GetLastError();int native_amount=amount;
    /* Positive amount removes soldiers at this verified direct combat call.
       Transfers, healing, fire and unknown callers retain native behavior. */
    if(type==27 && source_type==27 && amount>0 && id!=source_id &&
       (uintptr_t)__builtin_return_address(0)==battle_base+0x166861)
        amount=s14_troop_damage(battle_army_at(battle_world(),id),amount);
    SetLastError(troop_error);
    if (!s14_battle_enabled()) return original_troops(type,id,amount,source_type,source_id);
    DWORD error=GetLastError(); BattleEvent e=battle_begin(B_TROOPS,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)(intptr_t)type; e.args[1]=(uintptr_t)(intptr_t)id; e.args[2]=(uintptr_t)(intptr_t)amount;
    e.args[3]=(uintptr_t)(intptr_t)source_type; e.args[4]=(uintptr_t)(intptr_t)source_id;
    e.args[5]=(uintptr_t)(intptr_t)native_amount;
    void *target=type==27?battle_army_at(e.world,id):NULL,*source=source_type==27?battle_army_at(e.world,source_id):NULL;
    e.target=battle_object(e.world,target); e.source=battle_object(e.world,source);
    void *hex=NULL;
    if (e.target.kind==2 && e.target.active) {
        e.hex_id=e.target.tile;hex=battle_hex_at(e.world,e.hex_id);e.fire_before=battle_fire_value(hex);
    }
    LONG64 previous=battle_parent;BattleEvent *old_damage=battle_damage;battle_damage=&e;battle_parent=e.id; SetLastError(error);
    uintptr_t result=original_troops(type,id,amount,source_type,source_id); error=GetLastError(); battle_parent=previous;battle_damage=old_damage;
    e.native_return=result; e.target_after=battle_object(e.world,target); e.source_after=battle_object(e.world,source);
    e.fire_after=battle_fire_value(hex);
    e.post_identity_matches=same_object(&e.target,&e.target_after); e.elapsed=GetTickCount64()-e.tick;
    battle_enqueue(&e); SetLastError(error); return result;
}
static uintptr_t hooked_battle_remove(void *target,void *source,void *other,int reason,int option) {
    if (!s14_battle_enabled()){uintptr_t result=original_remove(target,source,other,reason,option);DWORD error=GetLastError();s14_troop_forget(target);s14_ai_forget(target);SetLastError(error);return result;}
    DWORD error=GetLastError(); BattleEvent e=battle_begin(B_REMOVE,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)target; e.args[1]=(uintptr_t)source; e.args[2]=(uintptr_t)other;
    e.args[3]=(uintptr_t)(intptr_t)reason; e.args[4]=(uintptr_t)(intptr_t)option;
    e.target=battle_object(e.world,target); e.source=battle_object(e.world,source); e.other=battle_object(e.world,other);
    event_place(&e);
    LONG64 previous=battle_parent; battle_parent=e.id; SetLastError(error);
    uintptr_t result=original_remove(target,source,other,reason,option); error=GetLastError(); battle_parent=previous;
    s14_troop_forget(target);s14_ai_forget(target);
    e.native_return=result; e.target_after=battle_object(e.world,target); e.source_after=battle_object(e.world,source);
    e.post_identity_matches=same_object(&e.target,&e.target_after); e.elapsed=GetTickCount64()-e.tick;
    battle_enqueue(&e); SetLastError(error); return result;
}
static uintptr_t hooked_battle_injury(void *target,int mode,void *source,int option) {
    if (!s14_battle_enabled()) return original_injury(target,mode,source,option);
    DWORD error=GetLastError(); BattleEvent e=battle_begin(B_INJURY,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)target; e.args[1]=(uintptr_t)(intptr_t)mode; e.args[2]=(uintptr_t)source; e.args[3]=(uintptr_t)(intptr_t)option;
    e.target=battle_object(e.world,target); e.source=battle_object(e.world,source);
    event_place(&e);
    LONG64 previous=battle_parent; battle_parent=e.id; SetLastError(error);
    uintptr_t result=original_injury(target,mode,source,option); error=GetLastError(); battle_parent=previous;
    e.native_return=result; e.target_after=battle_object(e.world,target); e.post_identity_matches=same_object(&e.target,&e.target_after);
    e.elapsed=GetTickCount64()-e.tick; battle_enqueue(&e); SetLastError(error); return result;
}
static uintptr_t hooked_battle_status(void *person,int status) {
    // The generic setter also runs for thousands of non-combat state changes.
    // Keep only changes nested inside an observed damage/removal/injury call.
    if (!s14_battle_enabled() || !battle_parent) return original_status(person,status);
    DWORD error=GetLastError(); BattleEvent e=battle_begin(B_STATUS,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)person; e.args[1]=(uintptr_t)(intptr_t)status; e.target=battle_object(e.world,person);
    SetLastError(error); uintptr_t result=original_status(person,status); error=GetLastError();
    e.native_return=result; e.target_after=battle_object(e.world,person); e.post_identity_matches=same_object(&e.target,&e.target_after);
    if(battle_capture && e.parent==battle_capture->id && e.target.address==battle_capture->target.address && e.post_identity_matches && e.target.raw_status!=6 && e.target_after.raw_status==6)battle_capture->capture_status_matches=1;
    if (e.target.raw_status!=e.target_after.raw_status) battle_enqueue(&e);
    SetLastError(error); return result;
}
static uintptr_t hooked_duel_settlement(void *first,void *second,int consequence,int report_context) {
    if(!s14_battle_enabled())return original_duel(first,second,consequence,report_context);
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_DUEL,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)first;e.args[1]=(uintptr_t)second;e.args[2]=(uintptr_t)(intptr_t)consequence;e.args[3]=(uintptr_t)(intptr_t)report_context;
    e.source=battle_object(e.world,first);e.target=battle_object(e.world,second);
    event_place(&e);
    LONG64 previous=battle_parent;battle_parent=e.id;SetLastError(error);
    uintptr_t value=original_duel(first,second,consequence,report_context);error=GetLastError();battle_parent=previous;
    e.native_return=value;e.source_after=battle_object(e.world,first);e.target_after=battle_object(e.world,second);
    e.post_identity_matches=same_object(&e.source,&e.source_after)&&same_object(&e.target,&e.target_after);
    e.elapsed=GetTickCount64()-e.tick;battle_enqueue(&e);SetLastError(error);return value;
}
static uintptr_t hooked_capture_settlement(void *person,void *army,int flag,int option,void *force) {
    if(!s14_battle_enabled())return original_capture(person,army,flag,option,force);
    DWORD error=GetLastError();BattleEvent e=battle_begin(B_CAPTURE,(uintptr_t)__builtin_return_address(0)-battle_base);
    e.args[0]=(uintptr_t)person;e.args[1]=(uintptr_t)army;e.args[2]=(uintptr_t)(intptr_t)flag;e.args[3]=(uintptr_t)(intptr_t)option;e.args[4]=(uintptr_t)force;
    e.target=battle_object(e.world,person);e.source=battle_object(e.world,army);e.other=battle_object(e.world,force);
    event_place(&e);
    BattleEvent *previous_capture=battle_capture;battle_capture=&e;
    LONG64 previous=battle_parent;battle_parent=e.id;SetLastError(error);
    uintptr_t value=original_capture(person,army,flag,option,force);error=GetLastError();battle_parent=previous;battle_capture=previous_capture;
    e.native_return=value;e.target_after=battle_object(e.world,person);e.source_after=battle_object(e.world,army);
    e.post_identity_matches=same_object(&e.target,&e.target_after);e.elapsed=GetTickCount64()-e.tick;
    battle_enqueue(&e);SetLastError(error);return value;
}
static void battle_text(wchar_t out[512],const wchar_t *text,int *truncated) {
    if (!text) return;
    for (int at=0;at<511;) {
        wchar_t block[32]; int n=511-at<32?511-at:32;
        if (!battle_read(text+at,block,(size_t)n*2)) { n=1; if (!battle_read(text+at,block,2)) { *truncated=1; break; } }
        for (int i=0;i<n;i++) { wchar_t c=block[i]; if (!c) return; out[at++]=c; }
    }
    out[511]=0; *truncated=1;
}
void s14_battle_log(uintptr_t caller,const uintptr_t args[10],const wchar_t *short_text,const wchar_t *long_text) {
    if (!s14_battle_enabled() || (!battle_parent && (caller<0x160000 || caller>=0x390000))) return;
    DWORD error=GetLastError(); BattleEvent e=battle_begin(B_LOG,caller);
    memcpy(e.args,args,sizeof(e.args)); battle_text(e.text,long_text?long_text:short_text,&e.text_truncated);
    if(battle_capture && e.parent==battle_capture->id && caller==0x2aebe8 && !e.text_truncated){
        wchar_t clean[512];int at=0;for(int i=0;e.text[i] && at<511;i++){if(e.text[i]==L'^' && e.text[i+1]>=L'0' && e.text[i+1]<=L'9' && e.text[i+2]>=L'0' && e.text[i+2]<=L'9'){i+=2;continue;}clean[at++]=e.text[i];}clean[at]=0;
        const wchar_t *victim=battle_capture->target.name,*captor=battle_capture->source.name;
        const wchar_t *v=victim[0]?wcsstr(clean,victim):NULL,*by=v?wcsstr(v,L"被"):NULL,*a=by && captor[0]?wcsstr(by,captor):NULL;
        if(a && wcsstr(a,L"俘虏"))battle_capture->capture_report_matches=1;
    }
    battle_enqueue(&e); SetLastError(error);
}
void s14_battle_planning(uintptr_t world,int day,int force) {
    if (!s14_battle_enabled()) return;
    if ((uintptr_t)InterlockedCompareExchange64(&planning_world,0,0)==world && InterlockedCompareExchange(&battle_day,0,0)==day && InterlockedCompareExchange(&battle_force,0,0)==force) return;
    InterlockedExchange64(&planning_world,(LONG64)world); InterlockedExchange(&battle_day,day); InterlockedExchange(&battle_force,force);
    BattleEvent e=battle_begin(B_PLANNING,0); e.args[0]=(uintptr_t)InterlockedExchange(&battle_running,0); battle_enqueue(&e);
    InterlockedExchange(&battle_snapshot_pending,1);
}
void s14_battle_progress(void) {
    if (!s14_battle_enabled()) return;
    BattleEvent e=battle_begin(B_PROGRESS,0); InterlockedExchange(&battle_running,1); battle_enqueue(&e);
}
#include "battle_save.inc"
int s14_battle_install(uintptr_t base,uintptr_t end,unsigned char **manager) {
    battle_base=base; battle_end=end; battle_manager=manager;
    void *detours[]={hooked_battle_troops,hooked_battle_remove,hooked_battle_injury,hooked_battle_status,hooked_battle_action,hooked_battle_effect,hooked_battle_fire,hooked_battle_category,hooked_battle_wound_rate,hooked_battle_abnormal,hooked_battle_save_commit,hooked_battle_load,hooked_battle_load_reader,hooked_battle_new_game,hooked_duel_settlement,hooked_capture_settlement};
    void **originals[]={(void**)&original_troops,(void**)&original_remove,(void**)&original_injury,(void**)&original_status,(void**)&original_action,(void**)&original_effect,(void**)&original_fire,(void**)&original_category,(void**)&original_wound_rate,(void**)&original_abnormal,(void**)&original_save_commit,(void**)&original_load,(void**)&original_load_reader,(void**)&original_new_game,(void**)&original_duel,(void**)&original_capture};
    special_hooks=0;duel_semantics_ready=capture_semantics_ready=0;
    for (int i=0;i<BATTLE_BASE_ENTRY_COUNT;i++) {
        const S14BattleEntry *entry=&battle_entries[i]; unsigned char bytes[16];
        if (end<base || entry->rva>end-base || sizeof(bytes)>end-base-entry->rva ||
            !battle_read((void*)(base+entry->rva),bytes,sizeof(bytes)) || memcmp(bytes,entry->bytes,sizeof(bytes))) return 0;
    }
    for (int i=0;i<BATTLE_BASE_ENTRY_COUNT;i++) if (MH_CreateHook((void*)(base+battle_entries[i].rva),detours[i],originals[i])!=MH_OK) {
        for (int j=0;j<i;j++) MH_RemoveHook((void*)(base+battle_entries[j].rva)); return 0;
    }
    // Optional probes cannot disable existing battle counters on another layout.
    for(int i=BATTLE_BASE_ENTRY_COUNT;i<BATTLE_ENTRY_COUNT;i++){
        const S14BattleEntry *entry=&battle_entries[i];unsigned char bytes[16];
        if(end>=base && entry->rva<=end-base && 16<=end-base-entry->rva && battle_read((void*)(base+entry->rva),bytes,16) && !memcmp(bytes,entry->bytes,16) &&
           MH_CreateHook((void*)(base+entry->rva),detours[i],originals[i])==MH_OK)special_hooks|=1<<(i-BATTLE_BASE_ENTRY_COUNT);
    }
    if(special_hooks&1){
        duel_semantics_ready=1;
        for(int i=0;i<3;i++){const S14DuelAnchor *a=&duel_anchors[i];unsigned char bytes[23];
            if(end<base || a->rva>end-base || a->size>end-base-a->rva || !battle_read((void*)(base+a->rva),bytes,a->size) || memcmp(bytes,a->bytes,a->size))duel_semantics_ready=0;
        }
    }
    if(special_hooks&2){capture_semantics_ready=1;for(int i=0;i<3;i++){const S14CaptureAnchor *a=&capture_anchors[i];unsigned char bytes[32];
        if(end<base || a->rva>end-base || a->size>end-base-a->rva || !battle_read((void*)(base+a->rva),bytes,a->size) || memcmp(bytes,a->bytes,a->size))capture_semantics_ready=0;}}
    InterlockedExchange(&battle_ready,1); return 1;
}
void s14_battle_configure(unsigned int flags,int enabled) {
    InterlockedExchange(&battle_requested,enabled!=0);
    int active=enabled && (flags&S14_MASTER) && InterlockedCompareExchange(&battle_ready,0,0) && !InterlockedCompareExchange(&battle_fault,0,0);
    LONG before=InterlockedExchange(&battle_active,active);
    if(before && !active) {s14_battle_round_capture_gap();s14_stats_gap();s14_special_gap();s14_timeline_gap();}
    if(!before && active) InterlockedExchange64(&planning_world,0);
}
#ifdef S14_SELFTEST
__declspec(dllexport) int S14TestBattlePrologue(int index,unsigned char *out) {
    if(index<0 || index>=BATTLE_ENTRY_COUNT || !out) return 0;
    memcpy(out,battle_entries[index].bytes,16);return (int)battle_entries[index].rva;
}
#endif
static void json_text(char *out,size_t size,const wchar_t *text) {
    char raw[2048]={0}; size_t at=0;
    if (!WideCharToMultiByte(CP_UTF8,0,text,-1,raw,sizeof(raw),NULL,NULL)) { if(size) out[0]=0; return; }
    for (size_t i=0;raw[i] && at+7<size;i++) {
        unsigned char c=(unsigned char)raw[i];
        if (c<L' ') { int n=snprintf(out+at,size-at,"\\u%04x",c); at+=(size_t)n; }
        else { if (c=='"' || c=='\\') out[at++]='\\'; out[at++]=(char)c; }
    }
    out[at]=0;
}
static int append(char **cursor,size_t *left,const char *format,...) {
    va_list args; va_start(args,format); int n=vsnprintf(*cursor,*left,format,args); va_end(args);
    if (n<0 || (size_t)n>=*left) return 0; *cursor+=n; *left-=(size_t)n; return 1;
}
static int json_object(char **p,size_t *left,const char *key,const BattleObject *o) {
    char name[256]; json_text(name,sizeof(name),o->name);
    if (!append(p,left,"\"%s\":{\"address\":\"0x%llx\",\"vtable_rva\":\"0x%llx\",\"kind\":%d,\"id\":%d,\"leader_id\":%d,\"name\":\"%s\",\"raw_force\":%d,\"raw_status\":%d,\"health_raw\":%d,\"troops\":%d,\"field18_raw\":%d,\"tile\":%d,\"active\":%d,\"raw_size\":%d,\"raw_hex\":\"",
        key,(unsigned long long)o->address,(unsigned long long)(o->vtable>=battle_base?o->vtable-battle_base:o->vtable),o->kind,o->id,o->leader,name,o->raw_force,o->raw_status,o->health,o->troops,o->field18,o->tile,o->active,o->raw_size)) return 0;
    for(int i=0;i<o->raw_size;i++) if(!append(p,left,"%02x",o->raw[i])) return 0;
    return append(p,left,"\"}");
}
static int ordinary_duel_verified(const BattleEvent *e){
    return duel_semantics_ready && e->kind==B_DUEL && (e->caller==0x331f03 || e->caller==0x331f3e) && (int)e->args[2]==0 &&
        ((int)e->args[3]==0 || (int)e->args[3]==1) && e->post_identity_matches &&
        e->source.kind==1 && e->target.kind==1 && e->source.active && e->target.active &&
        e->source.id>0 && e->source.id!=e->target.id && e->source.raw_status>=1 && e->source.raw_status<=4 && e->target.raw_status>=1 && e->target.raw_status<=4 &&
        e->source.force_id>0 && e->target.force_id>0 && e->source.force_id!=e->target.force_id;
}
__declspec(dllexport) int S14TestDuelAnchor(int index,unsigned char *out,int *length){
    if(index<0 || index>=3 || !out || !length)return 0;
    memcpy(out,duel_anchors[index].bytes,duel_anchors[index].size);*length=duel_anchors[index].size;return (int)duel_anchors[index].rva;
}
static int capture_verified(const BattleEvent *e){
    return capture_semantics_ready && e->kind==B_CAPTURE && e->caller==0x236391 && e->args[2]==1 && e->args[3]==1 && e->native_return==1 &&
        e->capture_report_matches && e->capture_status_matches && e->post_identity_matches && e->target.kind==1 && e->target.active && e->target.raw_status>=2 && e->target.raw_status<=5 && e->target_after.raw_status==6 &&
        e->source.kind==2 && e->source.active && same_object(&e->source,&e->source_after) && e->source.force_id>0 && e->target.force_id>0 && e->source.force_id!=e->target.force_id;
}
__declspec(dllexport) int S14TestCaptureAnchor(int index,unsigned char *out,int *size){if(index<0 || index>=3 || !out || !size)return 0;const S14CaptureAnchor *a=&capture_anchors[index];memcpy(out,a->bytes,a->size);*size=a->size;return (int)a->rva;}
static int battle_json(const BattleEvent *e,char out[BATTLE_JSON_SIZE]) {
    static const char *kinds[]={"unknown","troop_change","unit_remove","officer_injury","person_status","native_log","planning","progress","skill_action_attempt","skill_effect_dispatch","hex_fire_transition","planning_army_snapshot","skill_effect_category","wounded_generation_ratio","army_abnormal_application","save_completed","load_begin","load_completed","new_campaign","duel_settlement_candidate","capture_settlement_candidate"};
    if (e->kind<1 || e->kind>B_CAPTURE) return 0;
    char text[4096]; json_text(text,sizeof(text),e->text); char *p=out; size_t left=BATTLE_JSON_SIZE;
    if (!append(&p,&left,"{\"event\":\"battle_observation\",\"kind\":\"%s\",\"id\":%lld,\"parent_id\":%lld,\"thread\":%lu,\"tick_ms\":%llu,\"elapsed_ms\":%llu,\"caller_rva\":\"0x%llx\",\"world\":\"0x%llx\",\"planning_day\":%d,\"player_force\":%d,\"clock_raw\":\"%02x%02x%02x%02x%02x%02x\",\"native_return\":\"0x%llx\",\"post_identity_matches\":%d,\"args\":[",
        kinds[e->kind],(long long)e->id,(long long)e->parent,(unsigned long)e->tid,(unsigned long long)e->tick,(unsigned long long)e->elapsed,
        (unsigned long long)e->caller,(unsigned long long)e->world,e->planning_day,e->force,e->clock_raw[0],e->clock_raw[1],e->clock_raw[2],e->clock_raw[3],e->clock_raw[4],e->clock_raw[5],(unsigned long long)e->native_return,e->post_identity_matches)) return 0;
    for (int i=0;i<10;i++) if (!append(&p,&left,"%s\"0x%llx\"",i?",":"",(unsigned long long)e->args[i])) return 0;
    if (!append(&p,&left,"],")) return 0;
    char city[256],area[256];json_text(city,sizeof(city),e->place.city);json_text(area,sizeof(area),e->place.area);
    if(!append(&p,&left,"\"capture_report_matches\":%d,\"capture_status_matches\":%d,\"capture_verified\":%s,\"battle_place\":{\"tile\":%d,\"city_id\":%d,\"area_id\":%d,\"basis\":%d,\"city\":\"%s\",\"area\":\"%s\"},",e->capture_report_matches,e->capture_status_matches,capture_verified(e)?"true":"false",e->place.basis?e->place.tile:-1,e->place.city_id,e->place.area_id,e->place.basis,city,area))return 0;
    if(!append(&p,&left,"\"save_hash_valid\":%d,\"save_content_sha256\":\"",e->save_hash_valid)) return 0;
    for(int i=0;i<32;i++) if(!append(&p,&left,"%02x",e->save_hash[i])) return 0;
    if(!append(&p,&left,"\",")) return 0;
    if(!append(&p,&left,"\"player_force_id\":%d,\"target_force_id\":%d,\"source_force_id\":%d,\"target_after_force_id\":%d,\"source_after_force_id\":%d,",e->player_force_id,e->target.force_id,e->source.force_id,e->target_after.force_id,e->source_after.force_id)) return 0;
    const BattleObject *objects[]={&e->target,&e->source,&e->other,&e->target_after,&e->source_after};
    const char *keys[]={"target","source","other","target_after","source_after"};
    for (int i=0;i<5;i++) if (!json_object(&p,&left,keys[i],objects[i]) || !append(&p,&left,",")) return 0;
    if(!append(&p,&left,"\"target_role\":\"%s\",",e->kind==B_EFFECT || e->kind==B_CATEGORY?"dispatch_subject":"affected_entity")) return 0;
    char tactic[128];json_text(tactic,sizeof(tactic),e->tactic_name);
    if(!append(&p,&left,"\"schema_version\":7,\"action_id\":%lld,\"effect_id\":%lld,\"entity_type\":%d,\"entity_id\":%d,\"tactic_id\":%d,\"tactic_name\":\"%s\",\"hex_id\":%d,\"fire_before\":%d,\"fire_after\":%d,\"scope_matches_target\":%d,\"effect_categories\":[",
        (long long)e->action_id,(long long)e->effect_id,e->entity_type,e->entity_id,e->tactic_id,tactic,e->hex_id,e->fire_before,e->fire_after,e->scope_matches_target)) return 0;
    for(int i=0;i<e->effect_count;i++) if(!append(&p,&left,"%s%d",i?",":"",e->effects[i])) return 0;
    if(!append(&p,&left,"],\"effect_category\":%d,\"effect_slot\":%d,\"source_context_verified\":%d,\"target_from_hex_occupant\":%d,\"tactic_parameters\":[",e->effect_category,e->effect_slot,e->source_context_verified,e->target_from_hex_occupant)) return 0;
    for(int i=0;i<e->parameter_count;i++) if(!append(&p,&left,"%s%d",i?",":"",e->parameters[i])) return 0;
    if(!append(&p,&left,"],")) return 0;
    if(!append(&p,&left,"\"wound_rate_count\":%d,\"wound_rate_float_bits\":\"0x%08x\",\"attrition_parameters_read\":%d,\"attrition_percent_raw\":%d,\"attrition_divisor_float_bits\":\"0x%08x\",\"abnormal_mode_raw\":%d,\"abnormal_requested_duration\":%d,\"abnormal_before\":%d,\"abnormal_after\":%d,",e->wound_rate_count,e->wound_rate_bits,e->attrition_read,e->attrition_percent,e->attrition_divisor_bits,e->abnormal_mode,e->abnormal_requested,e->abnormal_before,e->abnormal_after)) return 0;
    if (!append(&p,&left,"\"text\":\"%s\",\"text_truncated\":%d,\"semantics_verified\":%s,\"ordinary_duel_normalized_outcome\":%d}\n",text,e->text_truncated,
        ordinary_duel_verified(e) || capture_verified(e)?"true":"false",ordinary_duel_verified(e)?0:-1)) return 0;
    return (int)(p-out);
}
static int battle_write(const char *line,size_t length) {
    DWORD wrote=0;
    if (battle_file==INVALID_HANDLE_VALUE || battle_file_bytes+length>BATTLE_LOG_LIMIT ||
        !WriteFile(battle_file,line,(DWORD)length,&wrote,NULL) || wrote!=length) return 0;
    battle_file_bytes+=length; return 1;
}
static void battle_capture_planning(void) {
    if (!s14_battle_enabled() || !InterlockedExchange(&battle_snapshot_pending,0)) return;
    if (InterlockedCompareExchange(&battle_running,0,0)) return;
    BattleEvent phase=battle_begin(B_ARMY_SNAPSHOT,0);
    if (!phase.world || phase.world!=(uintptr_t)InterlockedCompareExchange64(&planning_world,0,0)) return;
    BattleObject *objects=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,501*sizeof(BattleObject));
    if(!objects) { InterlockedIncrement64(&battle_read_errors);return; }
    for(int i=0;i<=500;i++) objects[i]=battle_object(phase.world,battle_army_at(phase.world,i));
    BattleEvent check=battle_begin(B_ARMY_SNAPSHOT,0);
    if(check.world==phase.world && !memcmp(check.clock_raw,phase.clock_raw,6) && !InterlockedCompareExchange(&battle_running,0,0)) {
        for(int i=0;i<=500;i++) if(objects[i].kind==2 && objects[i].active) {
            BattleEvent e=phase;e.id=InterlockedIncrement64(&battle_serial);e.target=objects[i];
            char line[BATTLE_JSON_SIZE];int n=battle_json(&e,line);
            if(n && battle_write(line,(size_t)n)) InterlockedIncrement64(&battle_events);
            else { InterlockedExchange(&battle_fault,2);InterlockedExchange(&battle_active,0);InterlockedIncrement64(&battle_dropped);break; }
        }
    }
    HeapFree(GetProcessHeap(),0,objects);
}
static S14RoundObject round_object(const BattleObject *o) {
    S14RoundObject v={.kind=o->kind,.id=o->id,.leader=o->leader,.force=o->force_id,.troops=o->troops,.wounded=o->field18,.health=o->health,.active=o->active};
    memcpy(v.name,o->name,sizeof(v.name));return v;
}
static void publish_affix(const BattleEvent *e,int resume){
    S14BattleTotals row={0};int incomplete=0,bound=0,valid=s14_stats_row(e->world,S14_ELITE_OFFICER,&row,&incomplete,&bound);
    s14_affix_publish(e->world,row.enemy_loss,valid,bound,incomplete,e->planning_day,e->affix_epoch,resume);
}
static void battle_report_event(const BattleEvent *e) {
    if(e->kind==B_LOAD_BEGIN) {s14_stats_load_begin();return;}
    if(e->kind==B_LOAD_END || e->kind==B_NEW_GAME) {
        if((int)e->native_return==1) {s14_battle_round_reset();s14_turn_report_reset();}
        s14_stats_load_end(e->world,e->planning_day,(int)e->native_return==1,e->save_hash,e->save_hash_valid,e->kind==B_NEW_GAME);
        s14_special_load(e->world,e->planning_day,(int)e->native_return==1,e->save_hash,e->save_hash_valid,e->kind==B_NEW_GAME);
        s14_timeline_load(e->world,e->planning_day,(int)e->native_return==1,e->save_hash,e->save_hash_valid,e->kind==B_NEW_GAME);
        if((int)e->native_return==1){S14SpecialSnapshot *s=malloc(sizeof(*s));if(s && s14_special_snapshot(e->world,s))s14_timeline_import_special(s);free(s);}publish_affix(e,1);return;
    }
    if(e->kind==B_SAVE) {if(e->native_return==0) {
        if(!s14_stats_save(e->world,e->save_hash,e->save_hash_valid)) s14_stats_gap();
        if(!s14_special_save(e->world,e->save_hash,e->save_hash_valid)) s14_special_gap();
        if(!s14_timeline_save(e->world,e->save_hash,e->save_hash_valid)) s14_timeline_gap();
    }return;}
    if(e->kind==B_PROGRESS || e->kind==B_PLANNING){s14_special_begin(e->world,e->planning_day);s14_timeline_begin(e->world,e->planning_day);}
    if(e->kind==B_DUEL || e->kind==B_CAPTURE) {
        if(!e->post_identity_matches || e->target.kind!=1 || e->planning_day<0)return;
        int capture=e->kind==B_CAPTURE;
        if(capture && (e->target.raw_status==6 || e->target_after.raw_status!=6 || (int)e->native_return!=1))return;
        if(!capture && e->source.kind!=1)return;
        S14SpecialEvent candidate={.id=(uint64_t)e->id,.kind=capture?S14_SPECIAL_CAPTURE:S14_SPECIAL_DUEL,
            .day=e->planning_day,.actor=(!capture || (e->source.kind==2 && e->source.active && same_object(&e->source,&e->source_after)))?e->source.leader:0,.target=e->target.id,
            .actor_force=e->source.force_id,.target_force=e->target.force_id,
            .outcome=capture?1:ordinary_duel_verified(e)?0:(int)e->args[3],
            .actor_role=capture && !capture_verified(e)?S14_SPECIAL_ACTOR_COMMANDER:S14_SPECIAL_ACTOR_PERSON,
            .verified=capture?capture_verified(e):ordinary_duel_verified(e)};
        // The caller selects the winner before dispatch: first argument is winner,
        // second loser on both player-position branches. Integer arguments encode
        // consequences/report context, not winner selection. Verified against
        // Cao Ren (first, 518) defeating Zhang Ni (second, 613), and the losing-side injury.
        // Persist normalized outcome 0=actor wins; other paths stay candidates.
        memcpy(candidate.actor_name,e->source.name,sizeof(candidate.actor_name));
        memcpy(candidate.target_name,e->target.name,sizeof(candidate.target_name));
        memcpy(candidate.clock,e->clock_raw,6);if(!candidate.actor)candidate.actor_name[0]=0;
        s14_special_consume(e->world,&candidate);s14_timeline_special(e->world,&candidate,&e->place);s14_battle_round_special_at(e->world,&candidate,&e->place);return;
    }
    int kind=e->kind==B_PROGRESS?S14_ROUND_BEGIN:e->kind==B_PLANNING?S14_ROUND_END:
        e->kind==B_TROOPS?S14_ROUND_DAMAGE:e->kind==B_EFFECT?S14_ROUND_SKILL:e->kind==B_FIRE?S14_ROUND_FIRE:
        e->kind==B_ABNORMAL?S14_ROUND_ABNORMAL:e->kind==B_REMOVE?S14_ROUND_REMOVE:e->kind==B_INJURY?S14_ROUND_INJURY:0;
    if(!kind) return;
    // Transitions without a healthy-troop loss, and unsuccessful injuries, do
    // not manufacture participating officers or combat counters.
    if(kind==S14_ROUND_DAMAGE && (e->target.kind!=2 || e->target.troops<=e->target_after.troops)) return;
    S14RoundEvent event={.id=(uint64_t)e->id,.action_id=(uint64_t)e->action_id,.world=e->world,.kind=kind,.day=e->planning_day,
        .player_force=e->player_force_id,.stable=e->post_identity_matches,.source_stable=same_object(&e->source,&e->source_after),.reason=(int)e->args[3],.mode=e->abnormal_mode,
        .before=kind==S14_ROUND_FIRE?e->fire_before:e->abnormal_before,.after=kind==S14_ROUND_FIRE?e->fire_after:e->abnormal_after,
        .source_verified=e->source_context_verified,.fault=battle_fault!=0 || battle_dropped!=0 || battle_read_errors!=0,
        .source=round_object(&e->source),.target=round_object(&e->target),.source_after=round_object(&e->source_after),.target_after=round_object(&e->target_after)};
    event.place=e->place;memcpy(event.tactic,e->tactic_name,sizeof(event.tactic));memcpy(event.clock,e->clock_raw,6);s14_stats_consume(&event);s14_battle_round_consume(&event);
    if(kind==S14_ROUND_END)publish_affix(e,0);
}
static int compare_battle_slots(const void *a,const void *b) {
    LONG64 x=(*(const BattleSlot*const*)a)->e.id,y=(*(const BattleSlot*const*)b)->e.id;return (x>y)-(x<y);
}
void s14_battle_worker(const wchar_t *root) {
    s14_stats_root(root);s14_special_root(root);s14_timeline_root(root);if(root && wcslen(root)<MAX_PATH) wcscpy(battle_save_root,root);
    if(battle_fault || battle_dropped || battle_read_errors) {s14_stats_gap();s14_special_gap();s14_timeline_gap();}
    if (!InterlockedCompareExchange(&battle_requested,0,0) && battle_file==INVALID_HANDLE_VALUE) {
        BattleSlot *meta[BATTLE_QSIZE];int count=0;
        for(int i=0;i<BATTLE_QSIZE;i++) if(InterlockedCompareExchange(&battle_queue[i].state,3,2)==2) meta[count++]=&battle_queue[i];
        qsort(meta,(size_t)count,sizeof(meta[0]),compare_battle_slots);
        for(int i=0;i<count;i++) {if(meta[i]->e.kind>=B_SAVE) battle_report_event(&meta[i]->e);InterlockedExchange(&meta[i]->state,0);}
        s14_stats_gap();s14_special_gap();s14_timeline_gap();return;
    }
    if (battle_file==INVALID_HANDLE_VALUE && !InterlockedCompareExchange(&battle_fault,0,0)) {
        SYSTEMTIME now; GetLocalTime(&now);
        if (swprintf(battle_file_path,MAX_PATH,L"%ls\\SAN14ModManager\\logs\\battle-%04d%02d%02d-%02d%02d%02d-%lu.jsonl",root,
            now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,(unsigned long)GetCurrentProcessId())<0) { InterlockedExchange(&battle_fault,2); return; }
        battle_file=CreateFileW(battle_file_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if (battle_file==INVALID_HANDLE_VALUE) { InterlockedExchange(&battle_fault,2); InterlockedExchange(&battle_active,0); return; }
        char version[32]={0};WideCharToMultiByte(CP_UTF8,0,S14_MANAGER_VERSION,-1,version,sizeof(version),NULL,NULL);
        char header[384]; int n=snprintf(header,sizeof(header),"{\"event\":\"battle_probe_startup\",\"schema_version\":7,\"version\":\"%s\",\"pid\":%lu,\"hooks_ready\":%ld,\"gameplay_modified_by_probe\":false,\"career_statistics\":true,\"log_limit_bytes\":%llu}\n",version,(unsigned long)GetCurrentProcessId(),(long)InterlockedCompareExchange(&battle_ready,0,0),(unsigned long long)BATTLE_LOG_LIMIT);
        if (!battle_write(header,(size_t)n)) { InterlockedExchange(&battle_fault,2); InterlockedExchange(&battle_active,0); }
    }
    battle_capture_planning();
    BattleSlot *pending[BATTLE_QSIZE];int count=0;
    for(int i=0;i<BATTLE_QSIZE;i++) if(InterlockedCompareExchange(&battle_queue[i].state,3,2)==2) pending[count++]=&battle_queue[i];
    qsort(pending,(size_t)count,sizeof(pending[0]),compare_battle_slots);
    for(int i=0;i<count;i++) {
        battle_report_event(&pending[i]->e);
        char line[BATTLE_JSON_SIZE]; int n=battle_json(&pending[i]->e,line);
        if (n && battle_write(line,(size_t)n)) InterlockedIncrement64(&battle_events);
        else { InterlockedExchange(&battle_fault,2); InterlockedExchange(&battle_active,0); InterlockedIncrement64(&battle_dropped); }
        InterlockedExchange(&pending[i]->state,0);
    }
    ULONGLONG now=GetTickCount64();
    if (now-battle_last_status>=1000) {
        battle_last_status=now; char line[384]; int n=snprintf(line,sizeof(line),"{\"event\":\"battle_probe_health\",\"enabled\":%ld,\"hooks_ready\":%ld,\"fault\":%ld,\"written_events\":%lld,\"dropped\":%lld,\"read_errors\":%lld,\"log_bytes\":%llu}\n",
            (long)InterlockedCompareExchange(&battle_active,0,0),(long)InterlockedCompareExchange(&battle_ready,0,0),(long)InterlockedCompareExchange(&battle_fault,0,0),
            (long long)InterlockedCompareExchange64(&battle_events,0,0),(long long)InterlockedCompareExchange64(&battle_dropped,0,0),(long long)InterlockedCompareExchange64(&battle_read_errors,0,0),(unsigned long long)battle_file_bytes);
        if (battle_file!=INVALID_HANDLE_VALUE && !battle_write(line,(size_t)n)) { InterlockedExchange(&battle_fault,2); InterlockedExchange(&battle_active,0); }
        if (battle_file!=INVALID_HANDLE_VALUE && !FlushFileBuffers(battle_file)) { InterlockedExchange(&battle_fault,2); InterlockedExchange(&battle_active,0); }
        wchar_t path[MAX_PATH],values[8][40];
        if (swprintf(path,MAX_PATH,L"%ls\\SAN14ModManager\\battle-observation-status.ini",root)>0) {
            const wchar_t *keys[]={L"Enabled",L"HooksReady",L"Fault",L"WrittenEvents",L"Dropped",L"ReadErrors",L"LogBytes"};
            LONG64 vals[]={battle_active,battle_ready,battle_fault,battle_events,battle_dropped,battle_read_errors,(LONG64)battle_file_bytes};
            for (int i=0;i<7;i++) { swprintf(values[i],40,L"%lld",(long long)vals[i]); WritePrivateProfileStringW(L"BattleObservation",keys[i],values[i],path); }
            WritePrivateProfileStringW(L"BattleObservation",L"LogPath",battle_file_path,path);
            WritePrivateProfileStringW(L"BattleObservation",L"Version",S14_MANAGER_VERSION,path);
            wchar_t special_text[16];swprintf(special_text,16,L"%d",special_hooks);
            WritePrivateProfileStringW(L"BattleObservation",L"SpecialHooks",special_text,path);
            WritePrivateProfileStringW(L"BattleObservation",L"SpecialSemanticsVerified",L"0",path);
            WritePrivateProfileStringW(L"BattleObservation",L"OrdinaryDuelVerified",duel_semantics_ready?L"1":L"0",path);
            WritePrivateProfileStringW(L"BattleObservation",L"CaptureReportVerified",capture_semantics_ready?L"1":L"0",path);
            wchar_t pid_text[32];swprintf(pid_text,32,L"%lu",(unsigned long)GetCurrentProcessId());
            WritePrivateProfileStringW(L"BattleObservation",L"Pid",pid_text,path);
            WritePrivateProfileStringW(L"BattleObservation",L"Reason",NULL,path);
        }
    }
}
