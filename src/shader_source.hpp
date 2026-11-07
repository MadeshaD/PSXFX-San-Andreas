static const char kShader[] = R"PSXHLSL(
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
#ifndef PIXEL_STAGE
// PSX SA v0.5. Eligible fixed-function geometry, shader model 3.
row_major float4x4 WVP : register(c0);
row_major float4x4 World : register(c4);
row_major float4x4 NormalMatrix : register(c8);
float4 Grid : register(c12); // width, height, snap amount, PS2 building modulation
float4 MatDiffuse : register(c13);
float4 MatAmbient : register(c14);
float4 MatEmissive : register(c15);
float4 GlobalAmbient : register(c16);
float4 Sources : register(c17); // diffuse, ambient, emissive, lighting enabled
float4 Directions[8] : register(c18);
float4 LightColors[8] : register(c26);
float4 LightAmbient[8] : register(c34);
row_major float4x4 View : register(c42);
float4 Fog : register(c46); // enabled, mode, start, end
float4 Misc : register(c47); // fog density, range fog, snap type, clip precision
struct Input {
 float3 pos : POSITION0;
#ifndef NO_COLOR
 float4 color : COLOR0;
#endif
#ifndef NO_UV
 float2 uv : TEXCOORD0;
#endif
#ifdef HAS_NORMAL
 float3 normal : NORMAL0;
#endif
};
struct Output {
 float4 pos : POSITION0;
 float4 color : COLOR0;
 float3 uvw : TEXCOORD0;
 float3 viewPos : TEXCOORD1;
 float2 baseUV : TEXCOORD2;
 float3 weightedRGB : TEXCOORD3;
};
float4 SelectSource(float mode, float4 material, float4 color) {
 return mode > 0.5 ? color : material;
}
// BEGIN_GEOMETRY_POSITION
float4 GeometryPosition(float4 p,float4 Grid,float4 Misc) {
 // Do not snap points behind or extremely close to the eye plane.
 float snapAmount=Grid.z;
 if (p.w > 0.0009765625 && snapAmount > 0.0) {
  float2 target;
  if(Misc.z<0.5) {
  float2 s = (p.xy / p.w * float2(0.5,-0.5) + 0.5) * Grid.xy;
  float2 snapped = floor(s + 0.5);
  target = ((snapped / Grid.xy - 0.5) * float2(2,-2)) * p.w;
  } else if(Misc.z<1.5) {
   float2 resolution=Grid.xy*0.5;
   target=floor(p.xy/p.w*resolution)/resolution*p.w;
  } else {
   target=floor(p.xy*Misc.w)/Misc.w;
  }
  p.xy=lerp(p.xy,target,snapAmount);
 }
 return p;
}
// END_GEOMETRY_POSITION
Output VSMain(Input i) {
 Output o;
#ifdef NO_COLOR
 float4 inputColor=float4(1,1,1,1);
#else
 float4 inputColor=i.color;
#endif
#ifdef NO_UV
 float2 inputUV=float2(0,0);
#else
 float2 inputUV=i.uv;
#endif
 float4 p = mul(float4(i.pos,1), WVP);
 p=GeometryPosition(p,Grid,Misc);
 o.pos = p; // z and w remain unchanged: keep D3D clipping/depth.
 // Carry both interpolants; blend in the pixel shader, not through clip.w.
 o.uvw = float3(inputUV * p.w, p.w);
 o.baseUV = inputUV;
 o.color = inputColor;
 if (Sources.w > 0.5) {
  float4 diffuse = SelectSource(Sources.x,MatDiffuse,inputColor);
  float3 ambient = SelectSource(Sources.y,MatAmbient,inputColor).rgb;
  float3 emissive = SelectSource(Sources.z,MatEmissive,inputColor).rgb;
  float3 color = emissive + ambient * GlobalAmbient.rgb;
#ifdef HAS_NORMAL
  float3 n = mul(float4(i.normal,0),NormalMatrix).xyz;
  n /= max(length(n),0.000001);
  [unroll] for (int j=0;j<8;j++) {
   color += ambient*LightAmbient[j].rgb;
   color += diffuse.rgb*LightColors[j].rgb*saturate(dot(n,-Directions[j].xyz));
  }
#else
  [unroll] for (int j=0;j<8;j++) color += ambient*LightAmbient[j].rgb;
#endif
  o.color = float4(saturate(color),diffuse.a);
 }
 o.weightedRGB = saturate(o.color.rgb) * p.w;
 float4 wp = mul(float4(i.pos,1),World);
 o.viewPos = mul(wp,View).xyz;
 return o;
}
// Inputs are endpoints from each actual mesh, transformed using its own world
// matrix. Interpolate AFTER snapping the endpoints, including clip Z and W.
// BEGIN_STITCH_POSITION
float4 BridgePosition(float4 a,float4 b,float t,float4 grid,float4 misc){
 return lerp(GeometryPosition(a,grid,misc),GeometryPosition(b,grid,misc),t);
}
float4 FaceBridgePosition(float4 a,float4 b,float4 c,float2 weight,float4 grid,float4 misc){
 return BridgePosition(a,b,weight.x,grid,misc)+weight.y*(GeometryPosition(c,grid,misc)-GeometryPosition(a,grid,misc));
}
// END_STITCH_POSITION
Output StitchVS(Input i,float4 a:TEXCOORD3,float4 b:TEXCOORD4,float2 t:TEXCOORD5,float4 c:TEXCOORD6){
 Output o=VSMain(i);
 o.pos=FaceBridgePosition(a,b,c,t,Grid,Misc);
 o.uvw=float3(o.baseUV*o.pos.w,o.pos.w);
 o.weightedRGB=saturate(o.color.rgb)*o.pos.w;
 return o;
}
#else
struct Output {
 float4 pos : POSITION0;
 float4 color : COLOR0;
 float3 uvw : TEXCOORD0;
 float3 viewPos : TEXCOORD1;
 float2 baseUV : TEXCOORD2;
 float3 weightedRGB : TEXCOORD3;
};
// Pixel constants occupy a different register bank from vertex constants.
float4 FogColor : register(c0);
float4 PSFog : register(c1); // enabled, mode, start, end
float4 PSMisc : register(c2); // density, range fog
float4 TextureGrid : register(c4); // block size, color-only, affine type, RGB brightness
float4 AffineControl : register(c3); // strength, max displacement in base texels, width, height
// BEGIN_BOUNDED_UV: also executed by the regression test harness.
float2 BoundedUV(float2 baseUV, float3 affineUVW, float4 control) {
 float amount = saturate(control.x);
 if (amount <= 0.0 || control.y <= 0.0 || affineUVW.z <= 0.0001) return baseUV;
 float2 size = max(control.zw, float2(1.0,1.0));
 float2 delta = (affineUVW.xy / affineUVW.z - baseUV) * size;
 // Smooth radial cap. Apply amount AFTER the cap for linear strength control.
 delta /= 1.0 + length(delta) / control.y;
 return baseUV + amount * delta / size;
}
// END_BOUNDED_UV
// BEGIN_GEOMETRY_UV
// Smooth cap (0), classic affine with hard cap (1), homogeneous blend with hard cap (2).
float2 GeometryUV(float2 baseUV,float3 uvw,float4 control,float kind) {
 if(kind<0.5)return BoundedUV(baseUV,uvw,control);
 float amount=saturate(control.x);
 if(amount<=0.0||control.y<=0.0||uvw.z<=0.0001)return baseUV;
 float2 target;
 if(kind<1.5)target=lerp(baseUV,uvw.xy/uvw.z,amount);
 else target=lerp(baseUV,uvw.xy,amount)/max(lerp(1.0,uvw.z,amount),0.0001);
 float2 size=max(control.zw,float2(1.0,1.0));
 float2 delta=(target-baseUV)*size;
 float limit=amount*control.y;
 delta*=min(1.0,limit/max(length(delta),0.000001));
 return baseUV+delta/size;
}
// END_GEOMETRY_UV
// BEGIN_PRECISION
float2 PrecisionUV(float2 uv,float2 steps){
 if(steps.x<1.0)return uv;
 return (floor(uv*steps)+0.5)/steps;
}
float3 VertexRGB(float3 base,float3 weighted,float w,float strength){
 if(strength<=0.0||w<=0.0001)return base;
 return lerp(base,saturate(weighted/w),strength);
}
// END_PRECISION
sampler2D BaseTexture : register(s0);
float4 PSMain(Output i,float2 raster:VPOS) : COLOR0 {
 if(TextureGrid.w<0)return float4(1,0,1,1); // diagnostic color for bridge draws only
 float2 uv = GeometryUV(i.baseUV, i.uvw, AffineControl,TextureGrid.z);
 float2 sampled=uv;
 if(TextureGrid.x>1.0){
  float2 base=GridPerspectiveUV(i.baseUV,1.0/max(i.uvw.z,0.000001),raster,TextureGrid.x);
  float2 affineUV=GridUV(i.uvw.xy/max(i.uvw.z,0.000001),raster,TextureGrid.x);
  // Retain the interpolated W weight for the homogeneous blend.
  sampled=GeometryUV(base,float3(affineUV*i.uvw.z,i.uvw.z),AffineControl,TextureGrid.z);
 }
 // Quantize the final sample coordinates, keeping original gradients for mip selection.
 sampled = PrecisionUV(sampled, float2(PSMisc.z,FogColor.a));
 float4 c = i.color;
 c.rgb=VertexRGB(c.rgb,i.weightedRGB,i.uvw.z,PSMisc.w);
 if(TextureGrid.y<0.5)c*=tex2Dgrad(BaseTexture,sampled,ddx(uv),ddy(uv));
 c.rgb=saturate(c.rgb*TextureGrid.w);
 if(PSFog.x > 0.5) {
  float z = PSMisc.y > 0.5 ? length(i.viewPos) : abs(i.viewPos.z);
  float f = 1;
  if (PSFog.y < 1.5) f = exp(-PSMisc.x*z); // D3DFOG_EXP=1
  else if (PSFog.y < 2.5) f = exp(-pow(PSMisc.x*z,2));
  else f = (PSFog.w-z)/max(PSFog.w-PSFog.z,0.000001);
  c.rgb = lerp(FogColor.rgb,c.rgb,saturate(f));
 }
 return c;
}

#endif
)PSXHLSL";
