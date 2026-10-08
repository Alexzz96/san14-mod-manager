#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
#include "detail_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef struct {uintptr_t at;size_t n;unsigned char *p;} Segment;
static Segment segments[32];static int segment_count,mutate;
static unsigned char registry[128],controller[404],world[50000],dialog[832],tab[328],basic[368],person[64],other[64],table[8],handle[4],node[16],nodes[32784],pages[24];
static const uintptr_t base=0x140000000,world_addr=0x1000000,dlg_addr=0x2000000,body_addr=0x3000000,basic_addr=0x4000000,person_addr=0x5000000;
static void add(uintptr_t at,void *p,size_t n) {segments[segment_count++]=(Segment){at,n,p};}
static void put64(unsigned char *p,size_t at,uintptr_t v) {memcpy(p+at,&v,8);}
static void put32(unsigned char *p,size_t at,unsigned int v) {memcpy(p+at,&v,4);}
static int reader(void *unused,uintptr_t at,void *out,size_t n) {
    (void)unused;
    for(int i=0;i<segment_count;i++) if(at>=segments[i].at && at-segments[i].at<=segments[i].n && n<=segments[i].n-(at-segments[i].at)) {
        memcpy(out,segments[i].p+at-segments[i].at,n);
        if(mutate && at==body_addr && n==sizeof(tab)) {put64(tab,0x140,person_addr+0x1000);mutate=0;}
        return 1;
    }return 0;
}
static void fixture(void) {
    segment_count=0;memset(registry,0,sizeof(registry));memset(controller,0,sizeof(controller));memset(world,0,sizeof(world));memset(dialog,0,sizeof(dialog));memset(tab,0,sizeof(tab));memset(basic,0,sizeof(basic));memset(person,0,sizeof(person));
    static uintptr_t world_pointer;world_pointer=world_addr;add(base+0x1fc91d0,&world_pointer,8);
    put64(registry,0x38,1);put64(registry,0x40,0x6000000);put32(registry,0x70,1);add(base+0x201c320,registry,sizeof(registry));
    put64(controller,3*16+8,0x6100000);add(base+0x19e6390,controller,sizeof(controller));
    put32(handle,0,0);add(0x6100000,handle,4);put64(table,0,0x6200000);add(0x6000000,table,8);put64(node,0,dlg_addr);put64(node,8,0);add(0x6200000,node,16);
    put64(dialog,0,base+0x13812f8);put32(dialog,0x40,47);put64(dialog,0x2e0,body_addr);int box[]={191,147,1538,735};memcpy(dialog+0x20,box,16);add(dlg_addr,dialog,sizeof(dialog));
    put64(tab,0,base+0x1351680);put32(tab,0x40,15);put64(tab,0x130,0x6300000);put32(tab,0x138,3);put64(tab,0x140,person_addr);add(body_addr,tab,sizeof(tab));
    put64(pages,0,basic_addr);add(0x6300000,pages,sizeof(pages));put64(basic,0,base+0x1381700);put64(basic,0x168,person_addr);add(basic_addr,basic,sizeof(basic));
    put64(person,0,base+0x12a00d0);unsigned short id=518;memcpy(person+0x10,&id,2);memcpy(person+0x12,L"曹",4);memcpy(person+0x24,L"仁",4);add(person_addr,person,64);
    put64(world,0x148+518*8,person_addr);add(world_addr,world,sizeof(world));
}
int main(void) {
    S14DetailFrame f;fixture();assert(s14_detail_capture(reader,NULL,base,&f));assert(f.officer_id==518 && !wcscmp(f.name,L"曹仁") && f.world==world_addr && f.bytes<262144);
    put32(dialog,0x40,46);assert(!s14_detail_capture(reader,NULL,base,&f));put32(dialog,0x40,47);
    put64(tab,0x140,0);assert(!s14_detail_capture(reader,NULL,base,&f));put64(tab,0x140,person_addr);
    put64(basic,0x168,person_addr+0x1000);assert(!s14_detail_capture(reader,NULL,base,&f));put64(basic,0x168,person_addr);
    put64(world,0x148+518*8,0);assert(!s14_detail_capture(reader,NULL,base,&f));put64(world,0x148+518*8,person_addr);
    mutate=1;assert(!s14_detail_capture(reader,NULL,base,&f));fixture();
    memcpy(other,person,sizeof(other));unsigned short id=613;memcpy(other+0x10,&id,2);memcpy(other+0x12,L"张",4);memcpy(other+0x24,L"嶷",4);add(person_addr+0x1000,other,64);
    put64(world,0x148+613*8,person_addr+0x1000);put64(tab,0x140,person_addr+0x1000);put64(basic,0x168,person_addr+0x1000);
    assert(s14_detail_capture(reader,NULL,base,&f) && f.officer_id==613 && !wcscmp(f.name,L"张嶷"));
    fixture();put64(node,8,0x6200000);assert(!s14_detail_capture(reader,NULL,base,&f));fixture();
    for(int i=0;i<2049;i++) {put64(nodes,i*16,dlg_addr);put64(nodes,i*16+8,i<2048?0x7000000+(i+1)*16:0);}add(0x7000000,nodes,sizeof(nodes));put64(table,0,0x7000000);put32(dialog,0x40,46);assert(!s14_detail_capture(reader,NULL,base,&f) && f.calls<=8192 && f.bytes<=262144);
    fixture();RECT bar;int scale;RECT panel={191,147,1729,882};assert(s14_detail_rect(&panel,1920,1080,&bar,&scale) && scale==96 && bar.right==1729 && bar.top==109);
    RECT twice;assert(s14_detail_rect(&panel,3840,2160,&twice,&scale) && scale==192 && twice.left==bar.left*2 && twice.bottom==bar.bottom*2);
    assert(s14_detail_rect(&panel,2560,1080,&twice,&scale) && twice.left==bar.left+320);
    assert(s14_detail_rect(&panel,1280,720,&twice,&scale) && scale==64);assert(!s14_detail_rect(&panel,640,360,&twice,&scale));
    assert(!s14_detail_validate(reader,NULL,base));
    printf("{\"status\":\"passed\",\"read_only_selection\":true,\"person_switch\":true,\"native_page_identity_crosscheck\":true,\"world_person_crosscheck\":true,\"close_hides\":true,\"switch_race_rejected\":true,\"registry_budget\":true,\"ultrawide_and_4k_geometry\":true}\n");return 0;
}
