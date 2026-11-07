"""Run the actual BoundedUV function via ReShade FX -> GLSL -> EGL/llvmpipe.
Usage: python3 tests/test_bounded_uv.py /path/to/validate_fx
Requires numpy, moderngl. This is not a GTA/D3D9 runtime test.
"""
import pathlib, subprocess, sys, tempfile, struct
import numpy as np
import moderngl
root=pathlib.Path(__file__).resolve().parents[1]
shader=(root/'src/psx.hlsl').read_text()
helper=shader.split('// BEGIN_BOUNDED_UV:')[1].split('\n',1)[1].split('// END_BOUNDED_UV')[0]
fx='''uniform float4 Control=float4(0.35,8,128,256);
uniform float Depth=10.0;
texture2D Back : COLOR;
sampler2D S { Texture=Back; MinFilter=POINT; MagFilter=POINT; MipFilter=POINT; };
'''+helper+'''
void VS(uint id:SV_VertexID,out float4 p:SV_Position,out float2 uv:TEXCOORD0) {
 uv=float2(id==2?2.0:0.0,id==1?2.0:0.0);p=float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float4 PS(float4 p:SV_Position,float2 uv:TEXCOORD0):SV_Target {
 float4 x=tex2Dlod(S,float4(uv,0,0));
 return float4(BoundedUV(x.xy,float3(x.zw,Depth),Control),0,1);
}
technique Test { pass { VertexShader=VS;PixelShader=PS; } }
'''
ctx=moderngl.create_standalone_context(require=430,backend='egl')
print('Renderer:',ctx.info['GL_RENDERER'])
with tempfile.TemporaryDirectory() as t:
 path=pathlib.Path(t);(path/'test.fx').write_text(fx)
 subprocess.run([str(pathlib.Path(sys.argv[1]).resolve()),str(path/'test.fx'),str(path/'test.glsl'),'glsl','128','128'],check=True,capture_output=True)
 glsl=(path/'test.glsl').read_text()
prog=ctx.program(vertex_shader=glsl.replace('#version 430','#version 430\n#define E_xVS',1),fragment_shader=glsl.replace('#version 430','#version 430\n#define E_xPS',1))
vao=ctx.vertex_array(prog,[])
rng=np.random.default_rng(81)
base=rng.uniform(-4,4,(128,128,2)).astype('f4')
aff=rng.uniform(-10000,10000,(128,128,2)).astype('f4')
# Include tiny offsets and exactly equal UVs, not only saturated cases.
aff[:32]=base[:32]+rng.uniform(-.02,.02,(32,128,2));aff[32:40]=base[32:40]
def render(amount=.35,cap=8,width=128,height=256,depth=10):
 x=np.concatenate([base,aff*depth],axis=-1).astype('f4')
 tex=ctx.texture((128,128),4,x.tobytes(),dtype='f4');tex.filter=(moderngl.NEAREST,moderngl.NEAREST);tex.use(0)
 target=ctx.texture((128,128),4,dtype='f4');fbo=ctx.framebuffer([target]);fbo.use();ctx.viewport=(0,0,128,128)
 ubo=ctx.buffer(struct.pack('<8f',amount,cap,width,height,depth,0,0,0));ubo.bind_to_uniform_block(0);vao.render(vertices=3)
 out=np.frombuffer(fbo.read(components=4,dtype='f4'),dtype='f4').reshape(128,128,4)[::-1,:,:2].copy()
 for obj in [tex,fbo,target,ubo]:obj.release()
 return out
for size in [(128,256),(64,64),(1024,256)]:
 for cap in [0,1,8,32]:
  for amount in [0,.25,.35,1]:
   out=render(amount,cap,*size)
   assert np.isfinite(out).all()
   assert np.max(np.linalg.norm((out-base)*size,axis=-1)) <= amount*cap+.001
   if amount==0 or cap==0:assert np.array_equal(out,base)
for depth in [0,-1,-100,.000001]:
 assert np.array_equal(render(depth=depth),base)
full=render(1)
for a in [.25,.35,.5,.75]:
 assert np.max(np.abs(render(a)-(base+a*(full-base))))<2e-6
for depth in [.001,.1,1,10,10000]:
 assert np.max(np.abs(render(depth=depth)-render(depth=10)))<2e-6
# Regression: old depth-weighted blending was not linear in Affine.
a=.1;harmonic_w=30
old_effective=a*harmonic_w/(1-a+a*harmonic_w)
assert old_effective>.76
print('PASS: 48 GPU parameter combinations x 16384 pixels; finite output and texel bound;')
print('      zero strength/cap exact bypass; nonpositive/near-zero denominator fallback;')
print('      linear strength; invariance to common clip-w scaling; negative/tiled UVs.')
print('Old Affine=0.1 at harmonic clip-w=30 gives effective blend:',round(old_effective,6))
print('NOT TESTED: GTA, Microsoft D3DCompile, Windows device state.')
