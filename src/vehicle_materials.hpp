struct CarMaterials {
 std::vector<std::pair<DWORD*,DWORD>> saved;
 explicit CarMaterials(void*entry){
  if(!AtmosphereActive()||!noCarReflections||!entry)return;
  BYTE*header=(BYTE*)entry+0x18;DWORD count=*(DWORD*)(header+4);
  if(count>4096)return;
  for(DWORD k=0;k<count;k++){
   void*material=*(void**)(header+0x40+k*0x24+8);if(!material)continue;
   DWORD*flags=(DWORD*)((BYTE*)material+0x10);
   if(std::find_if(saved.begin(),saved.end(),[&](const auto&e){return e.first==flags;})!=saved.end())continue;
   saved.push_back({flags,*flags});*flags&=~7u;
  }
 }
 ~CarMaterials(){for(auto&e:saved)*e.first=e.second;}
};
