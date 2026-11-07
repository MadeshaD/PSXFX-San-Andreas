// Mod Loader reads the live CRT call operand after it installs the active
// profile. Preserve the live destination, including any existing loader thunk.
#pragma once
namespace ml_start {
using Main = int (WINAPI*)(HINSTANCE,HINSTANCE,LPSTR,int);
static constexpr uintptr_t site=0x8246EC;
static Main next=nullptr;
static DWORD savedOperand=0,ourOperand=0;
static bool armed=false;

static bool executable(const void* ptr, MEMORY_BASIC_INFORMATION& m){
 return VirtualQuery(ptr,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&PAGE_GUARD) &&
        (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static void restore(){
 if(!armed)return;
 auto* bytes=reinterpret_cast<BYTE*>(site);
 DWORD operand;
 memcpy(&operand,bytes+1,4);
 if(bytes[0]==0xe8 && operand==ourOperand)
  Write(bytes+1,&savedOperand,4);
 armed=false;
}
static int WINAPI enter(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show){
 Main forward=next;
 DWORD lastError=GetLastError();
 restore();
 HMODULE keep=nullptr;
 if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(self),&keep)){
  InitializeASI();
  Log("Mod Loader: WinMain startup reached");
 }
 SetLastError(lastError);
 return forward(instance,previous,command,show);
}
static bool arm(){
 if(!GetModuleHandleW(L"modloader.asi"))return false;
 auto* image=GetModuleHandleW(nullptr);
 if(reinterpret_cast<uintptr_t>(image)!=0x400000)return false;
 auto* call=reinterpret_cast<BYTE*>(site);
 MEMORY_BASIC_INFORMATION region{};
 if(!executable(call,region)||region.AllocationBase!=image)return false;
 auto begin=reinterpret_cast<uintptr_t>(region.BaseAddress);
 if(site-5<begin||site+13>begin+region.RegionSize)return false;
 const BYTE before[]={0x56,0x56,0xff,0xd7,0x50,0xe8};
 const BYTE after[]={0x8b,0xf8,0x89,0x7d,0x94,0x39,0x75,0xe4};
 if(memcmp(call-5,before,sizeof(before))||memcmp(call+5,after,sizeof(after)))return false;
 memcpy(&savedOperand,call+1,4);
 next=reinterpret_cast<Main>(DWORD(site+5+savedOperand));
 if(next==enter||!executable(reinterpret_cast<void*>(next),region))return false;
 ourOperand=DWORD(reinterpret_cast<uintptr_t>(&enter)-site-5);
 armed=Write(call+1,&ourOperand,4);
 return armed;
}
}
