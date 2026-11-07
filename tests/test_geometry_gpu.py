"""Execute production mode helpers through ReShade FX -> GLSL/EGL llvmpipe.
NOT Windows/D3D9 validation. CLI: test_geometry_gpu.py /path/to/validate_fx
"""
from pathlib import Path
import subprocess,tempfile,struct,sys
import numpy as np
import moderngl
root=Path(__file__).resolve().parents[1];src=(root/'src/psx.hlsl').read_text()
pos=src.split('// BEGIN_GEOMETRY_POSITION\n')[1].split('// END_GEOMETRY_POSITION')[0]
bounded=src.split('// BEGIN_BOUNDED_UV:')[1].split('\n',1)[1].split('// END_BOUNDED_UV')[0]
uv=src.split('// BEGIN_GEOMETRY_UV\n')[1].split('// END_GEOMETRY_UV')[0]
precision=src.split('// BEGIN_PRECISION\n')[1].split('// END_PRECISION')[0]
ctx=moderngl.create_standalone_context(require=430,backend='egl');print('Renderer:',ctx.info['GL_RENDERER'])
fx='''uniform float4 Control=float4(0,0,0,0);
uniform float4 Params=float4(0,0,0,0);
texture2D Back : COLOR;
sampler2D S { Texture=Back; MinFilter=POINT; MagFilter=POINT; MipFilter=POINT; };
'''+pos+bounded+uv+precision+'''
void VS(uint id:SV_VertexID,out float4 p:SV_Position,out float2 uv:TEXCOORD0) {
 uv=float2(id==2?2.0:0.0,id==1?2.0:0.0);p=float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float4 PS(float4 p:SV_Position,float2 uv:TEXCOORD0):SV_Target {
 float4 x=tex2Dlod(S,float4(uv,0,0));
 if(Params.x>2.5)return float4(VertexRGB(x.rgb,Control.rgb,Params.y,Params.z),x.a);
 if(Params.x>1.5)return float4(PrecisionUV(x.xy,Control.xy),0,1);
 if(Params.x<0.5)return GeometryPosition(x,Control,float4(0,0,Params.z,Params.w));
 return float4(GeometryUV(x.xy,float3(x.zw,Params.y),Control,Params.z),0,1);
}
technique Test { pass { VertexShader=VS;PixelShader=PS; } }
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.fx').write_text(fx)
 subprocess.run([str(Path(sys.argv[1]).resolve()),str(p/'test.fx'),str(p/'test.glsl'),'glsl','64','64'],capture_output=True,check=True)
 glsl=(p/'test.glsl').read_text()
prog=ctx.program(vertex_shader=glsl.replace('#version 430','#version 430\n#define E_xVS',1),fragment_shader=glsl.replace('#version 430','#version 430\n#define E_xPS',1));vao=ctx.vertex_array(prog,[])
def render(data,c,p):
 tex=ctx.texture((64,64),4,np.array(data,dtype='f4').tobytes(),dtype='f4');tex.filter=(moderngl.NEAREST,moderngl.NEAREST);tex.use(0)
 target=ctx.texture((64,64),4,dtype='f4');fbo=ctx.framebuffer([target]);fbo.use();ctx.viewport=(0,0,64,64)
 ubo=ctx.buffer(struct.pack('<8f',*c,*p));ubo.bind_to_uniform_block(0);vao.render(vertices=3)
 out=np.frombuffer(fbo.read(components=4,dtype='f4'),dtype='f4').reshape(64,64,4)[::-1].copy()
 for obj in [tex,target,fbo,ubo]:obj.release()
 return out
rng=np.random.default_rng(19);data=rng.uniform(-3,3,(64,64,4)).astype('f4');data[:,:,3]=rng.uniform(.01,20,(64,64));data[:4,:,3]=np.array([-1,0,.0001,.0009765625])[:,None] # guarded rows
for mode in range(3):
 for amount in [0,.5,1]:
  out=render(data,[427,240,amount,0],[0,0,mode,37]);expected=data.astype('f8').copy();w=expected[:,:,3:4];safeW=np.where(w==0,1,w);xy=expected[:,:,:2].copy();valid=w[:,:,0]>1/1024
  if mode==0:
   screen=(xy/safeW*[.5,-.5]+.5)*[427,240];target=(np.floor(screen+.5)/[427,240]-.5)*[2,-2]*w
  elif mode==1:target=np.floor(xy/safeW*[213.5,120])/[213.5,120]*w
  else:target=np.floor(xy*37)/37
  expected[:,:,:2][valid]+=(target-xy)[valid]*amount
  error=abs(out-expected)
  # At a floor discontinuity FP32 division/FMA can choose either adjacent cell.
  # Permit ONE cell only for samples within 8 FP32 ULPs of that boundary.
  q=screen+.5 if mode==0 else xy/safeW*[213.5,120] if mode==1 else xy*37
  near=abs(q-np.rint(q))<=np.maximum(abs(q),1)*np.finfo('f4').eps*8
  bad=error[:,:,:2]>=3e-5
  assert np.all(~bad|near), (mode,amount,'non-boundary mismatch')
  assert np.count_nonzero(bad)<=8
  step=abs(w)/[213.5,120] if mode<2 else 1/37
  assert np.all(error[:,:,:2]<=np.where(near,step*amount+3e-5,3e-5))
  assert np.array_equal(out[:,:,2:],data[:,:,2:])
  assert np.array_equal(out[:4],data[:4])
# Independent triangle barycentrics: perspective vs linear screen interpolation.
tri=rng.uniform(-2,2,(64,64,3,2));lam=np.array([.2,.3,.5])
for ws in [[.4,1,9],[2,8,25],[1,1,1]]:
 ws=np.array(ws);den=(lam/ws).sum();harmonic=1/den;base=(tri*(lam/ws)[None,None,:,None]).sum(2)/den;aff=(tri*lam[None,None,:,None]).sum(2)
 values=np.concatenate([base,aff*harmonic],axis=-1)
 for kind in range(3):
  for amount in [0,.1,.5,1]:
   out=render(values,[amount,8,128,256],[1,harmonic,kind,0])[:,:,:2]
   if kind==0:
    delta=(aff-base)*[128,256];expected=base+amount*delta/(1+np.linalg.norm(delta,axis=-1,keepdims=True)/8)/[128,256]
   elif kind==1:expected=base+(aff-base)*amount
   else:
    factors=(1-amount)+amount*ws
    expected=(tri*(lam*factors/ws)[None,None,:,None]).sum(2)/(lam*factors/ws).sum()
   if kind:
    delta=(expected-base)*[128,256]
    delta*=np.minimum(1,amount*8/np.maximum(np.linalg.norm(delta,axis=-1,keepdims=True),1e-6))
    expected=base+delta/[128,256]
   assert np.max(abs(out-expected))<3e-6,(kind,amount)
   assert np.max(np.linalg.norm((out-base)*[128,256],axis=-1))<=amount*8+1e-4
   zero=render(values,[amount,0,128,256],[1,harmonic,kind,0])[:,:,:2]
   assert np.max(abs(zero-base))<2e-7
print('PASS: 9 snap + 36 affine GPU combinations x 4096 samples; negative coordinates,')
print('      original/both new snap types, unchanged Z/W, near-eye bypass, independent triangle UV interpolation.')
print('NOT TESTED: Microsoft SM3 compiler, D3D9 driver or GTA rendering/FPS.')

for steps in [0,1,128,256,4096]:
 got=render(data,[steps,steps,0,0],[2,0,0,0])[:,:,:2]
 uv=data[:,:,:2];expected=(np.floor(uv*steps)+.5)/steps if steps else uv
 assert np.allclose(got,expected,rtol=0,atol=1e-6)
data[:,:,:3]=np.clip(data[:,:,:3],0,1)
for w in [0,3,18]:
 for strength in [0,.5,1]:
  color=np.array([.2,.6,.8]);weighted=color*w
  got=render(data,[*weighted,0],[3,w,strength,0])
  expected=data[:,:,:3] if w==0 else data[:,:,:3]*(1-strength)+color*strength
  assert np.max(abs(got[:,:,:3]-expected))<2e-6
  assert np.array_equal(got[:,:,3],data[:,:,3])
print('PASS: production UV grid and weighted RGB, 0/.5/1 blend, alpha retained, near-eye bypass.')

# Count actual point-sampled texel changes on a deterministic textured ramp.
for w,h in [(64,64),(128,64),(256,256),(512,128)]:
 uv=data[:,:,:2]*2.3-.4
 # Include negative coordinates and repeated tiles.
 inp=data.copy();inp[:,:,:2]=uv
 dims=np.array([w,h])
 grids=np.minimum(256,np.maximum(1,dims//2))
 quant=render(inp,[*grids,0,0],[2,0,0,0])[:,:,:2]
 origidx=np.floor((uv%1)*dims).astype(int)%dims
 newidx=np.floor((quant%1)*dims).astype(int)%dims
 def lookup(idx): return (idx[:,:,0]*17+idx[:,:,1]*31)%251
 changed=np.mean(lookup(origidx)!=lookup(newidx))
 assert changed>.15,(w,h,changed)
 print(f'{w}x{h}, 2 texels minimum: {100*changed:.1f}% of synthetic point samples changed')

