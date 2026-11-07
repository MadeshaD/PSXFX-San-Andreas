struct FogRange {float start,end;};
static FogRange usualFog{},sfFog{},sandFog{};
static FogRange WeatherFog(short weather){
 if(weather==9)return sfFog;
 if(weather==19)return sandFog;
 return usualFog;
}
static void UpdateWeatherFog(){
 if(!distanceFog)return;
 const float blend=std::clamp(*(const float*)0xC8130C,0.f,1.f);
 FogRange old=WeatherFog(*(const short*)0xC81320),next=WeatherFog(*(const short*)0xC8131C);
 fogStart=old.start+(next.start-old.start)*blend;
 fogEnd=old.end+(next.end-old.end)*blend;
}
static FogRange ReadWeatherFog(const wchar_t*start,const wchar_t*end,float defaultStart){
 float nearFog=std::clamp(IniFloat(start,defaultStart),-2000.f,2000.f);
 return {nearFog,std::clamp(IniFloat(end,145.f),nearFog+1.f,3000.f)};
}
// Engine call sites verified against the exact supplied executable. No whole-game FX quality change.
#include "atmosphere_math.hpp"
static bool AtmosphereActive(){return enabled&&!failed;}
static void ReadAtmosphereSettings(){
 noGrass=GetPrivateProfileIntW(L"PSX",L"DisableGrass",1,ini.c_str())!=0;
 noCarReflections=GetPrivateProfileIntW(L"PSX",L"DisableVehicleReflections",1,ini.c_str())!=0;
 noSpeedBlur=GetPrivateProfileIntW(L"PSX",L"DisableSpeedBlur",1,ini.c_str())!=0;
 noHeat=GetPrivateProfileIntW(L"PSX",L"DisableHeatHaze",1,ini.c_str())!=0;
 simplePed=GetPrivateProfileIntW(L"PSX",L"SimplePedShadows",1,ini.c_str())!=0;
 noStencil=GetPrivateProfileIntW(L"PSX",L"DisableStencilShadows",1,ini.c_str())!=0;
 worldLighting=GetPrivateProfileIntW(L"PSX",L"WorldLighting",1,ini.c_str())!=0;
 ambientScale=std::clamp(IniFloat(L"AmbientLightScale",1.0f),0.f,2.f);
 directionalScale=std::clamp(IniFloat(L"DirectionalLightScale",0.65f),0.f,2.f);
 distanceFog=GetPrivateProfileIntW(L"PSX",L"DistanceFog",1,ini.c_str())!=0;
 fogStart=std::clamp(IniFloat(L"FogStart",60.0f),0.f,2000.f);
 fogEnd=std::clamp(IniFloat(L"FogEnd",260.0f),fogStart+1.f,3000.f);
 usualFog={fogStart,fogEnd};
 sfFog=ReadWeatherFog(L"FoggySFStart",L"FoggySFEnd",-70.f);
 sandFog=ReadWeatherFog(L"SandstormStart",L"SandstormEnd",-111.f);
}
static int __fastcall PedQuality(void*fx,void*){
 return atmosphere::ShadowQuality(*(int*)((BYTE*)fx+0x54),AtmosphereActive(),simplePed);
}
static int __fastcall StencilQuality(void*fx,void*){
 return atmosphere::ShadowQuality(*(int*)((BYTE*)fx+0x54),AtmosphereActive(),noStencil);
}
static void __cdecl HeatHaze(float intensity,bool alphaMask){
 if(AtmosphereActive()&&noHeat)return;
 ((void(__cdecl*)(float,bool))0x701780)(intensity,alphaMask);
}
static void __cdecl WorldLights(void*world){
 // The original repopulates the cached colors every frame, avoiding cumulative dimming.
 ((void(__cdecl*)(void*))0x7354E0)(world);
 if(!AtmosphereActive()||!worldLighting)return;
 float*ambient=(float*)0xC886D4;
 float*objectAmbient=(float*)0xC886C4;
 float*directional=(float*)0xC886B4;
 atmosphere::ScaleRGB(ambient,ambientScale);atmosphere::ScaleRGB(objectAmbient,ambientScale);
 atmosphere::ScaleRGB(directional,directionalScale);
 auto setColor=(void*(__cdecl*)(void*,const float*))0x751A90;
 void*ambLight=*(void**)0xC886E8;void*dirLight=*(void**)0xC886EC;
 if(ambLight)setColor(ambLight,ambient);
 if(dirLight)setColor(dirLight,directional);
}
static void __cdecl Water(){
 ++waterDepth;
 ((void(__cdecl*)())0x6EF650)();
 --waterDepth;
}
static void __cdecl Grass(){if(!(AtmosphereActive()&&noGrass))((void(__cdecl*)())0x5DBAE0)();}
static void __cdecl SpeedBlur(float speed){if(!(AtmosphereActive()&&noSpeedBlur))((void(__cdecl*)(float))0x7030A0)(speed);}
// Car material flag word is read at material+0x10 by the native callback.
// Clear only its three env/spec bits during this callback; restore shared materials afterwards.
#include "vehicle_materials.hpp"
static void __cdecl Car(void*entry,void*object,unsigned char type,unsigned int flags){
 CarMaterials materials(entry);carDepth++;
 ((RenderCB)0x5D9900)(entry,object,type,flags);carDepth--;
}
static void __cdecl RegisterCar(void*node,RenderCB callback){
 ((void(__cdecl*)(void*,RenderCB))0x7573E0)(node,callback==(RenderCB)0x5D9900?&Car:callback);
}
struct AtmosphereHook{uintptr_t site,target;void*replacement;};
static const AtmosphereHook atmosphereHooks[]={
 {0x5E675E,0x49EA50,(void*)&PedQuality},{0x5E676D,0x49EA50,(void*)&PedQuality},
 {0x706BC4,0x49EA50,(void*)&PedQuality},{0x706BD3,0x49EA50,(void*)&PedQuality},
 {0x7113B8,0x49EA50,(void*)&StencilQuality},{0x711D95,0x49EA50,(void*)&StencilQuality},
 {0x70F9B8,0x49EA50,(void*)&StencilQuality},
 {0x705116,0x701780,(void*)&HeatHaze},{0x53E997,0x7354E0,(void*)&WorldLights},
 {0x53E103,0x5DBAE0,(void*)&Grass},{0x704E8A,0x7030A0,(void*)&SpeedBlur},
 {0x5D9FE9,0x7573E0,(void*)&RegisterCar},
 {0x53E004,0x6EF650,(void*)&Water},{0x53E142,0x6EF650,(void*)&Water}
};
static void CallBytes(BYTE*out,uintptr_t site,uintptr_t target){out[0]=0xe8;DWORD rel=(DWORD)(target-site-5);memcpy(out+1,&rel,4);}
static bool ValidateAtmosphere(){
 const BYTE carRegistration[]={0x68,0x00,0x99,0x5d,0x00,0x57};
 if(memcmp((void*)0x5D9FE3,carRegistration,sizeof(carRegistration)))return false;
 const BYTE quality[]={0x8b,0x41,0x54,0xc3};
 const BYTE lightColor[]={0x8b,0x4c,0x24,0x04,0x8b,0x44,0x24,0x08};
 if(memcmp((void*)0x49EA50,quality,sizeof(quality))||memcmp((void*)0x751A90,lightColor,sizeof(lightColor)))return false;
 for(const auto&h:atmosphereHooks){BYTE expected[5];CallBytes(expected,h.site,h.target);if(memcmp((void*)h.site,expected,5))return false;}
 return true;
}
static bool InstallAtmosphere(){
 unsigned installed=0;
 for(const auto&h:atmosphereHooks){
  BYTE replacement[5];CallBytes(replacement,h.site,(uintptr_t)h.replacement);
  if(!Write((void*)h.site,replacement,5)){
   for(unsigned i=0;i<installed;i++){const auto&old=atmosphereHooks[i];BYTE bytes[5];CallBytes(bytes,old.site,old.target);Write((void*)old.site,bytes,5);}
   return false;
  }
  installed++;
 }
 return true;
}
