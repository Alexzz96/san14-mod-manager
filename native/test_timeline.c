#include <assert.h>
#ifdef NDEBUG
#error "Assertions required"
#endif
#include "battle_timeline.h"
#include "battle_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *world,*hex,*area,*city;static int reads,unstable;
static int read_mock(void *context,uintptr_t at,void *out,size_t size){(void)context;reads++;uintptr_t begin[]={ (uintptr_t)world,(uintptr_t)hex,(uintptr_t)area,(uintptr_t)city};size_t lengths[]={0x85200,32,56,50};for(int i=0;i<4;i++)if(at>=begin[i] && at+size>=at && at+size<=begin[i]+lengths[i]){memcpy(out,(void*)at,size);if(unstable && i==2 && reads==5)((unsigned char*)out)[0x35]=3;return 1;}return 0;}
int main(int argc,char **argv){
    assert(argc==2 || argc==3);wchar_t root[MAX_PATH],dir[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,root,MAX_PATH));swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);CreateDirectoryW(dir,NULL);s14_timeline_root(root);s14_special_root(root);s14_stats_root(root);
    unsigned char h1[32]={1},h2[32]={2};S14TimelineSnapshot *out=malloc(sizeof(*out));assert(out);
    if(argc==3){s14_timeline_load(99,110,1,h2,1,0);assert(s14_timeline_snapshot(99,out) && out->count==3 && out->bound);assert(!wcscmp(out->events[0].place.city,L"襄阳郡") && !out->events[0].id);puts("{\"cross_process_restore\":true}");free(out);return 0;}
    world=calloc(1,0x85200);hex=calloc(1,32);area=calloc(1,56);city=calloc(1,50);assert(world && hex && area && city);
    uintptr_t base=0x140000000;*(uintptr_t*)hex=base+0x129f660;*(unsigned short*)(hex+0x12)=239;*(uintptr_t*)area=base+0x129ff98;memcpy(area+0x10,L"长坂",6);area[0x35]=24;*(uintptr_t*)city=base+0x129fd10;memcpy(city+0x12,L"襄阳郡",8);
    *(void**)(world+0xdfe0+31100*8)=hex;*(void**)(world+0x6c860+239*8)=area;*(void**)(world+0xdaa8+24*8)=city;
    S14BattlePlace place;assert(s14_place_capture(read_mock,NULL,base,(uintptr_t)world,31100,&place));assert(reads==8 && place.area_id==239 && place.city_id==24 && !wcscmp(place.city,L"襄阳郡") && !wcscmp(place.area,L"长坂"));
    reads=0;unstable=1;assert(!s14_place_capture(read_mock,NULL,base,(uintptr_t)world,31100,&place));unstable=0;reads=0;
    assert(!s14_place_capture(read_mock,NULL,base,(uintptr_t)world,48400,&place) && reads==0);
    *(unsigned short*)(hex+0x12)=501;assert(!s14_place_capture(read_mock,NULL,base,(uintptr_t)world,31100,&place));*(unsigned short*)(hex+0x12)=239;
    reads=0;assert(s14_place_capture(read_mock,NULL,base,(uintptr_t)world,31100,&place));place.basis=1;
    wchar_t text[256];s14_place_text(&place,text,256);assert(!wcscmp(text,L"襄阳郡 · 长坂"));area[0x35]=0;assert(s14_place_capture(read_mock,NULL,base,(uintptr_t)world,31100,&place));assert(!place.city[0] && place.area[0]);area[0x35]=24;assert(s14_place_capture(read_mock,NULL,base,(uintptr_t)world,31100,&place));place.basis=1;
    assert(!s14_timeline_snapshot(1,out));s14_timeline_load(1,100,1,NULL,0,1);assert(s14_timeline_save(1,h1,1));
    s14_stats_load_end(1,100,1,NULL,0,1);S14RoundEvent e={.id=1,.world=1,.day=100,.kind=S14_ROUND_REMOVE,.stable=1,.reason=1,.place=place};
    e.source=(S14RoundObject){.kind=2,.id=1,.leader=10,.force=1,.active=1,.troops=5000};e.target=(S14RoundObject){.kind=2,.id=2,.leader=20,.force=2,.active=1,.troops=0};wcscpy(e.source.name,L"曹仁");wcscpy(e.target.name,L"张嶷");e.clock[0]=7;e.clock[1]=1;e.clock[2]=7;e.clock[3]=11;
    s14_stats_consume(&e);s14_stats_consume(&e);e.id=9;s14_stats_consume(&e);assert(s14_timeline_snapshot(1,out) && out->count==1);assert(s14_timeline_matches(&out->events[0],10) && s14_timeline_matches(&out->events[0],20) && !s14_timeline_matches(&out->events[0],30));
    s14_timeline_describe(&out->events[0],10,text,256);assert(!wcscmp(text,L"击溃 张嶷 部队"));s14_timeline_describe(&out->events[0],20,text,256);assert(!wcscmp(text,L"所部被 曹仁 击溃"));s14_timeline_date(&out->events[0],text,256);assert(!wcscmp(text,L"263 年 7 月 11 日"));
    S14SpecialEvent d={.id=2,.day=100,.kind=S14_SPECIAL_DUEL,.actor=10,.target=20,.actor_force=1,.target_force=2,.outcome=0,.actor_role=S14_SPECIAL_ACTOR_PERSON,.verified=1};memcpy(d.clock,e.clock,6);wcscpy(d.actor_name,L"曹仁");wcscpy(d.target_name,L"张嶷");s14_timeline_special(1,&d,&place);s14_timeline_special(1,&d,&place);
    d.id=3;d.kind=S14_SPECIAL_CAPTURE;d.verified=0;d.actor_role=S14_SPECIAL_ACTOR_COMMANDER;s14_timeline_special(1,&d,&place);assert(s14_timeline_snapshot(1,out) && out->count==3);
    s14_timeline_describe(&out->events[1],20,text,256);assert(wcsstr(text,L"败北"));s14_timeline_describe(&out->events[2],10,text,256);assert(wcsstr(text,L"个人抓捕者待核实"));
    assert(s14_timeline_save(1,h2,1));s14_timeline_load(1,100,0,h1,1,0);assert(s14_timeline_snapshot(1,out) && out->count==3);
    s14_timeline_load(1,100,1,h1,1,0);assert(s14_timeline_snapshot(1,out) && !out->count);s14_timeline_load(2,100,1,h2,1,0);assert(s14_timeline_snapshot(2,out) && out->count==3 && out->events[0].id==0);assert(!s14_timeline_snapshot(1,out));
    d.id=1;d.verified=1;d.actor_role=S14_SPECIAL_ACTOR_PERSON;s14_timeline_special(2,&d,&place);assert(s14_timeline_snapshot(2,out) && out->count==4); // Restored session IDs cannot suppress new events.
    s14_timeline_load(3,200,1,NULL,0,1);S14SpecialSnapshot *legacy=calloc(1,sizeof(*legacy));assert(legacy);legacy->version=1;legacy->world=3;legacy->count=1;legacy->events[0]=d;s14_timeline_import_special(legacy);s14_timeline_import_special(legacy);assert(s14_timeline_snapshot(3,out) && out->count==1 && out->events[0].legacy && out->events[0].place.tile==-1);s14_place_text(&out->events[0].place,text,256);assert(!wcscmp(text,L"地点未记录"));free(legacy);
    s14_timeline_load(4,300,1,NULL,0,1);for(int i=0;i<S14_TIMELINE_MAX+2;i++){d.id=i+1;d.day=300;s14_timeline_special(4,&d,&place);}assert(s14_timeline_snapshot(4,out) && out->count==S14_TIMELINE_MAX && out->truncated==2 && out->first==2);
    puts("{\"status\":\"passed\",\"place_read_only\":true,\"city_area_dictionary\":true,\"unstable_place_rejected\":true,\"rout_duplicate_and_unit_lifetime\":true,\"actor_and_victim_timelines\":true,\"duel_and_capture_candidates_distinct\":true,\"save_rollback_and_failure\":true,\"session_id_reset\":true,\"legacy_place_not_invented\":true,\"bounded_recent_history\":true}");free(out);free(world);free(hex);free(area);free(city);return 0;
}
