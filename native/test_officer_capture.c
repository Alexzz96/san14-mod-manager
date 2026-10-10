#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "officer_model.h"
static unsigned char *image,*world,*person,*empty,*later,*settings,*strings,*def,*tactic;
static int mutate_empty;
static void ptr(void *where,void *value){memcpy(where,&value,8);}
static void half(void *where,unsigned short value){memcpy(where,&value,2);}
static void vt(void *where,uintptr_t offset){uintptr_t value=(uintptr_t)image+offset;memcpy(where,&value,8);}
static int read_fixture(void *context,uintptr_t address,void *out,size_t size){
    (void)context;memcpy(out,(void*)address,size);
    if(mutate_empty && address==(uintptr_t)empty && size==512){empty[0x11e]^=1;}
    return 1;
}
int main(void){
    image=calloc(1,0x2100000);world=calloc(1,0x90000);person=calloc(1,512);empty=calloc(1,512);later=calloc(1,512);settings=calloc(1,128);strings=calloc(1,0x2000);def=calloc(1,224);tactic=calloc(1,136);
    assert(image && world && person && empty && later && settings && strings && def && tactic);
    ptr(image+0x1fc91d0,world);ptr(world+0x85130,settings);settings[0x34]=7;
    ptr(image+0x1fc9190,strings);half(strings+10,4);strings[12]=1;unsigned offset=24;memcpy(strings+16,&offset,4);
    for(int i=0;i<10;i++){unsigned relative=0x1000;memcpy(strings+24+4*(0xf8+i),&relative,4);}memcpy(strings+24+0x1000,L"普通",6);
    ptr(world+0x7d440+8,def);vt(def,0x12a0658);memcpy(def+0x10,L"个性",6);
    ptr(world+0x76c00+8,tactic);vt(tactic,0x12a0298);memcpy(tactic+0x10,L"战法",6);
    ptr(world+0x148+8,person);vt(person,0x12a00d0);half(person+0x10,1);memcpy(person+0x12,L"张嶷",6);person[0x11e]=4;half(person+0x15e,303);
    ptr(world+0x148+3001*8,empty);vt(empty,0x12a00d0);empty[0x129]=100;half(empty+0x186,65535);empty[0x1e8]=7;half(empty+0x1f8,65535);
    ptr(world+0x148+5001*8,later);vt(later,0x12a00d0);half(later+0x10,5001);memcpy(later+0x12,L"灵帝",6);later[0x11e]=9;
    S14OfficerSnapshot *s=calloc(1,sizeof(*s));assert(s);
    assert(s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s));assert(s->count==2 && !s->read_errors && !s->unstable);assert(s->rows[1].id==5001 && s->rows[0].personalities[7]==303);
    memset(empty+0x124,1,5);assert(s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s) && s->count==2);empty[0x124]=10;assert(!s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s) && s->read_errors);memset(empty+0x124,0,5);
    /* A name/effect/status on an ID-zero record is not a safe placeholder. */
    empty[0x12]=1;assert(!s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s) && s->read_errors);empty[0x12]=0;
    half(empty+0x150,300);assert(!s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s) && s->read_errors);half(empty+0x150,0);
    empty[0x11e]=4;assert(!s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s) && s->read_errors);empty[0x11e]=0;
    half(later+0x10,5002);assert(!s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s) && s->read_errors);half(later+0x10,5001);
    mutate_empty=1;assert(!s14_officers_capture(read_fixture,NULL,(uintptr_t)image,s) && s->unstable);
    puts("{\"status\":\"passed\",\"uninitialized_records_skipped\":true,\"later_officers_preserved\":true,\"hidden_personalities_preserved\":true,\"named_or_effectful_zero_identity_rejected\":true,\"wrong_identity_rejected\":true,\"unstable_empty_rejected\":true}");
    return 0;
}
