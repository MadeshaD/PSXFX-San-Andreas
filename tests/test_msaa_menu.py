"""Run production AA wrappers with local stand-ins for RenderWare and menu memory."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/aa_compat.hpp').read_text()
wrappers = source[source.index('static UINT __cdecl MenuMaxMSAA'):source.index('struct MSAACall')]
for address, stub in {
 '((void(__cdecl*)(UINT))0x7F84F0)': 'EngineSet',
 '((int(__cdecl*)(UINT))0x7F8A90)': 'EngineChange',
 '*(UINT*)0xBA6810': 'menuApplied',
 '*(UINT*)0xBA6814': 'menuPending',
}.items():
 assert address in wrappers
 wrappers = wrappers.replace(address, stub)
program = '''
#include <cassert>
#define __cdecl
using UINT=unsigned;
bool enabled=true, failed=false, lowBoundary=true;
int renderHeight=240;
unsigned msaaOverrides=0,engineLevel=0,menuApplied=4,menuPending=4;
UINT HardwareMax(){return 4;}
void EngineSet(UINT level){engineLevel=level;}
int EngineChange(UINT level){engineLevel=level;return 37;}
''' + wrappers + '''
int main(){
 SetMSAA(menuApplied);
 assert(engineLevel==1 && menuApplied==1 && menuPending==1);
 enabled=false;
 assert(MenuMaxMSAA()==1);
 assert(ChangeMSAA(4)==37 && engineLevel==1);
 enabled=true;
 assert(engineLevel==1);
 renderHeight=0;lowBoundary=false;
 assert(ChangeMSAA(8)==37 && engineLevel==1);
}
'''
with tempfile.TemporaryDirectory() as temp:
 cpp=Path(temp)/'test.cpp'
 cpp.write_text(program)
 exe=cpp.with_suffix('')
 subprocess.run(['g++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: startup with stored MSAA, menu request while effects off, re-enable, RenderHeight=0 and preserved call result.')
