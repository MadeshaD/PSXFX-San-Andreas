"""Execute actual PostPS/CopyPS via ReShade FX->GLSL->EGL. Not a D3D9 runtime test."""
from pathlib import Path
import tempfile,subprocess,sys,struct
import numpy as np
import moderngl
root=Path(__file__).resolve().parents[1]
s=(root/'src/effects.hlsl').read_text()
s=s.replace('sampler2D Image : register(s0);','texture2D Back : COLOR;\nsampler2D Image { Texture=Back; MinFilter=POINT; MagFilter=POINT; MipFilter=POINT; };')
s=s.replace('float4 ColorControl : register(c0);','uniform float4 ColorControl;').replace('float4 ScreenControl : register(c1);','uniform float4 ScreenControl;')
s=s.replace('float4 DitherControl : register(c2);','uniform float4 DitherControl;')
fx=s+'''
void VS(uint id:SV_VertexID,out float4 p:SV_Position,out float2 uv:TEXCOORD0){
 uv=float2(id==2?2.0:0.0,id==1?2.0:0.0);p=float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float4 PS(float4 p:SV_Position,float2 uv:TEXCOORD0):SV_Target{return PostPS(uv);}
technique Test {pass {VertexShader=VS;PixelShader=PS;}}
'''
ctx=moderngl.create_standalone_context(require=430,backend='egl');print('Renderer:',ctx.info['GL_RENDERER'])
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.fx').write_text(fx)
 subprocess.run([str(Path(sys.argv[1]).resolve()),str(p/'test.fx'),str(p/'test.glsl'),'glsl','64','64'],check=True)
 glsl=(p/'test.glsl').read_text()
prog=ctx.program(vertex_shader=glsl.replace('#version 430','#version 430\n#define E_xVS',1),fragment_shader=glsl.replace('#version 430','#version 430\n#define E_xPS',1))
vao=ctx.vertex_array(prog,[]);rng=np.random.default_rng(12)
data=rng.uniform(0,1,(64,64,4)).astype('f4');tex=ctx.texture((64,64),4,data.tobytes(),dtype='f4');tex.filter=(moderngl.NEAREST,moderngl.NEAREST);tex.use(0)
target=ctx.texture((64,64),4,dtype='f4');fbo=ctx.framebuffer([target]);fbo.use();ctx.viewport=(0,0,64,64)
y,x=np.mgrid[:64,:64]
bayer=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],dtype='f4')
noise=(bayer[y%4,x%4]+.5)/16-.5
def render(bits,sat,dither,grid=0,contrast=1,kind=0):
 ubo=ctx.buffer(struct.pack('<12f',(1<<bits)-1,sat,dither,contrast,grid,grid,64,64,kind,0,0,0));ubo.bind_to_uniform_block(0);vao.render(vertices=3)
 out=np.frombuffer(fbo.read(components=4,dtype='f4'),dtype='f4').reshape(64,64,4)[::-1].copy();ubo.release();return out
for bits in [2,4,5,8]:
 for sat in [0,1,1.1,2]:
  for dith in [0,.75,1]:
   out=render(bits,sat,dith)
   l=data[:,:,:3]@np.array([.299,.587,.114],dtype='f4')
   c=np.clip(l[:,:,None]+sat*(data[:,:,:3]-l[:,:,None]),0,1)
   levels=(1<<bits)-1
   if bits<8:c=np.floor(np.clip(c+dith*noise[:,:,None]/levels,0,1)*levels+.5)/levels
   assert np.max(abs(out[:,:,:3]-c))<3e-6,(bits,sat,dith,np.max(abs(out[:,:,:3]-c)))
   assert np.array_equal(out[:,:,3],data[:,:,3])
for contrast in [0,.5,1,1.15,2,3]:
 for bits in [5,8]:
  for dith in [0,.75]:
   out=render(bits,1,dith,contrast=contrast)
   c=np.clip((data[:,:,:3]-.5)*contrast+.5,0,1)
   levels=(1<<bits)-1
   if bits<8:c=np.floor(np.clip(c+dith*noise[:,:,None]/levels,0,1)*levels+.5)/levels
   assert np.max(abs(out[:,:,:3]-c))<3e-6,(contrast,bits,dith)
   assert np.array_equal(out[:,:,3],data[:,:,3])
print('PASS: 24 contrast settings including zero, identity, clipping, dithering and unchanged alpha.')
assert np.max(abs(render(8,1,0)-data))<3e-7
out=render(8,1,0,16)
idx=(np.floor(np.arange(64)/4)*4+2).astype(int)
assert np.max(abs(out-data[idx[:,None],idx[None,:]]))<3e-7
assert np.allclose(render(5,0,0)[:,:,0],render(5,0,0)[:,:,1])
print('PASS: 48 color settings x 4096 pixels against independent NumPy reference;')
print('      exact Bayer 4x4 ordering, quantization, saturation, alpha preservation;')
print('      8-bit identity and optional screen pixel-grid sample centers.')
print('NOT TESTED: Direct3D9 framebuffer copy/state restoration, GTA HUD, MSAA/reset, runtime performance.')

# Exercise the actual GPU texture-copy shader, including alpha and sample centers.
copyfx=fx.replace('return PostPS(uv);','return CopyPS(uv);')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'copy.fx').write_text(copyfx)
 subprocess.run([str(Path(sys.argv[1]).resolve()),str(p/'copy.fx'),str(p/'copy.glsl'),'glsl','16','16'],check=True,capture_output=True)
 copyglsl=(p/'copy.glsl').read_text()
copyprog=ctx.program(vertex_shader=copyglsl.replace('#version 430','#version 430\n#define E_xVS',1),fragment_shader=copyglsl.replace('#version 430','#version 430\n#define E_xPS',1))
copyvao=ctx.vertex_array(copyprog,[])
small=ctx.texture((16,16),4,dtype='f4');smallfbo=ctx.framebuffer([small]);smallfbo.use();ctx.viewport=(0,0,16,16)
tex.use(0);copyvao.render(vertices=3)
out=np.frombuffer(smallfbo.read(components=4,dtype='f4'),dtype='f4').reshape(16,16,4)[::-1].copy()
indices=np.arange(16)*4+2
assert np.array_equal(out,data[indices[:,None],indices[None,:]])
print('PASS: actual CopyPS downsamples RGBA 64x64 to 16x16 with exact point-sample centers.')

fbo.use();ctx.viewport=(0,0,64,64);tex.use(0)
# Execute actual PS1 matrix/truncation, including alpha and low-grid anchoring.
matrix=np.array([[-4,0,-3,1],[2,-2,3,-1],[-3,1,-4,0],[3,-1,2,-2]],dtype='f4')
for bits in [2,4,5,6,8]:
 for dith in [0,.75,1]:
  for grid in [0,16]:
   src=data if not grid else data[idx[:,None],idx[None,:]]
   cellx=x if not grid else x//4;celly=y if not grid else y//4
   c=src[:,:,:3].copy();levels=(1<<bits)-1
   if bits<8:
    step=256/(levels+1)
    c=np.floor(np.clip(np.floor(c*255+.5)+matrix[celly%4,cellx%4,None]*dith*step/8,0,255)/step)/levels
   out=render(bits,1,dith,grid,kind=1)
   assert np.max(abs(out[:,:,:3]-c))<3e-6,(bits,dith,grid)
   assert np.array_equal(out[:,:,3],src[:,:,3])
print('PASS: 30 PS1 matrix/truncation GPU combinations, internal pixel anchoring, 8-bit bypass, alpha.')
