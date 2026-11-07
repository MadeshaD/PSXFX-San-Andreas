static const char kEffects[] = R"PSXFX(
sampler2D Image : register(s0);
float4 ColorControl : register(c0); // levels, saturation, dither, contrast
float4 ScreenControl : register(c1); // optional pixel grid width,height; target width,height
float4 DitherControl : register(c2); // type: 0 legacy Bayer, 1 PS1
// BEGIN_COLOR_FUNCTIONS
float Bayer4(float2 p) {
 float2 q=frac(floor(p)/4.0)*4.0;
 float2 lo=frac(q/2.0)*2.0, hi=floor(q/2.0);
 float low=2.0*lo.x+3.0*lo.y-4.0*lo.x*lo.y;
 float high=2.0*hi.x+3.0*hi.y-4.0*hi.x*hi.y;
 return (4.0*low+high+0.5)/16.0-0.5;
}
float3 Grade(float3 c,float2 p,float4 ctl) {
 float l=dot(c,float3(0.299,0.587,0.114));
 c=saturate(lerp(float3(l,l,l),c,ctl.y));
 c=saturate((c-0.5)*ctl.w+0.5);
 // 8-bit setting is a quantization/dither bypass.
 if(ctl.x<254.5)c=floor(saturate(c+ctl.z*Bayer4(p)/ctl.x)*ctl.x+0.5)/ctl.x;
 return c;
}
float PS1Offset(float2 p) {
 float2 q=frac(floor(p)/4.0)*4.0;
 float4 row;
 if(q.y<0.5)row=float4(-4,0,-3,1);
 else if(q.y<1.5)row=float4(2,-2,3,-1);
 else if(q.y<2.5)row=float4(-3,1,-4,0);
 else row=float4(3,-1,2,-2);
 return q.x<0.5?row.x:(q.x<1.5?row.y:(q.x<2.5?row.z:row.w));
}
float3 GradePS1(float3 c,float2 p,float4 ctl) {
 float l=dot(c,float3(0.299,0.587,0.114));
 c=saturate(lerp(float3(l,l,l),c,ctl.y));
 c=saturate((c-0.5)*ctl.w+0.5);
 if(ctl.x<254.5){
  float step=256.0/(ctl.x+1.0);
  float3 rgb8=floor(c*255.0+0.5);
  rgb8=clamp(rgb8+PS1Offset(p)*ctl.z*(step/8.0),0.0,255.0);
  c=floor(rgb8/step)/ctl.x;
 }
 return c;
}
// END_COLOR_FUNCTIONS
float4 CopyPS(float2 uv:TEXCOORD0) : COLOR0 {
 return tex2Dlod(Image,float4(uv,0,0));
}
float4 PostPS(float2 uv:TEXCOORD0) : COLOR0 {
 float2 cell=floor(uv*ScreenControl.zw);
 if(ScreenControl.y>0.5){cell=floor(uv*ScreenControl.xy);uv=(cell+0.5)/ScreenControl.xy;}
 float4 c=tex2Dlod(Image,float4(uv,0,0));
 c.rgb=DitherControl.x<0.5?Grade(c.rgb,cell,ColorControl):GradePS1(c.rgb,cell,ColorControl);
 return c;
}
)PSXFX";
