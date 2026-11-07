"""Exercise production face bridge formula using GPU transform feedback."""
from pathlib import Path
import moderngl,numpy as np
root=Path(__file__).resolve().parents[1]
s=(root/'src/psx.hlsl').read_text()
def part(name):return s.split('// BEGIN_'+name+'\n')[1].split('// END_'+name)[0]
f=(part('GEOMETRY_POSITION')+part('STITCH_POSITION')).replace('float4','vec4').replace('float2','vec2').replace('lerp(','mix(')
c=moderngl.create_standalone_context(require=330,backend='egl')
p=c.program(vertex_shader='#version 330\nin vec4 a,b,d;in vec2 w;out vec4 result;uniform vec4 grid,misc;'+f+'\nvoid main(){result=FaceBridgePosition(a,b,d,w,grid,misc);}',varyings=['result'])
p['grid']=(8,8,1,0);p['misc']=(0,0,0,32)
rng=np.random.default_rng(26);rows=[];expected=[]
for k in range(300):
 pts=rng.uniform(-.9,.9,(3,4));pts[:,2]=.2;pts[:,3]=1
 w=rng.dirichlet([1,1,1]);rows.append(np.r_[pts.ravel(),w[1:]])
 snapped=pts.copy();snapped[:,:2]=(np.floor((pts[:,:2]*[.5,-.5]+.5)*8+.5)/8-.5)*[2,-2]
 expected.append(w@snapped)
b=c.buffer(np.array(rows,dtype='f4').tobytes());out=c.buffer(reserve=300*16)
v=c.vertex_array(p,[(b,'4f 4f 4f 2f','a','b','d','w')])
fbo=c.simple_framebuffer((1,1));fbo.use();v.transform(out,vertices=300)
actual=np.frombuffer(out.read(),dtype='f4').reshape(-1,4)
assert np.allclose(actual,expected,atol=3e-7),(actual[:2],expected[:2])
print('PASS: 300 GPU face bridge positions match independently snapped triangle barycentrics (Mesa, not D3D9).')
