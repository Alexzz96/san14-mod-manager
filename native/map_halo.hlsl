cbuffer Halo : register(b0) { float4 circle; float4 viewport; float4 color; };
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    float2 corner=float2((id&1)?1:-1,(id&2)?1:-1);
    float2 pixel=circle.xy+corner*(circle.z+circle.w);
    return float4(pixel.x*2/viewport.x-1,1-pixel.y*2/viewport.y,0,1);
}
float4 ps(float4 pos : SV_POSITION) : SV_TARGET {
    float distance=length(pos.xy-circle.xy),radius=circle.z+3*viewport.z;
    if(distance<circle.z+viewport.z) discard;
    float delta=abs(distance-radius),glow=9*viewport.z;
    float alpha=delta<2*viewport.z ? .83*(1-delta*.18/viewport.z) : .4*exp(-delta*delta/(glow*glow));
    float fade=saturate((circle.z+circle.w-distance)/(circle.w-5*viewport.z));
    if(distance>radius+2*viewport.z) alpha*=fade*fade;
    return float4(color.rgb*alpha,alpha);
}
float edge(float2 p,float2 a,float2 b){float2 d=b-a;return length(p-a-d*saturate(dot(p-a,d)/dot(d,d)));}
// A deliberately simple silhouette remains legible at the native 24px size.
float4 troop_ps(float4 pos : SV_POSITION) : SV_TARGET {
    float2 p=(pos.xy-circle.xy)/circle.z*64+64;
    float3 gold=lerp(float3(.67,.45,.16),float3(1,.91,.56),saturate(1-p.y/128));
    float3 bg=color.rgb;
    float pole=min(edge(p,float2(22,109),float2(101,16)),edge(p,float2(106,109),float2(27,16)));
    pole=min(pole,min(edge(p,float2(101,16),float2(114,31)),edge(p,float2(27,16),float2(14,31))));
    pole=min(pole,min(edge(p,float2(114,31),float2(99,46)),edge(p,float2(14,31),float2(29,46))));
    float aa=128/max(1,2*circle.z);float shaft=1-smoothstep(3-aa*.5,3+aa*.5,pole);
    float3 ink=lerp(bg,gold,shaft);
    float x=abs(p.x-64),y=p.y;
    float span=y<36 ? (y-22)*31/14 : y<75 ? 31 : (113-y)*31/38;
    float inside=min(min(y-22,113-y),span-x);
    float mask=smoothstep(-aa*.5,aa*.5,inside);
    float border=1-smoothstep(3-aa*.5,3+aa*.5,inside);
    float3 steel=lerp(float3(.09,.12,.15),float3(.27,.31,.34),saturate(1-y/128));
    float seam=min(edge(p,float2(64,32),float2(64,98)),min(edge(p,float2(41,49),float2(64,73)),edge(p,float2(87,49),float2(64,73))));
    float detail=1-smoothstep(2-aa*.5,2+aa*.5,seam);
    float3 shield=lerp(steel,gold,max(border,detail*.9));
    return float4(lerp(ink,shield,mask),1);
}
struct SpriteOut { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
SpriteOut sprite_vs(uint id : SV_VertexID) {
    float2 corner=float2((id&1)?1:-1,(id&2)?1:-1);
    float2 pixel=circle.xy+corner*circle.zw;
    SpriteOut output;
    output.position=float4(pixel.x*2/viewport.x-1,1-pixel.y*2/viewport.y,0,1);
    output.uv=viewport.zw+float2((id&1)?1:0,(id&2)?1:0)*color.xy;
    return output;
}
Texture2D caption_image : register(t0);
SamplerState caption_sampler : register(s0);
float4 sprite_ps(SpriteOut input) : SV_TARGET {
    float alpha=caption_image.Sample(caption_sampler,input.uv).a*color.z;
    float3 ink=float3(70.0,45.0,39.0)/255.0;
    // The game's sRGB backbuffer encodes linear shader output. Convert the
    // requested sRGB ink first; UNORM targets use the byte colors directly.
    if(color.w>.5)ink=pow((ink+.055)/1.055,2.4);
    return float4(ink*alpha,alpha);
}
float4 ammo_ps(SpriteOut input) : SV_TARGET {
    float2 p=input.position.xy-(circle.xy-circle.zw);
    float2 size=2*circle.zw;
    float border=max(1,size.y/6);
    float3 ink=float3(.16,.18,.18);
    if(p.x<border||p.x>=size.x-border||p.y<border||p.y>=size.y-border)
        ink=color.x>0?float3(.73,.70,.51):float3(.76,.28,.18);
    else if((p.x-border)/(size.x-2*border)<color.x)
        ink=lerp(float3(.24,.58,.68),float3(.53,.82,.86),1-p.y/size.y);
    if(color.w>.5)ink=pow((ink+.055)/1.055,2.4);
    return float4(ink,1);
}
