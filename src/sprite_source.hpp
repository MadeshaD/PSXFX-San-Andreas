static const char kSprite[] = R"PSXSPR(
#ifdef PIXEL_STAGE
// BEGIN_TEXTURE_GRID
// p is D3D9 VPOS: integer raster centers. Size=1 is an exact bypass.
float2 GridUV(float2 uv,float2 p,float size) {
 float2 dx=ddx(uv),dy=ddy(uv);
 float2 center=floor((p+0.5)/size)*size+(size-1.0)*0.5;
 return uv+dx*(center.x-p.x)+dy*(center.y-p.y);
}
// Reconstruct perspective UV at the same raster cell center using linear uv/w and 1/w.
float2 GridPerspectiveUV(float2 uv,float invW,float2 p,float size) {
 float3 h=float3(uv*invW,invW);
 float2 center=floor((p+0.5)/size)*size+(size-1.0)*0.5;
 float3 sampled=h+ddx(h)*(center.x-p.x)+ddy(h)*(center.y-p.y);
 return sampled.z>0.000001?sampled.xy/sampled.z:uv;
}
// END_TEXTURE_GRID
#endif
// Simple one-stage fixed-function pixel path. Original vertex processing retained.
sampler2D Image : register(s0);
float4 RasterGrid : register(c0); // block size, reserved
float4 StageColor : register(c1); // operation, argument1 selector, argument2 selector, reserved
float4 StageAlpha : register(c2);
float4 Factor : register(c3);
float4 Argument(float selector,float4 diffuse,float4 tex) {
 // CPU maps D3D arguments to base 0=diffuse/current, 1=texture, 2=tfactor.
 // +4=alpha replicate; +8=complement.
 float complement=step(7.5,selector);selector-=complement*8.0;
 float replicate=step(3.5,selector);selector-=replicate*4.0;
 float4 x=selector<0.5?diffuse:(selector<1.5?tex:Factor);
 x=lerp(x,x.aaaa,replicate);return lerp(x,1.0-x,complement);
}
float4 Combine(float op,float4 a,float4 b) {
 if(op<2.5)return a; // SELECTARG1=2
 if(op<3.5)return b; // SELECTARG2=3
 if(op<4.5)return a*b; // MODULATE=4
 if(op<5.5)return saturate(2.0*a*b);
 if(op<6.5)return saturate(4.0*a*b);
 return saturate(a+b); // ADD=7
}
float4 SpritePS(float2 uv:TEXCOORD0,float4 color:COLOR0,float2 p:VPOS):COLOR0 {
 float2 snapped=GridUV(uv,p,RasterGrid.x);
 float4 tex=tex2Dgrad(Image,snapped,ddx(uv),ddy(uv));
 float4 rgb=Combine(StageColor.x,Argument(StageColor.y,color,tex),Argument(StageColor.z,color,tex));
 float4 a=Combine(StageAlpha.x,Argument(StageAlpha.y,color,tex),Argument(StageAlpha.z,color,tex));
 return float4(rgb.rgb,a.a);
}
)PSXSPR";
