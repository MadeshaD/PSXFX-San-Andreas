"""Exercise engine-start forwarding before the first scene with mocked RW/D3D."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
code=(Path(__file__).resolve().parents[1]/'src/plugin.cpp').read_text()
wrapper=code.split('static int __cdecl StartRenderWareWithUI()',1)[1].split('static void InstallEarlyUI',1)[0]
wrapper=wrapper.replace('reinterpret_cast<EngineStart>(0x7F2E70)','static_cast<EngineStart>(&engineStart)').replace('*reinterpret_cast<IDirect3DDevice9**>(0xC97C28)','currentDevice')
mock='''
#include <cassert>
#define __cdecl
struct IDirect3DDevice9{} device,*currentDevice;
unsigned sceneThread=0;int starts=0,attaches=0,result=0;bool failed=false;
int engineStart(){++starts;return result;}
unsigned GetCurrentThreadId(){return 17;}
void AttachDevice(IDirect3DDevice9*d){assert(d==&device && sceneThread==17);++attaches;}
int startUI()
'''
cases='''
int main(){
 currentDevice=&device;
 assert(startUI()==0 && attaches==0 && sceneThread==0);
 result=7;currentDevice=nullptr;assert(startUI()==7 && attaches==0);
 currentDevice=&device;assert(startUI()==7 && attaches==1);
 failed=true;assert(startUI()==7 && attaches==1 && starts==4);
}
'''
with TemporaryDirectory() as d:
 p=Path(d)/'check.cpp';p.write_text(mock+wrapper+cases)
 subprocess.run(['g++','-std=c++17',str(p),'-o',d+'/check'],check=True)
 subprocess.run([d+'/check'],check=True)
print('PASS: forwards RW startup result, sets UI render thread before device attach, handles failed/missing renderer.')
