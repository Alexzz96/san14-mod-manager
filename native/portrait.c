#define WIN32_LEAN_AND_MEAN
#include "portrait.h"
#include "vendor/miniz/miniz_tinfl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define PORTRAIT_MAX_ENTRIES 32768
#define PORTRAIT_CACHE 128
#define PORTRAIT_BYTES (16u*1024*1024)
typedef struct {uint32_t id,type,flags;uint64_t size;char address[96];} Resource;
typedef struct {wchar_t root[MAX_PATH];Resource *entries;unsigned int count;int *faces;} Archive;
typedef struct {int id,state;uint64_t used;uint32_t *pixels,*faded;} Face;
struct S14PortraitCache {Archive archive;CRITICAL_SECTION lock;HANDLE wake,thread;volatile LONG stop;Face faces[PORTRAIT_CACHE];uint64_t serial;};
static uint16_t u16(const void *p){uint16_t v;memcpy(&v,p,2);return v;}
static uint32_t u32(const void *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint64_t u64(const void *p){uint64_t v;memcpy(&v,p,8);return v;}
static unsigned char *read_range(const wchar_t *path,uint64_t offset,size_t length){
    if(!length||length>PORTRAIT_BYTES||offset>INT64_MAX-length)return NULL;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_RANDOM_ACCESS,NULL);if(f==INVALID_HANDLE_VALUE)return NULL;
    LARGE_INTEGER size,pos;pos.QuadPart=(LONGLONG)offset;unsigned char *out=NULL;DWORD got;
    if(GetFileSizeEx(f,&size)&&offset+(uint64_t)length<=(uint64_t)size.QuadPart&&SetFilePointerEx(f,pos,NULL,FILE_BEGIN)){
        out=malloc(length);if(out && (!ReadFile(f,out,(DWORD)length,&got,NULL)||got!=length)){free(out);out=NULL;}
    }CloseHandle(f);return out;
}
static unsigned char *load_resource(Archive *a,const Resource *r,size_t *length){
    if(r->size>PORTRAIT_BYTES)return NULL;wchar_t path[MAX_PATH];size_t blob_length=0;uint64_t off=0;unsigned char *blob=NULL;
    if(r->flags&0x10000){
        if(swprintf(path,MAX_PATH,L"%ls\\0002\\data\\0x%08x.file",a->root,r->id)<0)return NULL;
        WIN32_FILE_ATTRIBUTE_DATA st;if(!GetFileAttributesExW(path,GetFileExInfoStandard,&st)||st.nFileSizeHigh||st.nFileSizeLow>PORTRAIT_BYTES)return NULL;
        blob_length=st.nFileSizeLow;
    }else{
        char *p,*end;off=strtoull(r->address,&p,16);if(p==r->address||*p!='@')return NULL;
        unsigned long long size=strtoull(p+1,&end,16);if(end==p+1||size<48||size>PORTRAIT_BYTES)return NULL;blob_length=(size_t)size;p=end;
        char suffix[48]={0};size_t used=0;
        if(*p=='#'){unsigned long index=strtoul(p+1,&end,16);if(end==p+1||index>65535)return NULL;used=(size_t)snprintf(suffix,sizeof(suffix),"%lu",index);p=end;}
        if(*p=='&'){unsigned long sub=strtoul(p+1,&end,16);if(end==p+1||sub>65535||used>=sizeof(suffix))return NULL;snprintf(suffix+used,sizeof(suffix)-used,"_%lu",sub);p=end;}
        if(*p)return NULL;wchar_t wide[48];if(!MultiByteToWideChar(CP_UTF8,0,suffix,-1,wide,48))return NULL;
        if(swprintf(path,MAX_PATH,L"%ls\\0002\\ScreenLayout.rdb.bin%ls",a->root,wide)<0)return NULL;
    }
    if(blob_length<48)return NULL;blob=read_range(path,off,blob_length);if(!blob)return NULL;
    uint64_t entry=u64(blob+8),content=u64(blob+16),size=u64(blob+24);
    if(memcmp(blob,"IDRK0000",8)||blob_length<48||entry>blob_length||content>entry||entry-content<48||size!=r->size||u32(blob+36)!=r->id){free(blob);return NULL;}
    unsigned char *out=malloc((size_t)size? (size_t)size:1);if(!out){free(blob);return NULL;}
    const unsigned char *payload=blob+entry-content;
    if(r->flags&0x100000){
        size_t at=0,written=0;int ok=1;
        while(at+4<=content){size_t n=u32(payload+at);at+=4;if(!n)break;if(n>content-at||written>=size){ok=0;break;}
            size_t got=tinfl_decompress_mem_to_mem(out+written,(size_t)size-written,payload+at,n,TINFL_FLAG_PARSE_ZLIB_HEADER);
            if(got==(size_t)-1||!got||got>size-written){ok=0;break;}written+=got;at+=n;
        }if(!ok||written!=size){free(out);out=NULL;}
    }else if(content!=size){free(out);out=NULL;}else memcpy(out,payload,(size_t)size);
    free(blob);if(out)*length=(size_t)size;return out;
}
static int resource_compare(const void *a,const void *b){uint32_t x=((const Resource*)a)->id,y=((const Resource*)b)->id;return(x>y)-(x<y);}
static Resource *resource(Archive *a,uint32_t id){Resource key={.id=id};return bsearch(&key,a->entries,a->count,sizeof(Resource),resource_compare);}
static void archive_free(Archive *a){free(a->entries);free(a->faces);a->entries=NULL;a->faces=NULL;a->count=0;}
static int archive_index(Archive *a,volatile LONG *stop){
    wchar_t path[MAX_PATH];if(swprintf(path,MAX_PATH,L"%ls\\0002\\ScreenLayout.rdb",a->root)<0)return 0;
    WIN32_FILE_ATTRIBUTE_DATA st;if(!GetFileAttributesExW(path,GetFileExInfoStandard,&st)||st.nFileSizeHigh||st.nFileSizeLow<32||st.nFileSizeLow>PORTRAIT_BYTES)return 0;
    unsigned char *bytes=read_range(path,0,st.nFileSizeLow);if(!bytes)return 0;
    unsigned int count=u32(bytes+16);size_t at=u32(bytes+8),size=st.nFileSizeLow;
    if(memcmp(bytes,"_DRK0000",8)||count>PORTRAIT_MAX_ENTRIES||!count||at<32||at>size){free(bytes);return 0;}
    a->entries=calloc(count,sizeof(*a->entries));a->faces=malloc(65536*sizeof(*a->faces));if(!a->entries||!a->faces){free(bytes);archive_free(a);return 0;}
    for(int i=0;i<65536;i++)a->faces[i]=-1;
    int ok=1;
    for(unsigned int i=0;i<count;i++){
        if(at>size-48||memcmp(bytes+at,"IDRK0000",8)){ok=0;break;}uint64_t entry=u64(bytes+at+8),content=u64(bytes+at+16),declared=u64(bytes+at+24);
        // The index also contains unrelated large UI textures. Limit their
        // bytes when loading a portrait, rather than rejecting the whole index.
        if(entry<48||entry>size-at||content>entry||entry-content<48){ok=0;break;}
        const char *address=(const char*)bytes+at+entry-content;const char *zero=memchr(address,0,(size_t)content);
        uint32_t flags=u32(bytes+at+44);
        // External resources have no address string: their ID names the file.
        if((content&&(!zero||zero-address>=(ptrdiff_t)sizeof(a->entries[i].address)))||(!content&&!(flags&0x10000))){ok=0;break;}
        a->entries[i]=(Resource){.id=u32(bytes+at+36),.type=u32(bytes+at+40),.flags=flags,.size=declared};if(content)memcpy(a->entries[i].address,address,(size_t)(zero-address));
        at+=(size_t)((entry+3)&~3ull);
    }free(bytes);if(!ok||at!=size){archive_free(a);return 0;}a->count=count;qsort(a->entries,count,sizeof(Resource),resource_compare);
    for(unsigned int i=0;i<count;i++){
        if(stop&&InterlockedCompareExchange(stop,0,0))return 0;Resource *r=&a->entries[i];if(r->type!=0xf20de437||r->size!=108)continue;
        size_t n=0;unsigned char *info=load_resource(a,r,&n);if(!info)continue;
        const char *s=(const char*)info+4;int number=-1,used=0;
        if(n==108 && u32(info)==1 && memchr(s,0,64) && sscanf(s,"face_%d_ctr%n",&number,&used)==1 && used>0 && !s[used] && number>=0&&number<65536){
            uint32_t id=(uint32_t)(0x723a2881u*r->id+0xa4d80e4bu);Resource *texture=resource(a,id);
            if(texture&&texture->type==0xafbec60c)a->faces[number]=(int)(texture-a->entries);
        }free(info);
    }return 1;
}
static void color565(uint16_t c,unsigned char out[3]){out[0]=(unsigned char)(((c>>11)&31)*255/31);out[1]=(unsigned char)(((c>>5)&63)*255/63);out[2]=(unsigned char)((c&31)*255/31);}
static uint32_t dxt_pixel(const unsigned char *p,int type,unsigned int index){
    unsigned char alpha=255;const unsigned char *c=p;
    if(type==0x5b){c=p+8;unsigned char values[8]={p[0],p[1]};if(p[0]>p[1])for(int i=1;i<=6;i++)values[i+1]=(unsigned char)(((7-i)*p[0]+i*p[1])/7);else{for(int i=1;i<=4;i++)values[i+1]=(unsigned char)(((5-i)*p[0]+i*p[1])/5);values[6]=0;values[7]=255;}
        uint64_t bits=0;for(int i=0;i<6;i++)bits|=(uint64_t)p[2+i]<<(8*i);alpha=values[(bits>>(3*index))&7];}
    unsigned char colors[4][3];uint16_t lo=u16(c),hi=u16(c+2);color565(lo,colors[0]);color565(hi,colors[1]);
    if(lo>hi||type==0x5b)for(int i=0;i<3;i++){colors[2][i]=(unsigned char)((2*colors[0][i]+colors[1][i])/3);colors[3][i]=(unsigned char)((colors[0][i]+2*colors[1][i])/3);}else for(int i=0;i<3;i++){colors[2][i]=(unsigned char)((colors[0][i]+colors[1][i])/2);colors[3][i]=0;}
    unsigned int key=(u32(c+4)>>(2*index))&3;if(type==0x59&&lo<=hi&&key==3)alpha=0;
    unsigned int r=(colors[key][0]*alpha+244*(255-alpha))/255,g=(colors[key][1]*alpha+239*(255-alpha))/255,b=(colors[key][2]*alpha+229*(255-alpha))/255;
    return b|(g<<8)|(r<<16)|0xff000000u;
}
int s14_portrait_decode(const unsigned char *data,size_t size,uint32_t out[128*128]){
    if(!data||!out||size<40||memcmp(data,"GT1G",4))return 0;unsigned int table=u32(data+12),count=u32(data+16);
    if(count!=1||table>size-4)return 0;uint64_t at=(uint64_t)table+u32(data+table);if(at>size-8)return 0;
    int type=data[at+1];unsigned int dims=data[at+2],width=1u<<(dims&15),height=1u<<(dims>>4);if((type!=0x59&&type!=0x5b)||width<4||height<4||width>2048||height>2048)return 0;
    unsigned int extra=0;if(data[at+7]){if(at>size-12)return 0;extra=u32(data+at+8);if(extra<4||extra>256)return 0;}
    uint64_t payload=at+8+extra,bytes=(uint64_t)width*height*(type==0x59?8:16)/16;if(payload>size||bytes>size-payload)return 0;
    for(unsigned int y=0;y<128;y++)for(unsigned int x=0;x<128;x++){unsigned int sx=x*width/128,sy=y*height/128;const unsigned char *block=data+payload+((sy/4)*(width/4)+sx/4)*(type==0x59?8:16);out[y*128+x]=dxt_pixel(block,type,(sy%4)*4+sx%4);}return 1;
}
static uint32_t *extract(Archive *a,int id){
    if(id<0||id>=65536||!a->faces||a->faces[id]<0)return NULL;size_t n=0;unsigned char *data=load_resource(a,&a->entries[a->faces[id]],&n);if(!data)return NULL;
    uint32_t *pixels=malloc(128*128*4);if(pixels&&!s14_portrait_decode(data,n,pixels)){free(pixels);pixels=NULL;}free(data);return pixels;
}
int s14_portrait_extract(const wchar_t *root,int id,uint32_t out[128*128]){
    Archive a={0};if(!root||wcslen(root)>=MAX_PATH-56)return 0;wcscpy(a.root,root);int ok=archive_index(&a,NULL);uint32_t *pixels=ok?extract(&a,id):NULL;
    if(pixels)memcpy(out,pixels,128*128*4);free(pixels);archive_free(&a);return pixels!=NULL;
}
static DWORD WINAPI asset_worker(void *parameter){
    S14PortraitCache *c=parameter;int ready=archive_index(&c->archive,&c->stop);
    while(!InterlockedCompareExchange(&c->stop,0,0)){
        int slot=-1,id=-1;EnterCriticalSection(&c->lock);for(int i=0;i<PORTRAIT_CACHE;i++)if(c->faces[i].state==1){slot=i;id=c->faces[i].id;c->faces[i].state=2;break;}LeaveCriticalSection(&c->lock);
        if(slot<0){WaitForSingleObject(c->wake,500);continue;}
        uint32_t *pixels=ready?extract(&c->archive,id):NULL,*faded=pixels?malloc(128*128*4):NULL;
        if(faded)for(int i=0;i<128*128;i++){uint32_t p=pixels[i];unsigned int r=(p>>16)&255,g=(p>>8)&255,b=p&255,gray=(r*30+g*59+b*11)/100;r=(r+2*gray)/3;g=(g+2*gray)/3;b=(b+2*gray)/3;faded[i]=0xff000000u|(r<<16)|(g<<8)|b;}
        EnterCriticalSection(&c->lock);c->faces[slot].pixels=pixels;c->faces[slot].faded=faded;c->faces[slot].state=pixels?3:4;LeaveCriticalSection(&c->lock);
    }return 0;
}
S14PortraitCache *s14_portrait_create(const wchar_t *root){
    if(!root||wcslen(root)>=MAX_PATH-56)return NULL;S14PortraitCache *c=calloc(1,sizeof(*c));if(!c)return NULL;wcscpy(c->archive.root,root);InitializeCriticalSection(&c->lock);c->wake=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(c->wake)c->thread=CreateThread(NULL,0,asset_worker,c,0,NULL);if(!c->thread){if(c->wake)CloseHandle(c->wake);DeleteCriticalSection(&c->lock);free(c);return NULL;}return c;
}
void s14_portrait_destroy(S14PortraitCache *c){if(!c)return;InterlockedExchange(&c->stop,1);SetEvent(c->wake);WaitForSingleObject(c->thread,INFINITE);CloseHandle(c->thread);CloseHandle(c->wake);for(int i=0;i<PORTRAIT_CACHE;i++){free(c->faces[i].pixels);free(c->faces[i].faded);}archive_free(&c->archive);DeleteCriticalSection(&c->lock);free(c);}
int s14_portrait_draw(S14PortraitCache *c,int id,HDC dc,const RECT *rect,int faded){
    if(!c||id<0||id>=65536)return 0;int ready=0;EnterCriticalSection(&c->lock);int slot=-1,evict=-1;uint64_t oldest=UINT64_MAX;
    for(int i=0;i<PORTRAIT_CACHE;i++){if(c->faces[i].state&&c->faces[i].id==id){slot=i;break;}if(c->faces[i].state!=1&&c->faces[i].state!=2&&c->faces[i].used<oldest){oldest=c->faces[i].used;evict=i;}}
    if(slot<0&&evict>=0){slot=evict;Face *f=&c->faces[slot];free(f->pixels);free(f->faded);*f=(Face){.id=id,.state=1};SetEvent(c->wake);}
    if(slot>=0){Face *f=&c->faces[slot];f->used=++c->serial;if(f->state==3){BITMAPINFO bi={0};bi.bmiHeader=(BITMAPINFOHEADER){.biSize=sizeof(BITMAPINFOHEADER),.biWidth=128,.biHeight=-128,.biPlanes=1,.biBitCount=32};
        int width=rect->right-rect->left,height=rect->bottom-rect->top,crop=height>width?128*width/height:128,x=(128-crop)/2;SetStretchBltMode(dc,HALFTONE);StretchDIBits(dc,rect->left,rect->top,width,height,x,0,crop,128,faded&&f->faded?f->faded:f->pixels,&bi,DIB_RGB_COLORS,SRCCOPY);ready=1;}}
    LeaveCriticalSection(&c->lock);return ready;
}
int s14_portrait_identity(uintptr_t base,uintptr_t world,int officer){
    if(!base||!world||officer<=0||officer>6000)return -1;SIZE_T n;uintptr_t current=0,person=0,vt=0;unsigned char raw[0xe0];HANDLE process=GetCurrentProcess();
    if(!ReadProcessMemory(process,(void*)(base+0x1fc91d0),&current,8,&n)||n!=8||current!=world||!ReadProcessMemory(process,(void*)(world+0x148+(size_t)officer*8),&person,8,&n)||n!=8||!person||!ReadProcessMemory(process,(void*)person,raw,sizeof(raw),&n)||n!=sizeof(raw))return -1;
    memcpy(&vt,raw,8);if(vt!=base+0x12a00d0||u16(raw+0x10)!=officer)return -1;
    uintptr_t again=0;if(!ReadProcessMemory(process,(void*)(base+0x1fc91d0),&again,8,&n)||n!=8||again!=world)return -1;return u16(raw+0xde);
}
