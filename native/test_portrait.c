#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
#include "portrait.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void store(unsigned char *p,unsigned int v){memcpy(p,&v,4);}
static int bitmap(const wchar_t *path,uint32_t *pixels){FILE *f=_wfopen(path,L"wb");if(!f)return 0;BITMAPFILEHEADER h={.bfType=0x4d42,.bfSize=54+128*128*4,.bfOffBits=54};BITMAPINFOHEADER b={.biSize=40,.biWidth=128,.biHeight=-128,.biPlanes=1,.biBitCount=32};int ok=fwrite(&h,14,1,f)==1&&fwrite(&b,40,1,f)==1&&fwrite(pixels,128*128*4,1,f)==1;fclose(f);return ok;}
int wmain(int argc,wchar_t **argv){
    uint32_t pixels[128*128];unsigned char data[72]={0};memcpy(data,"GT1G",4);store(data+12,32);store(data+16,1);store(data+32,4);data[37]=0x59;data[38]=0x22;data[44]=0;data[45]=0xf8;
    assert(s14_portrait_decode(data,52,pixels));for(int i=0;i<128*128;i++)assert(pixels[i]==0xffff0000u);
    assert(!s14_portrait_decode(data,43,pixels));store(data+16,2);assert(!s14_portrait_decode(data,52,pixels));store(data+16,1);
    data[37]=0x5b;data[44]=255;data[45]=255;memset(data+46,0,6);data[52]=0;data[53]=0xf8;assert(s14_portrait_decode(data,60,pixels));assert(pixels[0]==0xffff0000u);
    data[43]=1;store(data+44,1000);assert(!s14_portrait_decode(data,60,pixels));data[43]=0;data[38]=0xff;assert(!s14_portrait_decode(data,60,pixels));
    uintptr_t base=(uintptr_t)VirtualAlloc(NULL,0x2000000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE),world=(uintptr_t)calloc(1,0xc000),person=(uintptr_t)calloc(1,0xe0);assert(base&&world&&person);
    memcpy((void*)(base+0x1fc91d0),&world,8);memcpy((void*)(world+0x148+518*8),&person,8);uintptr_t vt=base+0x12a00d0;memcpy((void*)person,&vt,8);unsigned short id=518,face=24;memcpy((void*)(person+0x10),&id,2);memcpy((void*)(person+0xde),&face,2);
    assert(s14_portrait_identity(base,world,518)==24);assert(s14_portrait_identity(base,world,0)==-1);assert(s14_portrait_identity(base,world+8,518)==-1);assert(s14_portrait_identity(base,world,511)==-1);vt=0;memcpy((void*)person,&vt,8);assert(s14_portrait_identity(base,world,518)==-1);VirtualFree((void*)base,0,MEM_RELEASE);free((void*)world);free((void*)person);
    int extracted=0;if(argc==3){const int faces[]={24,47,87,217,655};for(int i=0;i<5;i++){assert(s14_portrait_extract(argv[1],faces[i],pixels));wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\native-face-%04d.bmp",argv[2],faces[i]);assert(bitmap(path,pixels));extracted++;}}
    printf("{\"status\":\"passed\",\"malformed_resource_bounds\":true,\"dxt1_dxt5\":true,\"person_id_separate_from_portrait\":true,\"scene_and_record_guards\":true,\"local_portraits_extracted\":%d,\"gameplay_modified\":false}\n",extracted);return 0;
}
