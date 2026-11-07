// The shared RW D3D9 atomic node calls its selected render callback at 0x757875.
// Cover ordinary atomics through that dispatch, not a model-name list.
extern "C" void __stdcall PSXCommonRender(RenderCB callback,void*entry,void*object,unsigned int type,unsigned int flags){
 // These callbacks already have scoped wrappers; never nest the same render scope twice.
 if(callback==&DN||callback==&Plain||callback==&Car||callback==(RenderCB)0x5D9900){callback(entry,object,(unsigned char)type,flags);return;}
 Building(callback,entry,object,(unsigned char)type,flags);
}
#ifndef PSX_PORTABLE_COMMON_TEST
// Entry: EAX=callback, stack=[return,entry,object,type,flags].
// Make stdcall args [callback,entry,object,type,flags]. The original callback is
// cdecl; its arguments are cleaned by the compiler inside PSXCommonRender.
// ret 16 also replaces the overwritten native 'add esp,16' after call eax.
#if defined(_MSC_VER)
__declspec(naked) static void CommonAtomicBridge(){
 __asm {
  push dword ptr [esp+16]
  push dword ptr [esp+16]
  push dword ptr [esp+16]
  push dword ptr [esp+16]
  push eax
  call PSXCommonRender
  ret 16
 }
}
#else
__attribute__((naked)) static void CommonAtomicBridge(){
 __asm__ volatile("pushl 16(%esp)\n\tpushl 16(%esp)\n\tpushl 16(%esp)\n\tpushl 16(%esp)\n\tpushl %eax\n\tcall _PSXCommonRender@20\n\tret $16\n\t");
}
#endif
static const BYTE commonAtomicSignature[]={0xff,0xd0,0x83,0xc4,0x10};

#endif
