#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include "map_render.h"
#include "MinHook.h"
#include "map_render.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"FAILED %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define OK(x) REQUIRE(SUCCEEDED(x))
#undef RELEASE
#define RELEASE(p) do{if(p){IUnknown_Release((IUnknown*)p);p=NULL;}}while(0)
static ID3D11Texture2D *texture(ID3D11Device *device,UINT bind){D3D11_TEXTURE2D_DESC d={.Width=256,.Height=256,.MipLevels=1,.ArraySize=1,.Format=DXGI_FORMAT_R8G8B8A8_UNORM,.SampleDesc={1,0},.Usage=D3D11_USAGE_DEFAULT,.BindFlags=bind};ID3D11Texture2D *t=NULL;OK(ID3D11Device_CreateTexture2D(device,&d,NULL,&t));return t;}
int main(void){
    S14MapFrame binding={.world=1,.army=2,.root=3,.face=4,.sampled_at=100,.scene={.main_map=1}};
    s14_map_render_publish((HWND)5,6,1,&binding,NULL,NULL,100);REQUIRE(control_current(&control,100));
    // Republishing old coordinates must not extend the sample's lease.
    s14_map_render_publish((HWND)5,6,1,&binding,NULL,NULL,1101);REQUIRE(!control_current(&control,1101));
    s14_map_render_publish((HWND)5,6,0,&binding,NULL,NULL,1200);REQUIRE(!control_current(&control,1200) && !control.binding.army && !control.tick);
    binding.sampled_at=1300;s14_map_render_publish((HWND)5,6,1,&binding,NULL,NULL,1300);REQUIRE(control_current(&control,1300));
    binding.modal=1;s14_map_render_publish((HWND)5,6,1,&binding,NULL,NULL,1301);REQUIRE(!control.binding.army && !control_current(&control,1301));
    binding.modal=0;binding.sampled_at=1400;s14_map_render_publish((HWND)5,6,1,&binding,NULL,NULL,1400);REQUIRE(control_current(&control,1400));
    s14_map_render_publish((HWND)5,6,1,NULL,NULL,NULL,1401);REQUIRE(!control.binding.army && !control_current(&control,1401));
    binding.scene.main_map=0;s14_map_render_publish((HWND)5,6,1,&binding,NULL,NULL,1402);REQUIRE(!control.binding.army && !control_current(&control,1402));
    binding.scene.main_map=1;S14MapBatch batch={.count=2};batch.frames[0]=batch.frames[1]=binding;
    s14_map_render_publish_all((HWND)5,6,1,S14_AI_RANDOM_AFFIX,12,&batch,NULL,NULL,1402);REQUIRE(control.batch.count==2 && control.tick==1400);
    s14_map_render_publish_all((HWND)5,6,1,S14_AI_RANDOM_AFFIX,12,&batch,NULL,NULL,2401);REQUIRE(!control_current(&control,2401));
    s14_map_render_publish_all((HWND)5,6,0,S14_AI_RANDOM_AFFIX,12,&batch,NULL,NULL,1402);REQUIRE(!control.batch.count && !control.binding.army);
    ID3D11Device *device=NULL;ID3D11DeviceContext *ctx=NULL;OK(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&device,NULL,&ctx));
    S14MapGraphics g={0};REQUIRE(s14_map_graphics_init(&g,device));
    ID3D11Texture2D *t=texture(device,D3D11_BIND_RENDER_TARGET),*extra=texture(device,D3D11_BIND_RENDER_TARGET),*resource=texture(device,D3D11_BIND_SHADER_RESOURCE);
    ID3D11RenderTargetView *rt[2]={0};OK(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource*)t,NULL,&rt[0]));OK(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource*)extra,NULL,&rt[1]));
    ID3D11ShaderResourceView *srv=NULL;OK(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)resource,NULL,&srv));
    D3D11_BUFFER_DESC cb={.ByteWidth=64,.BindFlags=D3D11_BIND_CONSTANT_BUFFER};ID3D11Buffer *native_cb=NULL;OK(ID3D11Device_CreateBuffer(device,&cb,NULL,&native_cb));
    D3D11_BUFFER_DESC vb={.ByteWidth=64,.BindFlags=D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_INDEX_BUFFER};ID3D11Buffer *native_vb=NULL;OK(ID3D11Device_CreateBuffer(device,&vb,NULL,&native_vb));
    D3D11_VIEWPORT viewports[2]={{3,5,180,170,.1f,.9f},{15,20,100,90,.2f,.8f}};RECT scissors[2]={{4,5,160,170},{15,20,60,70}};
    float color[4]={.2f,.3f,.4f,1},factors[4]={.1f,.2f,.3f,.4f};UINT stride=12,offset=16;
    ID3D11DeviceContext_OMSetRenderTargets(ctx,2,rt,NULL);ID3D11DeviceContext_OMSetBlendState(ctx,g.blend,factors,0xff11ff11);ID3D11DeviceContext_OMSetDepthStencilState(ctx,g.depth,37);
    ID3D11DeviceContext_RSSetState(ctx,g.raster);ID3D11DeviceContext_RSSetViewports(ctx,2,viewports);ID3D11DeviceContext_RSSetScissorRects(ctx,2,scissors);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);ID3D11DeviceContext_IASetVertexBuffers(ctx,3,1,&native_vb,&stride,&offset);ID3D11DeviceContext_IASetIndexBuffer(ctx,native_vb,DXGI_FORMAT_R16_UINT,4);
    ID3D11DeviceContext_VSSetShader(ctx,g.vs,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,g.ps,NULL,0);ID3D11DeviceContext_VSSetConstantBuffers(ctx,0,1,&native_cb);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&native_cb);ID3D11DeviceContext_PSSetShaderResources(ctx,3,1,&srv);
    for(int i=0;i<8;i++){
        REQUIRE(s14_troop_graphics_draw(&g,t,128,128,48,i&1,i&2));
        ID3D11DeviceContext_ClearRenderTargetView(ctx,rt[0],color);REQUIRE(s14_map_graphics_draw(&g,t,128.25f+i*.25f,127.75f,32,1));
        ID3D11RenderTargetView *after_rt[2]={0};ID3D11DeviceContext_OMGetRenderTargets(ctx,2,after_rt,NULL);REQUIRE(after_rt[0]==rt[0] && after_rt[1]==rt[1]);RELEASE(after_rt[0]);RELEASE(after_rt[1]);
        ID3D11BlendState *blend=NULL;float after_factors[4];UINT mask=0;ID3D11DeviceContext_OMGetBlendState(ctx,&blend,after_factors,&mask);REQUIRE(blend==g.blend && !memcmp(factors,after_factors,sizeof(factors)) && mask==0xff11ff11);RELEASE(blend);
        ID3D11DepthStencilState *depth=NULL;UINT reference=0;ID3D11DeviceContext_OMGetDepthStencilState(ctx,&depth,&reference);REQUIRE(depth==g.depth && reference==37);RELEASE(depth);
        ID3D11RasterizerState *raster=NULL;ID3D11DeviceContext_RSGetState(ctx,&raster);REQUIRE(raster==g.raster);RELEASE(raster);
        D3D11_VIEWPORT after_vp[2];UINT count=2;ID3D11DeviceContext_RSGetViewports(ctx,&count,after_vp);REQUIRE(count==2 && !memcmp(viewports,after_vp,sizeof(viewports)));
        RECT after_scissors[2];count=2;ID3D11DeviceContext_RSGetScissorRects(ctx,&count,after_scissors);REQUIRE(count==2 && !memcmp(scissors,after_scissors,sizeof(scissors)));
        D3D11_PRIMITIVE_TOPOLOGY topology;ID3D11DeviceContext_IAGetPrimitiveTopology(ctx,&topology);REQUIRE(topology==D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        ID3D11Buffer *buffer=NULL;UINT after_stride,after_offset;ID3D11DeviceContext_IAGetVertexBuffers(ctx,3,1,&buffer,&after_stride,&after_offset);REQUIRE(buffer==native_vb && stride==after_stride && offset==after_offset);RELEASE(buffer);
        DXGI_FORMAT format;ID3D11DeviceContext_IAGetIndexBuffer(ctx,&buffer,&format,&after_offset);REQUIRE(buffer==native_vb && format==DXGI_FORMAT_R16_UINT && after_offset==4);RELEASE(buffer);
        ID3D11DeviceContext_VSGetConstantBuffers(ctx,0,1,&buffer);REQUIRE(buffer==native_cb);RELEASE(buffer);ID3D11DeviceContext_PSGetConstantBuffers(ctx,0,1,&buffer);REQUIRE(buffer==native_cb);RELEASE(buffer);
        ID3D11ShaderResourceView *after_srv=NULL;ID3D11DeviceContext_PSGetShaderResources(ctx,3,1,&after_srv);REQUIRE(after_srv==srv);RELEASE(after_srv);
        ID3D11VertexShader *vs=NULL;ID3D11PixelShader *ps=NULL;ID3D11DeviceContext_VSGetShader(ctx,&vs,NULL,NULL);ID3D11DeviceContext_PSGetShader(ctx,&ps,NULL,NULL);REQUIRE(vs==g.vs && ps==g.ps);RELEASE(vs);RELEASE(ps);
    }
    D3D11_TEXTURE2D_DESC desc;ID3D11Texture2D_GetDesc(t,&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D *staging=NULL;OK(ID3D11Device_CreateTexture2D(device,&desc,NULL,&staging));ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)staging,(ID3D11Resource*)t);
    D3D11_MAPPED_SUBRESOURCE data;OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)staging,0,D3D11_MAP_READ,0,&data));unsigned char *pixels=data.pData,*center=pixels+128*data.RowPitch+128*4,*outside=pixels+10*data.RowPitch+10*4,*ring=pixels+128*data.RowPitch+165*4;
    REQUIRE(!memcmp(center,outside,4) && ring[0]>center[0]+30 && ring[2]>center[2]+30);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)staging,0);
    ID3D11DeviceContext_ClearRenderTargetView(ctx,rt[0],color);
    REQUIRE(graphics_draw(&g,t,64,128,24,1,0,0,0,1));REQUIRE(graphics_draw(&g,t,192,128,24,1,0,0,0,0));
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)staging,(ID3D11Resource*)t);
    OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)staging,0,D3D11_MAP_READ,0,&data));pixels=data.pData;
    unsigned char *red=pixels+128*data.RowPitch+90*4,*purple=pixels+128*data.RowPitch+218*4;
    REQUIRE(red[0]>red[2]+60 && purple[2]>purple[0]+20);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)staging,0);
    ID3D11DeviceContext_ClearRenderTargetView(ctx,rt[0],color);
    REQUIRE(s14_troop_graphics_draw(&g,t,64,64,96,0,0));REQUIRE(s14_troop_graphics_draw(&g,t,192,64,96,1,0));
    REQUIRE(s14_troop_graphics_draw(&g,t,64,192,48,1,1));REQUIRE(s14_troop_graphics_draw(&g,t,160,192,24,1,1));
    REQUIRE(!s14_troop_graphics_draw(&g,t,NAN,64,48,1,0));
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)staging,(ID3D11Resource*)t);
    OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)staging,0,D3D11_MAP_READ,0,&data));
    pixels=data.pData;unsigned char *gold=pixels+40*data.RowPitch+64*4,*dark=pixels+64*data.RowPitch+80*4;
    REQUIRE(gold[0]>dark[0]+40 && gold[1]>dark[1]+40);
    FILE *bmp=fopen("troop-icon-preview.bmp","wb");REQUIRE(bmp);BITMAPFILEHEADER file={.bfType=0x4d42,.bfSize=54+256*256*4,.bfOffBits=54};
    BITMAPINFOHEADER info={.biSize=40,.biWidth=256,.biHeight=-256,.biPlanes=1,.biBitCount=32,.biSizeImage=256*256*4};
    fwrite(&file,1,sizeof(file),bmp);fwrite(&info,1,sizeof(info),bmp);
    for(int y=0;y<256;y++){unsigned char row[1024];memcpy(row,pixels+y*data.RowPitch,1024);for(int x=0;x<256;x++){unsigned char b=row[x*4];row[x*4]=row[x*4+2];row[x*4+2]=b;}fwrite(row,1,1024,bmp);}fclose(bmp);
    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)staging,0);
    ID3D11DeviceContext_ClearState(ctx);RELEASE(staging);RELEASE(rt[0]);RELEASE(rt[1]);RELEASE(t);RELEASE(extra);RELEASE(srv);RELEASE(resource);RELEASE(native_cb);RELEASE(native_vb);s14_map_graphics_destroy(&g);RELEASE(ctx);RELEASE(device);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Private GPU test",WS_POPUP,0,0,64,64,NULL,NULL,GetModuleHandleW(NULL),NULL);REQUIRE(window);
    DXGI_SWAP_CHAIN_DESC swap_desc={.BufferDesc={.Width=64,.Height=64,.Format=DXGI_FORMAT_R8G8B8A8_UNORM},.SampleDesc={1,0},.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT,.BufferCount=1,.OutputWindow=window,.Windowed=TRUE,.SwapEffect=DXGI_SWAP_EFFECT_DISCARD};IDXGISwapChain *swap=NULL;
    OK(D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&swap_desc,&swap,&device,NULL,&ctx));REQUIRE(s14_map_graphics_init(&g,device));ID3D11Texture2D *back=NULL;OK(IDXGISwapChain_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&back));REQUIRE(s14_map_graphics_draw(&g,back,32,32,12,1));RELEASE(back);OK(IDXGISwapChain_ResizeBuffers(swap,1,128,128,DXGI_FORMAT_UNKNOWN,0));
    HRESULT before=IDXGISwapChain_Present(swap,0,DXGI_PRESENT_TEST);REQUIRE(MH_Initialize()==MH_OK && s14_map_render_install());HRESULT after=IDXGISwapChain_Present(swap,0,DXGI_PRESENT_TEST);REQUIRE(before==after);
    char log[1024];REQUIRE(s14_map_render_log(log,sizeof(log)) && strstr(log,"\"presents\":1") && strstr(log,"\"draws\":0"));REQUIRE(MH_DisableHook(MH_ALL_HOOKS)==MH_OK && MH_Uninitialize()==MH_OK);
    s14_map_graphics_destroy(&g);RELEASE(swap);RELEASE(ctx);RELEASE(device);DestroyWindow(window);
    puts("{\"status\":\"passed\",\"warp_gpu_render\":true,\"native_pipeline_restored\":true,\"transparent_center\":true,\"ring_pixels_verified\":true,\"fractional_movement\":true,\"no_retained_backbuffer\":true,\"resize_buffers\":true,\"actual_present_hook\":true,\"present_hresult_preserved\":true,\"present_test_flag_skipped\":true,\"blocked_ui_clears_binding\":true,\"modal_and_missing_frame_clear_binding\":true,\"stale_samples_not_renewed\":true,\"map_resumes_after_close\":true}");return 0;
}
