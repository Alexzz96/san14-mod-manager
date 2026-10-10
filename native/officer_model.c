#include "officer_model.h"
#include "pinyin.h"
#include "career_affix.h"
#include "ai_affix.h"
#include <string.h>
#include <stdio.h>
#include <wctype.h>
#include <stdlib.h>

/* These offsets were traced from the native officer detail view; no game
   function is called. Read failures/scene changes discard the candidate. */
#define WORLD_SLOT 0x1fc91d0
#define PERSON_VT 0x12a00d0
#define CITY_VT 0x129fd10
#define GATE_VT 0x12a0138
#define FORCE_VT 0x129fe58
#define ARMY_VT 0x123e288
typedef struct { S14OfficerRead read;void *context;S14OfficerSnapshot *out;unsigned int calls; } Reader;
static int get(Reader *r,uintptr_t address,void *out,size_t size) {
    if (!address || !size || size>65536 || ++r->calls>20000 || r->out->bytes_read+size>8*1024*1024) { r->out->read_errors++;return 0; }
    if (!r->read(r->context,address,out,size)) { r->out->read_errors++;return 0; }
    r->out->bytes_read+=size;return 1;
}
static unsigned short u16(const unsigned char *p) { unsigned short n;memcpy(&n,p,2);return n; }
static uintptr_t uptr(const unsigned char *p) { uintptr_t n;memcpy(&n,p,sizeof(n));return n; }
static unsigned int u32(const unsigned char *p) { unsigned int n;memcpy(&n,p,4);return n; }
static int zero_bytes(const unsigned char *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return 0;return 1;}
/* Allocated but uninitialized person records have ID zero and empty identity,
   gameplay and effect fields. Padding/default counters need not be all zero.
   Never accept a named/mismatched live officer as an empty record. */
static int empty_person(const unsigned char raw[512]){
    /* Some reserved/custom officer templates initialize the five abilities
       to 1 rather than 0. Both patterns were observed in this game's table. */
    for(int i=0;i<5;i++)if(raw[0x124+i]>1)return 0;
    return !u16(raw+0x10) && zero_bytes(raw+0x12,54) &&
        zero_bytes(raw+0x118,7) && !raw[0x198] &&
        zero_bytes(raw+0x150,18) && zero_bytes(raw+0x168,10);
}
/* Native display callbacks 0x310ef0 / 0x312470 map these signed
   inner values to five grades. Preserve unusual mod values as unknown grades.
   Loyalty callback 0x3177c0 uses the full byte, including mod values over 100. */
int s14_officer_inner_grade(int raw) {
    if (raw<-20 || raw>20 || raw%10) return -1;
    return raw/10+3;
}
void s14_officer_decode_character(const unsigned char raw[512],S14Officer *out) {
    out->ambition_raw=(int)(int16_t)u16(raw+0x1a2);
    out->bond_raw=(int)(int16_t)u16(raw+0x1a4);
    out->ambition=s14_officer_inner_grade(out->ambition_raw);
    out->bond=s14_officer_inner_grade(out->bond_raw);
    out->loyalty_raw=raw[0x120];
    /* Native person detail excludes rulers / unaffiliated people. */
    out->loyalty=out->force && out->status>=2 && out->status<=4?out->loyalty_raw:-1;
}
static void text(wchar_t *out,size_t capacity,const unsigned char *raw,size_t characters) {
    size_t i=0;for (;i<characters && i+1<capacity;i++) { wchar_t c=(wchar_t)u16(raw+i*2);out[i]=c;if (!c) break; }
    out[i<capacity?i:capacity-1]=0;
}
static void append(wchar_t *out,size_t capacity,const wchar_t *part) {
    if (!part[0]) return;size_t n=wcslen(out);if (n && n+2<capacity) { out[n++]=L'、';out[n]=0; }
    wcsncat(out,part,capacity-n-1);
}
static int strings(Reader *r,uintptr_t base,wchar_t status[10][24],wchar_t health[4][24]) {
    uintptr_t data=0;unsigned char header[24];
    if (!get(r,base+0x1fc9190,&data,8) || !get(r,data,header,sizeof(header))) return 0;
    unsigned int stride=u16(header+10),offset=u32(header+16);
    if (header[12]!=1 || !stride || stride>128 || !offset || offset>32*1024*1024) return 0;
    for (int i=0;i<10;i++) {
        unsigned int id=0xf8+i,relative=0;
        unsigned char raw[48];
        if (!get(r,data+offset+stride*id,&relative,4) || relative>32*1024*1024 ||
            !get(r,data+offset+relative,raw,sizeof(raw))) return 0;
        text(status[i],24,raw,24);
    }
    wcscpy(health[0],L"健康");
    for (int i=1;i<4;i++) swprintf(health[i],24,L"伤病状态 %d",i);
    return status[1][0]!=0;
}
static int definitions(Reader *r,uintptr_t base,uintptr_t world,int personality) {
    uintptr_t pointers[S14_PERSONALITY_MAX];int count=personality?S14_PERSONALITY_MAX:S14_TACTICS_MAX;
    if (!get(r,world+(personality?0x7d440:0x76c00),pointers,(size_t)count*8)) return 0;
    for (int i=0;i<count;i++) {
        unsigned char raw[224];size_t size=personality?224:136;
        if (!pointers[i]) continue;
        if (!get(r,pointers[i],raw,size)) return 0;
        if (uptr(raw)!=base+(personality?0x12a0658:0x12a0298)) return 0;
        S14OfficerDefinition *d=personality?&r->out->personalities[i]:&r->out->tactics[i];
        text(d->name,12,raw+0x10,personality?6:5);
        text(d->description,64,raw+(personality?0x3c:0x1a),personality?51:19);
    }
    return personality?r->out->personalities[1].name[0]!=0:r->out->tactics[1].name[0]!=0;
}
static void foothold(wchar_t *out,size_t capacity,unsigned int value,wchar_t cities[52][40],wchar_t gates[11][40]) {
    if (value>=1 && value<=51 && cities[value][0]) wcsncpy(out,cities[value],capacity-1);
    else if (value>=52 && value<=61 && gates[value-51][0]) wcsncpy(out,gates[value-51],capacity-1);
    else swprintf(out,capacity,L"—");
    out[capacity-1]=0;
}
int s14_officers_capture(S14OfficerRead read,void *context,uintptr_t base,S14OfficerSnapshot *out) {
    if (!read || !out || !base) return 0;
    memset(out,0,sizeof(*out));out->schema_version=2;
    Reader r={read,context,out,0};uintptr_t world=0,settings=0,again=0;
    unsigned char clock[7],clock_after[7];
    if (!get(&r,base+WORLD_SLOT,&world,8) || !world || !get(&r,world+0x85130,&settings,8) ||
        !get(&r,settings+0x34,clock,7)) return 0;
    if (!clock[0] || clock[6]>51) return 0;
    out->world=world;out->settings=settings;out->player_force=clock[6];memcpy(out->clock,clock,6);
    wchar_t statuses[10][24]={{0}},health[4][24]={{0}},cities[52][40]={{0}},gates[11][40]={{0}};
    unsigned short force_leader[52]={0};
    unsigned char force_map[52]={0};uintptr_t force_pointers[52];
    if (!get(&r,world+0xde40,force_pointers,sizeof(force_pointers))) return 0;
    for (int i=0;i<52;i++) {
        unsigned char raw[17];if (!force_pointers[i]) continue;
        if (!get(&r,force_pointers[i],raw,sizeof(raw)) || uptr(raw)!=base+0x129fec8) return 0;
        force_map[i]=raw[0x10]<=51?raw[0x10]:0;
    }
    out->player_force=force_map[out->player_force];
    if (!strings(&r,base,statuses,health) || !definitions(&r,base,world,1) || !definitions(&r,base,world,0)) return 0;
    for (int group=0;group<3;group++) {
        int count=group==1?11:52;uintptr_t pointers[52];
        if (!get(&r,world+(group==0?0xdaa8:group==1?0xdc48:0xdca0),pointers,(size_t)count*8)) return 0;
        for (int i=1;i<count;i++) {
            unsigned char raw[128];if (!pointers[i]) continue;
            if (!get(&r,pointers[i],raw,sizeof(raw))) return 0;
            if (uptr(raw)!=base+(group==0?CITY_VT:group==1?GATE_VT:FORCE_VT)) return 0;
            if (group==2) force_leader[i]=u16(raw+0x10);
            else text(group==0?cities[i]:gates[i],40,raw+0x12,16);
        }
    }
    uintptr_t people[S14_OFFICER_MAX];
    if (!get(&r,world+0x148,people,sizeof(people))) return 0;
    int by_id[S14_OFFICER_MAX];for (int i=0;i<S14_OFFICER_MAX;i++) by_id[i]=-1;
    for (int id=1;id<S14_OFFICER_MAX;id++) {
        unsigned char raw[512],second[512];if (!people[id]) continue;
        if (!get(&r,people[id],raw,sizeof(raw))) break;
        if (uptr(raw)!=base+PERSON_VT) continue;
        if (!get(&r,people[id],second,sizeof(second))) break;
        if (memcmp(raw,second,sizeof(raw))) { out->unstable++;continue; }
        if(empty_person(raw))continue;
        if (u16(raw+0x10)!=id || raw[0x118]>51 || raw[0x11e]>9 || raw[0x198]>3) { out->read_errors++;break; }
        S14Officer *p=&out->rows[out->count];p->id=id;p->raw_force=raw[0x118];p->force=force_map[p->raw_force];p->status=raw[0x11e];p->health=raw[0x198];
        text(p->name,24,raw+0x12,9);wchar_t family[12]={0};text(family,12,raw+0x24,9);wcsncat(p->name,family,23-wcslen(p->name));
        if (!p->name[0]) continue;
        wchar_t titled[24];s14_affix_display(world,id,p->name,titled,24);if(titled[0])wcscpy(p->name,titled);if(s14_ai_person_name(world,id,p->name,titled,24))wcscpy(p->name,titled);
        text(p->courtesy,12,raw+0x36,9);
        wcscpy(p->status_name,statuses[p->status]);wcscpy(p->health_name,health[p->health]);
        s14_officer_decode_character(raw,p);
        p->home=u16(raw+0x11a);p->current=u16(raw+0x11c);p->army=-1;p->tile=-1;p->troops=-1;
        foothold(p->home_name,40,p->home,cities,gates);
        for (int j=0;j<5;j++) { p->ability[j]=raw[0x124+j];p->total+=p->ability[j]; }
        for (int j=0;j<9;j++) {
            p->personalities[j]=u16(raw+0x150+j*2);int index=p->personalities[j];
            if (index>=S14_PERSONALITY_MAX) { out->read_errors++;break; }
            if (index) append(p->personality_text,240,out->personalities[index].name);
        }
        for (int j=0;j<10;j++) {
            p->tactics[j]=raw[0x168+j];int index=p->tactics[j];
            if (index>=S14_TACTICS_MAX) { out->read_errors++;break; }
            if (index) append(p->tactics_text,240,out->tactics[index].name);
        }
        by_id[id]=out->count++;
    }
    if (!out->count || out->read_errors || out->unstable) return 0;
    uintptr_t armies[501];unsigned char army_raw[501][44];unsigned char valid_army[501]={0};
    if (!get(&r,world+0x7df60,armies,sizeof(armies))) return 0;
    for (int id=1;id<=500;id++) if (armies[id]) {
        if (!get(&r,armies[id],army_raw[id],44)) return 0;
        valid_army[id]=uptr(army_raw[id])==base+ARMY_VT && army_raw[id][0x10] && u16(army_raw[id]+0x12)>0;
    }
    for (int i=0;i<out->count;i++) {
        S14Officer *p=&out->rows[i];int leader=force_leader[p->force];
        if (leader>0 && leader<S14_OFFICER_MAX && by_id[leader]>=0) wcscpy(p->force_name,out->rows[by_id[leader]].name);
        else if (p->force) swprintf(p->force_name,32,L"势力 #%d",p->force);
        else wcscpy(p->force_name,L"无所属");
        if (p->current>=62 && p->current<=561) {
            p->army=p->current-61;p->place_kind=2;
            if (valid_army[p->army]) {
                p->troops=u16(army_raw[p->army]+0x16);p->tile=u16(army_raw[p->army]+0x2a);
                swprintf(p->location,96,L"出征 · (%d, %d)",p->tile%220,p->tile/220);
            } else wcscpy(p->location,L"部队状态更新中");
        } else if (p->current>=1 && p->current<=61) {
            p->place_kind=1;foothold(p->location,96,p->current,cities,gates);
        } else if (p->current>=562 && p->current<=1061) {
            p->place_kind=3;swprintf(p->location,96,L"执行任务 · 归属 %ls",p->home_name);
        } else { p->place_kind=0;wcscpy(p->location,L"位置未定"); }
    }
    uintptr_t *people_after=malloc(S14_OFFICER_MAX*8);
    if (!people_after) return 0;
    /* Validate every captured identity pointer again so a concurrent load
       cannot combine two campaigns. Clock alone does not identify a load. */
    int ok=get(&r,world+0x148,people_after,S14_OFFICER_MAX*8) && !memcmp(people,people_after,sizeof(people));
    if (ok) for (int i=0;i<out->count;i++) {
        unsigned char raw[18];int id=out->rows[i].id;
        if (!get(&r,people_after[id],raw,18) || uptr(raw)!=base+PERSON_VT || u16(raw+16)!=id) { ok=0;break; }
    }
    free(people_after);
    if (!ok || !get(&r,base+WORLD_SLOT,&again,8) || again!=world ||
        !get(&r,world+0x85130,&again,8) || again!=settings || !get(&r,settings+0x34,clock_after,7) || memcmp(clock,clock_after,7)) return 0;
    return !out->read_errors;
}
int s14_officer_matches(const S14Officer *p,int player_force,const S14OfficerFilter *f) {
    if (!f->include_history && (p->status==0 || p->status==7 || p->status==9)) return 0;
    if (f->own_force && (!player_force || p->force!=player_force)) return 0;
    if (f->place && p->place_kind!=f->place) return 0;
    const wchar_t *q=f->query;
    while (*q) {
        while (iswspace(*q)) q++;const wchar_t *end=q;while (*end && !iswspace(*end)) end++;
        size_t n=(size_t)(end-q);if (!n) break;
        if (!s14_search_contains(p->name,q,n) && !s14_search_contains(p->courtesy,q,n) && !s14_search_contains(p->personality_text,q,n)) return 0;
        q=end;
    }
    return 1;
}
static int number(uint64_t a,uint64_t b) { return (a>b)-(a<b); }
unsigned int s14_officer_special_flag(int key){return key<=S14_SORT_DUEL_LOSSES?S14_SPECIAL_DUEL:S14_SPECIAL_CAPTURE;}
uint64_t s14_officer_special_value(const S14Officer *p,int key){
    switch(key){case S14_SORT_DUELS:return p->special.duels;case S14_SORT_DUEL_WINS:return p->special.wins;
    case S14_SORT_DUEL_LOSSES:return p->special.losses;case S14_SORT_CAPTURES:return p->special.captures;
    case S14_SORT_CAPTURED:return p->special.captured;case S14_SORT_UNIQUE_CAPTIVES:return p->special.unique_captives;default:return 0;}
}
int s14_officer_count_key(int key){return key==S14_SORT_KILLS || key==S14_SORT_ROUTS || (key>=S14_SORT_ENEMY_LOSS && key<=S14_SORT_UNIQUE_CAPTIVES);}
uint64_t s14_officer_count_value(const S14Officer *p,int key){
    if(key>=S14_SORT_DUELS && key<=S14_SORT_UNIQUE_CAPTIVES)return p->special.valid_mask&s14_officer_special_flag(key)?s14_officer_special_value(p,key):0;
    if(key==S14_SORT_KILLS)return p->career.valid_mask&S14_CAREER_KILLS?p->career.officer_kills:0;
    if(key==S14_SORT_ROUTS){if(p->battle.valid_mask&S14_STATS_ROUTS)return p->battle.units_routed;return p->career.valid_mask&S14_CAREER_ROUTS?p->career.units_routed:0;}
    unsigned int flag=key==S14_SORT_ENEMY_LOSS?S14_STATS_DAMAGE:key==S14_SORT_DEFEATS?S14_STATS_DEFEATS:key==S14_SORT_OWN_LOSS?S14_STATS_LOSSES:key==S14_SORT_INJURIES?S14_STATS_INJURIES:0;
    if(!(p->battle.valid_mask&flag))return 0;
    return key==S14_SORT_ENEMY_LOSS?p->battle.enemy_loss:key==S14_SORT_DEFEATS?p->battle.units_defeated:key==S14_SORT_OWN_LOSS?p->battle.own_loss:p->battle.officers_injured;
}
void s14_officer_kda_text(const S14Officer *p,wchar_t *out,size_t size){
    uint64_t kills=s14_officer_count_value(p,S14_SORT_ENEMY_LOSS),loss=s14_officer_count_value(p,S14_SORT_OWN_LOSS);
    if(!loss)swprintf(out,size,kills?L"∞":L"0.00");else swprintf(out,size,L"%.2f",(double)kills/(double)loss);
}
static int compare_kda(const S14Officer *a,const S14Officer *b){
    uint64_t x=s14_officer_count_value(a,S14_SORT_ENEMY_LOSS),y=s14_officer_count_value(b,S14_SORT_ENEMY_LOSS);
    uint64_t dx=s14_officer_count_value(a,S14_SORT_OWN_LOSS),dy=s14_officer_count_value(b,S14_SORT_OWN_LOSS);
    int ix=x && !dx,iy=y && !dy;if(ix || iy)return ix-iy;if(!dx)dx=1;if(!dy)dy=1;
    // Cross products fit 128 bits even for a complete uint64 counter.
    unsigned __int128 left=(unsigned __int128)x*dy,right=(unsigned __int128)y*dx;return (left>right)-(left<right);
}
int s14_officer_compare(const S14Officer *a,const S14Officer *b,int key,int descending) {
    int diff=0;
    if(key==S14_SORT_KDA)diff=compare_kda(a,b);
    else if(s14_officer_count_key(key))diff=number(s14_officer_count_value(a,key),s14_officer_count_value(b,key));
    else if (key>=S14_SORT_LEADERSHIP && key<=S14_SORT_CHARM) diff=number(a->ability[key-S14_SORT_LEADERSHIP],b->ability[key-S14_SORT_LEADERSHIP]);
    else if (key==S14_SORT_TOTAL) diff=number(a->total,b->total);
    else if (key==S14_SORT_NAME) diff=wcscmp(a->name,b->name);
    else if (key==S14_SORT_FORCE) diff=wcscmp(a->force_name,b->force_name);
    else if (key==S14_SORT_STATUS) diff=a->status-b->status;
    else if (key==S14_SORT_LOCATION) diff=wcscmp(a->location,b->location);
    else if (key==S14_SORT_PERSONALITY) diff=wcscmp(a->personality_text,b->personality_text);
    else if (key==S14_SORT_TROOPS) {
        if ((a->troops<0)!=(b->troops<0)) return a->troops<0?1:-1;
        diff=number(a->troops,b->troops);
    } else if (key==S14_SORT_AMBITION || key==S14_SORT_BOND || key==S14_SORT_LOYALTY) {
        int x=key==S14_SORT_AMBITION?a->ambition:key==S14_SORT_BOND?a->bond:a->loyalty;
        int y=key==S14_SORT_AMBITION?b->ambition:key==S14_SORT_BOND?b->bond:b->loyalty;
        if ((x<0)!=(y<0)) return x<0?1:-1;
        diff=(x>y)-(x<y);
    }
    if (diff) return descending?-(diff>0?1:-1):(diff>0?1:-1);
    return (a->id>b->id)-(a->id<b->id);
}
void s14_officers_attach_career(S14OfficerSnapshot *snapshot,const S14OfficerCareerProvider *provider) {
    for (int i=0;i<snapshot->count;i++) {
        memset(&snapshot->rows[i].career,0,sizeof(S14OfficerCareer));
        if (!provider || provider->version!=1 || !provider->get || !provider->campaign_id || !provider->branch_id) continue;
        S14OfficerCareer value={0};
        if (provider->get(provider->context,provider->campaign_id,provider->branch_id,snapshot->rows[i].id,&value))
            snapshot->rows[i].career=value;
    }
}
void s14_officers_attach_battle(S14OfficerSnapshot *s,const S14BattleStatsSnapshot *stats) {
    s->battle_connected=s->battle_incomplete=s->battle_bound=0;s->battle_start_day=-1;
    for(int i=0;i<s->count;i++) memset(&s->rows[i].battle,0,sizeof(s->rows[i].battle));
    if(!stats || stats->version!=1 || stats->world!=s->world) return;
    s->battle_connected=1;s->battle_incomplete=stats->incomplete;s->battle_bound=stats->bound_to_save;s->battle_start_day=stats->start_day;
    for(int i=0;i<s->count;i++) {int id=s->rows[i].id;if(id>0 && id<S14_STATS_MAX) s->rows[i].battle=stats->rows[id];}
}
