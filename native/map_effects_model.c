#include "map_effects_model.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
enum {ROOT_VT=0x1293950,FACE_VT=0x1292750,BASIC_VT=0x138b9e8,TRAITS_VT=0x138d428};
typedef struct {S14OfficerRead read;void *context;S14MapFrame *f;} Reader;
static uintptr_t ptr(const unsigned char *p){uintptr_t v;memcpy(&v,p,8);return v;}
static unsigned int word(const unsigned char *p){unsigned int v;memcpy(&v,p,4);return v;}
static unsigned short half(const unsigned char *p){unsigned short v;memcpy(&v,p,2);return v;}
static int get(Reader *r,uintptr_t at,void *out,size_t n){
    if(!at || !n || n>4096 || ++r->f->calls>16384 || r->f->bytes+n>393216)return 0;
    r->f->bytes+=(unsigned)n;return r->read(r->context,at,out,n);
}
int s14_map_validate(S14OfficerRead read,void *c,uintptr_t base){
    // Validate only the adapter instructions we consume; never call the game.
    const unsigned char binding[]={0x48,0x89,0x91,0xe0,0x01,0x00,0x00};
    const unsigned char size[]={0x8b,0x80,0xf0,0x00,0x00,0x00};unsigned char b[8];uintptr_t method=0;
    const unsigned char state_manager[]={0x48,0x8d,0x05,0x35,0x6d,0x9d,0x01};
    const unsigned char state_top[]={0x48,0x8b,0x50,0x10,0x48,0x8b,0x48,0x20};
    return read && base && read(c,base+0x194870,b,sizeof(binding)) && !memcmp(b,binding,sizeof(binding)) &&
        read(c,base+0x82e1e3,b,sizeof(size)) && !memcmp(b,size,sizeof(size)) &&
        read(c,base+0xf5f4,b,sizeof(state_manager)) && !memcmp(b,state_manager,sizeof(state_manager)) &&
        read(c,base+0x509bed,b,sizeof(state_top)) && !memcmp(b,state_top,sizeof(state_top)) &&
        read(c,base+0x12cc498+5*8,&method,8) && method==base+0x3f8ef0 &&
        read(c,base+0x12cc760+5*8,&method,8) && method==base+0x3f8200;
}
static int scene_read(Reader *r,uintptr_t base,S14MapScene *s){
    unsigned char manager[0x28],state[0x70];memset(s,0,sizeof(*s));
    // This is the same active-state stack used by the native 0x509bd0 test.
    // Map widgets remain visible underneath dialogs; only the *top* state is
    // authoritative. Unknown states are blocked, including future dialogs.
    if(!get(r,base+0x19e6330,manager,sizeof(manager)))return 0;
    s->depth=ptr(manager+0x10);s->array=ptr(manager+0x20);
    uintptr_t capacity=ptr(manager+0x18);
    if(!s->depth || s->depth>64 || capacity<s->depth || capacity>256 || !s->array ||
       !get(r,s->array+(s->depth-1)*8,&s->top,8) || !get(r,s->top,state,sizeof(state)))return 0;
    s->vtable=ptr(state);s->lifecycle=word(state+0x6c);
    if(s->vtable==base+0x12cc498 && !get(r,s->top+0x470,&s->phase,4))return 0;
    s->main_map=s->lifecycle==3 && ((s->vtable==base+0x12cc498 && s->phase==2) || s->vtable==base+0x12cc760);
    unsigned char again[0x28];uintptr_t top=0;
    return get(r,base+0x19e6330,again,sizeof(again)) && !memcmp(manager+0x10,again+0x10,24) &&
        get(r,s->array+(s->depth-1)*8,&top,8) && top==s->top;
}
static int scene_stable(Reader *r,uintptr_t base,const S14MapScene *s){
    S14MapScene again;return scene_read(r,base,&again) && again.main_map && !memcmp(s,&again,sizeof(again));
}
static int header(Reader *r,uintptr_t at,uintptr_t vt,unsigned char *b){return get(r,at,b,72) && ptr(b)==vt;}
static int visible(const unsigned char *b){return (word(b+0x40)&3)==3;}
static int discover(Reader *r,uintptr_t base,S14MapCache *c){
    uintptr_t faces[S14_MAP_CACHE_MAX];int faces_count=0;c->markers=0;c->basic=c->personality=0;memset(c->dialogs,0,sizeof(c->dialogs));
    for(int bucket=0;bucket<2;bucket++){
        uintptr_t node=c->heads[bucket];unsigned int n=0;
        for(;node && n<6144;n++){
            uintptr_t link[2],vt=0;if(!get(r,node,link,16) || link[1]==node || !get(r,link[0],&vt,8))return 0;
            if(vt==base+ROOT_VT){if(c->markers>=S14_MAP_CACHE_MAX)return 0;c->marker[c->markers++]=(S14MapMarker){link[0],0};}
            if(vt==base+FACE_VT){if(faces_count>=S14_MAP_CACHE_MAX)return 0;faces[faces_count++]=link[0];}
            if(vt==base+BASIC_VT){if(c->basic && c->basic!=link[0])return 0;c->basic=link[0];}
            if(vt==base+TRAITS_VT){if(c->personality && c->personality!=link[0])return 0;c->personality=link[0];}
            if(vt==base+0x137e970)c->dialogs[0]=link[0];
            if(vt==base+0x13812f8)c->dialogs[1]=link[0];
            node=link[1];
        }
        if(node)return 0;
    }
    for(int i=0;i<faces_count;i++){
        uintptr_t ancestor=faces[i];unsigned char b[72];
        for(int depth=0;depth<3 && ancestor;depth++){if(!get(r,ancestor,b,72))return 0;ancestor=ptr(b+8);}
        for(int j=0;j<c->markers;j++)if(c->marker[j].root==ancestor){if(c->marker[j].face)return 0;c->marker[j].face=faces[i];}
    }
    c->initialized=1;return 1;
}
static int node_geometry(Reader *r,uintptr_t layout,int index,RECT *out,float *circle){
    uintptr_t array=0,node=0;unsigned int count=0;unsigned char b[0x118];float m[16],sx,sy;
    if(!get(r,layout+0x130,&array,8) || !get(r,layout+0x138,&count,4) || count<=(unsigned)index || count>256 ||
       !get(r,array+index*8,&node,8) || !get(r,node,b,sizeof(b)))return 0;
    int w=(short)half(b+0xf0),h=(short)half(b+0xf2);memcpy(&sx,b+0xf4,4);memcpy(&sy,b+0xf8,4);
    uintptr_t matrix=ptr(b+(ptr(b+0xd0)?0xe0:0xe8));
    if(w<=0 || w>2048 || h<=0 || h>1080 || !isfinite(sx) || !isfinite(sy) || sx<.25f || sx>4 || sy<.25f || sy>4 || !get(r,matrix,m,sizeof(m)))return 0;
    for(int i=0;i<16;i++)if(!isfinite(m[i]))return 0;
    // Current map labels use axis-aligned, unscaled transforms. A different
    // rendering path is hidden until its geometry is verified, never guessed.
    if(fabsf(m[0]-1)>0.01f || fabsf(m[5]-1)>0.01f || fabsf(m[1])>.01f || fabsf(m[4])>.01f)return 0;
    int ax=b[0x10c]>>4,ay=b[0x10c]&15;if(ax>2 || ay>2)return 0;
    float x=960+m[12]-ax*w*sx*.5f,y=540-m[13]-ay*h*sy*.5f;
    if(x< -2048 || x>4096 || y< -1080 || y>2160)return 0;
    *out=(RECT){(LONG)lroundf(x),(LONG)lroundf(y),(LONG)lroundf(x+w*sx),(LONG)lroundf(y+h*sy)};
    if(circle){circle[0]=x+w*sx*.5f;circle[1]=y+h*sy*.5f;circle[2]=w*sx*.5f;circle[3]=h*sy*.5f;}return 1;
}
static int node_rect(Reader *r,uintptr_t layout,int index,RECT *out){return node_geometry(r,layout,index,out,NULL);}
static int live_army(Reader *r,uintptr_t base,uintptr_t world,uintptr_t army,int *id,unsigned char *identity){
    unsigned char b[32];uintptr_t pool=0,canonical=0;
    if(!get(r,army,b,32) || ptr(b)!=base+0x123e288 || b[0x10]!=1 || (half(b+0x12)<1 || half(b+0x12)>6000) || !half(b+0x16) ||
       !get(r,world+0x7df60,&pool,8) || army<=pool || (army-pool)%512 || (army-pool)/512>500)return 0;
    *id=(int)((army-pool)/512);
    if(!get(r,world+0x7df60+(uintptr_t)*id*8,&canonical,8) || canonical!=army)return 0;
    memcpy(identity,b+0x10,16);return 1;
}
static int face_rect(Reader *r,uintptr_t base,const S14MapMarker *m,RECT *out){
    unsigned char b[72];uintptr_t at=m->face;int x=0,y=0;
    for(int i=0;i<4;i++){
        if(!get(r,at,b,72) || !visible(b))return 0;
        if((i==0 && ptr(b)!=base+FACE_VT) || (i==2 && ptr(b)!=base+0x12930b8) || (i==3 && at!=m->root))return 0;
        int v[4];memcpy(v,b+0x20,16);if(abs(v[0])>4096 || abs(v[1])>2160)return 0;x+=v[0];y+=v[1];at=ptr(b+8);
    }
    if(!header(r,at,base+0x1298818,b) || !visible(b))return 0;
    RECT root,mask,frame;if(!node_rect(r,m->face,0,&root) || !node_rect(r,m->face,1,&mask) || !node_rect(r,m->face,2,&frame) ||
        abs(root.left-x)>2 || abs(root.top-y)>2 || mask.right-mask.left<24 || mask.right-mask.left>96 ||
        abs((mask.right-mask.left)-(mask.bottom-mask.top))>2 || mask.left<root.left || mask.top<root.top || mask.right>root.right || mask.bottom>root.bottom)return 0;
    if(frame.right-frame.left<mask.right-mask.left || frame.right-frame.left>96 || abs((frame.right-frame.left)-(frame.bottom-frame.top))>2 ||
       abs(frame.left+frame.right-mask.left-mask.right)>2 || abs(frame.top+frame.bottom-mask.top-mask.bottom)>2)return 0;
    *out=frame;return frame.left>=12 && frame.top>=12 && frame.right<=1908 && frame.bottom<=1068;
}
int s14_map_card(const RECT *panel,const RECT *traits,RECT *card){
    if(!panel || !card || panel->left<0 || panel->top<0 || panel->right>1920 || panel->bottom>1080 || panel->right<=panel->left || panel->bottom<=panel->top)return 0;
    int x=panel->right+12,y=panel->top+8;if(traits){x=traits->left;y=traits->bottom+10;}
    int width=174,height=174;
    if(y+height>1068){if(!traits)return 0;x=traits->right+12;y=traits->top;}
    if(x<panel->right || x+width>1908 || y<12 || y+height>1068)return 0;
    *card=(RECT){x,y,x+width,y+height};return 1;
}
int s14_map_capture_all(S14OfficerRead read,void *ctx,uintptr_t base,S14MapCache *c,ULONGLONG now,S14MapFrame *f,S14MapBatch *batch,S14MapQualify qualify,void *qualification){
    if(batch)batch->count=0;
    if(!f)return 0;memset(f,0,sizeof(*f));if(!read || !base || !c)return 0;f->sampled_at=now;Reader r={read,ctx,f};
    if(!scene_read(&r,base,&f->scene))return 0;
    if(!f->scene.main_map){f->modal=1;return 1;}
    unsigned char reg[128],ctrl[64];uintptr_t world=0,heads[2]={0};unsigned int idx[2]={0};
    if(!get(&r,base+0x1fc91d0,&world,8) || !world || !get(&r,base+0x201c320,reg,128) || !get(&r,base+0x19e6390,ctrl,64))return 0;
    uintptr_t table=ptr(reg+0x40),anchor=ptr(reg+0x38);unsigned int count=word(reg+0x70);
    if(!table || !anchor || !count || count>0x40000)return 0;
    for(int i=0;i<2;i++)if(!get(&r,ptr(ctrl+(i?56:8)),&idx[i],4) || idx[i]>=count || !get(&r,table+(uintptr_t)idx[i]*8,&heads[i],8))return 0;
    if(!c->initialized || c->base!=base || c->world!=world || c->table!=table || c->anchor!=anchor || c->count!=count || memcmp(c->indices,idx,sizeof(idx)) || memcmp(c->heads,heads,sizeof(heads)) || now>=c->scan_due){
        memset(c,0,sizeof(*c));c->base=base;c->world=world;c->table=table;c->anchor=anchor;c->count=count;memcpy(c->indices,idx,sizeof(idx));memcpy(c->heads,heads,sizeof(heads));c->scan_due=now+5000;
        if(!discover(&r,base,c)){c->initialized=0;return 0;}f->discovered=1;
    }
    unsigned char b[72];f->world=world;f->registry_table=table;f->detail_handle=ptr(ctrl+56);f->detail_index=idx[1];f->detail_head=heads[1];
    for(int i=0;i<2;i++)if(c->dialogs[i]){
        if(!header(&r,c->dialogs[i],base+(i?0x13812f8:0x137e970),b)){c->initialized=0;return 0;}
        if(visible(b)){f->modal=1;return 1;}
    }
    S14MapFrame context=*f;uintptr_t selected=0;
    if(c->basic && header(&r,c->basic,base+BASIC_VT,b) && visible(b))get(&r,c->basic+0x168,&selected,8);
    int matches=0;
    for(int i=0;i<c->markers;i++){
        S14MapMarker *m=&c->marker[i];uintptr_t army=0;int id;unsigned char ident[16];
        if(!header(&r,m->root,base+ROOT_VT,b)){c->initialized=0;if(batch)batch->count=0;return 0;}
        if(!visible(b) || !get(&r,m->root+0x1e0,&army,8) || !army || !live_army(&r,base,world,army,&id,ident))continue;
        int leader=half(ident+2),effect=qualify?qualify(qualification,world,army,leader):leader==518?1:0;
        if(!effect)continue;
        if(!batch && ++matches>1)return 0;
        S14MapFrame item=context;item.calls=item.bytes=0;item.army=army;item.army_id=id;item.leader=leader;item.root=m->root;item.face=m->face;item.effect=effect;
        unsigned short serial=0;if(!get(&r,army+0x28,&serial,2))continue;item.native_serial=serial;
        item.halo=face_rect(&r,base,m,&item.portrait);
        if(army==selected && c->basic && header(&r,c->basic,base+BASIC_VT,b) && visible(b)){
            uintptr_t parent=ptr(b+8);int xywh[4];memcpy(xywh,b+0x20,16);unsigned char summary[72];
            if(header(&r,parent,base+0x138beb0,summary) && visible(summary)){
                int pos[4];memcpy(pos,summary+0x20,16);RECT panel={pos[0]+xywh[0],pos[1]+xywh[1],pos[0]+xywh[0]+xywh[2],pos[1]+xywh[1]+xywh[3]},traits;RECT *t=NULL;
                if(c->personality && header(&r,c->personality,base+TRAITS_VT,b) && visible(b)){
                    uintptr_t traits_army=0;if(!get(&r,c->personality+0x168,&traits_army,8) || traits_army!=army || !node_rect(&r,c->personality,1,&traits))continue;t=&traits;
                }
                item.tooltip=s14_map_card(&panel,t,&item.card);
                uintptr_t again=0;if(!get(&r,c->basic+0x168,&again,8) || again!=army || !get(&r,c->basic+0x40,b,4) || (word(b)&3)!=3)item.tooltip=0;
            }
        }
        uintptr_t after=0;unsigned char final[16];
        if(!get(&r,m->root+0x1e0,&after,8) || after!=army || !get(&r,army+0x10,final,16) || memcmp(ident,final,16))continue;
        if(batch && item.halo && item.face){int duplicate=0;for(int j=0;j<batch->count;j++)if(batch->frames[j].army==army)duplicate=1;if(duplicate)continue;
            if(batch->count>=500){batch->count=0;return 0;}batch->frames[batch->count++]=item;}
        if(!f->army || item.tooltip){item.calls=f->calls;item.bytes=f->bytes;*f=item;}
    }
    uintptr_t after=0;
    int ok=get(&r,base+0x1fc91d0,&after,8) && after==world && get(&r,base+0x201c320+0x40,&after,8) && after==table && scene_stable(&r,base,&context.scene);
    if(!ok && batch)batch->count=0;return ok;
}
int s14_map_capture(S14OfficerRead read,void *ctx,uintptr_t base,S14MapCache *c,ULONGLONG now,S14MapFrame *f){return s14_map_capture_all(read,ctx,base,c,now,f,NULL,NULL,NULL);}
int s14_map_scale(const RECT *p,int w,int h,RECT *out,int *scale){
    if(!p || !out || !scale || w<800 || h<450 || w>16384 || h>16384 || p->left<0 || p->top<0 || p->right>1920 || p->bottom>1080)return 0;
    int s=w*96/1920;if(h*96/1080<s)s=h*96/1080;if(s<40)return 0;
    int x=(w-MulDiv(1920,s,96))/2,y=(h-MulDiv(1080,s,96))/2;
    *out=(RECT){x+MulDiv(p->left,s,96),y+MulDiv(p->top,s,96),x+MulDiv(p->right,s,96),y+MulDiv(p->bottom,s,96)};*scale=s;return 1;
}
int s14_map_present_capture(S14OfficerRead read,void *ctx,uintptr_t base,const S14MapFrame *binding,const uintptr_t dialogs[2],S14MapFrame *f){
    if(!f)return 0;memset(f,0,sizeof(*f));if(!read || !base || !binding || binding->modal || !binding->scene.main_map || !binding->world || !binding->army || !binding->root || !binding->face || !binding->registry_table || !binding->detail_handle)return 0;
    Reader r={read,ctx,f};uintptr_t world=0,army=0;unsigned char b[72],identity[16];int id=0;
    if(!scene_read(&r,base,&f->scene) || !f->scene.main_map || memcmp(&f->scene,&binding->scene,sizeof(f->scene)))return 0;
    uintptr_t table=0,handle=0,head=0;unsigned int index=0;
    // UI entry can happen between worker sampling and Present. Reject a new
    // native dialog context before using cached modal pointers or map geometry.
    if(!get(&r,base+0x201c360,&table,8) || table!=binding->registry_table ||
       !get(&r,base+0x19e63c8,&handle,8) || handle!=binding->detail_handle ||
       !get(&r,handle,&index,4) || index!=binding->detail_index ||
       !get(&r,table+(uintptr_t)index*8,&head,8) || head!=binding->detail_head)return 0;
    if(!get(&r,base+0x1fc91d0,&world,8) || world!=binding->world || !live_army(&r,base,world,binding->army,&id,identity) || id!=binding->army_id)return 0;
    if(binding->leader && half(identity+2)!=binding->leader)return 0;
    unsigned short serial=0;if(binding->leader && (!get(&r,binding->army+0x28,&serial,2) || serial!=binding->native_serial))return 0;
    for(int i=0;i<2;i++)if(dialogs && dialogs[i]){
        if(!header(&r,dialogs[i],base+(i?0x13812f8:0x137e970),b) || visible(b))return 0;
    }
    uintptr_t at=binding->face;
    const unsigned int vts[]={FACE_VT,0,0x12930b8,ROOT_VT,0x1298818};
    for(int i=0;i<5;i++){
        if(!get(&r,at,b,72) || !visible(b) || (vts[i] && ptr(b)!=base+vts[i]) || (i==3 && at!=binding->root))return 0;
        at=ptr(b+8);
    }
    if(!get(&r,binding->root+0x1e0,&army,8) || army!=binding->army)return 0;
    float circle[4];if(!node_geometry(&r,binding->face,2,&f->portrait,circle) || circle[2]<12 || circle[2]>64 || fabsf(circle[2]-circle[3])>1 ||
       circle[0]+circle[2]<0 || circle[0]-circle[2]>1920 || circle[1]+circle[2]<0 || circle[1]-circle[2]>1080)return 0;
    uintptr_t after=0;unsigned char stable[16];
    if(!get(&r,base+0x1fc91d0,&after,8) || after!=world || !get(&r,binding->root+0x1e0,&after,8) || after!=army ||
       !get(&r,army+0x10,stable,16) || stable[0]!=identity[0] || half(stable+2)!=half(identity+2) || !half(stable+6))return 0;
    if(binding->leader && (!get(&r,army+0x28,&serial,2) || serial!=binding->native_serial))return 0;
    if(!scene_stable(&r,base,&f->scene))return 0;
    f->world=world;f->army=army;f->army_id=id;f->root=binding->root;f->face=binding->face;f->center_x=circle[0];f->center_y=circle[1];f->radius=circle[2];f->halo=1;return 1;
}
