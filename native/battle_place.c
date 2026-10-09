#include "battle_place.h"
#include <string.h>
#include <stdio.h>
static uintptr_t pointer(const unsigned char *p){uintptr_t x;memcpy(&x,p,8);return x;}
static unsigned int word(const unsigned char *p){unsigned short x;memcpy(&x,p,2);return x;}
static void name(wchar_t out[40],const unsigned char *p,int count){int i;for(i=0;i<count && i<39;i++){out[i]=(wchar_t)word(p+i*2);if(!out[i])break;}out[i]=0;}
int s14_place_capture(S14PlaceRead read,void *context,uintptr_t base,uintptr_t world,int tile,S14BattlePlace *out){
    memset(out,0,sizeof(*out));out->tile=tile;out->city_id=out->area_id=-1;
    if(!read || !base || !world || tile<0 || tile>=48400)return 0;
    uintptr_t hex=0,area=0,city=0,again=0;unsigned char h[32],a[56],b[56],c[50];
    if(!read(context,world+0xdfe0+(size_t)tile*8,&hex,8) || !hex || !read(context,hex,h,sizeof(h)) || pointer(h)!=base+0x129f660)return 0;
    int id=(int)word(h+0x12);if(id<1 || id>500)return 0;
    if(!read(context,world+0x6c860+(size_t)id*8,&area,8) || !area || !read(context,area,a,sizeof(a)) || pointer(a)!=base+0x129ff98)return 0;
    // Native hex->area getter 0x2080d0; area city reference +0x35 is also
    // used by 0x20adb0 and 0x20fa30. Do not infer cities from proximity.
    int city_id=a[0x35];if(city_id>51)return 0;
    if(!read(context,area,b,sizeof(b)) || memcmp(a,b,sizeof(a)) ||
       !read(context,world+0xdfe0+(size_t)tile*8,&again,8) || again!=hex)return 0;
    out->area_id=id;name(out->area,a+0x10,9);out->city_id=city_id;
    if(city_id && read(context,world+0xdaa8+(size_t)city_id*8,&city,8) && city && read(context,city,c,sizeof(c)) && pointer(c)==base+0x129fd10)name(out->city,c+0x12,16);
    return out->area[0]!=0;
}
void s14_place_text(const S14BattlePlace *p,wchar_t *out,size_t size){
    if(p->area[0])swprintf(out,size,L"%ls · %ls",p->city[0]?p->city:L"城市未识别",p->area);
    else if(p->tile>=0 && p->tile<48400)swprintf(out,size,L"地区未识别 · (%d, %d)",p->tile%220,p->tile/220);
    else swprintf(out,size,L"地点未记录");
}
