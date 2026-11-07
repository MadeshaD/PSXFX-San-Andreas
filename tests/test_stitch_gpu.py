"""Rasterize two separate meshes plus the actual bridge-position formula on Mesa."""
from pathlib import Path
import moderngl,numpy as np
root=Path(__file__).resolve().parents[1]
hlsl=(root/'src/psx.hlsl').read_text()
def part(marker):
    return hlsl.split('// BEGIN_'+marker+'\n')[1].split('// END_'+marker)[0]
functions=part('GEOMETRY_POSITION')+part('STITCH_POSITION')
functions=functions.replace('float4','vec4').replace('float2','vec2').replace('lerp(','mix(')
ctx=moderngl.create_standalone_context(require=330,backend='egl')
vs='#version 330\nin vec4 a,b;in float t;uniform vec4 grid,misc;'+functions+'\nvoid main(){gl_Position=BridgePosition(a,b,t,grid,misc);}'
prog=ctx.program(vertex_shader=vs,fragment_shader='#version 330\nuniform vec3 color;out vec4 c;void main(){c=vec4(color,1);}')
prog['grid']=(8,8,1,0);prog['misc']=(0,0,0,32)
tex=ctx.texture((160,160),4);fbo=ctx.framebuffer([tex]);fbo.use();ctx.viewport=(0,0,160,160)
A=np.array([-.75,-.5,.2,1]);B=np.array([.75,.5,.2,1]);T=A+(B-A)*.36
P=np.array([-.75,.75,.2,1]);Q=np.array([.75,-.75,.2,1])
def edge(a,b=None,t=0):
    return np.r_[a,a if b is None else b,t]
def draw(vertices,c):
    buf=ctx.buffer(np.array(vertices,dtype='f4').tobytes());vao=ctx.vertex_array(prog,[(buf,'4f 4f 1f','a','b','t')])
    prog['color']=c;vao.render();vao.release();buf.release()
def world(mid):
    fbo.clear(.1,.4,.2,1)
    draw([edge(A),edge(B),edge(P)],(.8,.3,.1))
    draw([edge(A),mid,edge(Q),mid,edge(B),edge(Q)],(.8,.3,.1))
def img():return np.frombuffer(fbo.read(components=3),dtype='u1').copy().reshape(160,160,3)
world(edge(A,B,.36));expected=img()
world(edge(T));before=img()
count=np.count_nonzero(np.any(before!=expected,axis=2));assert count>30
# Use span parameters from the production CPU search for both topologies.
import subprocess,tempfile
with tempfile.TemporaryDirectory() as tmp:
    exe=Path(tmp)/'scan'
    subprocess.run(['g++','-std=c++17','-O2',str(root/'tests/test_pair_scan.cpp'),'-o',str(exe)],check=True)
    results=subprocess.check_output([str(exe),'emit'],text=True).splitlines()
segments=[(A,B),(A,T),(T,B)]
for internal in [0,1]:
    strips=[]
    for row in results:
        kind,i,j,*s=row.split()
        if int(kind)!=internal:continue
        ai,bi=segments[int(i)];aj,bj=segments[int(j)]
        s0,s1,t0,t1=map(float,s)
        a,b,c,d=edge(ai,bi,s0),edge(ai,bi,s1),edge(aj,bj,t0),edge(aj,bj,t1)
        strips.extend((a,b,c,c,b,d))
    assert len(strips)==12
    world(edge(T));draw(strips,(.8,.3,.1))
    assert np.array_equal(img(),expected)
    print(f'PASS: {"intra-mesh" if internal else "inter-mesh"} scan and actual bridge shader formula covered {count} exposed pixels exactly.')
