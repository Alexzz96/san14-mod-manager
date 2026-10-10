#include "troop_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void json_text(const char *s) {
    putchar('"');for(;*s;s++) {
        unsigned char c=(unsigned char)*s;
        if(c=='"' || c=='\\'){putchar('\\');putchar(c);}
        else if(c<32)printf("\\u%04x",c);else putchar(c);
    }putchar('"');
}
static int number(const char *s,uint64_t max,uint64_t *out) {
    if(!s || !*s)return 0;for(const char *p=s;*p;p++)if(*p<'0'||*p>'9')return 0;
    char *end;errno=0;unsigned long long n=strtoull(s,&end,10);
    if(errno || *end || n>max)return 0;*out=n;return 1;
}
int main(int argc,char **argv) {
    const S14TroopRegistry *r=s14_troop_builtin_registry();
    if(argc==2 && !strcmp(argv[1],"--list")) {
        printf("{\"schema_version\":%d,\"gameplay_integrated\":false,\"troops\":[",S14_TROOP_SCHEMA);
        for(size_t i=0;i<s14_troop_registry_count(r);i++) {
            const S14TroopDefinition *d=s14_troop_registry_at(r,i);if(i)putchar(',');
            printf("{\"id\":");json_text(d->id);printf(",\"name\":");json_text(d->name);
            printf(",\"revision\":%u,\"native_carrier\":%d,\"carrier_name\":",d->revision,d->native_carrier);
            /* Carrier label follows; capacity is emitted as a separate field. */
            json_text(s14_troop_native_carrier_name(d->native_carrier));printf(",\"icon\":");json_text(d->icon);
            printf(",\"max_soldiers\":%u",d->max_soldiers?d->max_soldiers:S14_TROOP_MAX_SOLDIERS);
            printf(",\"unlock_key\":");json_text(d->unlock_key);
            printf(",\"commander_scope\":\"%s\",\"commanders\":[",d->commander_scope==S14_TROOP_ANY_COMMANDER?"all":"whitelist");
            for(int j=0;j<d->commander_count;j++)printf("%s%d",j?",":"",d->commanders[j]);
            printf("],\"attribute_bonus_bp\":{");const char *names[]={"attack","siege_attack","siege_break","mobility","defense"};
            for(int j=0;j<S14_TROOP_ATTRIBUTES;j++)printf("%s\"%s\":%d",j?",":"",names[j],d->bonus_bp[j]);
            printf("},\"effects\":[");
            for(int j=0;j<d->effect_count;j++) {
                const S14TroopEffect *e=&d->effects[j];
                printf("%s{\"kind\":\"%s\",\"value_bp\":%d,\"applied\":false",j?",":"",
                    e->kind==S14_TROOP_DAMAGE_REDUCTION?"damage_reduction":e->kind==S14_TROOP_SURROUND_IMMUNITY?"surround_immunity":"status_immunity",e->value_bp);
                if(e->status==S14_TROOP_CONFUSION)printf(",\"status\":\"confusion\"");putchar('}');
            }
            printf("],\"extra_gold\":{\"fixed\":%llu,\"per_1000_soldiers\":%llu},\"missing_capabilities\":%u}",
                (unsigned long long)d->fixed_gold,(unsigned long long)d->gold_per_1000,s14_troop_missing_capabilities(d,0));
        }puts("]}");return 0;
    }
    if(argc==7 && !strcmp(argv[1],"--quote")) {
        uint64_t commander,soldiers,funds,unlocked;
        if(!number(argv[3],6000,&commander) || !number(argv[4],S14_TROOP_MAX_SOLDIERS,&soldiers) ||
           !number(argv[5],UINT64_MAX,&funds) || !number(argv[6],1,&unlocked))goto usage;
        const S14TroopDefinition *d=s14_troop_registry_find(r,argv[2]);
        S14TroopRequest request={(int)commander,d?d->native_carrier:0,1,(int)unlocked,(uint32_t)soldiers,funds};
        S14TroopQuote quote;S14TroopResult result=s14_troop_quote(r,argv[2],&request,&quote);
        printf("{\"result\":\"%s\",\"extra_gold\":%llu,\"fixed_gold\":%llu,\"variable_gold\":%llu,"
            "\"soldier_groups\":%u,\"funds_deducted\":false,\"gameplay_integrated\":false}\n",
            s14_troop_result_name(result),(unsigned long long)quote.extra_gold,(unsigned long long)quote.fixed_gold,
            (unsigned long long)quote.variable_gold,quote.soldier_groups);
        return 0; /* A well-formed quote can contain a denied selection. */
    }
usage:
    fputs("Usage: troop_catalog --list\n       troop_catalog --quote ID COMMANDER SOLDIERS FUNDS UNLOCKED(0|1)\n",stderr);
    return 2;
}
