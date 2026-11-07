// Linux -no-pie fixture for the x86 relative call and Mod Loader re-read.
// Model the upstream bRan path and verify forwarding and ownership.
#include <sys/mman.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cwchar>
using BYTE=unsigned char;using DWORD=uint32_t;using HMODULE=void*;using HINSTANCE=void*;using LPSTR=char*;using LPCWSTR=const wchar_t*;
#define WINAPI
enum{MEM_COMMIT=1,PAGE_GUARD=2,PAGE_EXECUTE=4,PAGE_EXECUTE_READ=8,PAGE_EXECUTE_READWRITE=16,PAGE_EXECUTE_WRITECOPY=32,GET_MODULE_HANDLE_EX_FLAG_PIN=1,GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS=4};
struct MEMORY_BASIC_INFORMATION{void* BaseAddress;size_t RegionSize;void* AllocationBase;unsigned State,Protect;};
void*self=(void*)1;
DWORD lastError=123;
bool loaderPresent=true,alreadyRunning=true;
unsigned initialized=0,entered=0;
HMODULE GetModuleHandleW(LPCWSTR name){return !name?(void*)0x400000:(loaderPresent?self:nullptr);}
bool VirtualQuery(const void*p,MEMORY_BASIC_INFORMATION*m,size_t){
 uintptr_t base=reinterpret_cast<uintptr_t>(p)&~uintptr_t(4095);
 *m={(void*)base,4096,(void*)0x400000,MEM_COMMIT,PAGE_EXECUTE_READ};return true;
}
bool Write(void*p,const void*v,size_t n){memcpy(p,v,n);return true;}
DWORD GetLastError(){return lastError;}void SetLastError(DWORD e){lastError=e;}
bool GetModuleHandleExW(unsigned,LPCWSTR,HMODULE*){return true;}
void InitializeASI(){++initialized;lastError=42;}
void Log(const char*){}
#include "../src/modloader_start.hpp"
char args[]="-test";
int loaderThunk(HINSTANCE a,HINSTANCE b,LPSTR c,int d){
 assert(a==self&&b==nullptr&&c==args&&d==3&&alreadyRunning&&lastError==123);++entered;return 47;
}
int main(){
 auto p=(BYTE*)mmap((void*)0x824000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON|MAP_FIXED_NOREPLACE,-1,0);assert(p==(void*)0x824000);
 BYTE*site=p+0x6ec;
 const BYTE original[]={0x56,0x56,0xff,0xd7,0x50,0xe8,0,0,0,0,0x8b,0xf8,0x89,0x7d,0x94,0x39,0x75,0xe4};
 memcpy(site-5,original,sizeof(original));DWORD raw=DWORD(uintptr_t(&loaderThunk)-ml_start::site-5);
 memcpy(site+1,&raw,4);
 loaderPresent=false;assert(!ml_start::arm());loaderPresent=true;
 site[5]=0;assert(!ml_start::arm());site[5]=0x8b;
 assert(ml_start::arm());DWORD patched;memcpy(&patched,site+1,4);
 auto reread=reinterpret_cast<ml_start::Main>(DWORD(ml_start::site+5+patched));
 assert(reread(self,nullptr,args,3)==47&&entered==1&&initialized==1);
 assert(!memcmp(site+1,&raw,4));
 assert(ml_start::arm());DWORD replacement=99;memcpy(site+1,&replacement,4);
 ml_start::restore();assert(!memcmp(site+1,&replacement,4));
}
