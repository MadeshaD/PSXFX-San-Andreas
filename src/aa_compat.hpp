#pragma once
static UINT __cdecl MenuMaxMSAA(){return 1;}
static void __cdecl SetMSAA(UINT){
 *(UINT*)0xBA6810=1;
 *(UINT*)0xBA6814=1;
 ((void(__cdecl*)(UINT))0x7F84F0)(1);
}
static int __cdecl ChangeMSAA(UINT){return ((int(__cdecl*)(UINT))0x7F8A90)(1);}
struct MSAACall { DWORD at,target; void* fn; };
template<size_t N>
static bool InstallMSAACalls(const MSAACall (&sites)[N]){
 BYTE originals[N][5];
 for(size_t i=0;i<N;++i){
  memcpy(originals[i],(void*)sites[i].at,5);
  DWORD rel;
  memcpy(&rel,originals[i]+1,4);
  if(originals[i][0]!=0xe8||sites[i].at+5+rel!=sites[i].target){
   Log("MSAA hook unavailable: changed call at %08lx",sites[i].at);
   return false;
  }
 }
 for(size_t i=0;i<N;++i){
  BYTE patch[5]={0xe8};
  DWORD rel=(DWORD)(uintptr_t)sites[i].fn-sites[i].at-5;
  memcpy(patch+1,&rel,4);
  if(!Write((void*)sites[i].at,patch,5)){
   for(size_t j=0;j<i;++j)Write((void*)sites[j].at,originals[j],5);
   return false;
  }
 }
 return true;
}
static bool InstallMSAAHooks(){
 const MSAACall engine[]={
  {0x746350,0x7F84F0,(void*)&SetMSAA},
  {0x5744FD,0x7F8A90,(void*)&ChangeMSAA},
  {0x57D162,0x7F8A90,(void*)&ChangeMSAA},
  {0x57D2A6,0x7F8A90,(void*)&ChangeMSAA}
 };
 const MSAACall menu[]={
  {0x57D0EA,0x7F84E0,(void*)&MenuMaxMSAA},
  {0x57D11A,0x7F84E0,(void*)&MenuMaxMSAA},
  {0x57D136,0x7F84E0,(void*)&MenuMaxMSAA}
 };
 if(!InstallMSAACalls(engine))return false;
 return InstallMSAACalls(menu);
}
