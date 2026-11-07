"""HLSL frontend/SPIR-V checks; not the Windows SM3 compiler or a D3D9 driver."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[1];tools=Path(sys.argv[1]).resolve();lines=[]
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'psx.hlsl').write_text((root/'src/grid.hlsl').read_text()+(root/'src/psx.hlsl').read_text());(p/'effects.hlsl').write_text((root/'src/effects.hlsl').read_text());(p/'sprite.hlsl').write_text((root/'src/grid.hlsl').read_text()+(root/'src/sprite.hlsl').read_text())
 for k in range(13):
  macros=[]
  if k<8:
   for bit,name in [(1,'HAS_NORMAL'),(2,'NO_COLOR'),(4,'NO_UV')]:
    if k&bit:macros.append('-D'+name+'=1')
  elif k==12:macros=['-DHAS_NORMAL=1']
  else:macros=['-DPIXEL_STAGE=1']
  entry='VSMain' if k<8 else ['PSMain','CopyPS','PostPS','SpritePS','StitchVS'][k-8]
  src='psx.hlsl' if k<9 or k==12 else 'effects.hlsl' if k!=11 else 'sprite.hlsl'
  cmd=[str(tools/'glslangValidator'),'-D','--hlsl-dx9-compatible','-V','-S','vert' if k<8 or k==12 else 'frag','-e',entry,*macros,str(p/src),'-o',str(p/'shader.spv')]
  for args in [cmd,[str(tools/'spirv-val'),str(p/'shader.spv')]]:
   done=subprocess.run(args,capture_output=True,text=True);assert done.returncode==0,done.stdout+done.stderr
  lines.append(f'PASS variant {k}: {entry}')
lines.append('HLSL frontend + SPIR-V validation only; NOT D3D9 D3DCompile or native adapted bytecode validation.')
print('\n'.join(lines))
