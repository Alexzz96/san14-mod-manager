#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include "map_render.h"
#include "map_shaders.h"
#include "MinHook.h"
#include "career_affix.h"
#include "ai_affix.h"
#include "features.h"
#include "troop_runtime.h"
#include "archer_probe.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <wchar.h>
#include <wincodec.h>
#define RELEASE(p) do{if(p){IUnknown_Release((IUnknown*)(p));(p)=NULL;}}while(0)
static int load_caption_image(S14MapGraphics *g,ID3D11Device *device){
    HMODULE module=NULL;HRSRC resource=NULL;HGLOBAL loaded=NULL;void *bytes=NULL;DWORD byte_count=0;
    IWICImagingFactory *factory=NULL;IWICStream *stream=NULL;IWICBitmapDecoder *decoder=NULL;
    IWICBitmapFrameDecode *frame=NULL;IWICFormatConverter *converter=NULL;ID3D11Texture2D *texture=NULL;
    unsigned char *pixels=NULL;UINT width=0,height=0;HRESULT hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);int uninitialize=SUCCEEDED(hr);
    if(FAILED(hr)&&hr!=RPC_E_CHANGED_MODE)return 0;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)(uintptr_t)&s14_map_graphics_init,&module))goto done;
    resource=FindResourceW(module,MAKEINTRESOURCEW(3),RT_RCDATA);if(!resource)goto done;
    byte_count=SizeofResource(module,resource);loaded=LoadResource(module,resource);bytes=loaded?LockResource(loaded):NULL;
    if(!bytes||!byte_count)goto done;
    hr=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void**)&factory);
    if(SUCCEEDED(hr))hr=IWICImagingFactory_CreateStream(factory,&stream);
    if(SUCCEEDED(hr))hr=IWICStream_InitializeFromMemory(stream,(BYTE*)bytes,byte_count);
    if(SUCCEEDED(hr))hr=IWICImagingFactory_CreateDecoderFromStream(factory,(IStream*)stream,NULL,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=IWICBitmapDecoder_GetFrame(decoder,0,&frame);
    if(SUCCEEDED(hr))hr=IWICBitmapFrameDecode_GetSize(frame,&width,&height);
    if(SUCCEEDED(hr)&&(!width||!height||width>4096||height>4096||(size_t)width*height>16000000))hr=E_INVALIDARG;
    if(SUCCEEDED(hr))hr=IWICImagingFactory_CreateFormatConverter(factory,&converter);
    if(SUCCEEDED(hr))hr=IWICFormatConverter_Initialize(converter,(IWICBitmapSource*)frame,&GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(hr)){pixels=malloc((size_t)width*height*4);if(!pixels)hr=E_OUTOFMEMORY;}
    if(SUCCEEDED(hr))hr=IWICFormatConverter_CopyPixels(converter,NULL,width*4,width*height*4,pixels);
    if(SUCCEEDED(hr)){
        UINT left=width,top=height,right=0,bottom=0;
        for(UINT y=0;y<height;y++)for(UINT x=0;x<width;x++)if(pixels[((size_t)y*width+x)*4+3]>16){if(x<left)left=x;if(y<top)top=y;if(x+1>right)right=x+1;if(y+1>bottom)bottom=y+1;}
        if(left>=right||top>=bottom)hr=E_INVALIDARG;
        else{g->caption_uv[0]=(float)left/width;g->caption_uv[1]=(float)top/height;g->caption_uv[2]=(float)(right-left)/width;g->caption_uv[3]=(float)(bottom-top)/height;g->caption_aspect=(float)(right-left)/(bottom-top);}
    }
    if(SUCCEEDED(hr)){
        D3D11_TEXTURE2D_DESC desc={.Width=width,.Height=height,.MipLevels=1,.ArraySize=1,.Format=DXGI_FORMAT_R8G8B8A8_UNORM,.SampleDesc={1,0},.Usage=D3D11_USAGE_IMMUTABLE,.BindFlags=D3D11_BIND_SHADER_RESOURCE};
        D3D11_SUBRESOURCE_DATA data={pixels,width*4,0};hr=ID3D11Device_CreateTexture2D(device,&desc,&data,&texture);
    }
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)texture,NULL,&g->caption_image);
    if(SUCCEEDED(hr)){
        D3D11_SAMPLER_DESC sampler={.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR,.AddressU=D3D11_TEXTURE_ADDRESS_CLAMP,.AddressV=D3D11_TEXTURE_ADDRESS_CLAMP,.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP,.MaxLOD=D3D11_FLOAT32_MAX};
        hr=ID3D11Device_CreateSamplerState(device,&sampler,&g->caption_sampler);
    }
done:
    free(pixels);RELEASE(texture);RELEASE(converter);RELEASE(frame);RELEASE(decoder);RELEASE(stream);RELEASE(factory);if(uninitialize)CoUninitialize();
    if(FAILED(hr)||!g->caption_image||!g->caption_sampler){RELEASE(g->ammo_ps);RELEASE(g->caption_sampler);RELEASE(g->caption_image);return 0;}return 1;
}
int s14_map_graphics_init(S14MapGraphics *g,ID3D11Device *device){
    if(!g || !device)return 0;s14_map_graphics_destroy(g);g->device=device;ID3D11Device_AddRef(device);
    ID3D11Device1 *device1=NULL;HRESULT hr=ID3D11Device_QueryInterface(device,&IID_ID3D11Device1,(void**)&device1);
    if(SUCCEEDED(hr)){ID3D11Device_GetImmediateContext(device,&g->context);hr=ID3D11DeviceContext_QueryInterface(g->context,&IID_ID3D11DeviceContext1,(void**)&g->context1);}
    D3D_FEATURE_LEVEL level=ID3D11Device_GetFeatureLevel(device);
    UINT flags=ID3D11Device_GetCreationFlags(device)&D3D11_CREATE_DEVICE_SINGLETHREADED?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
    if(SUCCEEDED(hr))hr=ID3D11Device1_CreateDeviceContextState(device1,flags,&level,1,D3D11_SDK_VERSION,&IID_ID3D11Device,NULL,&g->state);
    RELEASE(device1);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateVertexShader(device,s14_map_vs,sizeof(s14_map_vs),NULL,&g->vs);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreatePixelShader(device,s14_map_ps,sizeof(s14_map_ps),NULL,&g->ps);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreatePixelShader(device,s14_map_troop_ps,sizeof(s14_map_troop_ps),NULL,&g->troop_ps);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateVertexShader(device,s14_map_sprite_vs,sizeof(s14_map_sprite_vs),NULL,&g->sprite_vs);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreatePixelShader(device,s14_map_sprite_ps,sizeof(s14_map_sprite_ps),NULL,&g->sprite_ps);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreatePixelShader(device,s14_map_ammo_ps,sizeof(s14_map_ammo_ps),NULL,&g->ammo_ps);
    /* A caption decode failure must not disable unrelated native/map visuals.
       The command renderer keeps its native text until PNG drawing succeeds. */
    if(SUCCEEDED(hr))load_caption_image(g,device);
    D3D11_BUFFER_DESC cb={.ByteWidth=48,.Usage=D3D11_USAGE_DYNAMIC,.BindFlags=D3D11_BIND_CONSTANT_BUFFER,.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE};
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateBuffer(device,&cb,NULL,&g->constants);
    D3D11_BLEND_DESC blend={0};blend.RenderTarget[0]=(D3D11_RENDER_TARGET_BLEND_DESC){TRUE,D3D11_BLEND_ONE,D3D11_BLEND_INV_SRC_ALPHA,D3D11_BLEND_OP_ADD,D3D11_BLEND_ONE,D3D11_BLEND_INV_SRC_ALPHA,D3D11_BLEND_OP_ADD,D3D11_COLOR_WRITE_ENABLE_ALL};
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateBlendState(device,&blend,&g->blend);
    D3D11_RASTERIZER_DESC raster={.FillMode=D3D11_FILL_SOLID,.CullMode=D3D11_CULL_NONE,.DepthClipEnable=TRUE,.MultisampleEnable=TRUE};
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateRasterizerState(device,&raster,&g->raster);
    D3D11_DEPTH_STENCIL_DESC depth={.DepthEnable=FALSE,.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO,.DepthFunc=D3D11_COMPARISON_ALWAYS};
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateDepthStencilState(device,&depth,&g->depth);
    if(FAILED(hr)){s14_map_graphics_destroy(g);g->error=hr;return 0;}return 1;
}
void s14_map_graphics_destroy(S14MapGraphics *g){if(!g)return;RELEASE(g->ammo_ps);RELEASE(g->caption_sampler);RELEASE(g->caption_image);RELEASE(g->troop_ps);RELEASE(g->sprite_ps);RELEASE(g->sprite_vs);RELEASE(g->state);RELEASE(g->vs);RELEASE(g->ps);RELEASE(g->constants);RELEASE(g->blend);RELEASE(g->raster);RELEASE(g->depth);RELEASE(g->context1);RELEASE(g->context);RELEASE(g->device);memset(g,0,sizeof(*g));}
static int graphics_draw(S14MapGraphics *g,ID3D11Texture2D *buffer,float cx,float cy,float radius,float scale,int troop,int selected,int detail,int red){
    if(!g || !g->state || !buffer || !isfinite(cx) || !isfinite(cy) || !isfinite(radius) || !isfinite(scale) || radius<(troop?3:8) || radius>256 || scale<=0 || scale>8)return 0;
    D3D11_TEXTURE2D_DESC desc;ID3D11Texture2D_GetDesc(buffer,&desc);if(!desc.Width || !desc.Height || desc.Width>16384 || desc.Height>16384)return 0;
    ID3D11RenderTargetView *rtv=NULL;HRESULT hr=ID3D11Device_CreateRenderTargetView(g->device,(ID3D11Resource*)buffer,NULL,&rtv);if(FAILED(hr)){g->error=hr;return 0;}
    ID3DDeviceContextState *previous=NULL;ID3D11DeviceContext1_SwapDeviceContextState(g->context1,g->state,&previous);
    D3D11_MAPPED_SUBRESOURCE map;hr=ID3D11DeviceContext_Map(g->context,(ID3D11Resource*)g->constants,0,D3D11_MAP_WRITE_DISCARD,0,&map);
    if(SUCCEEDED(hr)){
        float values[12]={cx,cy,radius,12*scale,(float)desc.Width,(float)desc.Height,scale,0,red?238/255.0f:183/255.0f,red?72/255.0f:99/255.0f,red?62/255.0f:246/255.0f,1};
        if(troop){values[3]=0;values[8]=detail?.19f:selected?.55f:.89f;values[9]=detail?.17f:selected?.26f:.88f;values[10]=detail?.14f:selected?.09f:.76f;
        }
        memcpy(map.pData,values,sizeof(values));ID3D11DeviceContext_Unmap(g->context,(ID3D11Resource*)g->constants,0);
        D3D11_VIEWPORT viewport={0,0,(float)desc.Width,(float)desc.Height,0,1};float factors[4]={0};
        ID3D11DeviceContext_OMSetRenderTargets(g->context,1,&rtv,NULL);ID3D11DeviceContext_OMSetBlendState(g->context,g->blend,factors,UINT_MAX);ID3D11DeviceContext_OMSetDepthStencilState(g->context,g->depth,0);
        ID3D11DeviceContext_RSSetState(g->context,g->raster);ID3D11DeviceContext_RSSetViewports(g->context,1,&viewport);
        ID3D11DeviceContext_IASetInputLayout(g->context,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(g->context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ID3D11DeviceContext_VSSetShader(g->context,g->vs,NULL,0);ID3D11DeviceContext_PSSetShader(g->context,troop?g->troop_ps:g->ps,NULL,0);ID3D11DeviceContext_VSSetConstantBuffers(g->context,0,1,&g->constants);ID3D11DeviceContext_PSSetConstantBuffers(g->context,0,1,&g->constants);
        if(SUCCEEDED(hr))ID3D11DeviceContext_Draw(g->context,4,0);
    }
    // The private state must hold no swap-chain buffer references across frames.
    ID3D11DeviceContext_ClearState(g->context);ID3D11DeviceContext1_SwapDeviceContextState(g->context1,previous,NULL);RELEASE(previous);RELEASE(rtv);
    g->error=hr;return SUCCEEDED(hr);
}
int s14_map_graphics_draw(S14MapGraphics *g,ID3D11Texture2D *b,float x,float y,float r,float s){return graphics_draw(g,b,x,y,r,s,0,0,0,0);}
int s14_troop_graphics_draw(S14MapGraphics *g,ID3D11Texture2D *b,float x,float y,float size,int selected,int detail){return graphics_draw(g,b,x,y,size*.5f,1,1,selected,detail,0);}
static int rect_graphics_draw(S14MapGraphics *g,ID3D11Texture2D *buffer,float cx,float cy,float width,float height,float alpha,int ammo,float ratio){
    if(!g||(!ammo&&(!g->caption_image||!g->caption_sampler))||!g->sprite_vs||!g->sprite_ps||!g->ammo_ps||!buffer||!isfinite(cx)||!isfinite(cy)||!isfinite(width)||width<8||width>1024||!isfinite(height)||height<(ammo?2:8)||height>512||!isfinite(alpha)||alpha<=0||alpha>1)return 0;
    D3D11_TEXTURE2D_DESC desc;ID3D11Texture2D_GetDesc(buffer,&desc);if(!desc.Width||!desc.Height||desc.Width>16384||desc.Height>16384)return 0;
    ID3D11RenderTargetView *rtv=NULL;HRESULT hr=ID3D11Device_CreateRenderTargetView(g->device,(ID3D11Resource*)buffer,NULL,&rtv);if(FAILED(hr)){g->error=hr;return 0;}
    ID3DDeviceContextState *previous=NULL;ID3D11DeviceContext1_SwapDeviceContextState(g->context1,g->state,&previous);
    D3D11_MAPPED_SUBRESOURCE map;hr=ID3D11DeviceContext_Map(g->context,(ID3D11Resource*)g->constants,0,D3D11_MAP_WRITE_DISCARD,0,&map);
    if(SUCCEEDED(hr)){
        int srgb=desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||desc.Format==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        float values[12]={cx,cy,width*.5f,height*.5f,(float)desc.Width,(float)desc.Height,g->caption_uv[0],g->caption_uv[1],g->caption_uv[2],g->caption_uv[3],alpha,(float)srgb};
        if(ammo)values[8]=ratio;
        memcpy(map.pData,values,sizeof(values));ID3D11DeviceContext_Unmap(g->context,(ID3D11Resource*)g->constants,0);
        D3D11_VIEWPORT viewport={0,0,(float)desc.Width,(float)desc.Height,0,1};float factors[4]={0};
        ID3D11DeviceContext_OMSetRenderTargets(g->context,1,&rtv,NULL);ID3D11DeviceContext_OMSetBlendState(g->context,g->blend,factors,UINT_MAX);ID3D11DeviceContext_OMSetDepthStencilState(g->context,g->depth,0);
        ID3D11DeviceContext_RSSetState(g->context,g->raster);ID3D11DeviceContext_RSSetViewports(g->context,1,&viewport);
        ID3D11DeviceContext_IASetInputLayout(g->context,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(g->context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ID3D11DeviceContext_VSSetShader(g->context,g->sprite_vs,NULL,0);ID3D11DeviceContext_PSSetShader(g->context,ammo?g->ammo_ps:g->sprite_ps,NULL,0);
        ID3D11DeviceContext_VSSetConstantBuffers(g->context,0,1,&g->constants);ID3D11DeviceContext_PSSetConstantBuffers(g->context,0,1,&g->constants);
        ID3D11DeviceContext_PSSetShaderResources(g->context,0,1,&g->caption_image);ID3D11DeviceContext_PSSetSamplers(g->context,0,1,&g->caption_sampler);ID3D11DeviceContext_Draw(g->context,4,0);
    }
    ID3D11DeviceContext_ClearState(g->context);ID3D11DeviceContext1_SwapDeviceContextState(g->context1,previous,NULL);RELEASE(previous);RELEASE(rtv);g->error=hr;return SUCCEEDED(hr);
}
int s14_caption_graphics_draw(S14MapGraphics *g,ID3D11Texture2D *b,float x,float y,float w,float h,float a){return rect_graphics_draw(g,b,x,y,w,h,a,0,0);}
int s14_ammo_graphics_draw(S14MapGraphics *g,ID3D11Texture2D *b,float x,float y,float w,float h,int n,int max){if(max<1||n<0||n>max)return 0;return rect_graphics_draw(g,b,x,y,w,h,1,1,(float)n/max);}
typedef HRESULT (STDMETHODCALLTYPE *Present)(IDXGISwapChain*,UINT,UINT);
typedef struct {float x,y,width,height,alpha;ULONGLONG tick;int visible;} Caption;
typedef struct {HWND owner,obstructions[4];uintptr_t base,dialogs[2];int enabled;unsigned int feature_flags,preferences;S14MapBatch batch,ammo;uintptr_t ammo_dialogs[2];ULONGLONG ammo_tick;S14MapFrame binding;Caption caption;ULONGLONG tick;} Control;
static SRWLOCK control_lock=SRWLOCK_INIT;static Control control;
static Present original_present;static void *present_entry;static volatile LONG installed,busy,visible;
static volatile LONG64 presents,draws,rejected,caption_draws,ammo_draws;static HRESULT render_error;static S14MapGraphics graphics;
static volatile LONG64 caption_painted_at;
static uintptr_t failed_device;static ULONGLONG retry_graphics;
static unsigned int last_calls,last_bytes;static float last_x,last_y;static LONG64 logged_draws=-1,logged_rejected=-1;static int logged_ready=-1;
static LONG64 icon_draws,logged_icons=-1;static float icon_x,icon_y;static int icon_detail;
static int memory(void *unused,uintptr_t at,void *out,size_t n){(void)unused;SIZE_T got;return ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
int s14_map_caption_drawn(ULONGLONG now){
    ULONGLONG painted=(ULONGLONG)InterlockedCompareExchange64(&caption_painted_at,0,0);
    return painted&&now>=painted&&now-painted<150;
}
void s14_map_caption_publish(float x,float y,float width,float height,float alpha,int show,ULONGLONG now){
    AcquireSRWLockExclusive(&control_lock);
    if(show&&isfinite(x)&&isfinite(y)&&isfinite(width)&&width>=8&&width<=256&&isfinite(height)&&height>=8&&height<=128&&isfinite(alpha)&&alpha>0&&alpha<=1)control.caption=(Caption){.x=x,.y=y,.width=width,.height=height,.alpha=alpha,.tick=now,.visible=1};
    else{control.caption=(Caption){0};InterlockedExchange64(&caption_painted_at,0);}
    ReleaseSRWLockExclusive(&control_lock);
}
void s14_map_render_publish(HWND owner,uintptr_t base,int enabled,const S14MapFrame *f,const uintptr_t dialogs[2],const HWND obstructions[4],ULONGLONG now){
    (void)now;int valid=enabled && f && !f->modal && f->scene.main_map && f->world && f->army && f->face && f->root && f->sampled_at;
    AcquireSRWLockExclusive(&control_lock);control.owner=owner;control.base=base;control.enabled=enabled;
    if(valid){control.binding=*f;control.tick=f->sampled_at;}
    else{memset(&control.binding,0,sizeof(control.binding));control.tick=0;}
    if(dialogs)memcpy(control.dialogs,dialogs,sizeof(control.dialogs));if(obstructions)memcpy(control.obstructions,obstructions,sizeof(control.obstructions));ReleaseSRWLockExclusive(&control_lock);
    if(!valid)InterlockedExchange(&visible,0);
}
void s14_map_render_publish_all(HWND owner,uintptr_t base,int enabled,unsigned int features,unsigned int preferences,const S14MapBatch *batch,const uintptr_t dialogs[2],const HWND obstructions[4],ULONGLONG now){
    AcquireSRWLockExclusive(&control_lock);control.owner=owner;control.base=base;control.enabled=enabled;control.feature_flags=features;control.preferences=preferences;control.tick=now;control.batch.count=0;
    if(enabled && batch && batch->count>=0 && batch->count<=500)control.batch=*batch;
    if(control.batch.count){control.binding=control.batch.frames[0];control.tick=control.binding.sampled_at;}else{memset(&control.binding,0,sizeof(control.binding));control.tick=0;}
    if(dialogs)memcpy(control.dialogs,dialogs,sizeof(control.dialogs));if(obstructions)memcpy(control.obstructions,obstructions,sizeof(control.obstructions));ReleaseSRWLockExclusive(&control_lock);
    if(!control.batch.count)InterlockedExchange(&visible,0);
}
static int ammo_qualify(void *ctx,uintptr_t world,uintptr_t army,int leader){(void)ctx;(void)world;(void)leader;int n,m;return s14_archer_ammo_snapshot(army,&n,&m)?3:0;}
void s14_map_ammo_publish(HWND owner,uintptr_t base,int on,ULONGLONG now){
 static S14MapCache cache;static ULONGLONG next;S14MapBatch batch={0};S14MapFrame f;
 if(on&&s14_archer_ammo_enabled()){
  if(now<next)return;next=now+33;
  if(s14_map_validate(memory,NULL,base))s14_map_capture_all(memory,NULL,base,&cache,now,&f,&batch,ammo_qualify,NULL);
 }
 AcquireSRWLockExclusive(&control_lock);control.owner=owner;control.base=base;control.ammo=batch;control.ammo_tick=now;memcpy(control.ammo_dialogs,cache.dialogs,sizeof(control.ammo_dialogs));ReleaseSRWLockExclusive(&control_lock);
}
static int control_current(const Control *c,ULONGLONG now){return c->enabled && c->owner && c->binding.world && c->binding.army && c->binding.face && c->binding.root && !c->binding.modal && c->binding.scene.main_map && c->tick && now>=c->tick && now-c->tick<=1000;}
static HRESULT STDMETHODCALLTYPE hooked_present(IDXGISwapChain *swap,UINT interval,UINT flags){
    DWORD error=GetLastError();InterlockedIncrement64(&presents);
    if(!(flags&DXGI_PRESENT_TEST) && InterlockedCompareExchange(&busy,1,0)==0){
        Control c;AcquireSRWLockShared(&control_lock);c=control;ReleaseSRWLockShared(&control_lock);int draw=0;
        if(control_current(&c,GetTickCount64()) && GetForegroundWindow()==c.owner && !IsIconic(c.owner)){
            int blocked=0;for(int i=0;i<4;i++)if(c.obstructions[i] && IsWindowVisible(c.obstructions[i]))blocked=1;
            DXGI_SWAP_CHAIN_DESC swap_desc;
            if(!blocked && SUCCEEDED(IDXGISwapChain_GetDesc(swap,&swap_desc)) && swap_desc.OutputWindow==c.owner){
                ID3D11Device *device=NULL;ID3D11Texture2D *buffer=NULL;HRESULT hr=IDXGISwapChain_GetDevice(swap,&IID_ID3D11Device,(void**)&device);
                if(SUCCEEDED(hr) && device!=graphics.device){ULONGLONG now=GetTickCount64();
                    if(failed_device==(uintptr_t)device && now<retry_graphics)hr=render_error;
                    else if(!s14_map_graphics_init(&graphics,device)){hr=graphics.error;failed_device=(uintptr_t)device;retry_graphics=now+5000;}
                    else{failed_device=0;retry_graphics=0;}}
                if(SUCCEEDED(hr))hr=IDXGISwapChain_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&buffer);
                if(SUCCEEDED(hr)){
                    D3D11_TEXTURE2D_DESC desc;ID3D11Texture2D_GetDesc(buffer,&desc);float scale=fminf(desc.Width/1920.0f,desc.Height/1080.0f);
                    int count=c.batch.count?c.batch.count:1;
                    for(int i=0;i<count;i++){const S14MapFrame *bound=c.batch.count?&c.batch.frames[i]:&c.binding;S14MapFrame frame;
                        int red=(c.feature_flags&S14_AI_RANDOM_AFFIX) && s14_ai_active((void*)bound->army);
                        int eligible=red?(c.preferences&4):((c.feature_flags&S14_CAO_REN_BUFF || !c.batch.count) && (c.preferences&1 || !c.batch.count) && s14_affix_active(bound->world,bound->leader?bound->leader:518));
                        if(!eligible || !s14_map_present_capture(memory,NULL,c.base,bound,c.dialogs,&frame))continue;
                        float cx=(desc.Width-1920*scale)*.5f+frame.center_x*scale,cy=(desc.Height-1080*scale)*.5f+frame.center_y*scale;
                        int painted=graphics_draw(&graphics,buffer,cx,cy,frame.radius*scale,scale,0,0,0,red);if(!painted)hr=graphics.error;draw|=painted;
                        last_calls=frame.calls;last_bytes=frame.bytes;last_x=cx;last_y=cy;
                    }
                }
                render_error=hr;RELEASE(buffer);RELEASE(device);
            }
        }
        ULONGLONG ammo_now=GetTickCount64();
        if(c.enabled&&c.ammo.count&&ammo_now>=c.ammo_tick&&ammo_now-c.ammo_tick<250&&GetForegroundWindow()==c.owner&&!IsIconic(c.owner)){
         int blocked=0;for(int i=0;i<4;i++)if(c.obstructions[i]&&IsWindowVisible(c.obstructions[i]))blocked=1;
         DXGI_SWAP_CHAIN_DESC sd;
         if(!blocked&&SUCCEEDED(IDXGISwapChain_GetDesc(swap,&sd))&&sd.OutputWindow==c.owner){
          ID3D11Device *device=NULL;ID3D11Texture2D *buffer=NULL;HRESULT hr=IDXGISwapChain_GetDevice(swap,&IID_ID3D11Device,(void**)&device);
          if(SUCCEEDED(hr)&&device!=graphics.device&&!s14_map_graphics_init(&graphics,device))hr=graphics.error;
          if(SUCCEEDED(hr))hr=IDXGISwapChain_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&buffer);
          if(SUCCEEDED(hr)){D3D11_TEXTURE2D_DESC d;ID3D11Texture2D_GetDesc(buffer,&d);float scale=fminf(d.Width/1920.f,d.Height/1080.f);
           for(int i=0;i<c.ammo.count;i++){S14MapFrame f;RECT rect;int remaining,maximum;
            if(!s14_map_present_capture(memory,NULL,c.base,&c.ammo.frames[i],c.ammo_dialogs,&f)||!s14_map_ammo_rect(memory,NULL,&f,&rect)||!s14_archer_ammo_snapshot(f.army,&remaining,&maximum))continue;
            float x=(d.Width-1920*scale)*.5f+(rect.left+rect.right)*.5f*scale,y=(d.Height-1080*scale)*.5f+(rect.top+rect.bottom)*.5f*scale;
            if(s14_ammo_graphics_draw(&graphics,buffer,x,y,(rect.right-rect.left)*scale,(rect.bottom-rect.top)*scale,remaining,maximum)){InterlockedIncrement64(&ammo_draws);draw=1;}
           }
          }render_error=hr;RELEASE(buffer);RELEASE(device);
         }
        }
        if(draw)InterlockedIncrement64(&draws);else InterlockedIncrement64(&rejected);
        /* Troop icons are UI content, independent of the map-only halo toggle.
           Read the actual current native page on every Present: never retain
           a rectangle after closing/switching the page. */
        S14TroopIconFrame icon;int blocked=0;for(int i=0;i<4;i++)if(c.obstructions[i] && IsWindowVisible(c.obstructions[i]))blocked=1;
        DXGI_SWAP_CHAIN_DESC desc;
        if(!blocked && c.owner && GetForegroundWindow()==c.owner && !IsIconic(c.owner) &&
           SUCCEEDED(IDXGISwapChain_GetDesc(swap,&desc)) && desc.OutputWindow==c.owner && s14_troop_icon_frame(&icon)){
            ID3D11Device *device=NULL;ID3D11Texture2D *buffer=NULL;HRESULT hr=IDXGISwapChain_GetDevice(swap,&IID_ID3D11Device,(void**)&device);
            if(SUCCEEDED(hr) && device!=graphics.device){ULONGLONG now=GetTickCount64();
                if(failed_device==(uintptr_t)device && now<retry_graphics)hr=render_error;
                else if(!s14_map_graphics_init(&graphics,device)){hr=graphics.error;failed_device=(uintptr_t)device;retry_graphics=now+5000;}
                else{failed_device=0;retry_graphics=0;}}
            if(SUCCEEDED(hr))hr=IDXGISwapChain_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&buffer);
            if(SUCCEEDED(hr)){D3D11_TEXTURE2D_DESC d;ID3D11Texture2D_GetDesc(buffer,&d);float s=fminf(d.Width/1920.f,d.Height/1080.f);
                if(s14_troop_graphics_draw(&graphics,buffer,(d.Width-1920*s)*.5f+icon.x*s,(d.Height-1080*s)*.5f+icon.y*s,icon.size*s,icon.selected,icon.detail)){
                    icon_draws++;icon_x=icon.x;icon_y=icon.y;icon_detail=icon.detail;
                }else hr=graphics.error;}
            if(FAILED(hr))render_error=hr;
            RELEASE(buffer);RELEASE(device);
        }
        ULONGLONG caption_now=GetTickCount64();int caption_blocked=0;
        for(int i=0;i<4;i++)if(c.obstructions[i]&&IsWindowVisible(c.obstructions[i]))caption_blocked=1;
        if(c.caption.visible&&c.caption.tick&&caption_now>=c.caption.tick&&caption_now-c.caption.tick<300&&
           !caption_blocked&&c.owner&&GetForegroundWindow()==c.owner&&!IsIconic(c.owner)&&
           SUCCEEDED(IDXGISwapChain_GetDesc(swap,&desc))&&desc.OutputWindow==c.owner){
            ID3D11Device *device=NULL;ID3D11Texture2D *buffer=NULL;HRESULT hr=IDXGISwapChain_GetDevice(swap,&IID_ID3D11Device,(void**)&device);
            if(SUCCEEDED(hr)&&device!=graphics.device){ULONGLONG now=GetTickCount64();
                if(failed_device==(uintptr_t)device&&now<retry_graphics)hr=render_error;
                else if(!s14_map_graphics_init(&graphics,device)){hr=graphics.error;failed_device=(uintptr_t)device;retry_graphics=now+5000;}
                else{failed_device=0;retry_graphics=0;}}
            if(SUCCEEDED(hr))hr=IDXGISwapChain_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&buffer);
            if(SUCCEEDED(hr)){
                D3D11_TEXTURE2D_DESC d;ID3D11Texture2D_GetDesc(buffer,&d);float scale=fminf(d.Width/1920.f,d.Height/1080.f);
                float cx=(d.Width-1920*scale)*.5f+c.caption.x*scale,cy=(d.Height-1080*scale)*.5f+c.caption.y*scale;
                if(s14_caption_graphics_draw(&graphics,buffer,cx,cy,c.caption.width*scale,c.caption.height*scale,c.caption.alpha)){
                    InterlockedIncrement64(&caption_draws);InterlockedExchange64(&caption_painted_at,(LONG64)caption_now);
                }else{InterlockedExchange64(&caption_painted_at,0);hr=FAILED(graphics.error)?graphics.error:E_FAIL;}
            }
            if(FAILED(hr))render_error=hr;RELEASE(buffer);RELEASE(device);
        }
        InterlockedExchange(&visible,draw);InterlockedExchange(&busy,0);
    }
    SetLastError(error);return original_present(swap,interval,flags);
}
int s14_map_render_install(void){
    if(InterlockedCompareExchange(&installed,0,0))return 1;
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"S14 graphics initialization",WS_POPUP,0,0,32,32,NULL,NULL,GetModuleHandleW(NULL),NULL);if(!window)return 0;
    DXGI_SWAP_CHAIN_DESC desc={.BufferDesc={.Width=32,.Height=32,.Format=DXGI_FORMAT_R8G8B8A8_UNORM},.SampleDesc={1,0},.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT,.BufferCount=1,.OutputWindow=window,.Windowed=TRUE,.SwapEffect=DXGI_SWAP_EFFECT_DISCARD};
    ID3D11Device *device=NULL;ID3D11DeviceContext *context=NULL;IDXGISwapChain *swap=NULL;
    HRESULT hr=D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&desc,&swap,&device,NULL,&context);
    if(SUCCEEDED(hr)){
        present_entry=(void*)swap->lpVtbl->Present;MEMORY_BASIC_INFORMATION info;wchar_t module[MAX_PATH];
        int valid=VirtualQuery(present_entry,&info,sizeof(info)) && GetModuleFileNameW(info.AllocationBase,module,MAX_PATH) && wcsrchr(module,L'\\') && !_wcsicmp(wcsrchr(module,L'\\')+1,L"dxgi.dll");
        if(valid){MH_STATUS status=MH_CreateHook(present_entry,hooked_present,(void**)&original_present);if(status==MH_OK){status=MH_EnableHook(present_entry);if(status==MH_OK)InterlockedExchange(&installed,1);else MH_RemoveHook(present_entry);}if(status!=MH_OK)hr=E_FAIL;}else hr=E_NOINTERFACE;
    }
    RELEASE(swap);RELEASE(context);RELEASE(device);DestroyWindow(window);render_error=hr;return s14_map_render_ready();
}
int s14_map_render_ready(void){return InterlockedCompareExchange(&installed,0,0)!=0;}
int s14_map_render_visible(void){return InterlockedCompareExchange(&visible,0,0)!=0;}
int s14_map_render_log(char *out,size_t size){
    static LONG64 logged_captions=-1,logged_ammo=-1;static int logged_caption_visible=-1;static HRESULT logged_error=1;
    int caption_visible;AcquireSRWLockShared(&control_lock);caption_visible=control.caption.visible;ReleaseSRWLockShared(&control_lock);
    LONG64 captions=InterlockedCompareExchange64(&caption_draws,0,0);
    LONG64 d=InterlockedCompareExchange64(&draws,0,0),r=InterlockedCompareExchange64(&rejected,0,0);int ready=s14_map_render_ready();
    if(logged_ready==ready && (logged_draws>0)==(d>0) && (logged_rejected>0)==(r>0) && (logged_icons>0)==(icon_draws>0)&&(logged_captions>0)==(captions>0)&&(logged_ammo>0)==(ammo_draws>0)&&logged_caption_visible==caption_visible&&logged_error==render_error)return 0;
    logged_ready=ready;logged_draws=d;logged_rejected=r;logged_icons=icon_draws;logged_captions=captions;logged_ammo=ammo_draws;logged_caption_visible=caption_visible;logged_error=render_error;
    snprintf(out,size,"{\"event\":\"map_frame_renderer\",\"ready\":%d,\"presents\":%lld,\"draws\":%lld,\"skipped\":%lld,\"error\":%ld,\"read_calls\":%u,\"read_bytes\":%u,\"center\":[%.3f,%.3f],\"troop_icon_draws\":%lld,\"troop_icon_center\":[%.3f,%.3f],\"troop_icon_detail\":%d,\"caption_pending\":%d,\"caption_draws\":%lld,\"ammo_draws\":%lld,\"backend\":\"D3D11_Present\"}\n",ready,(long long)presents,(long long)d,(long long)r,(long)render_error,last_calls,last_bytes,last_x,last_y,(long long)icon_draws,icon_x,icon_y,icon_detail,caption_visible,(long long)captions,(long long)ammo_draws);return 1;
}
