#include "detail_model.h"
#include "career_affix.h"
#include <string.h>
#include <wchar.h>
#include <limits.h>
#define DETAIL_VT 0x13812f8
#define TAB_VT 0x1351680
#define PERSON_VT 0x12a00d0
#define WORLD_SLOT 0x1fc91d0
#define CONTROLLER 0x19e6390
#define REGISTRY 0x201c320
typedef struct {S14OfficerRead read;void *context;S14DetailFrame *out;} DetailReader;
static int get(DetailReader *r,uintptr_t at,void *out,size_t n) {
    if(!at || !n || n>8192 || ++r->out->calls>8192 || r->out->bytes+n>262144) return 0;
    r->out->bytes+=(unsigned int)n;return r->read(r->context,at,out,n);
}
static uintptr_t ptr(const unsigned char *p) {uintptr_t n;memcpy(&n,p,8);return n;}
static unsigned int word(const unsigned char *p) {unsigned int n;memcpy(&n,p,4);return n;}
static unsigned short half(const unsigned char *p) {unsigned short n;memcpy(&n,p,2);return n;}
int s14_detail_validate(S14OfficerRead read,void *context,uintptr_t base) {
    // Read-only adapter anchors: native selected-data assignment, basic-page
    // person assignment, and the native UI-controller singleton getter.
    const unsigned char selected[]={0x48,0x8b,0x87,0xe0,0x02,0x00,0x00,0x48,0x89,0x98,0x40,0x01,0x00,0x00};
    const unsigned char person[]={0x48,0x89,0x91,0x68,0x01,0x00,0x00};
    const unsigned char singleton[]={0x48,0x8d,0x05,0xb5,0x6e,0x9d,0x01};
    unsigned char b[16];return read && base && read(context,base+0x58bae9,b,sizeof(selected)) && !memcmp(b,selected,sizeof(selected)) &&
        read(context,base+0x80a264,b,sizeof(person)) && !memcmp(b,person,sizeof(person)) &&
        read(context,base+0xf4d4,b,sizeof(singleton)) && !memcmp(b,singleton,sizeof(singleton));
}
static int candidate(DetailReader *r,uintptr_t base,uintptr_t object,uintptr_t world,S14DetailFrame *found) {
    unsigned char dlg[0x330],tab[0x148],basic[0x170],person[64];uintptr_t canonical=0,page=0;
    if(!get(r,object,dlg,sizeof(dlg)) || ptr(dlg)!=base+DETAIL_VT || (word(dlg+0x40)&3)!=3) return 0;
    uintptr_t body=ptr(dlg+0x2e0);if(!get(r,body,tab,sizeof(tab)) || ptr(tab)!=base+TAB_VT || (word(tab+0x40)&3)!=3) return 0;
    uintptr_t selected=ptr(tab+0x140),array=ptr(tab+0x130);unsigned int count=word(tab+0x138);
    if(!selected || !array || count!=3 || !get(r,array,&page,8) || !get(r,page,basic,sizeof(basic)) || ptr(basic)!=base+0x1381700 || ptr(basic+0x168)!=selected) return 0;
    if(!get(r,selected,person,sizeof(person)) || ptr(person)!=base+PERSON_VT) return 0;
    int id=half(person+0x10);if(id<=0 || id>=S14_STATS_MAX || !get(r,world+0x148+(uintptr_t)id*8,&canonical,8) || canonical!=selected) return 0;
    memcpy(&found->panel,dlg+0x20,sizeof(RECT)); // Native x,y,width,height.
    long x=found->panel.left,y=found->panel.top,w=found->panel.right,h=found->panel.bottom;
    if(x<0 || y<0 || w<800 || w>1920 || h<300 || h>1080 || x+w>1920 || y+h>1080) return 0;
    found->panel.right=x+w;found->panel.bottom=y+h;found->world=world;found->dialog=object;found->person=selected;found->officer_id=id;
    unsigned int length=0;for(int group=0;group<2;group++) for(int i=0;i<9 && length<23;i++) {wchar_t c=(wchar_t)half(person+0x12+group*18+i*2);if(!c) break;found->name[length++]=c;}found->name[length]=0;
    wchar_t titled[24];s14_affix_display(world,id,found->name,titled,24);if(titled[0])wcscpy(found->name,titled);
    if(!length) return 0;
    // Close/switch/load races fail closed; no stale label is retained.
    unsigned char again[0x148],header[0x48];uintptr_t after_world=0;
    if(!get(r,body,again,sizeof(again)) || ptr(again)!=base+TAB_VT || ptr(again+0x140)!=selected ||
       !get(r,object,header,sizeof(header)) || ptr(header)!=base+DETAIL_VT || (word(header+0x40)&3)!=3 ||
       !get(r,base+WORLD_SLOT,&after_world,8) || after_world!=world) return 0;
    return 1;
}
int s14_detail_capture(S14OfficerRead read,void *context,uintptr_t base,S14DetailFrame *out) {
    if(!out) return 0;memset(out,0,sizeof(*out));if(!read || !base) return 0;DetailReader r={read,context,out};
    unsigned char registry[0x80],controller[0x194],after[0x80];uintptr_t world=0;
    if(!get(&r,base+WORLD_SLOT,&world,8) || !world || !get(&r,base+REGISTRY,registry,sizeof(registry)) || !get(&r,base+CONTROLLER,controller,sizeof(controller))) return 0;
    uintptr_t table=ptr(registry+0x40),handle=ptr(controller+3*16+8);unsigned int count=word(registry+0x70),index=0;
    if(!ptr(registry+0x38) || !table || !count || count>0x40000 || !handle || !get(&r,handle,&index,4) || index>=count) return 0;
    uintptr_t node=0; if(!get(&r,table+(uintptr_t)index*8,&node,8)) return 0;
    int matches=0;unsigned int n=0;S14DetailFrame selected={0};
    for(;node && n<2048;n++) {
        uintptr_t raw[2],vt=0;if(!get(&r,node,raw,16) || raw[1]==node || !raw[0] || !get(&r,raw[0],&vt,8)) return 0;
        if(vt==base+DETAIL_VT) {S14DetailFrame f={0};if(candidate(&r,base,raw[0],world,&f)) {selected=f;if(++matches>1) return 0;}}
        node=raw[1];
    }
    if(node || matches!=1 || !get(&r,base+REGISTRY,after,sizeof(after)) || memcmp(registry+0x38,after+0x38,16) || word(registry+0x70)!=word(after+0x70)) return 0;
    selected.calls=out->calls;selected.bytes=out->bytes;*out=selected;return 1;
}
int s14_detail_rect(const RECT *p,int width,int height,RECT *bar,int *scale) {
    if(!p || !bar || !scale || width<800 || height<450 || width>16384 || height>16384) return 0;
    int factor=width*96/1920,vertical=height*96/1080;if(vertical<factor) factor=vertical;
    if(factor<40) return 0;int left=(width-MulDiv(1920,factor,96))/2,top=(height-MulDiv(1080,factor,96))/2;
    int x=p->left+MulDiv(p->right-p->left,387,1000),y=p->top-38;
    bar->left=left+MulDiv(x,factor,96);bar->right=left+MulDiv(p->right,factor,96);
    bar->top=top+MulDiv(y,factor,96);bar->bottom=bar->top+MulDiv(76,factor,96);
    if(bar->left<0 || bar->top<0 || bar->right>width || bar->bottom>height || bar->right-bar->left<MulDiv(780,factor,96)) return 0;
    *scale=factor;return 1;
}
