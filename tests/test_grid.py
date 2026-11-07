"""Execute production grid functions on EGL; compare linear/projective UV to analytic cell-center values."""
from pathlib import Path
import tempfile,subprocess,sys,struct
import numpy as np
import moderngl
root=Path(__file__).resolve().parents[1]
helper=(root/'src/grid.hlsl').read_text().split('// BEGIN_TEXTURE_GRID')[1].split('// END_TEXTURE_GRID')[0]
fx='''uniform float4 RowU;
uniform float4 RowV;
uniform float4 Grid;
'''+helper+'''
void VS(uint id:SV_VertexID,out float4 p:SV_Position,out float2 uv:TEXCOORD0){
 uv=float2(id==2?2.0:0.0,id==1?2.0:0.0);p=float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float4 PS(float4 p:SV_Position,float2 t:TEXCOORD0):SV_Target{
 float2 pixel=t*float2(96,64)-0.5;
 float2 pos=pixel+Grid.zw;
 float3 h=float3(dot(float3(pixel,1),RowU.xyz),dot(float3(pixel,1),RowV.xyz),1.0+RowU.w*pixel.x+RowV.w*pixel.y);
 float2 result=Grid.y>0.5?GridPerspectiveUV(h.xy/h.z,h.z,pos,Grid.x):GridUV(h.xy,pos,Grid.x);
 return float4(result,0,1);
}
technique Test{pass{VertexShader=VS;PixelShader=PS;}}
'''
ctx=moderngl.create_standalone_context(require=430,backend='egl')
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.fx').write_text(fx)
 subprocess.run([str(Path(sys.argv[1]).resolve()),str(p/'test.fx'),str(p/'test.glsl'),'glsl','96','64'],check=True,capture_output=True)
 glsl=(p/'test.glsl').read_text()
# FX emits unused derivative helpers in both stages; stub only the unused VS helpers.
# GL framebuffer Y points up; D3D9 raster Y points down. Convert only the derivative sign in PS.
prog=ctx.program(vertex_shader=glsl.replace('#version 430','#version 430\n#define E_xVS\n#define dFdx(x) ((x)*0.0)\n#define dFdy(x) ((x)*0.0)',1),fragment_shader=glsl.replace('#version 430','#version 430\n#define E_xPS\n#define dFdy(x) (-dFdy(x))',1))
vao=ctx.vertex_array(prog,[]);target=ctx.texture((96,64),4,dtype='f4');fbo=ctx.framebuffer([target]);fbo.use();ctx.viewport=(0,0,96,64)
y,x=np.mgrid[:64,:96];pixel=np.stack([x,y],-1)
cases=0
for U,V in [([.02,0,-.5,.002],[0,.03,.1,.005]),([.1,.04,-4,.02],[-.03,.001,2,-.003]),([-.008,.012,1,-.004],[.005,.009,-2,.001])]:
 for size in [1,2,3,4,8,12]:
  for offset in [(0,0),(11,7)]:
   for perspective in [0,1]:
    ubo=ctx.buffer(struct.pack('<12f',*U,*V,size,perspective,*offset));ubo.bind_to_uniform_block(0);vao.render(vertices=3)
    out=np.frombuffer(fbo.read(components=4,dtype='f4'),dtype='f4').reshape(64,96,4)[::-1,:,:2].copy();ubo.release()
    center=np.floor((pixel+offset+.5)/size)*size+(size-1)*.5-np.array(offset)
    expected=np.stack([center[:,:,0]*U[0]+center[:,:,1]*U[1]+U[2],center[:,:,0]*V[0]+center[:,:,1]*V[1]+V[2]],-1)
    if perspective:expected/= (1+center[:,:,0]*U[3]+center[:,:,1]*V[3])[:,:,None]
    error=np.max(abs(out-expected));assert error<8e-5,(U,size,offset,perspective,error)
    assert np.isfinite(out).all();cases+=1
print('PASS:',cases,'GPU grid cases x 6144 pixels; linear/projective UV vs analytic reference;')
print('      stretch, rotation, shear, negative/tiled UV, nonzero viewport origins, block sizes 1..12.')
print('Renderer:',ctx.info['GL_RENDERER'])
print('NOT TESTED: GTA rasterizer, atlas seams at triangle boundaries, Microsoft SM3 bytecode.')
