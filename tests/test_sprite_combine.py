"""Production sprite color/alpha combiners on EGL vs independent NumPy reference."""
from pathlib import Path
import tempfile,subprocess,struct,sys
import numpy as np
import moderngl
root=Path(__file__).resolve().parents[1]
source=(root/'src/sprite.hlsl').read_text();helper=source[source.index('float4 Argument('):source.index('float4 SpritePS(')]
fx='''uniform float4 Control;
uniform float4 Diffuse;
uniform float4 Texel;
uniform float4 Factor;
'''+helper+'''
void VS(uint id:SV_VertexID,out float4 p:SV_Position){float2 uv=float2(id==2?2.0:0.0,id==1?2.0:0.0);p=float4(uv*float2(2,-2)+float2(-1,1),0,1);}
float4 PS(float4 p:SV_Position):SV_Target{return Combine(Control.x,Argument(Control.y,Diffuse,Texel),Argument(Control.z,Diffuse,Texel));}
technique Test{pass{VertexShader=VS;PixelShader=PS;}}
'''
ctx=moderngl.create_standalone_context(require=430,backend='egl')
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'s.fx').write_text(fx)
 subprocess.run([str(Path(sys.argv[1]).resolve()),str(p/'s.fx'),str(p/'s.glsl'),'glsl','2','2'],check=True,capture_output=True);g=(p/'s.glsl').read_text()
program=ctx.program(vertex_shader=g.replace('#version 430','#version 430\n#define E_xVS',1),fragment_shader=g.replace('#version 430','#version 430\n#define E_xPS',1));vao=ctx.vertex_array(program,[])
target=ctx.texture((2,2),4,dtype='f4');fbo=ctx.framebuffer([target]);fbo.use();ctx.viewport=(0,0,2,2)
D=np.array([.2,.7,.4,.3],dtype='f4');T=np.array([.9,.1,.6,.8],dtype='f4');F=np.array([.4,.8,.2,.6],dtype='f4')
def arg(s):
 out=[D,T,F][s%4].copy()
 if s&4:out[:]=out[3]
 if s&8:out=1-out
 return out
n=0
for op in range(2,8):
 for a in [b+flags for flags in [0,4,8,12] for b in range(3)]:
  for b in [v+flags for flags in [0,4,8,12] for v in range(3)]:
   A,B=arg(a),arg(b)
   expected=A if op==2 else B if op==3 else np.clip(A*B*(2**(op-4)),0,1) if op<=6 else np.clip(A+B,0,1)
   ubo=ctx.buffer(struct.pack('<16f',op,a,b,0,*D,*T,*F));ubo.bind_to_uniform_block(0);vao.render(vertices=3)
   out=np.frombuffer(fbo.read(components=4,dtype='f4'),dtype='f4').reshape(4,4)[0];ubo.release()
   assert np.max(abs(out-expected))<2e-7,(op,a,b,out,expected);n+=1
print('PASS:',n,'GPU cases: SELECTARG1/2, MODULATE/2X/4X, ADD; diffuse/texture/factor; complement and alpha replicate.')
print('RGBA including opacity verified; GPU tests do not validate real GTA render-state selection.')
