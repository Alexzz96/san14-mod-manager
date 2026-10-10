#include "troop_registry.h"
#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "army_model.h"
_Static_assert((int)S14_TROOP_ATTACK==(int)S14_ARMY_ATTACK &&
               (int)S14_TROOP_SIEGE_ATTACK==(int)S14_ARMY_CITY &&
               (int)S14_TROOP_SIEGE_BREAK==(int)S14_ARMY_BREAK &&
               (int)S14_TROOP_MOBILITY==(int)S14_ARMY_MOVE &&
               (int)S14_TROOP_DEFENSE==(int)S14_ARMY_DEFENSE,"Army attribute order must match");

int main(void) {
    const S14TroopRegistry *catalog=s14_troop_builtin_registry();
    assert(s14_troop_registry_count(catalog)==1);
    const S14TroopDefinition *d=s14_troop_registry_find(catalog,"san14.xianzhen");
    assert(d && s14_troop_validate(d)==S14_TROOP_OK && d==s14_troop_registry_at(catalog,0));
    assert(!strcmp(d->name,"陷阵营") && !strcmp(d->icon,"assets/troops/xianzhen.svg"));
    assert(d->native_carrier==1 && s14_troop_commander_allowed(d,254) && !s14_troop_commander_allowed(d,518));
    assert(!s14_troop_registry_at(catalog,1) && !s14_troop_registry_find(catalog,"missing"));
    assert(!s14_troop_registry_at(NULL,0) && !s14_troop_registry_find(NULL,"id"));
    assert(s14_troop_registry_count(NULL)==0 && !s14_troop_registry_find(catalog,NULL));
    assert(!strcmp(s14_troop_native_carrier_name(1),"大戟") && !s14_troop_native_carrier_name(21));
    unsigned required=s14_troop_required_capabilities(d);assert(required==127);
    assert(s14_troop_missing_capabilities(d,0)==required); /* Nothing is active merely by registering. */
    assert(s14_troop_missing_capabilities(d,S14_TROOP_CAP_ATTRIBUTE)==(required&~S14_TROOP_CAP_ATTRIBUTE));
    S14TroopRegistry *builder=s14_troop_registry_create();assert(builder);
    S14TroopDefinition copy=*d;assert(s14_troop_registry_register(builder,&copy)==S14_TROOP_OK);
    copy.bonus_bp[0]=999;assert(s14_troop_registry_at(builder,0)->bonus_bp[0]==30000);
    assert(s14_troop_registry_register(builder,d)==S14_TROOP_DUPLICATE && s14_troop_registry_count(builder)==1);
    for(int i=1;i<S14_TROOP_LIMIT;i++) {
        copy=*d;snprintf(copy.id,sizeof(copy.id),"test.troop_%03d",i);
        assert(s14_troop_registry_register(builder,&copy)==S14_TROOP_OK);
    }
    assert(s14_troop_registry_count(builder)==256 && s14_troop_registry_find(builder,"test.troop_022"));
    copy=*d;strcpy(copy.id,"test.overflow");
    assert(s14_troop_registry_register(builder,&copy)==S14_TROOP_FULL && s14_troop_registry_count(builder)==256);
    assert(s14_troop_registry_seal(builder)==S14_TROOP_OK && s14_troop_registry_seal(builder)==S14_TROOP_SEALED);
    assert(s14_troop_registry_register(builder,d)==S14_TROOP_SEALED);
    s14_troop_registry_destroy(builder);
    builder=s14_troop_registry_create();assert(builder);
    copy=*d;copy.native_carrier=21;assert(s14_troop_registry_register(builder,&copy)==S14_TROOP_INVALID);
    copy=*d;copy.revision=0;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.fixed_gold=UINT64_MAX;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.commander_count=65;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.commanders[1]=254;copy.commander_count=2;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.commander_scope=S14_TROOP_ANY_COMMANDER;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy.commander_count=0;assert(s14_troop_validate(&copy)==S14_TROOP_OK && s14_troop_commander_allowed(&copy,518));
    copy=*d;copy.bonus_bp[0]=-10000;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.effect_count=9;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.effects[0].kind=999;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.effects[1]=copy.effects[0];assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;copy.effects[1].value_bp=5000;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;strcpy(copy.icon,"assets/troops/../x.svg");assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;memset(copy.id,'a',sizeof(copy.id));assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;strcpy(copy.id,"game;invalid");assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;strcpy(copy.name,"\xc0\x80");assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;strcpy(copy.name,"\xed\xa0\x80");assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy=*d;strcpy(copy.name,"\xf4\x90\x80\x80");assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    assert(s14_troop_registry_count(builder)==0);s14_troop_registry_destroy(builder);

    assert(d->max_soldiers==1000);
    copy=*d;copy.max_soldiers=100001;assert(s14_troop_validate(&copy)==S14_TROOP_INVALID);
    copy.max_soldiers=0;assert(s14_troop_validate(&copy)==S14_TROOP_OK);
    S14TroopRequest request={254,1,1,1,1000,10600},before=request;
    S14TroopQuote quote;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_OK);
    assert(quote.extra_gold==2500 && quote.fixed_gold==2000 && quote.variable_gold==500 && quote.soldier_groups==1);
    assert(!memcmp(&request,&before,sizeof(request))); /* Quote never deducts. */
    unsigned soldiers[]={1,999,1000,1001,5000,100000};uint64_t costs[]={2500,2500,2500,3000,4500,52000};
    for(size_t i=0;i<sizeof(soldiers)/sizeof(soldiers[0]);i++) {
        request.soldiers=soldiers[i];request.treasury=UINT64_MAX;
        if(soldiers[i]<=1000)assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_OK && quote.extra_gold==costs[i]);
        else assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_SOLDIERS_INVALID);
    }
    builder=s14_troop_registry_create();copy=*d;copy.max_soldiers=0;
    assert(s14_troop_registry_register(builder,&copy)==S14_TROOP_OK && s14_troop_registry_seal(builder)==S14_TROOP_OK);
    for(size_t i=0;i<sizeof(soldiers)/sizeof(soldiers[0]);i++) {
        request.soldiers=soldiers[i];request.treasury=UINT64_MAX;
        assert(s14_troop_quote(builder,d->id,&request,&quote)==S14_TROOP_OK && quote.extra_gold==costs[i]);
    }
    s14_troop_registry_destroy(builder);
    request=before;request.treasury=2500;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_OK);
    request.treasury=2499;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_GOLD_INSUFFICIENT);
    request=before;request.unlocked=0;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_LOCKED && quote.extra_gold==2500);
    request=before;request.commander_id=518;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_COMMANDER_DENIED);
    request=before;request.native_allowed=0;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_NATIVE_DENIED);
    request=before;request.native_carrier=2;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_CARRIER_MISMATCH);
    request=before;request.soldiers=0;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_SOLDIERS_INVALID);
    request.soldiers=100001;assert(s14_troop_quote(catalog,d->id,&request,&quote)==S14_TROOP_SOLDIERS_INVALID);
    request=before;assert(s14_troop_quote(catalog,"missing",&request,&quote)==S14_TROOP_NOT_FOUND && quote.extra_gold==0);
    assert(s14_troop_quote(catalog,d->id,NULL,&quote)==S14_TROOP_INVALID);
    double native[]={1000,300,20,20,1000},saved[5],plan[5];memcpy(saved,native,sizeof(native));
    for(int i=0;i<1000;i++) {
        assert(s14_troop_plan_attributes(catalog,d->id,&request,native,plan)==S14_TROOP_OK);
        assert(fabs(plan[0]-4000)<1e-8 && fabs(plan[1]-330)<1e-8 && plan[2]==24 && plan[3]==16 && fabs(plan[4]-3000)<1e-8);
        assert(!memcmp(saved,native,sizeof(native))); /* Never compounds on render or repeated requests. */
    }
    double unchanged[]={1,2,3,4,5};memcpy(plan,unchanged,sizeof(plan));
    request.soldiers=1001;assert(s14_troop_plan_attributes(catalog,d->id,&request,native,plan)==S14_TROOP_SOLDIERS_INVALID);
    assert(!memcmp(unchanged,plan,sizeof(plan)));request=before;
    request.commander_id=518;assert(s14_troop_plan_attributes(catalog,d->id,&request,native,plan)==S14_TROOP_COMMANDER_DENIED);
    assert(!memcmp(unchanged,plan,sizeof(plan)));
    request=before;request.unlocked=0;assert(s14_troop_plan_attributes(catalog,d->id,&request,native,plan)==S14_TROOP_LOCKED);
    request=before;native[4]=NAN;assert(s14_troop_plan_attributes(catalog,d->id,&request,native,plan)==S14_TROOP_INVALID);
    assert(!memcmp(unchanged,plan,sizeof(plan)));native[4]=INFINITY;
    assert(s14_troop_plan_attributes(catalog,d->id,&request,native,plan)==S14_TROOP_INVALID);native[4]=-1;
    assert(s14_troop_plan_attributes(catalog,d->id,&request,native,plan)==S14_TROOP_INVALID);
    puts("{\"status\":\"passed\",\"registry_capacity\":256,\"beyond_native_21\":true,\"duplicate_rejected\":true,"
         "\"sealed_readers\":true,\"deep_copy\":true,\"commander_scoped\":true,\"explicit_selection_required\":true,"
         "\"unlocked_required\":true,\"extra_gold_ceiling\":true,\"no_funds_deducted\":true,\"plan_no_compounding\":true,"
         "\"soldier_cap_verified\":true,\"general_cap_compat\":true,\"pending_effects_not_applied\":true,\"invalid_inputs_rejected\":true,\"gameplay_integrated\":false}");
    return 0;
}
