"""Run: python3 tests/verify.py /path/to/original/gta_sa.exe
Static PE and hook checks + mathematical invariants. No game execution.
"""
import json
import re
import hashlib
import pathlib
import struct
import sys
import numpy as np
ROOT=pathlib.Path(__file__).resolve().parents[1]
class PE:
 def __init__(self,path):
  self.b=pathlib.Path(path).read_bytes();self.pe=struct.unpack_from('<I',self.b,60)[0]
  assert self.b[self.pe:self.pe+4]==b'PE\0\0'
  self.machine,self.n=struct.unpack_from('<HH',self.b,self.pe+4)
  size=struct.unpack_from('<H',self.b,self.pe+20)[0];self.opt=self.pe+24
  assert struct.unpack_from('<H',self.b,self.opt)[0]==0x10b
  self.base=struct.unpack_from('<I',self.b,self.opt+28)[0]
  self.sections=[struct.unpack_from('<8sIIIIIIHHI',self.b,self.opt+size+40*i) for i in range(self.n)]
 def off(self,rva):
  for s in self.sections:
   if s[2]<=rva<s[2]+s[3]:return s[4]+rva-s[2]
  raise ValueError(hex(rva))
 def at(self,va,n):
  p=self.off(va-self.base);return self.b[p:p+n]
 def string(self,rva):
  p=self.off(rva);return self.b[p:self.b.index(b'\0',p)].decode()
 def export_names(self):
  rva=struct.unpack_from('<I',self.b,self.opt+96)[0];p=self.off(rva)
  count=struct.unpack_from('<I',self.b,p+24)[0];names=self.off(struct.unpack_from('<I',self.b,p+32)[0])
  return [self.string(struct.unpack_from('<I',self.b,names+4*i)[0]) for i in range(count)]
 def imports(self):
  rva=struct.unpack_from('<I',self.b,self.opt+104)[0];p=self.off(rva);out=[]
  while True:
   desc=struct.unpack_from('<5I',self.b,p)
   if not any(desc):return out
   out.append(self.string(desc[3]));p+=20
exe=PE(sys.argv[1]);assert exe.machine==0x14c and exe.base==0x400000
assert hashlib.sha256(exe.b).hexdigest() in ('a5112e9cbbb0d06f271399697abe2a4e76aa8ea94568f51dfea19117518d7521','a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26')
for va,data in {
 0x619D2E:'e8 3d 91 1d 00 85 c0 74 e8',
 0x57D0EA:'e8 f1 b3 27 00',
 0x57D11A:'e8 c1 b3 27 00',
 0x57D136:'e8 a5 b3 27 00',
 0x746350:'e8 9b 21 0b 00',
 0x5744FD:'e8 8e 45 28 00',
 0x57D162:'e8 29 b9 27 00',
 0x57D2A6:'e8 e5 b7 27 00',
 0x53EB0D:'e8 de cd fc ff',
 0x7F84F0:'a1 2c 24 8e 00',
 0x53E227:'e9 b4 64 1c 00',
 0x58A38A:'e8 71 c7 ff ff',
 0x575B44:'e8 07 25 01 00',
 0x734E90:'a1 24 7b c9 00 ff 60 30',
 0x734EA6:'ff 62 34',
 0x734EB0:'8b 15 24 7b c9 00 ff 62 2c',
 0x734EC0:'8b 0d 24 7b c9 00 ff 61 28',
 0x583480:'a0 a1 67 ba 00',
 0x53EAD3:'e8 98 f6 ff ff',
 0x53EABF:'e8 7c f4 ff ff',
 0x53EB12:'e8 19 f7 ff ff',
 0x5D67F3:'68 80 64 5d 00 57 e8 e2 0b 18 00',
 0x5D7B0A:'68 d0 77 5d 00 57 e8 cb f8 17 00',
 0x7FA320:'e8 db 1e 00 00 8b 54 24 18 a1 28 7c c9 00',
 0x5D6480:'51 0f b6 44 24 10 8b 4c 24 0c 55 56 57 50 33 f6',
 0x5D77D0:'51 0f b6 44 24 10 8b 4c 24 0c 55 56 57 50 33 f6'
}.items():
 expected=bytes.fromhex(data);assert exe.at(va,len(expected))==expected
# Confirm the callback registration calls target the known setter.
for va in [0x5D67F9,0x5D7B10]:
 b=exe.at(va,5);assert b[0]==0xe8
 assert va+5+struct.unpack('<i',b[1:])[0]==0x7573E0
for va,target in [(0x53EAD3,0x53E170),(0x53EABF,0x53DF40),(0x53EB12,0x53E230)]:
 assert va+5+struct.unpack('<i',exe.at(va+1,4))[0]==target
hooks=json.loads((ROOT/'docs/atmosphere_hooks.json').read_text())
header=(ROOT/'src/atmosphere.hpp').read_text()
implemented=[(int(a,16),int(b,16)) for a,b in re.findall(r'\{(0x[0-9A-F]+),(0x[0-9A-F]+),\(void\*\)\&',header)]
assert implemented==[(int(h['address'],16),int(h['target'],16)) for h in hooks]
for h in hooks:
 va=int(h['address'],16);b=exe.at(va,5)
 assert b==bytes.fromhex(h['bytes']) and b[0]==0xe8
 assert va+5+struct.unpack('<i',b[1:])[0]==int(h['target'],16)
assert exe.at(0x49EA50,4)==bytes.fromhex('8b 41 54 c3')
assert exe.at(0x751A90,8)==bytes.fromhex('8b 4c 24 04 8b 44 24 08')
# Native simple-ped fallback really calls StoreShadowForPedObject.
b=exe.at(0x5E6900,5);assert b[0]==0xe8 and 0x5E6905+struct.unpack('<i',b[1:])[0]==0x707B40
assert exe.at(0x5D9FE3,6)==bytes.fromhex('68 00 99 5d 00 57')
# Native callback layout used by the scoped car material flags guard.
assert exe.at(0x5D99B5,3)==bytes.fromhex('8b 46 20')
assert exe.at(0x5D99C4,4)==bytes.fromhex('89 7c 24 2c')
assert exe.at(0x5D9A00,6)==bytes.fromhex('8b 77 08 8b 5e 10')
asi=PE(ROOT/'PSXFX.SA.asi');assert asi.machine==0x14c
assert struct.unpack_from('<H',asi.b,asi.pe+22)[0]&0x2000
assert 'InitializeASI' in asi.export_names()
assert set(x.lower() for x in asi.imports())<={'advapi32.dll','kernel32.dll','msvcrt.dll','user32.dll'}
grid_source=(ROOT/'src/grid.hlsl').read_text()
source=grid_source+(ROOT/'src/psx.hlsl').read_text()
assert (ROOT/'src/shader_source.hpp').read_text()=='static const char kShader[] = R"PSXHLSL(\n'+source+')PSXHLSL";\n'
effects=(ROOT/'src/effects.hlsl').read_text()
assert (ROOT/'src/effects_source.hpp').read_text()=='static const char kEffects[] = R"PSXFX(\n'+effects+')PSXFX";\n'
sprite=grid_source+(ROOT/'src/sprite.hlsl').read_text()
assert (ROOT/'src/sprite_source.hpp').read_text()=='static const char kSprite[] = R"PSXSPR(\n'+sprite+')PSXSPR";\n'
assert 'VertexSnap=0.85' in (ROOT/'PSXFX.SA.ini').read_text()
# Derive affine interpolation independently of the shader implementation.
rng=np.random.default_rng(21)
for _ in range(500):
 w=rng.uniform(.1,100,3);uv=rng.uniform(-10,10,(3,2));lam=rng.dirichlet(np.ones(3))
 perspective_weights=lam/w;perspective_weights/=perspective_weights.sum()
 numerator=(perspective_weights[:,None]*(uv*w[:,None])).sum(axis=0)
 denominator=(perspective_weights*w).sum()
 assert np.allclose(numerator/denominator,lam@uv)
 p=rng.uniform(-100,100,4);p[3]=rng.uniform(.1,100);grid=np.array([320.,240.])
 screen=(p[:2]/p[3]*[.5,-.5]+.5)*grid
 snapped=np.floor(screen+.5);out=p.copy();out[:2]=(snapped/grid-.5)*[2,-2]*p[3]
 assert np.array_equal(out[2:],p[2:])
 assert np.allclose((out[:2]/out[3]*[.5,-.5]+.5)*grid,snapped)
print('PASS: exact EXE hash; 26 memory signatures (including UI scope calls/RW slots/map flag); callback, scene, effects and pre-HUD call targets;')
print('      x86 PE DLL; InitializeASI export; system-only imports; embedded shader consistency;')
print('      500 affine interpolation and snapping/depth invariants.')
print('      14 atmosphere/car/plant/speed/water call sites + quality/light ABI signatures; native simple ped shadow fallback;')
print('Imports:',', '.join(asi.imports()))
print('NOT TESTED: Windows execution, D3DCompile SM3 bytecode, GTA rendering, device reset, performance.')
