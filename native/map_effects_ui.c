#define WIN32_LEAN_AND_MEAN
#include "map_effects_ui.h"
#include "map_render.h"
#include "career_affix.h"
#include "ai_affix.h"
#include "features.h"
#include <math.h>
#include <string.h>
static const wchar_t visual_class[]=L"SAN14ModManager.CaoRenMapEffects.v1";
static int memory(void *unused,uintptr_t at,void *out,size_t n){(void)unused;SIZE_T got;return ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
void s14_map_halo_pixels(unsigned int *pixels,int w,int h,int margin){
    if(!pixels || w<16 || h<16 || w>512 || h>512 || margin<2 || margin*2>=w || margin*2>=h)return;
    float radius=(float)(w-2*margin)*.5f+3.0f,glow=(float)margin*.75f;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        float dx=x+.5f-w*.5f,dy=y+.5f-h*.5f,dist=sqrtf(dx*dx+dy*dy),d=fabsf(dist-radius);
        float a=d<2?210.0f*(1-d*.18f):100.0f*expf(-d*d/(glow*glow));
        if(dist<radius-2)a=0; // The portrait and faction ring remain untouched.
        if(dist>radius+2){float fade=(w*.5f-1-dist)/(w*.5f-1-radius-2);if(fade<0)fade=0;if(fade>1)fade=1;a*=fade*fade;}
        int alpha=(int)a;if(alpha<2)alpha=0;if(alpha>255)alpha=255;
        pixels[y*w+x]=((unsigned)alpha<<24)|((unsigned)(183*alpha/255)<<16)|((unsigned)(99*alpha/255)<<8)|(unsigned)(246*alpha/255);
    }
}
static void text(HDC dc,HFONT font,const wchar_t *s,RECT rect,COLORREF color){HGDIOBJ old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,s,-1,&rect,DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER);SelectObject(dc,old);}
void s14_map_card_paint(HDC dc,int w,int h,int scale,HFONT title,HFONT body,HFONT small){
    int pad=MulDiv(12,scale,96);RECT all={0,0,w,h};HBRUSH bg=CreateSolidBrush(RGB(29,24,40)),edge=CreateSolidBrush(RGB(165,105,221));FillRect(dc,&all,bg);FrameRect(dc,&all,edge);DeleteObject(bg);DeleteObject(edge);
    const wchar_t *lines[]={L"战绩词条",L"百战精锐",L"攻军  +10%",L"防御  +10%",L"当前生效 · 神 曹仁",L"斩敌 ≥5000（含伤兵）"};
    const int ys[]={8,33,65,94,128,150},heights[]={22,22,24,24,18,17};
    for(int i=0;i<6;i++)text(dc,i==0?title:i<4?body:small,lines[i],(RECT){pad,MulDiv(ys[i],scale,96),w-pad,MulDiv(ys[i]+heights[i],scale,96)},i==0?RGB(199,147,245):i<4?RGB(246,236,255):RGB(192,182,204));
}
static void random_card(S14MapEffectsUI *ui,HDC dc){
    int w=ui->card_width,scale=ui->scale,pad=MulDiv(12,scale,96);RECT all={0,0,w,ui->card_height};HBRUSH bg=CreateSolidBrush(RGB(40,24,24)),edge=CreateSolidBrush(RGB(222,82,74));FillRect(dc,&all,bg);FrameRect(dc,&all,edge);DeleteObject(bg);DeleteObject(edge);
    const wchar_t *lines[]={L"出征词条",L"禁军精锐",L"攻军  +10%",L"防御  +10%",ui->card_name,L"每城 9 次保底 · 随机 10%"};const int ys[]={8,33,65,94,128,150},heights[]={22,22,24,24,18,17};
    for(int i=0;i<6;i++)text(dc,i==0?ui->title:i<4?ui->body:ui->small,lines[i],(RECT){pad,MulDiv(ys[i],scale,96),w-pad,MulDiv(ys[i]+heights[i],scale,96)},i==0?RGB(255,140,133):RGB(246,236,235));
}
static int qualifies(void *ctx,uintptr_t world,uintptr_t army,int leader){S14MapEffectsUI *ui=ctx;if((ui->feature_flags&S14_AI_RANDOM_AFFIX) && s14_ai_active((void*)army))return 2;if((ui->feature_flags&S14_CAO_REN_BUFF) && s14_affix_active(world,leader))return 1;return 0;}
static LRESULT CALLBACK procedure(HWND window,UINT msg,WPARAM w,LPARAM l){
    S14MapEffectsUI *ui=(void*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(msg==WM_NCCREATE){ui=((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui);}
    if(msg==WM_NCHITTEST)return HTTRANSPARENT;if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT && ui){PAINTSTRUCT p;HDC dc=BeginPaint(window,&p);if(window==ui->card && ui->frame.effect==2)random_card(ui,dc);else if(window==ui->card)s14_map_card_paint(dc,ui->card_width,ui->card_height,ui->scale,ui->title,ui->body,ui->small);EndPaint(window,&p);return 0;}
    return DefWindowProcW(window,msg,w,l);
}
static void hide(S14MapEffectsUI *ui){if(ui->halo)ShowWindow(ui->halo,SW_HIDE);if(ui->card)ShowWindow(ui->card,SW_HIDE);ui->halo_shown=ui->card_shown=0;}
static HWND create(S14MapEffectsUI *ui){
    WNDCLASSEXW c={.cbSize=sizeof(c),.lpfnWndProc=procedure,.hInstance=ui->instance,.lpszClassName=visual_class};
    if(!RegisterClassExW(&c) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return NULL;
    return CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED,visual_class,L"曹仁部队特殊效果",WS_POPUP,0,0,1,1,ui->owner,NULL,ui->instance,ui);
}
static int fonts(S14MapEffectsUI *ui,int scale){
    if(ui->scale==scale && ui->title && ui->body && ui->small)return 1;
    if(ui->title)DeleteObject(ui->title);if(ui->body)DeleteObject(ui->body);if(ui->small)DeleteObject(ui->small);ui->scale=scale;
    ui->title=CreateFontW(-MulDiv(16,scale,96),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->body=CreateFontW(-MulDiv(18,scale,96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->small=CreateFontW(-MulDiv(12,scale,96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");return ui->title && ui->body && ui->small;
}
void s14_map_ui_tick(S14MapEffectsUI *ui,HINSTANCE instance,HWND owner,uintptr_t base,int enabled,unsigned int preferences,int obstructed,ULONGLONG now){
    ui->feature_flags=enabled;ui->preferences=preferences;
    if(!enabled || !(preferences&15) || obstructed || !owner || GetForegroundWindow()!=owner || !IsWindowVisible(owner) || IsIconic(owner)){memset(&ui->frame,0,sizeof(ui->frame));ui->batch.count=0;hide(ui);return;}
    if(ui->owner && ui->owner!=owner)s14_map_ui_destroy(ui);ui->owner=owner;ui->instance=instance;
    if(now<ui->next_read)return;ui->next_read=now+33;
    if(ui->base!=base){ui->base=base;ui->ready=s14_map_validate(memory,NULL,base);memset(&ui->cache,0,sizeof(ui->cache));}
    if(!ui->ready){hide(ui);return;}
    S14MapFrame f;if(!s14_map_capture_all(memory,NULL,base,&ui->cache,now,&f,&ui->batch,qualifies,ui)){ui->frame=f;hide(ui);ui->next_read=now+250;return;}ui->frame=f;
    if(f.modal || !f.army || !qualifies(ui,f.world,f.army,f.leader)){hide(ui);return;}RECT client,rect;POINT origin={0};int scale;
    if(!GetClientRect(owner,&client) || !ClientToScreen(owner,&origin)){hide(ui);return;}
    // The halo uses the current rendered frame, independent of worker polling.
    ui->halo_shown=(preferences&5) && s14_map_render_visible();
    if(!ui->halo_shown && ui->halo)ShowWindow(ui->halo,SW_HIDE);
    if((preferences&(f.effect==2?8u:2u)) && f.tooltip && s14_map_scale(&f.card,client.right,client.bottom,&rect,&scale) && fonts(ui,scale)){
        if(f.effect==2){uintptr_t person=0;unsigned char raw[72];ui->card_name[0]=L'禁';ui->card_name[1]=L' ';ui->card_name[2]=0;if(memory(NULL,f.world+0x148+(uintptr_t)f.leader*8,&person,8) && memory(NULL,person,raw,72)){size_t n=2;for(int part=0;part<2;part++)for(int j=0;j<9 && n<23;j++){wchar_t ch;memcpy(&ch,raw+0x12+part*18+j*2,2);if(!ch)break;ui->card_name[n++]=ch;}ui->card_name[n]=0;}}
        if(!ui->card){ui->card=create(ui);if(ui->card && !SetLayeredWindowAttributes(ui->card,0,242,LWA_ALPHA)){DestroyWindow(ui->card);ui->card=NULL;}}
        ui->card_width=rect.right-rect.left;ui->card_height=rect.bottom-rect.top;
        ui->card_shown=ui->card && SetWindowPos(ui->card,HWND_TOPMOST,origin.x+rect.left,origin.y+rect.top,ui->card_width,ui->card_height,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=0;
        if(ui->card)InvalidateRect(ui->card,NULL,FALSE);
    }else ui->card_shown=0;
    if(!ui->card_shown && ui->card)ShowWindow(ui->card,SW_HIDE);
}
void s14_map_ui_destroy(S14MapEffectsUI *ui){
    if(ui->halo)DestroyWindow(ui->halo);if(ui->card)DestroyWindow(ui->card);if(ui->title)DeleteObject(ui->title);if(ui->body)DeleteObject(ui->body);if(ui->small)DeleteObject(ui->small);memset(ui,0,sizeof(*ui));
}
