#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "troop_runtime.c"
#include "troop_ui.c"
#undef assert
#define assert(x) do {if(!(x)){fprintf(stderr,"Failed: %s, line %d\n",#x,__LINE__);_Exit(2);}} while(0)
static unsigned char *w,*units,*officer,*city,*state,*tab,*button,*first,*packet,*formation,*detail_page;
static int create_calls,gold_calls,confirm_calls,refresh_calls,fail_creation;
static int ai_create_test,ai_expected,ai_provisional_seen;
static unsigned char *wrappers[5],*inners[5];
static int configured,text_calls,funds_calls,formation_calls,bind_native_third;
static int capacity_calls,capacity_row,native_maximum=5000,detail_calls,detail_text_calls;
static wchar_t detail_caption[64];
static void putptr(void *p,uintptr_t x){memcpy(p,&x,8);}
static void putint(void *p,int x){memcpy(p,&x,4);}
static void putword(void *p,int x){unsigned short s=x;memcpy(p,&s,2);}
static uintptr_t native_noop(void *p){(void)p;return 7;}
static uintptr_t native_two(void *p,void *q){(void)p;(void)q;return 8;}
static void *city_mock(void *world,int id){assert(world==w && id==12);return city;}
static uintptr_t gold_mock(void *c,int delta){assert(c==city && delta<0);gold_calls++;putint(city+0x34,integer((uintptr_t)city+0x34)+delta);return 0;}
static void selected_mock(void*,int);
static uintptr_t refresh_mock(void *p){assert(p==tab);refresh_calls++;for(int i=0;i<5;i++)selected_mock(inners[i],integer((uintptr_t)wrappers[i]+0x80)==integer((uintptr_t)tab+0x174));return 0;}
static void visible_mock(void *p,int on){assert(p && (on==0||on==1));putint((unsigned char*)p+0x40,(integer((uintptr_t)p+0x40)&~1)|on);}
static void selected_mock(void *p,int on){assert(p && (on==0||on==1));putint((unsigned char*)p+0x40,(integer((uintptr_t)p+0x40)&~0x100)|(on?0x100:0));putint((unsigned char*)p+0x188,on);}
static void enabled_mock(void *p,int on){assert(p && (on==0||on==1));putint((unsigned char*)p+0x40,(integer((uintptr_t)p+0x40)&~4)|(on?4:0));}
static void button_mock(void *p,void *f){assert(p==wrappers[2] && f==formation);configured++;putint(wrappers[2]+0x80,1);putint(inners[2]+0x80,1);}
static void text_mock(void *layout,int index,const wchar_t *text,int mode){assert(!wcscmp(text,L"陷阵营") && mode==1);if(layout==detail_page+0x128){assert(index==0x29);wcscpy(detail_caption,text);detail_text_calls++;}else {assert(layout==wrappers[2]+0x128 && index==4);text_calls++;}}
static int funds_mock(void *p){assert(p==state);funds_calls++;SetLastError(888);return integer((uintptr_t)city+0x34)-50;}
static int capacity_mock(void *s,int row){assert(s==state);capacity_calls++;capacity_row=row;SetLastError(879);return row>=0 && row<64?native_maximum:0;}
static uintptr_t detail_mock(void *p,void *a){assert(p==detail_page && a);detail_calls++;wcscpy(detail_caption,integer((uintptr_t)p+0x170)?L"走舸":L"大戟");SetLastError(987);return 17;}
static uintptr_t bind_mock(void *t,void *p){
    assert(t==tab && p==packet);memcpy(tab+0x168,p,0x94);
    for(int i=0;i<5;i++){visible_mock(wrappers[i],i<2 || (i==2 && bind_native_third));putint(wrappers[i]+0x80,i<2 || (i==2 && bind_native_third)?i+1:0);putint(inners[i]+0x80,integer((uintptr_t)wrappers[i]+0x80));selected_mock(inners[i],i==0);}
    return 8;
}
static uintptr_t formation_mock(void *ev,void *ref){assert(ev && ref);formation_calls++;uintptr_t widget=ptr((uintptr_t)ref);int id=integer(widget+0x80);if(id>=1 && id<=20){putint(tab+0x174,id);native_refresh(tab);native_copy(state,NULL);}return 8;}
static void mouse_click(int index){
    unsigned char *inner=inners[index];int flags=integer((uintptr_t)inner+0x40);
    /* Verified native mouse-down 0x786260 sets bit 0x100 without writing +188.
       The real button routes release to its registered formation callback. */
    assert(flags&4);putint(inner+0x40,flags|0x100);
    int logical=integer((uintptr_t)inner+0x188);
    for(int i=0;i<12;i++){tick_hook(state);assert((integer((uintptr_t)inner+0x40)&0x100) && integer((uintptr_t)inner+0x188)==logical);}
    uintptr_t ev[2]={0,(uintptr_t)tab},ref=(uintptr_t)inner;formation_hook(ev,&ref);
}
static uintptr_t copy_mock(void *p,void *q){assert(p==state && !q);memcpy(packet,tab+0x168,0x94);return 0;}
static uintptr_t confirm_mock(void *p,void *q){assert(p==state && q==button);confirm_calls++;return 42;}
static uintptr_t create_mock(void *p){assert(p==packet);create_calls++;if(fail_creation)return 0;unsigned char *a=units+512;memset(a,0,512);putptr(a,image+0x123e288);a[0x10]=1;putword(a+0x12,254);putword(a+0x14,0);putword(a+0x16,((int*)p)[2]);a[0x1b]=1;putword(a+0x28,create_calls);assert(!s14_troop_bound(a));assert(s14_troop_attribute(a,0,1,100)==(creating_definition?400:100));if(ai_create_test){assert(s14_ai_active(a)==ai_expected);ai_provisional_seen++;}SetLastError(777);return (uintptr_t)a;}
static uintptr_t init_mock(void *p,void *q){assert(p && q==packet);return (uintptr_t)p;}
static uintptr_t preview_mock(void *p){unsigned char temporary[512]={0};init_hook(temporary,p);assert(fabsf(s14_troop_attribute(temporary,0,1,100)-400)<0.001f);assert(s14_troop_attribute(temporary,0,0,100)==100);assert(fabsf(s14_troop_attribute(temporary,4,1,100)-300)<.001f);unsigned char other[512]={0};assert(s14_troop_attribute(other,0,1,100)==100);return 9;}
static float mobility_mock(void *a,void *f,int actual,int mode,void *hex){assert(a==units+512 && f==(void*)3 && mode==4 && hex==(void*)5 && (actual==0||actual==1));SetLastError(889);return 37;}
static float break_mock(void *a,void *f,int actual,int mode){assert(a==units+512 && f==(void*)3 && mode==4 && (actual==0||actual==1));SetLastError(891);return 20;}
static int query_mock(void *a,int category){assert(a);SetLastError(901);return category==21 || category==22;}
static int clear_calls;
static uintptr_t abnormal_mock(void *a,void *source,int mode,int duration,void *hex,const wchar_t *name,int option){
    assert(a==units+512 && !source && mode==0 && duration==-255 && !hex && !name && !option);clear_calls++;
    assert(!query_hook(a,21) && !query_hook(a,22) && !s14_troop_confusion_reject(a,0,duration));
    int out[119];for(int i=0;i<119;i++)out[i]=100+i;s14_troop_clear_modifiers(a,out);
    assert(out[9]==0 && out[10]==0 && out[11]==111 && out[12]==112 && out[22]==122);
    unsigned char *b=a;b[0x23]=0;SetLastError(902);return 0;
}
static void immunity_tests(void){
    unsigned char *a=units+512;a[0x23]=17;a[0x24]=8;a[0x25]=9;
    SetLastError(903);s14_troop_status_service(a);assert(clear_calls==1 && GetLastError()==903 && !a[0x23] && a[0x24]==8 && a[0x25]==9);
    assert(s14_troop_confusion_reject(a,0,10)==22 && !s14_troop_confusion_reject(a,1,10) && !s14_troop_confusion_reject(a,2,10));
    assert(!s14_troop_confusion_reject(a,0,-10) && !s14_troop_confusion_reject(units+1024,0,10));
    assert(query_hook(a,22)==1 && query_hook(a,23)==0 && query_hook(a,24)==0);
    int modifiers[119];for(int i=0;i<119;i++)modifiers[i]=i;s14_troop_clear_modifiers(a,modifiers);assert(modifiers[9]==9 && modifiers[10]==10);
    assert(s14_troop_surround_mode(a,4,1,image+0x15c084)==1);
    assert(s14_troop_surround_mode(a,4,0,image+0x15c084)==4 && s14_troop_surround_mode(a,4,1,image+0x15c085)==4);
    assert(s14_troop_surround_mode(units+1024,4,1,image+0x15c084)==4);
    s14_troop_configure(0);a[0x23]=13;assert(!s14_troop_confusion_reject(a,0,10) && s14_troop_surround_mode(a,4,1,image+0x15c084)==4);s14_troop_status_service(a);assert(a[0x23]==13);
    s14_troop_configure(1);assert(a[0x23]==13);s14_troop_status_service(a);assert(clear_calls==2 && !a[0x23] && a[0x24]==8 && a[0x25]==9);
    assert(s14_troop_capabilities()==127);
}
static void environment(void){
    image=(uintptr_t)calloc(1,0x2100000);assert(image);w=calloc(1,0x100000);units=calloc(501,512);officer=calloc(1,512);city=calloc(1,512);state=calloc(1,0x600);tab=calloc(1,0x600);button=calloc(1,256);packet=calloc(1,0x94);formation=calloc(1,192);assert(w&&units&&officer&&city&&state&&tab&&button&&packet&&formation);
    putptr((void*)(image+0x1fc91d0),(uintptr_t)w);for(int i=0;i<=500;i++)putptr(w+0x7df60+i*8,(uintptr_t)(units+i*512));
    putptr(w+0x148+254*8,(uintptr_t)officer);putptr(officer,image+0x12a00d0);putint(officer+0x164,6);putword(officer+0x11a,12);
    putptr(formation,image+0x12a0300);putword(formation+0x9a,50);putptr(w+0x76b58+8,(uintptr_t)formation);putptr(w+0x76b58+11*8,(uintptr_t)formation);
    putptr(state,image+0x133f438);putptr(state+0x490,(uintptr_t)tab);putptr(state+0x4b8,(uintptr_t)city);putword(city+0x10,12);putint(city+0x34,10600);
    static uintptr_t entry,packets[1];entry=(uintptr_t)state;packets[0]=(uintptr_t)packet;putptr((void*)(image+0x19e6330+16),1);putptr((void*)(image+0x19e6330+32),(uintptr_t)&entry);putptr(state+0x4c0,(uintptr_t)packets);putptr(state+0x4c8,(uintptr_t)(packets+1));
    int values[]={254,0,5000,1,11};memcpy(packet,values,20);memcpy(tab+0x168,packet,0x94);putptr(tab,image+0x133fd10);putint(tab+0x40,15);int tr[]={835,118,954,783};memcpy(tab+0x20,tr,16);putint(button+0x80,5);
    for(int i=0;i<5;i++){wrappers[i]=calloc(1,512);inners[i]=calloc(1,512);assert(wrappers[i] && inners[i]);putptr(tab+0x258+i*8,(uintptr_t)wrappers[i]);putptr(wrappers[i],image+0x133ff30);putptr(inners[i],image+0x12fc420);putptr(wrappers[i]+0x168,(uintptr_t)inners[i]);putint(wrappers[i]+0x138,5);int rect[]={96+118*i,456,100,86};memcpy(wrappers[i]+0x20,rect,16);putint(wrappers[i]+0x40,i<2?15:14);putint(inners[i]+0x40,0x47);putint(wrappers[i]+0x80,i<2?i+1:0);putint(inners[i]+0x80,i<2?i+1:0);}first=wrappers[0];
    detail_page=calloc(1,0x200);assert(detail_page);putptr(detail_page,image+0x137ebf0);putptr(detail_page+0x168,(uintptr_t)(units+512));putint(detail_page+0x138,100);
    original_tick=native_noop;original_exit=native_two;original_bind=bind_mock;original_formation=formation_mock;original_confirm=confirm_mock;original_create=create_mock;original_init=init_mock;original_preview=preview_mock;original_mobility=mobility_mock;original_refresh=refresh_mock;original_funds=funds_mock;original_capacity=capacity_mock;original_detail=detail_mock;native_refresh=refresh_hook;native_copy=copy_mock;native_city=city_mock;native_gold=gold_mock;native_button=button_mock;native_text=text_mock;native_visible=visible_mock;native_selected=selected_mock;native_enabled=enabled_mock;ready=enabled=1;live.world=(uintptr_t)w;
}
static void preview_image(const S14TroopFrame *f){
    S14TroopUI ui={.frame=*f,.width=460,.height=328,.scale=96};ui.font=CreateFontW(-16,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=460;info.bmiHeader.biHeight=-328;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL);void *pixels=NULL;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(bitmap && pixels);HGDIOBJ old=SelectObject(dc,bitmap);paint(&ui,dc);GdiFlush();
    FILE *file=fopen("troop-selection-preview.bmp","wb");assert(file);BITMAPFILEHEADER h={.bfType=0x4d42,.bfOffBits=sizeof(h)+sizeof(BITMAPINFOHEADER),.bfSize=sizeof(h)+sizeof(BITMAPINFOHEADER)+460*328*4};assert(fwrite(&h,sizeof(h),1,file)==1 && fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1 && fwrite(pixels,460*328*4,1,file)==1);fclose(file);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);DeleteObject(ui.font);
}
static void ai_creation_bridge_tests(const wchar_t *root){
    unsigned char settings[128]={0},group[64]={0};settings[0x3a]=1;group[0x10]=3;
    putptr(w+0x85130,(uintptr_t)settings);putptr(w+0xde40+16,(uintptr_t)group);putptr(group,image+0x129fec8);officer[0x118]=2;putword(officer+0x10,254);
    putptr(w+0xdaa8+12*8,(uintptr_t)city);putptr(city,image+0x129fd10);
    memset(live.pending,0,sizeof(live.pending));s14_troop_configure(0);s14_ai_attach(image,root,1);s14_ai_configure(1);
    unsigned char hash[32]={42};s14_ai_load_begin();s14_ai_load_end((uintptr_t)w,1,hash,1);
    ai_create_test=1;int hits=0;
    for(int i=0;i<250;i++){
        units[512+0x10]=0;s14_ai_forget(units+512);S14AiDraw expected,after;s14_ai_prepare(254,image+0x1d1586,&expected);assert(expected.eligible && expected.sequence==(unsigned)i+1 && expected.city==12 && expected.city_sequence==(unsigned)i+1 && expected.guaranteed==((i+1)%9==0));ai_expected=expected.hit;
        fail_creation=1;int calls=create_calls;assert(!create_hook(packet) && create_calls==calls+1);fail_creation=0;
        s14_ai_prepare(254,image+0x1d1586,&after);assert(after.rng==expected.rng && after.sequence==expected.sequence && after.hit==expected.hit);
        assert(create_hook(packet)==(uintptr_t)(units+512) && GetLastError()==777 && s14_ai_active(units+512)==expected.hit);hits+=expected.hit;
    }
    assert(ai_provisional_seen==250 && hits>0 && hits<250);
    units[512+0x10]=0;s14_ai_forget(units+512);group[0x10]=1;ai_expected=0;assert(create_hook(packet) && !s14_ai_active(units+512));units[512+0x10]=0;s14_ai_forget(units+512);group[0x10]=3;
    S14AiDraw d;s14_ai_prepare(254,0,&d);assert(d.sequence==251);
    ai_create_test=0;s14_ai_configure(0);s14_ai_attach(image,root,0);putptr(w+0x85130,0);putptr(w+0xde40+16,0);
}
static void icon_detail_tests(void){
    unsigned char dialog[0x330]={0},body[0x200]={0},sprite[0x280]={0};uintptr_t pages[2]={(uintptr_t)detail_page,0},nodes[41]={0},entry[2]={(uintptr_t)dialog,0},table[1]={(uintptr_t)entry};unsigned index=0;
    frame.visible=0;putptr(dialog,image+0x137e970);putint(dialog+0x40,3);putptr(dialog+0x2e0,(uintptr_t)body);
    putptr(body,image+0x1351680);putint(body+0x40,3);putptr(body+0x130,(uintptr_t)pages);putint(body+0x138,2);putptr(body+0x140,(uintptr_t)(units+512));
    putint(detail_page+0x40,3);putptr(detail_page+0x130,(uintptr_t)nodes);putptr(sprite,image+0x15602f8);nodes[40]=(uintptr_t)sprite;
    float local[]={352,-34};memcpy(sprite+0x130,local,8);putword(sprite+0xf0,24);putword(sprite+0xf2,24);int pos[]={835,220,954,630};memcpy(detail_page+0x20,pos,16);
    putptr((void*)(image+0x201c320+0x40),(uintptr_t)table);putint((void*)(image+0x201c320+0x70),1);putptr((void*)(image+0x19e6390+56),(uintptr_t)&index);
    S14TroopIconFrame f;assert(s14_troop_icon_frame(&f) && f.detail && f.x==1187 && f.y==254 && f.size==24);
    putint(detail_page+0x170,1);assert(!s14_troop_icon_frame(&f));putint(detail_page+0x170,0);
    /* A current registered page is mandatory: no retained rectangle on close. */
    putint(dialog+0x40,0);assert(!detail_icon((uintptr_t)dialog,&f));putint(dialog+0x40,3);
    putptr(body+0x140,(uintptr_t)(units+1024));assert(!detail_icon((uintptr_t)dialog,&f));putptr(body+0x140,(uintptr_t)(units+512));
    local[0]=NAN;memcpy(sprite+0x130,local,8);assert(!detail_icon((uintptr_t)dialog,&f));
    putptr((void*)(image+0x201c320+0x40),0);putptr((void*)(image+0x19e6390+56),0);putptr(detail_page+0x130,0);
}
static void native_ui_tests(void){
    bind_hook(tab,packet);assert(configured==1 && text_calls==1 && native_slot.wrapper==(uintptr_t)wrappers[2]);
    assert(integer((uintptr_t)wrappers[2]+0x80)==0 && integer((uintptr_t)inners[2]+0x80)==0 && (integer((uintptr_t)wrappers[2]+0x40)&1));
    for(int i=0;i<100;i++)tick_hook(state);assert(configured==1 && text_calls==1);
    assert(capacity_hook(state,0)==5000 && capacity_row==0);
    uintptr_t ev[2]={0,(uintptr_t)tab},ref=(uintptr_t)inners[2];mouse_click(2);assert(selection[254] && integer((uintptr_t)tab+0x174)==1 && ((int*)packet)[3]==1 && ((int*)packet)[2]==1000);
    assert(capacity_hook(state,0)==1000 && capacity_row==0 && GetLastError()==879);native_maximum=600;assert(capacity_hook(state,0)==600);native_maximum=5000;assert(capacity_hook(state,-1)==0);
    /* Model numeric input / maximize attempting to exceed the selected cap. */
    putint(tab+0x170,5000);native_refresh(tab);native_copy(state,NULL);assert(integer((uintptr_t)tab+0x170)==1000 && ((int*)packet)[2]==1000);
    assert(integer((uintptr_t)inners[2]+0x188)==1 && integer((uintptr_t)inners[0]+0x188)==0 && integer((uintptr_t)inners[1]+0x188)==0);
    formation_hook(ev,&ref);assert(selection[254] && integer((uintptr_t)inners[2]+0x188)==1);assert(formation_calls==2);
    assert(funds_hook(state)==8050 && funds_calls==1 && GetLastError()==888 && gold_calls==0 && integer((uintptr_t)city+0x34)==10600);
    putint(packet+8,1000);putint(tab+0x170,1000);assert(funds_hook(state)==8050);putint(city+0x34,2400);assert(funds_hook(state)==-150 && gold_calls==0);putint(city+0x34,10600);
    mouse_click(1);assert(!selection[254] && ((int*)packet)[3]==2 && integer((uintptr_t)inners[1]+0x188)==1 && integer((uintptr_t)inners[2]+0x188)==0);assert(funds_hook(state)==10550);
    assert(capacity_hook(state,0)==5000);mouse_click(0);assert(!selection[254] && ((int*)packet)[3]==1 && integer((uintptr_t)inners[0]+0x188)==1);
    ref=(uintptr_t)inners[2];formation_hook(ev,&ref);assert(selection[254]);bind_hook(tab,packet);assert(selection[254] && integer((uintptr_t)inners[2]+0x188)==1 && configured==2);
    s14_troop_configure(0);tick_hook(state);assert(!native_slot.wrapper && !selection[254] && !(integer((uintptr_t)wrappers[2]+0x40)&1) && integer((uintptr_t)inners[0]+0x188)==1 && funds_hook(state)==10550);
    s14_troop_configure(1);tick_hook(state);assert(native_slot.wrapper && !selection[254]);
    /* A real native third option on another commander is never replaced. */
    bind_native_third=1;putint(packet,255);int count=configured;bind_hook(tab,packet);assert(configured==count && !native_slot.wrapper && integer((uintptr_t)wrappers[2]+0x80)==3 && (integer((uintptr_t)wrappers[2]+0x40)&1));
    bind_native_third=0;putint(packet,254);bind_hook(tab,packet);assert(native_slot.wrapper);exit_hook(state,NULL);assert(!native_slot.wrapper && !(integer((uintptr_t)wrappers[2]+0x40)&1) && integer((uintptr_t)wrappers[2]+0x80)==0);
    bind_hook(tab,packet);S14TroopFrame f;s14_troop_frame(&f);RECT rect,client={0,0,1920,1080};assert(hint_rect(&f,96,&client,(POINT){1170,580},&rect) && rect.bottom<574 && rect.right<=1920);assert(!hint_rect(&f,96,&client,(POINT){1100,580},&rect));
    client=(RECT){0,0,3840,2160};assert(hint_rect(&f,192,&client,(POINT){2340,1160},&rect) && rect.right<=3840);
    f.x=1820;f.y=20;client=(RECT){0,0,1920,1080};assert(hint_rect(&f,96,&client,(POINT){1825,25},&rect) && rect.left>=0 && rect.right<=1920 && rect.top>=106 && rect.bottom<=1080);
    assert(procedure(NULL,WM_NCHITTEST,0,0)==HTTRANSPARENT && procedure(NULL,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE);
    exit_hook(state,NULL);refresh_calls=0;
}
int main(void){
    environment();native_ui_tests();bind_hook(tab,packet);S14TroopFrame f;s14_troop_frame(&f);assert(f.visible && !f.selected && f.extra_cost==2500 && f.max_soldiers==1000 && f.x==1167 && f.y==574);
    s14_troop_request(&f);tick_hook(state);s14_troop_frame(&f);assert(f.selected && refresh_calls==1);assert(preview_hook(packet)==9 && !preview_definition && !preview_army);
    S14TroopIconFrame icon;assert(s14_troop_icon_frame(&icon) && icon.x==1217 && icon.y==603 && icon.size==48 && icon.selected && !icon.detail);
    s14_troop_configure(0);assert(!s14_troop_icon_frame(&icon));s14_troop_configure(1);
    preview_image(&f);
    assert(confirm_hook(state,button)==42 && confirm_calls==1 && gold_calls==0);assert(live.pending[0].active);
    assert(create_hook(packet)==(uintptr_t)(units+512));assert(GetLastError()==777 && gold_calls==1 && integer((uintptr_t)city+0x34)==8100 && s14_troop_bound(units+512));
    /* Exact in-game regression: native creation returns home 0, then caller
       fills 13, while serial and slot stay unchanged. Never lose identity. */
    putword(units+512+0x14,13);assert(s14_troop_bound(units+512));
    icon_detail_tests();
    original_break=break_mock;original_query=query_mock;native_abnormal=abnormal_mock;immunity_tests();
    assert(break_hook(units+512,(void*)3,1,4)==24 && GetLastError()==891 && break_hook(units+512,(void*)3,0,4)==20);
    for(int i=0;i<100;i++)assert(s14_troop_attribute(units+512,0,1,200)==800 && s14_troop_attribute(units+512,0,0,200)==200);
    assert(detail_hook(detail_page,units+512)==17 && GetLastError()==987 && !wcscmp(detail_caption,L"陷阵营") && detail_calls==1 && detail_text_calls==1);
    putint(detail_page+0x170,1);detail_hook(detail_page,units+512);assert(!wcscmp(detail_caption,L"走舸") && detail_text_calls==1);putint(detail_page+0x170,0);
    s14_troop_configure(0);detail_hook(detail_page,units+512);assert(!wcscmp(detail_caption,L"大戟") && detail_text_calls==1);s14_troop_configure(1);
    detail_hook(detail_page,units+1024);assert(!wcscmp(detail_caption,L"大戟") && detail_text_calls==1);
    putptr(detail_page,image+0x137ebf1);detail_hook(detail_page,units+512);assert(!wcscmp(detail_caption,L"大戟") && detail_text_calls==1);putptr(detail_page,image+0x137ebf0);detail_hook(detail_page,units+512);assert(!wcscmp(detail_caption,L"陷阵营") && detail_text_calls==2);
    assert(fabsf(s14_troop_attribute(units+512,0,1,200)-800)<.001f);assert(fabsf(s14_troop_attribute(units+512,1,1,100)-110)<.001f);assert(s14_troop_attribute(units+512,2,1,20)==24);assert(fabsf(s14_troop_attribute(units+512,4,1,100)-300)<.001f);
    assert(s14_troop_attribute(units+512,0,0,200)==200 && s14_troop_attribute(units+512,0,2,200)==200);assert(s14_troop_damage(units+512,100)==20 && s14_troop_damage(units+512,1)==1 && s14_troop_damage(units+512,-100)==-100);
    assert(fabsf(mobility_hook(units+512,(void*)3,1,4,(void*)5)-29.6f)<.001f && GetLastError()==889);assert(mobility_hook(units+512,(void*)3,0,4,(void*)5)==37);
    s14_troop_configure(0);assert(!s14_troop_bound(units+512) && s14_troop_attribute(units+512,0,1,100)==100);s14_troop_configure(1);assert(s14_troop_bound(units+512));
    wchar_t temp[MAX_PATH],root[MAX_PATH];GetTempPathW(MAX_PATH,temp);swprintf(root,MAX_PATH,L"%lsS14-troops-%lu",temp,GetCurrentProcessId());assert(CreateDirectoryW(root,NULL));s14_troop_root(root);unsigned char hash[32]={7},other_hash[32]={8};assert(s14_troop_save((uintptr_t)w,hash));
    s14_troop_load_begin();assert(!s14_troop_bound(units+512));s14_troop_load_end((uintptr_t)w,0,NULL,0);assert(s14_troop_bound(units+512));
    s14_troop_new_game((uintptr_t)w);assert(!s14_troop_bound(units+512));s14_troop_load_begin();s14_troop_load_end((uintptr_t)w,1,hash,1);assert(s14_troop_bound(units+512));
    assert(!creating_definition && !creating_world && !creating_leader);putword(units+512+0x14,14);assert(s14_troop_bound(units+512));
    putword(units+512+0x28,100);assert(!s14_troop_bound(units+512));s14_troop_load_begin();s14_troop_load_end((uintptr_t)w,1,hash,1);assert(!live.units[1].active);putword(units+512+0x28,1);
    s14_troop_load_begin();s14_troop_load_end((uintptr_t)w,1,other_hash,1);assert(!s14_troop_bound(units+512));
    selection_state=(uintptr_t)state;selection_tab=(uintptr_t)tab;selection[254]=1;putint(city+0x34,2000);assert(confirm_hook(state,button)==0 && confirm_calls==1 && !live.pending[0].active);
    putint(city+0x34,10600);assert(confirm_hook(state,button)==42);putint(city+0x34,100);int old_create=create_calls;assert(!create_hook(packet) && create_calls==old_create && gold_calls==1);
    putint(city+0x34,10600);assert(confirm_hook(state,button)==42);fail_creation=1;assert(!create_hook(packet) && gold_calls==1);fail_creation=0;
    assert(confirm_hook(state,button)==42);selection[254]=0;assert(confirm_hook(state,button)==42 && !live.pending[0].active);assert(create_hook(packet) && !s14_troop_bound(units+512) && gold_calls==1);
    selection[254]=1;uintptr_t ev[2]={0,(uintptr_t)tab},ref=(uintptr_t)button;putint(button+0x80,1);formation_hook(ev,&ref);assert(!selection[254]);putint(button+0x80,5);
    selection[254]=1;assert(confirm_hook(state,button)==42);live.pending[0].revision++;int old_gold_calls=gold_calls;assert(create_hook(packet) && !s14_troop_bound(units+512) && gold_calls==old_gold_calls);
    selection[254]=1;publish((uintptr_t)state);assert(confirm_hook(state,button)==42);assert(create_hook(packet) && s14_troop_bound(units+512));s14_troop_forget(units+512);assert(s14_troop_bound(units+512));units[512+0x10]=0;s14_troop_forget(units+512);assert(!live.units[1].active);
    uintptr_t old_vt=ptr((uintptr_t)state);putptr(state,image+0x133f400);s14_troop_frame(&f);assert(!f.visible);putptr(state,old_vt);
    /* Defense against a queued command or other native entry bypassing UI. */
    int cc=create_calls,gc=gold_calls,nc=confirm_calls;putint(packet+8,1001);assert(!confirm_hook(state,button) && confirm_calls==nc && gold_calls==gc);putint(packet+8,1000);assert(confirm_hook(state,button)==42);putint(packet+8,1001);assert(!create_hook(packet) && create_calls==cc && gold_calls==gc);putint(packet+8,1000);
    ai_creation_bridge_tests(root);
    wchar_t path[MAX_PATH],dir[MAX_PATH];assert(path_for(hash,path));assert(DeleteFileW(path));swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager\\troops",root);assert(RemoveDirectoryW(dir));swprintf(dir,MAX_PATH,L"%ls\\SAN14ModManager",root);assert(RemoveDirectoryW(dir));assert(RemoveDirectoryW(root));
    puts("{\"status\":\"passed\",\"ai_shared_creation_bridge\":true,\"ai_provisional_and_committed_consistent\":true,\"mutable_home_identity\":true,\"creation_scoped_attributes\":true,\"break_abi\":true,\"confusion_refusal\":true,\"native_confusion_clear\":true,\"other_status_timers_preserved\":true,\"surround_penalty_only\":true,\"disable_restores_native\":true,\"deployment_soldier_cap\":1000,\"capacity_row_abi\":true,\"queued_cap_guard\":true,\"native_detail_caption\":true,\"native_button\":true,\"mouse_press_survives_frames\":true,\"native_radio_selection\":true,\"native_funds_preview\":true,\"native_widget_restore\":true,\"other_commanders_unchanged\":true,\"no_per_frame_text_allocation\":true,\"hover_only_hint\":true,\"click_through_hint\":true,\"preview_scoped\":true,\"actual_attributes_only\":true,\"mobility_abi\":true,\"one_time_charge\":true,\"insufficient_funds\":true,\"cancel_or_ordinary_safe\":true,\"unit_reuse_safe\":true,\"save_load\":true,\"failed_load_restores\":true,\"ui_context_guard\":true}");return 0;
}
