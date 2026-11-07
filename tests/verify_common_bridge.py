"""Static x86 bridge and stack-model check, not native execution."""
from pathlib import Path
import struct,subprocess,re
root=Path(__file__).resolve().parents[1]
b=(root/'PSXFX.SA.asi').read_bytes();pe=struct.unpack_from('<I',b,60)[0];n=struct.unpack_from('<H',b,pe+6)[0];optsz=struct.unpack_from('<H',b,pe+20)[0];opt=pe+24;base=struct.unpack_from('<I',b,opt+28)[0]
sections=[struct.unpack_from('<8sIIIIIIHHI',b,opt+optsz+40*i) for i in range(n)]
def rva_data(rva,size):
 for s in sections:
  if s[2]<=rva<s[2]+s[3]:return b[s[4]+rva-s[2]:s[4]+rva-s[2]+size]
 raise ValueError(rva)
symbols=subprocess.check_output(['objdump','-t',str(root/'PSXFX.SA.asi')],text=True)
def symbol(name):
 line=next(x for x in symbols.splitlines() if name in x and '(ty   20)' in x)
 sec=int(re.search(r'\(sec\s+(\d+)\)',line)[1]);offset=int(re.search(r'0x([0-9a-f]+)\s+\S+$',line)[1],16)
 return sections[sec-1][2]+offset
bridge=symbol('CommonAtomicBridge');handler=symbol('_PSXCommonRender@20');code=rva_data(bridge,25)
assert code[:17]==bytes.fromhex('ff742410'*4+'50'),code.hex()
assert code[17]==0xe8 and code[22:25]==bytes.fromhex('c21000')
assert bridge+22+struct.unpack_from('<i',code,18)[0]==handler
stack=['return','entry','object',1,0x2f,'caller-data']
for _ in range(4):stack.insert(0,stack[4])
stack.insert(0,'callback');assert stack[:5]==['callback','entry','object',1,0x2f]
# call pushes its return; stdcall consumes that return and its five copied arguments.
stack.insert(0,'bridge-return');del stack[:6]
assert stack[:5]==['return','entry','object',1,0x2f]
del stack[:5];assert stack==['caller-data']
exe=(root.parent/'sources/gta_sa.exe').read_bytes();ep=struct.unpack_from('<I',exe,60)[0];eo=ep+24;es=struct.unpack_from('<H',exe,ep+20)[0];en=struct.unpack_from('<H',exe,ep+6)[0]
for i in range(en):
 s=struct.unpack_from('<8sIIIIIIHHI',exe,eo+es+40*i);rva=0x357875
 if s[2]<=rva<s[2]+s[3]:assert exe[s[4]+rva-s[2]:s[4]+rva-s[2]+5]==bytes.fromhex('ffd083c410');break
else:raise AssertionError('hook site missing')
print('PASS: exact shared-call bytes; compiled bridge forwards five stdcall args, preserves caller stack cleanup, calls intended handler. Static/stack model only.')
