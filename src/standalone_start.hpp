// Mod Loader std.asi uses LoadLibrary before WinMain. Only arm the verified
// call here; COM, settings and renderer initialization run on the game thread.
#pragma once
#include <cstdint>
#include <cstring>
namespace standalone_start {

static bool armed=false;
static uint32_t redirect=0;
static bool operand(uint32_t value) {
 auto* p=reinterpret_cast<void*>(startupSite+1);
 DWORD protection=0,unused=0;
 if(!VirtualProtect(p,sizeof(value),PAGE_EXECUTE_READWRITE,&protection))return false;
 memcpy(p,&value,sizeof(value));
 FlushInstructionCache(GetCurrentProcess(),p,sizeof(value));
 VirtualProtect(p,sizeof(value),protection,&unused);
 return true;
}
static void restore(){
 if(!armed)return;
 auto* call=reinterpret_cast<const BYTE*>(startupSite);
 uint32_t target;
 memcpy(&target,call+1,sizeof(target));
 if(call[0]==0xe8 && target==redirect)
  operand(uint32_t(originalStartTarget-startupSite-5));
 armed=false;
}
static void run(){
 DWORD saved=GetLastError();
 restore();
 HMODULE keep=nullptr;
 if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(self),&keep))InitializeASI();
 SetLastError(saved);
}
static void arm(void* wrapper){
 // Silent calls LoadLibrary without invoking InitializeASI. This RenderWare
 // startup site also handles direct installs without Mod Loader.
 auto* image=GetModuleHandleW(nullptr);
 if(reinterpret_cast<uintptr_t>(image)!=0x400000)return;
 auto* call=reinterpret_cast<const BYTE*>(startupSite);
 MEMORY_BASIC_INFORMATION m{};
 if(!VirtualQuery(call,&m,sizeof(m)) || m.AllocationBase!=image || m.State!=MEM_COMMIT ||
    !(m.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) ||
    (m.Protect&PAGE_GUARD) || startupSite+sizeof(startupSignature)>reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize)return;
 if(memcmp(call,startupSignature,sizeof(startupSignature)))return;
 redirect=uint32_t(reinterpret_cast<uintptr_t>(wrapper)-startupSite-5);
 armed=operand(redirect);
}
}
