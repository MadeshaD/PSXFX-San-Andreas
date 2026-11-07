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
