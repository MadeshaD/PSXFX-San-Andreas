"""Execute production PSMain with brightness, fog, alpha and color-only cases on EGL."""
from pathlib import Path
import tempfile,subprocess,sys,struct,re
import numpy as np
import moderngl
root=Path(__file__).resolve().parents[1]
s='#define PIXEL_STAGE 1\n'+(root/'src/grid.hlsl').read_text()+(root/'src/psx.hlsl').read_text()
s=re.sub(r'float4 (\w+) : register\(c\d+\);',r'uniform float4 \1;',s)
s=s.replace('sampler2D BaseTexture : register(s0);','texture2D Back : COLOR;\nsampler2D BaseTexture { Texture=Back; MinFilter=POINT; MagFilter=POINT; };')
s+='''
void VS(uint id:SV_VertexID,out float4 p:SV_Position,out float2 uv:TEXCOORD0){
 uv=float2(id==2?2.0:0.0,id==1?2.0:0.0);p=float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float4 PS(float4 p:SV_Position,float2 uv:TEXCOORD0):SV_Target{
 Output i;i.pos=p;i.color=float4(.4,.6,.8,.3);i.uvw=float3(uv,1);i.baseUV=uv;i.viewPos=float3(0,0,50);
 return PSMain(i,p.xy);
}
technique Test{pass{VertexShader=VS;PixelShader=PS;}}
'''
ctx=moderngl.create_standalone_context(require=430,backend='egl')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.fx').write_text(s)
 subprocess.run([str(Path(sys.argv[1]).resolve()),str(p/'test.fx'),str(p/'test.glsl'),'glsl','16','16'],check=True,capture_output=True)
 glsl=(p/'test.glsl').read_text()
prog=ctx.program(vertex_shader=glsl.replace('#version 430','#version 430\n#define E_xVS\n#define dFdx(x) ((x)*0.0)\n#define dFdy(x) ((x)*0.0)',1),fragment_shader=glsl.replace('#version 430','#version 430\n#define E_xPS',1))
vao=ctx.vertex_array(prog,[])
rng=np.random.default_rng(43);data=rng.uniform(0,1,(16,16,4)).astype('f4');tex=ctx.texture((16,16),4,data.tobytes(),dtype='f4');tex.filter=(moderngl.NEAREST,moderngl.NEAREST);tex.use(0)
fbo=ctx.framebuffer([ctx.texture((16,16),4,dtype='f4')]);fbo.use();ctx.viewport=(0,0,16,16)
color=np.array([.4,.6,.8,.3],dtype='f4');fogcolor=np.array([.2,.3,.4],dtype='f4')
# Uniform order follows source declaration, NOT D3D register numbering:
# FogColor, PSFog, PSMisc, TextureGrid, AffineControl.
for gain in [0,.5,1,1.25,2,3]:
 for fog in [0,1]:
  for color_only in [0,1]:
   constants=[.2,.3,.4,1, fog,3,0,100, 0,0,0,0, 1,color_only,0,gain, 0,0,16,16]
   ubo=ctx.buffer(struct.pack('<20f',*constants));ubo.bind_to_uniform_block(0);vao.render(vertices=3)
   out=np.frombuffer(fbo.read(components=4,dtype='f4'),dtype='f4').reshape(16,16,4)[::-1].copy();ubo.release()
   expected=np.broadcast_to(color,data.shape).copy() if color_only else data*color
   expected[:,:,:3]=np.clip(expected[:,:,:3]*gain,0,1)
   if fog:expected[:,:,:3]=expected[:,:,:3]*.5+fogcolor*.5
   assert np.max(abs(out-expected))<2e-6,(gain,fog,color_only,np.max(abs(out-expected)))
print('PASS: actual PSMain, 24 brightness/textured/color-only/fog cases x256 RGBA pixels;')
print('      gain before fog, RGB clipping, neutral gain, alpha preserved. Renderer:',ctx.info['GL_RENDERER'])
print('NOT TESTED: actual GTA vehicle materials, lighting, D3D9 driver.')
