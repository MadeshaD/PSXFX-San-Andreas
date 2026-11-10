// A world pipeline can also render CObject and CDummy. Bind eligibility to
// the RenderWare root of the CBuilding currently being rendered.
namespace seam_owner {
template<class T> static T field(const BYTE* p,unsigned offset){
 T value;
 memcpy(&value,p+offset,sizeof(value));
 return value;
}
static bool eligible(const BYTE* entity,const BYTE* info){
 if(!info || (entity[0x36]&7)!=1 || (field<uint32_t>(entity,0x1c)&0x200))return false;
 if(field<int16_t>(info,0x10)!=-1 || (field<uint16_t>(info,0x12)&0x7800)==0x5800)return false;
 // Model vtable slot 2 is AsDamageAtomicModelInfoPtr.
 using DamageInfo=void* (__thiscall*)(const BYTE*);
 return !field<DamageInfo*>(info,0)[2](info);
}
static thread_local const BYTE* root=nullptr;
static bool owns(const BYTE* atomic){
 if(!root || !atomic || atomic[0]!=1)return false;
 return atomic==root || (root[0]==2 && field<const BYTE*>(atomic,0x3c)==root);
}
static void __fastcall render(const BYTE* entity,void*){
 const BYTE* previous=root;
 root=nullptr;
 if(seamFill && enabled && !failed){
  unsigned model=field<uint16_t>(entity,0x22);
  const auto* info=model<20000?reinterpret_cast<const BYTE*const*>(0xA9B0C8)[model]:nullptr;
  if(eligible(entity,info))root=field<const BYTE*>(entity,0x18);
 }
 reinterpret_cast<void(__thiscall*)(const BYTE*)>(0x534310)(entity);
 root=previous;
}
static bool install(){
 auto** slot=reinterpret_cast<void**>(0x858610); // CBuilding::vtable[18].
 if(*slot!=reinterpret_cast<void*>(0x534310))return false;
 void* replacement=reinterpret_cast<void*>(&render);
 return Write(slot,&replacement,sizeof(replacement));
}
}
