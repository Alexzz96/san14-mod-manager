#ifndef S14_MAP_RENDER_H
#define S14_MAP_RENDER_H
#define COBJMACROS
#include <d3d11_1.h>
#include "map_effects_model.h"
typedef struct {
    ID3D11Device *device;ID3D11DeviceContext *context;ID3D11DeviceContext1 *context1;
    ID3DDeviceContextState *state;ID3D11VertexShader *vs;ID3D11PixelShader *ps;ID3D11Buffer *constants;
    ID3D11BlendState *blend;ID3D11RasterizerState *raster;ID3D11DepthStencilState *depth;
    ID3D11PixelShader *troop_ps;
    HRESULT error;
} S14MapGraphics;
int s14_map_graphics_init(S14MapGraphics*,ID3D11Device*);
int s14_map_graphics_draw(S14MapGraphics*,ID3D11Texture2D*,float,float,float,float);
int s14_troop_graphics_draw(S14MapGraphics*,ID3D11Texture2D*,float,float,float,int,int);
void s14_map_graphics_destroy(S14MapGraphics*);
int s14_map_render_install(void);
int s14_map_render_ready(void);
void s14_map_render_publish_all(HWND,uintptr_t,int,unsigned int,unsigned int,const S14MapBatch*,const uintptr_t[2],const HWND[4],ULONGLONG);
void s14_map_render_publish(HWND,uintptr_t,int,const S14MapFrame*,const uintptr_t[2],const HWND[4],ULONGLONG);
int s14_map_render_visible(void);
int s14_map_render_log(char*,size_t);
#endif
