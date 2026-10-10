#include "troop_registry.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct S14TroopRegistry {
    size_t count, capacity;
    int sealed, borrowed;
    const S14TroopDefinition *entries;
};
#include "troop_catalog.h" /* Generated validated built-in definitions. */
static const S14TroopRegistry builtin_registry={
    sizeof(s14_builtin_troops)/sizeof(s14_builtin_troops[0]),
    sizeof(s14_builtin_troops)/sizeof(s14_builtin_troops[0]),1,1,s14_builtin_troops
};
const S14TroopRegistry *s14_troop_builtin_registry(void) { return &builtin_registry; }

static int utf8_text(const char *s,size_t cap) {
    size_t n=0;while(n<cap && s[n])n++;
    if(!n || n==cap)return 0;
    for(size_t i=0;i<n;) {
        unsigned char c=(unsigned char)s[i++];
        if(c<32 || c==127)return 0;
        if(c<128)continue;
        unsigned value,minimum;int rest;
        if(c>=0xc2 && c<=0xdf){value=c&31;minimum=0x80;rest=1;}
        else if(c>=0xe0 && c<=0xef){value=c&15;minimum=0x800;rest=2;}
        else if(c>=0xf0 && c<=0xf4){value=c&7;minimum=0x10000;rest=3;}
        else return 0;
        if(i+(size_t)rest>n)return 0;
        while(rest--){c=(unsigned char)s[i++];if((c&0xc0)!=0x80)return 0;value=(value<<6)|(c&63);}
        if(value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff))return 0;
    }
    return 1;
}
static int identifier(const char *s,size_t cap) {
    size_t n=0;while(n<cap && s[n])n++;
    if(!n || n==cap || s[0]<'a' || s[0]>'z')return 0;
    for(size_t i=0;i<n;i++) {
        char c=s[i];if(!((c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='.' || c=='_' || c=='-'))return 0;
    }
    return 1;
}
static int icon_path(const char *s,size_t cap) {
    static const char prefix[]="assets/troops/";
    size_t start=sizeof(prefix)-1;
    if(!utf8_text(s,cap) || strncmp(s,prefix,start) || strstr(s,".."))return 0;
    size_t n=strlen(s);if(n<start+5 || strcmp(s+n-4,".svg"))return 0;
    for(size_t i=start;i<n;i++) {
        char c=s[i];if(!((c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_' || c=='-' || c=='.'))return 0;
    }
    return 1;
}
S14TroopResult s14_troop_validate(const S14TroopDefinition *d) {
    if(!d || !identifier(d->id,sizeof(d->id)) || !identifier(d->unlock_key,sizeof(d->unlock_key)) ||
       !utf8_text(d->name,sizeof(d->name)) || !utf8_text(d->description,sizeof(d->description)) ||
       !icon_path(d->icon,sizeof(d->icon)) || !d->revision || d->max_soldiers>S14_TROOP_MAX_SOLDIERS || d->native_carrier<1 || d->native_carrier>20 ||
       d->commander_count<0 || d->commander_count>S14_TROOP_COMMANDERS ||
       d->effect_count<0 || d->effect_count>S14_TROOP_EFFECTS ||
       d->fixed_gold>1000000000ULL || d->gold_per_1000>1000000000ULL)return S14_TROOP_INVALID;
    if(d->commander_scope==S14_TROOP_ANY_COMMANDER){if(d->commander_count)return S14_TROOP_INVALID;}
    else if(d->commander_scope!=S14_TROOP_COMMANDER_WHITELIST || !d->commander_count)return S14_TROOP_INVALID;
    for(int i=0;i<d->commander_count;i++) {
        if(d->commanders[i]<0 || d->commanders[i]>6000)return S14_TROOP_INVALID;
        for(int j=0;j<i;j++)if(d->commanders[i]==d->commanders[j])return S14_TROOP_INVALID;
    }
    for(int i=0;i<S14_TROOP_ATTRIBUTES;i++)if(d->bonus_bp[i]<-9000 || d->bonus_bp[i]>30000)return S14_TROOP_INVALID;
    for(int i=0;i<d->effect_count;i++) {
        const S14TroopEffect *e=&d->effects[i];
        if(e->kind==S14_TROOP_DAMAGE_REDUCTION){if(e->status || e->value_bp<1 || e->value_bp>9000)return S14_TROOP_INVALID;}
        else if(e->kind==S14_TROOP_STATUS_IMMUNITY){if(e->status!=S14_TROOP_CONFUSION || e->value_bp!=10000)return S14_TROOP_INVALID;}
        else if(e->kind==S14_TROOP_SURROUND_IMMUNITY){if(e->status || e->value_bp!=10000)return S14_TROOP_INVALID;}
        else return S14_TROOP_INVALID;
        for(int j=0;j<i;j++)if(d->effects[j].kind==e->kind && d->effects[j].status==e->status)return S14_TROOP_INVALID;
    }
    return S14_TROOP_OK;
}
S14TroopRegistry *s14_troop_registry_create(void) { return calloc(1,sizeof(S14TroopRegistry)); }
void s14_troop_registry_destroy(S14TroopRegistry *r) {
    if(!r || r->borrowed)return;
    free((void*)r->entries);free(r);
}
S14TroopResult s14_troop_registry_register(S14TroopRegistry *r,const S14TroopDefinition *d) {
    if(!r)return S14_TROOP_INVALID;
    if(r->sealed)return S14_TROOP_SEALED;
    S14TroopResult valid=s14_troop_validate(d);if(valid!=S14_TROOP_OK)return valid;
    if(s14_troop_registry_find(r,d->id))return S14_TROOP_DUPLICATE;
    if(r->count>=S14_TROOP_LIMIT)return S14_TROOP_FULL;
    /* Copy before realloc: d may itself refer to an entry in this registry. */
    S14TroopDefinition copy=*d;
    if(r->count==r->capacity) {
        size_t capacity=r->capacity?r->capacity*2:8;
        S14TroopDefinition *entries=realloc((void*)r->entries,capacity*sizeof(*entries));
        if(!entries)return S14_TROOP_NO_MEMORY;
        r->entries=entries;r->capacity=capacity;
    }
    ((S14TroopDefinition*)r->entries)[r->count++]=copy;return S14_TROOP_OK;
}
S14TroopResult s14_troop_registry_seal(S14TroopRegistry *r) {
    if(!r)return S14_TROOP_INVALID;if(r->sealed)return S14_TROOP_SEALED;
    r->sealed=1;return S14_TROOP_OK;
}
size_t s14_troop_registry_count(const S14TroopRegistry *r) { return r?r->count:0; }
const S14TroopDefinition *s14_troop_registry_at(const S14TroopRegistry *r,size_t index) {
    return r && index<r->count?&r->entries[index]:NULL;
}
const S14TroopDefinition *s14_troop_registry_find(const S14TroopRegistry *r,const char *id) {
    if(!r || !id)return NULL;
    for(size_t i=0;i<r->count;i++)if(!strcmp(r->entries[i].id,id))return &r->entries[i];return NULL;
}
int s14_troop_commander_allowed(const S14TroopDefinition *d,int commander) {
    if(!d || commander<0 || commander>6000)return 0;
    if(d->commander_scope==S14_TROOP_ANY_COMMANDER)return 1;
    for(int i=0;i<d->commander_count;i++)if(d->commanders[i]==commander)return 1;return 0;
}
static S14TroopResult permission(const S14TroopDefinition *d,const S14TroopRequest *q) {
    if(!s14_troop_commander_allowed(d,q->commander_id))return S14_TROOP_COMMANDER_DENIED;
    if(q->unlocked!=1)return S14_TROOP_LOCKED;
    if(q->native_allowed!=1)return S14_TROOP_NATIVE_DENIED;
    if(q->native_carrier!=d->native_carrier)return S14_TROOP_CARRIER_MISMATCH;
    return S14_TROOP_OK;
}
S14TroopResult s14_troop_quote(const S14TroopRegistry *r,const char *id,const S14TroopRequest *q,S14TroopQuote *out) {
    if(!q || !out)return S14_TROOP_INVALID;
    memset(out,0,sizeof(*out));const S14TroopDefinition *d=s14_troop_registry_find(r,id);
    if(!d)return out->selection_result=S14_TROOP_NOT_FOUND;
    unsigned maximum=d->max_soldiers?d->max_soldiers:S14_TROOP_MAX_SOLDIERS;
    if(!q->soldiers || q->soldiers>maximum)return out->selection_result=S14_TROOP_SOLDIERS_INVALID;
    out->soldier_groups=q->soldiers/1000+(q->soldiers%1000!=0);
    out->fixed_gold=d->fixed_gold;out->variable_gold=(uint64_t)out->soldier_groups*d->gold_per_1000;
    out->extra_gold=out->fixed_gold+out->variable_gold;
    S14TroopResult result=permission(d,q);
    if(result==S14_TROOP_OK && q->treasury<out->extra_gold)result=S14_TROOP_GOLD_INSUFFICIENT;
    return out->selection_result=result;
}
S14TroopResult s14_troop_plan_attributes(const S14TroopRegistry *r,const char *id,const S14TroopRequest *q,
                                       const double native[S14_TROOP_ATTRIBUTES],double out[S14_TROOP_ATTRIBUTES]) {
    if(!q || !native || !out)return S14_TROOP_INVALID;
    const S14TroopDefinition *d=s14_troop_registry_find(r,id);if(!d)return S14_TROOP_NOT_FOUND;
    S14TroopResult result=permission(d,q);if(result!=S14_TROOP_OK)return result;
    if(!q->soldiers || q->soldiers>(d->max_soldiers?d->max_soldiers:S14_TROOP_MAX_SOLDIERS))return S14_TROOP_SOLDIERS_INVALID;
    double plan[S14_TROOP_ATTRIBUTES];
    for(int i=0;i<S14_TROOP_ATTRIBUTES;i++) {
        if(!isfinite(native[i]) || native[i]<0 || native[i]>1e9)return S14_TROOP_INVALID;
        plan[i]=native[i]*(10000+d->bonus_bp[i])/10000.0;
    }
    memcpy(out,plan,sizeof(plan));return S14_TROOP_OK;
}
unsigned s14_troop_required_capabilities(const S14TroopDefinition *d) {
    if(!d)return 0;
    unsigned result=S14_TROOP_CAP_BINDING|S14_TROOP_CAP_SELECTION_UI;
    for(int i=0;i<S14_TROOP_ATTRIBUTES;i++)if(d->bonus_bp[i])result|=S14_TROOP_CAP_ATTRIBUTE;
    if(d->fixed_gold || d->gold_per_1000)result|=S14_TROOP_CAP_GOLD_COMMIT;
    for(int i=0;i<d->effect_count;i++) {
        if(d->effects[i].kind==S14_TROOP_DAMAGE_REDUCTION)result|=S14_TROOP_CAP_DAMAGE_REDUCTION;
        if(d->effects[i].kind==S14_TROOP_STATUS_IMMUNITY)result|=S14_TROOP_CAP_STATUS_IMMUNITY;
        if(d->effects[i].kind==S14_TROOP_SURROUND_IMMUNITY)result|=S14_TROOP_CAP_SURROUND_IMMUNITY;
    }
    return result;
}
unsigned s14_troop_missing_capabilities(const S14TroopDefinition *d,unsigned available) {
    return s14_troop_required_capabilities(d)&~available;
}
const char *s14_troop_native_carrier_name(int id) {
    static const char *names[]={"无效","大戟","重骑","弓弩","刀盾","长枪","弓骑","突骑","井阑","冲车","投石","走舸","艨艟","楼船","四夷","亲卫","流民","货船","货船","辎重","货船"};
    return id>=1 && id<=20?names[id]:NULL;
}
const char *s14_troop_result_name(S14TroopResult result) {
    static const char *names[]={"ok","invalid","duplicate","full","sealed","no_memory","not_found",
        "commander_denied","locked","native_denied","carrier_mismatch","soldiers_invalid","gold_insufficient"};
    return result>=0 && (size_t)result<sizeof(names)/sizeof(names[0])?names[result]:"invalid";
}
