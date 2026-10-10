#define WIN32_LEAN_AND_MEAN
#include "map_effects_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"FAILED %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {unsigned char *p;size_t n;} Block;
static Block blocks[80];static int block_count,world_reads,race,scene_reads,scene_race;static uintptr_t base;
static void *alloc(size_t n){unsigned char *p=calloc(1,n);REQUIRE(p && block_count<80);blocks[block_count++]=(Block){p,n};return p;}
static void putptr(void *p,int o,const void *v){uintptr_t n=(uintptr_t)v;memcpy((char*)p+o,&n,8);}
static void putword(void *p,int o,unsigned int v){memcpy((char*)p+o,&v,4);}
static void putshort(void *p,int o,unsigned short v){memcpy((char*)p+o,&v,2);}
static int read_memory(void *ctx,uintptr_t at,void *out,size_t n){
    (void)ctx;if(at==base+0x1fc91d0){world_reads++;if(race && world_reads>=2){uintptr_t zero=0;memcpy(out,&zero,8);return 1;}}
    if(at==base+0x19e6330 && ++scene_reads==scene_race){memset(out,0,n);return 1;}
    for(int i=0;i<block_count;i++)if(at>=(uintptr_t)blocks[i].p && at-(uintptr_t)blocks[i].p<=blocks[i].n && n<=blocks[i].n-(at-(uintptr_t)blocks[i].p)){memcpy(out,(void*)at,n);return 1;}return 0;
}
static unsigned char *widget(unsigned int vt,void *parent,int x,int y,int w,int h){
    unsigned char *p=alloc(0x400);uintptr_t v=base+vt;memcpy(p,&v,8);putptr(p,8,parent);int xy[4]={x,y,w,h};memcpy(p+0x20,xy,16);putword(p,0x40,3);return p;
}
static unsigned char *node(int w,int h,float x,float y,void *parent){
    unsigned char *n=alloc(0x118);float *m=alloc(64);m[0]=m[5]=m[10]=m[15]=1;m[12]=x;m[13]=y;
    putptr(n,0xd0,parent);putptr(n,0xe0,m);putptr(n,0xe8,m);putshort(n,0xf0,w);putshort(n,0xf2,h);float s=1;memcpy(n+0xf4,&s,4);memcpy(n+0xf8,&s,4);return n;
}
static void list(unsigned char *table,int index,void **objects,int n){
    void *next=NULL;for(int i=n-1;i>=0;i--){void *link=alloc(16);putptr(link,0,objects[i]);putptr(link,8,next);next=link;}putptr(table,index*8,next);
}
static int everyone(void *ctx,uintptr_t world,uintptr_t army,int leader){(void)ctx;(void)world;(void)army;return leader==518?1:2;}
static void preview(const char *path){
    int w=710,h=410;BITMAPINFO info={.bmiHeader={sizeof(BITMAPINFOHEADER),w,-h,1,32,BI_RGB}};unsigned int *pixels=NULL;HDC dc=CreateCompatibleDC(NULL);REQUIRE(dc);HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);REQUIRE(bmp);HGDIOBJ old=SelectObject(dc,bmp);
    RECT all={0,0,w,h};HBRUSH bg=CreateSolidBrush(RGB(249,248,244));FillRect(dc,&all,bg);DeleteObject(bg);
    unsigned int halo[176*176]={0};s14_map_halo_pixels(halo,176,176,24);
    for(int y=0;y<176;y++)for(int x=0;x<176;x++){
        unsigned int v=halo[y*176+x],a=v>>24;unsigned int r=(v>>16)&255,g=(v>>8)&255,b=v&255;
        pixels[(y+72)*w+x+44]=((r+249*(255-a)/255)<<16)|((g+248*(255-a)/255)<<8)|(b+244*(255-a)/255);
    }
    HBRUSH disk=CreateSolidBrush(RGB(212,176,113));HGDIOBJ ob=SelectObject(dc,disk),op=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,68,96,196,224);SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(disk);
    HFONT fonts[3];for(int i=0;i<3;i++)fonts[i]=CreateFontW(i==0?-32:i==1?-36:-24,0,0,0,i?FW_NORMAL:FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    SetViewportOrgEx(dc,316,30,NULL);s14_map_card_paint(dc,348,348,192,fonts[0],fonts[1],fonts[2]);SetViewportOrgEx(dc,0,0,NULL);GdiFlush();
    FILE *f=fopen(path,"wb");REQUIRE(f);BITMAPFILEHEADER header={.bfType=0x4d42,.bfSize=54+w*h*4,.bfOffBits=54};REQUIRE(fwrite(&header,sizeof(header),1,f)==1 && fwrite(&info.bmiHeader,40,1,f)==1 && fwrite(pixels,w*h*4,1,f)==1);fclose(f);
    for(int i=0;i<3;i++)DeleteObject(fonts[i]);SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
}
int main(int argc,char **argv){
    base=(uintptr_t)alloc(0x2020000);unsigned char *world=alloc(0x7f000),*table=alloc(64*8),*map_handle=alloc(4),*detail_handle=alloc(4);
    putptr((void*)base,0x1fc91d0,world);putptr((void*)base,0x201c358,world);putptr((void*)base,0x201c360,table);putword((void*)base,0x201c390,64);
    putword(map_handle,0,19);putword(detail_handle,0,22);putptr((void*)base,0x19e6398,map_handle);putptr((void*)base,0x19e63c8,detail_handle);
    const unsigned char bind[]={0x48,0x89,0x91,0xe0,1,0,0},size[]={0x8b,0x80,0xf0,0,0,0};memcpy((void*)(base+0x194870),bind,sizeof(bind));memcpy((void*)(base+0x82e1e3),size,sizeof(size));
    const unsigned char state_manager[]={0x48,0x8d,0x05,0x35,0x6d,0x9d,1},state_top[]={0x48,0x8b,0x50,0x10,0x48,0x8b,0x48,0x20};
    memcpy((void*)(base+0xf5f4),state_manager,sizeof(state_manager));memcpy((void*)(base+0x509bed),state_top,sizeof(state_top));
    putptr((void*)base,0x12cc498+40,(void*)(base+0x3f8ef0));putptr((void*)base,0x12cc760+40,(void*)(base+0x3f8200));REQUIRE(s14_map_validate(read_memory,NULL,base));
    unsigned char *state=alloc(0x480),*other_state=alloc(0x480);void **stack=alloc(16*8);stack[0]=state;stack[1]=other_state;
    putptr((void*)base,0x19e6340,(void*)1);putptr((void*)base,0x19e6348,(void*)16);putptr((void*)base,0x19e6350,stack);
    putptr(state,0,(void*)(base+0x12cc498));putword(state,0x6c,3);putword(state,0x470,2);
    putptr(other_state,0,(void*)(base+0x132c028));putword(other_state,0x6c,3);
    unsigned char *pool=alloc(501*512),*army=pool+29*512;putptr(world,0x7df60,pool);putptr(world,0x7df60+29*8,army);uintptr_t vt=base+0x123e288;memcpy(army,&vt,8);army[0x10]=1;putshort(army,0x12,518);putshort(army,0x16,4429);
    unsigned char *map=widget(0x1298818,NULL,0,0,1920,1080),*root=widget(0x1293950,map,783,589,78,80),*normal=widget(0x12930b8,root,0,0,78,80),*action=widget(0x1293070,normal,-104,-56,288,140),*face=widget(0x1292750,action,111,56,64,75);putptr(root,0x1e0,army);
    unsigned char *face0=node(64,75,-170,-49,NULL),*face1=node(44,44,-160,-59,face0),*face2=node(64,64,-170,-49,face1);void **face_nodes=alloc(24);face_nodes[0]=face0;face_nodes[1]=face1;face_nodes[2]=face2;putptr(face,0x130,face_nodes);putword(face,0x138,3);
    unsigned char *summary=widget(0x138beb0,NULL,24,284,0,0),*basic=widget(0x138b9e8,summary,0,0,220,508),*traits=widget(0x138d428,NULL,244,284,136,92);putptr(basic,0x168,army);putptr(traits,0x168,army);
    unsigned char *traits0=node(136,92,-716,256,NULL),*traits1=node(136,206,-716,256,traits0);void **trait_nodes=alloc(16);trait_nodes[0]=traits0;trait_nodes[1]=traits1;putptr(traits,0x130,trait_nodes);putword(traits,0x138,2);
    unsigned char *dialog=widget(0x137e970,NULL,0,0,1200,600);putword(dialog,0x40,2);
    void *map_objects[]={root,face},*detail_objects[]={basic,traits,dialog};list(table,19,map_objects,2);list(table,22,detail_objects,3);
    S14MapCache cache={0};S14MapFrame f;REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,1,&f) && f.halo && f.tooltip && f.army_id==29 && f.discovered);
    REQUIRE(f.portrait.left==790 && f.portrait.top==589 && f.portrait.right==854 && f.card.left==244 && f.card.top==500);
    S14MapFrame binding=f,present;float *face_matrix=(void*)(uintptr_t)(*(uintptr_t*)(face2+0xe0));
    REQUIRE(binding.sampled_at==1 && binding.registry_table==(uintptr_t)table && binding.detail_handle==(uintptr_t)detail_handle);
    // Menus can change between capture and Present, before the worker discovers
    // the new dialog object. The context guard must work with empty modal caches.
    const uintptr_t no_dialogs[2]={0,0};putword(detail_handle,0,23);
    REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));putword(detail_handle,0,22);
    uintptr_t saved_head=*(uintptr_t*)(table+22*8);putptr(table,22*8,NULL);
    REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));putptr(table,22*8,(void*)saved_head);
    S14MapFrame modal_binding=binding;modal_binding.modal=1;REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&modal_binding,no_dialogs,&present));
    const uintptr_t bad_dialogs[2]={0xdeadbeef,0};REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,bad_dialogs,&present));
    // The map stays rendered behind arbitrary menus and blurred dialogs. Even
    // without any cached modal objects, a non-map top state blocks every frame.
    const unsigned int menus[]={0x132c028,0x137d3c8,0x132c100,0x1234567,0x12cc510};
    for(unsigned int i=0;i<sizeof(menus)/sizeof(menus[0]);i++){
        putptr(other_state,0,(void*)(base+menus[i]));putptr((void*)base,0x19e6340,(void*)2);
        REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));
        REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,2+i,&f) && f.modal && !f.halo && !f.tooltip && f.scene.vtable==base+menus[i]);
    }
    putptr((void*)base,0x19e6340,(void*)1);REQUIRE(s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));
    putword(state,0x6c,4);REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));putword(state,0x6c,3);
    putword(state,0x470,6);REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));putword(state,0x470,2);
    // Planning -> execution must acquire a fresh binding, then remain visible
    // during movement and pauses. A late stack transition is rejected too.
    putptr(state,0,(void*)(base+0x12cc760));REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));
    REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,10,&f) && f.scene.main_map && f.halo);
    REQUIRE(s14_map_present_capture(read_memory,NULL,base,&f,no_dialogs,&present));putptr(state,0,(void*)(base+0x12cc498));
    scene_reads=0;scene_race=3;REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,no_dialogs,&present));scene_race=0;
    scene_reads=0;scene_race=2;REQUIRE(!s14_map_capture(read_memory,NULL,base,&cache,11,&f));scene_race=0;
    // While a camera moves, the layout headers and other cached transforms can
    // belong to different update phases. Present uses only the actual face.
    for(int i=0;i<1024;i++){
        face_matrix[12]=-170+i*.25f;face_matrix[13]=-49+i*.125f;
        REQUIRE(s14_map_present_capture(read_memory,NULL,base,&binding,cache.dialogs,&present));
        REQUIRE(fabsf(present.center_x-(822+i*.25f))<.001f && fabsf(present.center_y-(621-i*.125f))<.001f && present.calls<60);
    }
    face_matrix[12]=-170;face_matrix[13]=-49;
    float zoom=1.28f;memcpy(face2+0xf4,&zoom,4);memcpy(face2+0xf8,&zoom,4);REQUIRE(s14_map_present_capture(read_memory,NULL,base,&binding,cache.dialogs,&present) && fabsf(present.radius-40.96f)<.001f);
    zoom=1;memcpy(face2+0xf4,&zoom,4);memcpy(face2+0xf8,&zoom,4);
    REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,34,&f) && !f.discovered);unsigned fast_calls=f.calls,fast_bytes=f.bytes;REQUIRE(fast_calls<100 && fast_bytes<6000);
    putshort(traits1,0xf2,54);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,68,&f) && f.card.top==348);
    putptr(basic,0x168,pool+512);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,101,&f) && !f.tooltip && f.halo);putptr(basic,0x168,army);
    putword(basic,0x40,2);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,134,&f) && !f.tooltip);putword(basic,0x40,3);
    putword(dialog,0x40,3);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,167,&f) && f.modal && !f.halo && !f.tooltip);putword(dialog,0x40,2);
    putword(dialog,0x40,3);REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,cache.dialogs,&present));putword(dialog,0x40,2);
    putshort(army,0x12,511);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,200,&f) && !f.halo && !f.tooltip);putshort(army,0x12,518);
    putshort(army,0x12,511);REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&binding,cache.dialogs,&present));putshort(army,0x12,518);
    putptr(world,0x7df60+29*8,pool+512);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,233,&f) && !f.halo);putptr(world,0x7df60+29*8,army);
    putshort(army,0x16,0);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,266,&f) && !f.halo);putshort(army,0x16,4429);
    putword(normal,0x40,2);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,299,&f) && !f.halo);putword(normal,0x40,3);
    float *matrix=(void*)(uintptr_t)(*(uintptr_t*)(face0+0xe8));matrix[12]+=100;REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,332,&f) && !f.halo);matrix[12]-=100;
    matrix[12]=NAN;REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,365,&f) && !f.halo);matrix[12]=-170;
    world_reads=0;race=1;REQUIRE(!s14_map_capture(read_memory,NULL,base,&cache,398,&f));race=0;
    putptr(root,0x1e0,NULL);REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,431,&f) && !f.halo && !f.tooltip);putptr(root,0x1e0,army);
    REQUIRE(s14_map_capture(read_memory,NULL,base,&cache,6000,&f) && f.discovered && f.halo);
    S14MapEffectsUI ui={.frame=f};s14_map_ui_tick(&ui,NULL,NULL,base,1,3,1,6001);REQUIRE(!ui.frame.army && !ui.frame.face && !ui.frame.sampled_at);
    RECT scaled;int scale;REQUIRE(s14_map_scale(&(RECT){790,589,854,653},3840,2160,&scaled,&scale) && scale==192 && scaled.left==1580);
    REQUIRE(s14_map_scale(&(RECT){790,589,854,653},2560,1080,&scaled,&scale) && scale==96 && scaled.left==1110);REQUIRE(!s14_map_scale(&(RECT){-1,0,64,64},1920,1080,&scaled,&scale));
    RECT card;REQUIRE(s14_map_card(&(RECT){24,284,244,1000},&(RECT){244,850,380,1030},&card) && card.left==392 && card.top==850);REQUIRE(!s14_map_card(&(RECT){1700,800,1910,1060},NULL,&card));
    unsigned int pixels[88*88]={0};s14_map_halo_pixels(pixels,88,88,12);REQUIRE(pixels[44*88+44]==0);int visible_pixels=0,soft=0;
    for(int i=0;i<88*88;i++){unsigned a=pixels[i]>>24;REQUIRE(((pixels[i]>>16)&255)<=a && ((pixels[i]>>8)&255)<=a && (pixels[i]&255)<=a);visible_pixels+=a!=0;soft+=a>0 && a<100;}
    REQUIRE(visible_pixels>500 && soft>100);preview(argc>1?argv[1]:"map-effects-preview.bmp");
    /* Two different native portraits, red qualification and independent live
       matrices; menu and reused army identities must still reject both. */
    unsigned char *second=pool+30*512;putptr(world,0x7df60+30*8,second);putptr(second,0,(void*)(base+0x123e288));second[0x10]=1;putshort(second,0x12,511);putshort(second,0x16,1000);putshort(second,0x28,42);
    unsigned char *root2=widget(0x1293950,map,983,589,78,80),*normal2=widget(0x12930b8,root2,0,0,78,80),*action2=widget(0x1293070,normal2,-104,-56,288,140),*faceb=widget(0x1292750,action2,111,56,64,75);putptr(root2,0x1e0,second);
    unsigned char *b0=node(64,75,30,-49,NULL),*b1=node(44,44,40,-59,b0),*b2=node(64,64,30,-49,b1);void **nodes2=alloc(24);nodes2[0]=b0;nodes2[1]=b1;nodes2[2]=b2;putptr(faceb,0x130,nodes2);putword(faceb,0x138,3);
    void *many[]={root,face,root2,faceb};list(table,19,many,4);cache.initialized=0;S14MapBatch batch;
    REQUIRE(s14_map_capture_all(read_memory,NULL,base,&cache,7000,&f,&batch,everyone,NULL) && batch.count==2);
    REQUIRE(batch.frames[0].effect==1 && batch.frames[1].effect==2 && batch.frames[1].native_serial==42);
    REQUIRE(s14_map_present_capture(read_memory,NULL,base,&batch.frames[1],cache.dialogs,&present) && present.center_x>1000);
    float *m2=(void*)(uintptr_t)(*(uintptr_t*)(b2+0xe0));m2[12]+=.25f;
    REQUIRE(s14_map_present_capture(read_memory,NULL,base,&batch.frames[1],cache.dialogs,&present) && fabsf(present.center_x-1022.25f)<.01f);
    putshort(second,0x28,43);REQUIRE(!s14_map_present_capture(read_memory,NULL,base,&batch.frames[1],cache.dialogs,&present));putshort(second,0x28,42);
    putptr((void*)base,0x19e6340,(void*)2);REQUIRE(s14_map_capture_all(read_memory,NULL,base,&cache,7001,&f,&batch,everyone,NULL) && f.modal && !batch.count);
    for(int i=0;i<block_count;i++)free(blocks[i].p);
    printf("{\"status\":\"passed\",\"main_map_positive_allowlist\":true,\"unknown_screens_blocked\":true,\"same_registry_menu_blocks_present\":true,\"scene_transition_race_guard\":true,\"planning_and_execution_supported\":true,\"present_movement_frames\":1024,\"fractional_coordinates\":true,\"present_geometry_single_source\":true,\"native_ui_context_change_blocks_frame\":true,\"invalid_modal_pointer_blocks_frame\":true,\"canonical_commander_only\":true,\"hover_selected_identity\":true,\"dynamic_personality_height\":true,\"dialog_and_hidden_guards\":true,\"world_race_guard\":true,\"cached_geometry_freshness\":true,\"fast_read_calls\":%u,\"fast_read_bytes\":%u,\"transparent_portrait_center\":true,\"premultiplied_soft_halo\":true,\"native_calls\":0}\n",fast_calls,fast_bytes);return 0;
}
