// Standalone Silent path and optional UAL initializer, emulated before RW opens.
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <cstring>
#include <sys/mman.h>
using BYTE=uint8_t;using DWORD=uint32_t;using HMODULE=void*;using LPCWSTR=const wchar_t*;
enum{MEM_COMMIT=1,PAGE_GUARD=2,PAGE_EXECUTE_READ=8,PAGE_EXECUTE_READWRITE=16,PAGE_EXECUTE_WRITECOPY=32,GET_MODULE_HANDLE_EX_FLAG_PIN=1,GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS=4};
struct MEMORY_BASIC_INFORMATION{void* BaseAddress;size_t RegionSize;void* AllocationBase;unsigned State,Protect;};
static constexpr uintptr_t startupSite=0x619cf4,originalStartTarget=0x7f2f70;
static constexpr BYTE startupSignature[]={0xe8,0x77,0x92,0x1d,0,0x83,0xc4,4,0x85,0xc0,0x74,0x24};
static HMODULE self=(void*)1;static DWORD error=17;static bool installed=false;static unsigned installs=0;
HMODULE GetModuleHandleW(LPCWSTR p){return p?nullptr:(void*)0x400000;}
bool VirtualQuery(const void*,MEMORY_BASIC_INFORMATION*m,size_t){*m={(void*)0x619000,4096,(void*)0x400000,MEM_COMMIT,PAGE_EXECUTE_READ};return true;}
bool VirtualProtect(void*,size_t,DWORD,DWORD*out){*out=PAGE_EXECUTE_READ;return true;}
void FlushInstructionCache(void*,void*,size_t){}
void* GetCurrentProcess(){return nullptr;}
bool GetModuleHandleExW(unsigned,LPCWSTR,HMODULE*){return true;}
DWORD GetLastError(){return error;}void SetLastError(DWORD e){error=e;}
void InitializeASI(){if(!installed){installed=true;++installs;}error=8;}
#include "../src/standalone_start.hpp"
int main(){
 void* page=mmap((void*)0x619000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(page==(void*)0x619000);
 auto call=(BYTE*)startupSite;
 for(bool exported:{false,true}){
  installed=false;installs=0;memcpy(call,startupSignature,sizeof(startupSignature));
  standalone_start::arm((void*)0x403000);assert(standalone_start::armed);
  if(exported)InitializeASI();
  error=17;standalone_start::run();
  assert(installs==1 && error==17 && !memcmp(call,startupSignature,sizeof(startupSignature)));
 }
 call[5]=0;standalone_start::arm((void*)0x403000);assert(!standalone_start::armed);
}
