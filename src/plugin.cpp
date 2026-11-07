#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <tlhelp32.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <cwctype>
#include <string>
#include <vector>
#include "effects_source.hpp"
#include "sprite_source.hpp"
#include "shader_source.hpp"

static HMODULE self;
static std::wstring folder, ini, logPath;
static LONG initialized=0;
static bool enabled=true, point=true, failed=false;
static int height=240, mode=0;
static int renderHeight=360,ditherType=1,frameLowHeight=0;
static bool lowActive=false,lowBroken=false,lowWarned=false,lowBoundary=false;
static void ClearLowResolution();
static bool LowScale(float&,float&);
static float uvMinTexels=2;
static IDirect3DVertexShader9* stitchVS=nullptr;
namespace stitching { static void clear(); }
static bool seamFill=true,seamDebug=false;
static float seamTolerance=.1f;
static float uvSteps=64,affineColor=.5f;
static float snap=.85f, affine=.75f, maxTexels=8;
static int textureSize=64, colorBits=4;
static int textureMode=1,texturePixelSize=1;
static bool noHeat=true,simplePed=true,noStencil=true,worldLighting=true,distanceFog=true;
static float ambientScale=1.f,directionalScale=0.65f,fogStart=60.f,fogEnd=260.f;
static void ReadAtmosphereSettings();
static bool hudPoint=true,effectsGrid=true,effectsPoint=true,inEffects=false;
static unsigned long long hudDraws=0,spriteGridDraws=0,gridSkipped=0;
static IDirect3DPixelShader9* spritePixel=nullptr;
static float saturation=1.45f, dither=1.f, contrast=1.10f;
static bool nativeTextureFallback=false,nativeGeometry=true,nativeSnapFallback=true;
static bool noGrass=true,noCarReflections=true,noSpeedBlur=true;
static unsigned carDepth=0, waterDepth=0;
static bool disableWaterSnap=true;
static float SnapAmount(){
 return (mode==0||mode==1)&&!(waterDepth&&disableWaterSnap)?snap:0.f;
}
static bool uiSnap=true;
static int uiSnapHeight=272;
static void Poll();
static int vertexSnapType=0,affineType=0,clipSnapPrecision=32;

static float vehicleBrightness=2.15f;
static void ClearNative();
static unsigned long long nativeTextureDraws=0;
static bool postEnabled=true, extendGeometry=true, inScene=false, internalDraw=false;
static DWORD sceneThread=0;
static unsigned frame=0, generatedThisFrame=0;
static unsigned long long textureDraws=0;
static IDirect3DPixelShader9 *copyPixel=nullptr,*postPixel=nullptr;
static void ClearResources();
static ULONGLONG startTick=0;
static DWORD buildingThread=0, lastReport=0;


static unsigned buildingDepth=0;
static IDirect3DDevice9* device=nullptr;
static IDirect3DVertexShader9* vertex[8]{};
static IDirect3DPixelShader9* pixel=nullptr;
using RenderCB=void(__cdecl*)(void*,void*,unsigned char,unsigned int);
using DrawIP=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
using DrawP=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
static DrawIP originalIP;
static DrawP originalP;
using DrawUP=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
using DrawIUP=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
static DrawUP originalUP;
static DrawIUP originalIUP;
static bool logging=false;
static int toggleKey=VK_F7,reloadKey=VK_F8;
static void Log(const char* fmt,...) {
 if(!logging)return;
 FILE* f=_wfopen(logPath.c_str(),L"ab"); if(!f)return;
 fprintf(f,"[%llu ms] ",GetTickCount64()-startTick);
 va_list args; va_start(args,fmt);vfprintf(f,fmt,args);va_end(args);fputc('\n',f);fclose(f);
}
static float IniFloat(const wchar_t* key,float def) {
 wchar_t b[64],d[64];swprintf(d,64,L"%.3f",def);
 GetPrivateProfileStringW(L"PSX",key,d,b,64,ini.c_str());
 wchar_t* end=nullptr;float v=wcstof(b,&end);return end==b||!std::isfinite(v)?def:v;
}
static void Settings() {
 logging=GetPrivateProfileIntW(L"PSX",L"Logging",0,ini.c_str())!=0;
 toggleKey=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"ToggleKey",VK_F7,ini.c_str()),1,254);
 reloadKey=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"ReloadKey",VK_F8,ini.c_str()),1,254);
 if(toggleKey==reloadKey)reloadKey=toggleKey==VK_F8?VK_F7:VK_F8;
 uiSnap=GetPrivateProfileIntW(L"PSX",L"UISnap",1,ini.c_str())!=0;
 uiSnapHeight=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"UISnapVirtualHeight",272,ini.c_str()),120,2160);
 renderHeight=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"RenderHeight",360,ini.c_str()),0,720);
 if(renderHeight>0)renderHeight=std::max(120,renderHeight);
 ditherType=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"DitherType",1,ini.c_str()),0,1);
 lowBroken=false;lowWarned=false;
 enabled=GetPrivateProfileIntW(L"PSX",L"Enabled",1,ini.c_str())!=0;
 point=GetPrivateProfileIntW(L"PSX",L"PointFilter",1,ini.c_str())!=0;
 height=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"VirtualHeight",240,ini.c_str()),120,720);
 uvMinTexels=std::clamp(IniFloat(L"UVMinTexels",2.0f),0.f,16.f);
 disableWaterSnap=GetPrivateProfileIntW(L"PSX",L"DisableWaterVertexSnap",1,ini.c_str())!=0;
 seamDebug=GetPrivateProfileIntW(L"PSX",L"SeamDebug",0,ini.c_str())!=0;
 seamTolerance=std::clamp(IniFloat(L"SeamJoinTolerance",0.1f),.001f,.1f);
 seamFill=GetPrivateProfileIntW(L"PSX",L"SeamFill",1,ini.c_str())!=0;
 uvSteps=std::clamp(IniFloat(L"UVPrecisionSteps",64.0f),0.f,4096.f);
 uvSteps=std::round(uvSteps);
 affineColor=std::clamp(IniFloat(L"AffineVertexColor",0.5f),0.f,1.f);
 snap=std::clamp(IniFloat(L"VertexSnap",0.85f),0.f,1.f);
 affine=std::clamp(IniFloat(L"Affine",0.75f),0.f,1.f);
 maxTexels=std::clamp(IniFloat(L"AffineMaxTexels",8.0f),0.f,32.f);
 mode=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"Mode",0,ini.c_str()),0,3);
 textureSize=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"TextureMaxSize",64,ini.c_str()),0,512);
 colorBits=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"ColorBits",4,ini.c_str()),2,8);
 contrast=std::clamp(IniFloat(L"Contrast",1.1f),0.f,3.f);
 nativeTextureFallback=GetPrivateProfileIntW(L"PSX",L"NativeTextureFallback",0,ini.c_str())!=0;
 nativeSnapFallback=GetPrivateProfileIntW(L"PSX",L"NativeSnapFallback",1,ini.c_str())!=0;
 nativeGeometry=GetPrivateProfileIntW(L"PSX",L"NativeGeometry",1,ini.c_str())!=0;
 vertexSnapType=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"VertexSnapType",0,ini.c_str()),0,2);
 affineType=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"AffineType",0,ini.c_str()),0,2);
 clipSnapPrecision=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"ClipSnapPrecision",32,ini.c_str()),1,4096);
 vehicleBrightness=std::clamp(IniFloat(L"VehicleBrightness",2.15f),0.f,3.f);
 saturation=std::clamp(IniFloat(L"Saturation",1.45f),0.f,2.f);
 dither=std::clamp(IniFloat(L"Dither",1.0f),0.f,1.f);
 postEnabled=GetPrivateProfileIntW(L"PSX",L"PostProcess",1,ini.c_str())!=0;
 extendGeometry=GetPrivateProfileIntW(L"PSX",L"ExtendGeometry",1,ini.c_str())!=0;
 textureMode=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"TextureMode",1,ini.c_str()),0,2);
 texturePixelSize=std::clamp((int)GetPrivateProfileIntW(L"PSX",L"TexturePixelSize",1,ini.c_str()),1,12);
 hudPoint=GetPrivateProfileIntW(L"PSX",L"HUDPointFilter",1,ini.c_str())!=0;
 effectsGrid=GetPrivateProfileIntW(L"PSX",L"EffectsTextureGrid",1,ini.c_str())!=0;
 effectsPoint=GetPrivateProfileIntW(L"PSX",L"EffectsPointFilter",1,ini.c_str())!=0;
 ReadAtmosphereSettings();
 ClearResources();
}
static bool Conflict() {
 HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
 if(h==INVALID_HANDLE_VALUE)return true;
 MODULEENTRY32W m{};m.dwSize=sizeof(m);bool bad=false;
 for(BOOL ok=Module32FirstW(h,&m);ok;ok=Module32NextW(h,&m)) {
  std::wstring n=m.szModule;for(auto& c:n)c=towlower(c);
  if(n.find(L"skygfx")!=std::wstring::npos||n==L"samp.dll"||n==L"mta.dll"||n==L"core.dll"||n==L"netc.dll"){bad=true;break;}
 }
 CloseHandle(h);return bad;
}
static bool ExactExe() {
 wchar_t path[32768];if(!GetModuleFileNameW(nullptr,path,32768))return false;
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
 if(f==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV p=0;HCRYPTHASH hash=0;
 bool ok=CryptAcquireContextW(&p,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0;
 if(ok)ok=CryptCreateHash(p,CALG_SHA_256,0,0,&hash)!=0;
 BYTE b[65536];DWORD n=0;
 while(ok){if(!ReadFile(f,b,sizeof(b),&n,nullptr)){ok=false;break;}if(!n)break;ok=CryptHashData(hash,b,n,0)!=0;}
 BYTE digest[32];DWORD size=32;if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)!=0;
 char hex[65]{};if(ok)for(int i=0;i<32;i++)sprintf(hex+2*i,"%02x",digest[i]);
 Log("EXE SHA256: %s",ok?hex:"read/hash failed");
 if(hash)CryptDestroyHash(hash);
 if(p)CryptReleaseContext(p,0);
 CloseHandle(f);
 return ok&&(!strcmp(hex,"a5112e9cbbb0d06f271399697abe2a4e76aa8ea94568f51dfea19117518d7521")||!strcmp(hex,"a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26"));
}
static bool Write(void* dst,const void* src,size_t size) {
 DWORD old;if(!VirtualProtect(dst,size,PAGE_EXECUTE_READWRITE,&old))return false;
 memcpy(dst,src,size);FlushInstructionCache(GetCurrentProcess(),dst,size);
 DWORD unused;VirtualProtect(dst,size,old,&unused);return true;
}
#include "atmosphere.hpp"
#include "ui_snap.hpp"
static bool Shaders(IDirect3DDevice9* d) {
 HMODULE compiler=LoadLibraryExW(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!compiler){Log("ERROR: system d3dcompiler_47.dll missing, code=%lu",GetLastError());return false;}
 decltype(&D3DCompile) compile=nullptr;
 FARPROC proc=GetProcAddress(compiler,"D3DCompile");
 static_assert(sizeof(compile)==sizeof(proc));memcpy(&compile,&proc,sizeof(proc));
 if(!compile){FreeLibrary(compiler);return false;}
 bool ok=true;
 for(int k=0;k<13&&ok;k++) {
  ID3DBlob *code=nullptr,*error=nullptr;
  std::vector<D3D_SHADER_MACRO> macros;
  if(k<8){if(k&1)macros.push_back({"HAS_NORMAL","1"});if(k&2)macros.push_back({"NO_COLOR","1"});if(k&4)macros.push_back({"NO_UV","1"});}
  else if(k==12)macros.push_back({"HAS_NORMAL","1"});
  else macros.push_back({"PIXEL_STAGE","1"});
  macros.push_back({nullptr,nullptr});
  const char*source=(k<9||k==12)?kShader:(k==11?kSprite:kEffects);
  HRESULT hr=compile(source,strlen(source),"PSXFX.SA",macros.data(),nullptr,
    k<8?"VSMain":(k==8?"PSMain":(k==9?"CopyPS":(k==10?"PostPS":(k==11?"SpritePS":"StitchVS")))),(k<8||k==12)?"vs_3_0":"ps_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);
  if(error){Log("Shader compiler: %s",(char*)error->GetBufferPointer());error->Release();}
  if(SUCCEEDED(hr)&&k==12)hr=d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&stitchVS);
  else if(SUCCEEDED(hr))hr=k<8?d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&vertex[k]):d->CreatePixelShader((DWORD*)code->GetBufferPointer(),(k==8?&pixel:(k==9?&copyPixel:(k==10?&postPixel:&spritePixel))));
  if(code)code->Release();
  if(k==12&&FAILED(hr)){Log("Optional seam shader unavailable");break;}
  ok=SUCCEEDED(hr);if(!ok)Log("ERROR: shader %d HRESULT=%08lx",k,(unsigned long)hr);
 }
 FreeLibrary(compiler);
 if(!ok){for(auto& v:vertex)if(v){v->Release();v=nullptr;}for(auto pp:{&pixel,&copyPixel,&postPixel,&spritePixel})if(*pp){(*pp)->Release();*pp=nullptr;}}
 return ok;
}
static D3DMATRIX Mul(const D3DMATRIX&a,const D3DMATRIX&b) {
 D3DMATRIX r{};for(int i=0;i<4;i++)for(int j=0;j<4;j++)for(int k=0;k<4;k++)r.m[i][j]+=a.m[i][k]*b.m[k][j];return r;
}
static bool NormalMatrix(const D3DMATRIX&w,D3DMATRIX&n) {
 const float a=w.m[0][0],b=w.m[0][1],c=w.m[0][2],d=w.m[1][0],e=w.m[1][1],f=w.m[1][2],g=w.m[2][0],h=w.m[2][1],i=w.m[2][2];
 float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(fabsf(det)<1e-12f)return false;
 n={};n.m[0][0]=(e*i-f*h)/det;n.m[0][1]=(f*g-d*i)/det;n.m[0][2]=(d*h-e*g)/det;
 n.m[1][0]=(c*h-b*i)/det;n.m[1][1]=(a*i-c*g)/det;n.m[1][2]=(b*g-a*h)/det;
 n.m[2][0]=(b*f-c*e)/det;n.m[2][1]=(c*d-a*f)/det;n.m[2][2]=(a*e-b*d)/det;n.m[3][3]=1;return true;
}
// Choose an independent UV grid for each texture axis, including non-square textures.
static float UVGrid(UINT extent){
 if(uvSteps<=0||uvMinTexels<=0)return uvSteps;
 return std::min(uvSteps,std::max(1.f,std::floor(float(extent)/uvMinTexels)));
}
static DWORD RS(IDirect3DDevice9*d,D3DRENDERSTATETYPE s){DWORD v=0;d->GetRenderState(s,&v);return v;}
static DWORD TS(IDirect3DDevice9*d,DWORD stage,D3DTEXTURESTAGESTATETYPE s){DWORD v=0;d->GetTextureStageState(stage,s,&v);return v;}
static float FloatState(DWORD v){float f;memcpy(&f,&v,4);return f;}
static void Color(DWORD c,float* out){out[0]=((c>>16)&255)/255.f;out[1]=((c>>8)&255)/255.f;out[2]=(c&255)/255.f;out[3]=((c>>24)&255)/255.f;}
#include "mesh_material.hpp"
struct Saved {
 D3DMATRIX stitchW{},stitchV{},stitchP{}; float marginX=0,marginY=0;bool stitchCandidate=false,stitchColors=false,stitchNormals=false;
 IDirect3DDevice9*d;float vs[48][4]{},ps[5][4]{};DWORD filters[3]{},fog=0;bool changed=false, shaderState=false;
 explicit Saved(IDirect3DDevice9*dev):d(dev){}
 ~Saved(){if(!changed)return;if(shaderState){d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetVertexShaderConstantF(0,&vs[0][0],48);d->SetPixelShaderConstantF(0,&ps[0][0],5);d->SetRenderState(D3DRS_FOGENABLE,fog);}for(int k=0;k<3;k++)d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters[k]);}
};
static bool Prepare(IDirect3DDevice9*d,Saved&saved) {
 if(!enabled||failed||d!=device||internalDraw||!((buildingDepth&&buildingThread==GetCurrentThreadId())||(extendGeometry&&inScene&&sceneThread==GetCurrentThreadId())))return false;
 // Only simple textured fixed-function draws. Unsupported passes retain original rendering.
 IDirect3DVertexShader9* oldVS=nullptr;IDirect3DPixelShader9* oldPS=nullptr;
 HRESULT vsResult=d->GetVertexShader(&oldVS), psResult=d->GetPixelShader(&oldPS);
 if(FAILED(vsResult)||FAILED(psResult)){if(oldVS)oldVS->Release();if(oldPS)oldPS->Release();return false;}
 bool custom=oldVS||oldPS;if(oldVS)oldVS->Release();if(oldPS)oldPS->Release();if(custom)return false;
 if(TS(d,1,D3DTSS_COLOROP)!=D3DTOP_DISABLE)return false;
 auto diffuseOnly=[&](D3DTEXTURESTAGESTATETYPE op,D3DTEXTURESTAGESTATETYPE a,D3DTEXTURESTAGESTATETYPE b){DWORD v=TS(d,0,op);return (v==D3DTOP_SELECTARG1&&TS(d,0,a)==D3DTA_DIFFUSE)||(v==D3DTOP_SELECTARG2&&TS(d,0,b)==D3DTA_DIFFUSE);};
 bool colorOnly=diffuseOnly(D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2)&&diffuseOnly(D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2);
 if(!colorOnly&&(TS(d,0,D3DTSS_COLOROP)!=D3DTOP_MODULATE||TS(d,0,D3DTSS_ALPHAOP)!=D3DTOP_MODULATE))return false;
 auto argsOK=[&](D3DTEXTURESTAGESTATETYPE a,D3DTEXTURESTAGESTATETYPE b){DWORD x=TS(d,0,a),y=TS(d,0,b);return (x==D3DTA_TEXTURE&&y==D3DTA_DIFFUSE)||(y==D3DTA_TEXTURE&&x==D3DTA_DIFFUSE);};
 if(!colorOnly&&(!argsOK(D3DTSS_COLORARG1,D3DTSS_COLORARG2)||!argsOK(D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2)))return false;
 if(TS(d,0,D3DTSS_TEXTURETRANSFORMFLAGS)!=D3DTTFF_DISABLE||TS(d,0,D3DTSS_TEXCOORDINDEX)!=0)return false;
 if(RS(d,D3DRS_SPECULARENABLE)||RS(d,D3DRS_CLIPPLANEENABLE)||RS(d,D3DRS_VERTEXBLEND)!=D3DVBF_DISABLE||RS(d,D3DRS_INDEXEDVERTEXBLENDENABLE))return false;
 D3DSURFACE_DESC textureDesc{};textureDesc.Width=textureDesc.Height=1;
 if(!colorOnly){
  IDirect3DBaseTexture9*tex=nullptr;if(FAILED(d->GetTexture(0,&tex))||!tex)return false;
  bool texture2D=tex->GetType()==D3DRTYPE_TEXTURE;
  HRESULT textureResult=texture2D?static_cast<IDirect3DTexture9*>(tex)->GetLevelDesc(0,&textureDesc):E_FAIL;
  tex->Release();if(FAILED(textureResult)||!textureDesc.Width||!textureDesc.Height)return false;
 }
 // Point-only mode leaves the original fixed-function transform and lighting intact.
 bool wantsGeometry=uvSteps>0||affineColor>0||(carDepth&&vehicleBrightness!=1.f)||((mode==0||mode==1)&&snap>0)||((mode==0||mode==2)&&affine>0&&maxTexels>0);
 if(!wantsGeometry&&!(textureMode==2&&texturePixelSize>1)){
  if(!point)return false;
  for(int k=0;k<3;k++)if(FAILED(d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),&saved.filters[k])))return false;
  saved.changed=true;
  for(int k=0;k<3;k++)if(FAILED(d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),D3DTEXF_POINT)))return false;
  return true;
 }
 IDirect3DVertexDeclaration9* decl=nullptr;if(FAILED(d->GetVertexDeclaration(&decl))||!decl)return false;
 D3DVERTEXELEMENT9 elems[MAXD3DDECLLENGTH+1];UINT count=MAXD3DDECLLENGTH+1;HRESULT hr=decl->GetDeclaration(elems,&count);decl->Release();if(FAILED(hr))return false;
 bool pos=false,color=false,uv=false,normal=false;
 for(UINT k=0;k<count&&elems[k].Stream!=0xff;k++){
  const auto&e=elems[k];
  if(e.Usage==D3DDECLUSAGE_POSITIONT||e.Usage==D3DDECLUSAGE_BLENDWEIGHT||e.Usage==D3DDECLUSAGE_BLENDINDICES)return false;
  if(e.UsageIndex)continue;
  if(e.Usage==D3DDECLUSAGE_POSITION&&e.Type==D3DDECLTYPE_FLOAT3)pos=true;
  if(e.Usage==D3DDECLUSAGE_COLOR&&e.Type==D3DDECLTYPE_D3DCOLOR)color=true;
  if(e.Usage==D3DDECLUSAGE_TEXCOORD&&e.Type==D3DDECLTYPE_FLOAT2)uv=true;
  if(e.Usage==D3DDECLUSAGE_NORMAL&&e.Type==D3DDECLTYPE_FLOAT3)normal=true;
 }
 if(!pos||(!uv&&!colorOnly))return false;
 D3DMATRIX w,v,p,n;if(FAILED(d->GetTransform(D3DTS_WORLD,&w))||FAILED(d->GetTransform(D3DTS_VIEW,&v))||FAILED(d->GetTransform(D3DTS_PROJECTION,&p))||!NormalMatrix(w,n))return false;
 D3DVIEWPORT9 vp;if(FAILED(d->GetViewport(&vp))||!vp.Width||!vp.Height)return false;
 float constants[48][4]{};D3DMATRIX wvp=Mul(Mul(w,v),p);
 memcpy(constants,wvp.m,64);memcpy(constants+4,w.m,64);memcpy(constants+8,n.m,64);memcpy(constants+42,v.m,64);
 float vh=(float)std::min(height,(int)vp.Height);constants[12][0]=floorf(vh*vp.Width/vp.Height+0.5f);constants[12][1]=vh;
 constants[12][2]=SnapAmount();
 D3DMATERIAL9 mat;if(FAILED(d->GetMaterial(&mat)))return false;
 memcpy(constants+13,&mat.Diffuse,16);memcpy(constants+14,&mat.Ambient,16);memcpy(constants+15,&mat.Emissive,16);Color(RS(d,D3DRS_AMBIENT),constants[16]);
 bool lighting=RS(d,D3DRS_LIGHTING)!=0;constants[17][3]=lighting?1.f:0.f;
 if(lighting){
  D3DRENDERSTATETYPE sources[]={D3DRS_DIFFUSEMATERIALSOURCE,D3DRS_AMBIENTMATERIALSOURCE,D3DRS_EMISSIVEMATERIALSOURCE};
  for(int k=0;k<3;k++){DWORD s=RS(d,D3DRS_COLORVERTEX)?RS(d,sources[k]):(DWORD)D3DMCS_MATERIAL;if(s>D3DMCS_COLOR1)return false;constants[17][k]=(float)s;}
  for(int k=0;k<8;k++){BOOL on=FALSE;if(FAILED(d->GetLightEnable(k,&on))||!on)continue;D3DLIGHT9 light;
   if(FAILED(d->GetLight(k,&light))||light.Type!=D3DLIGHT_DIRECTIONAL)return false;
   memcpy(constants+18+k,&light.Direction,12);memcpy(constants+26+k,&light.Diffuse,16);memcpy(constants+34+k,&light.Ambient,16);
  }
 }
 DWORD fogMode=RS(d,D3DRS_FOGTABLEMODE);if(!fogMode)fogMode=RS(d,D3DRS_FOGVERTEXMODE);
 constants[46][0]=RS(d,D3DRS_FOGENABLE)&&fogMode?1.f:0.f;constants[46][1]=(float)fogMode;
 constants[46][2]=FloatState(RS(d,D3DRS_FOGSTART));constants[46][3]=FloatState(RS(d,D3DRS_FOGEND));
 constants[47][0]=FloatState(RS(d,D3DRS_FOGDENSITY));constants[47][1]=RS(d,D3DRS_RANGEFOGENABLE)?1.f:0.f;constants[47][2]=(float)vertexSnapType;constants[47][3]=(float)clipSnapPrecision;
 float pc[5][4]{};Color(RS(d,D3DRS_FOGCOLOR),pc[0]);memcpy(pc+1,constants+46,32);
 pc[0][3]=UVGrid(textureDesc.Height);pc[2][2]=UVGrid(textureDesc.Width);pc[2][3]=affineColor;
 pc[3][0]=(mode==0||mode==2)?affine:0;pc[3][1]=maxTexels;pc[3][2]=(float)textureDesc.Width;pc[3][3]=(float)textureDesc.Height;
 pc[4][0]=textureMode==2?(float)texturePixelSize:1.f;pc[4][1]=colorOnly?1.f:0.f;pc[4][2]=(float)affineType;pc[4][3]=carDepth?vehicleBrightness:1.f;
 if(FAILED(d->GetVertexShaderConstantF(0,&saved.vs[0][0],48))||FAILED(d->GetPixelShaderConstantF(0,&saved.ps[0][0],5)))return false;
 for(int k=0;k<3;k++)if(FAILED(d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),&saved.filters[k])))return false;
 saved.fog=RS(d,D3DRS_FOGENABLE);saved.changed=true;saved.shaderState=true;
 if(FAILED(d->SetVertexShader(vertex[(normal?1:0)+(color?0:2)+(uv?0:4)]))||FAILED(d->SetPixelShader(pixel))||FAILED(d->SetVertexShaderConstantF(0,&constants[0][0],48))||FAILED(d->SetPixelShaderConstantF(0,&pc[0][0],5))||FAILED(d->SetRenderState(D3DRS_FOGENABLE,FALSE))){failed=true;Log("ERROR: setting draw state; effects disabled");return false;}
 if(point){for(int k=0;k<3;k++)if(FAILED(d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),D3DTEXF_POINT)))return false;}
 if(seamFill&&!colorOnly&&buildingScope.entry){saved.marginX=vertexSnapType==2?-1.f/clipSnapPrecision:2.f/std::max(1.f,constants[12][0]);saved.marginY=vertexSnapType==2?saved.marginX:2.f/vh;saved.stitchCandidate=true;saved.stitchColors=color;saved.stitchNormals=normal;saved.stitchW=w;saved.stitchV=v;saved.stitchP=p;}
 return true;
}
#include "effects.hpp"
#include "world_stitch.hpp"
#include "low_resolution.hpp"
#include "aa_compat.hpp"
#include "raster.hpp"
#include "fog.hpp"
#include "native_reassemble.hpp"
#include "native_geometry.hpp"
// The state guards finish before the original call on a failed shader setup.
template<class Draw> static HRESULT Dispatch(IDirect3DDevice9*d,Draw draw,MeshDraw mesh={}){
 if(!enabled||failed||internalDraw||d!=device||sceneThread!=GetCurrentThreadId())return draw();
 bool world=inScene||inEffects||buildingDepth;
 if(!world){RasterState r(d);r.PointHUD();return draw();}
 DistanceFogState fog(d,distanceFog&&!inEffects&&ObjectDeclaration(d));
 TextureOverride texture(d);
 bool done=false;HRESULT hr=S_OK;
 {Saved s(d);if(Prepare(d,s)){texture.Apply();hr=draw();if(SUCCEEDED(hr)&&s.stitchCandidate)stitching::draw(d,mesh,s.stitchW,s.stitchV,s.stitchP,s.stitchColors,s.stitchNormals,s.marginX,s.marginY);done=true;}}
 if(done)return hr;
 {NativeGeometryState n(d);if(n.Prepare()){texture.Apply();hr=draw();done=true;}}
 if(done)return hr;


 {RasterState r(d);texture.Apply();
  if(r.World())hr=draw();
  else {TextureOverride native(d,true);native.Apply();hr=draw();}
 }
 return hr;
}
static HRESULT STDMETHODCALLTYPE DrawIndexed(IDirect3DDevice9*d,D3DPRIMITIVETYPE type,INT base,UINT min,UINT vertices,UINT start,UINT primitives){
 return Dispatch(d,[&](){return originalIP(d,type,base,min,vertices,start,primitives);},{true,true,static_cast<UINT>(base),start,primitives});
}
static HRESULT STDMETHODCALLTYPE DrawPrimitive(IDirect3DDevice9*d,D3DPRIMITIVETYPE type,UINT start,UINT count){
 return Dispatch(d,[&](){return originalP(d,type,start,count);},{true,false,start,0,count});
}
static HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(IDirect3DDevice9*d,D3DPRIMITIVETYPE type,UINT count,const void*data,UINT stride){
 return Dispatch(d,[&](){return originalUP(d,type,count,data,stride);});
}
static HRESULT STDMETHODCALLTYPE DrawIndexedUP(IDirect3DDevice9*d,D3DPRIMITIVETYPE type,UINT min,UINT vertices,UINT primitives,const void*indices,D3DFORMAT fmt,const void*data,UINT stride){
 return Dispatch(d,[&](){return originalIUP(d,type,min,vertices,primitives,indices,fmt,data,stride);});
}
static bool AttachDevice(IDirect3DDevice9*d){
 if(device==d){if(!failed&&!uiRendererInstalled)InstallUIRenderer();return !failed;}
 if(device){Log("ERROR: device replaced; restart required; effects disabled");failed=true;return false;}
 D3DCAPS9 caps{};if(FAILED(d->GetDeviceCaps(&caps))||caps.VertexShaderVersion<D3DVS_VERSION(3,0)||caps.PixelShaderVersion<D3DPS_VERSION(3,0)||caps.MaxVertexShaderConst<48){Log("ERROR: Shader Model 3 unsupported");failed=true;return false;}
 if(!Shaders(d)){failed=true;return false;}
 void** table=*(void***)d;
 originalP=(DrawP)table[81];originalIP=(DrawIP)table[82];originalUP=(DrawUP)table[83];originalIUP=(DrawIUP)table[84];originalReset=(ResetFn)table[16];
 lowSetRT=(SetRTFn)table[37];lowSetDS=(SetDSFn)table[39];lowSetVP=(SetVPFn)table[47];lowSetSC=(SetSCFn)table[75];
 const unsigned slots[]={81,82,83,84,16,37,39,47,75};
 void* hooks[]={(void*)&DrawPrimitive,(void*)&DrawIndexed,(void*)&DrawPrimitiveUP,(void*)&DrawIndexedUP,(void*)&Reset,(void*)&LowSetRT,(void*)&LowSetDS,(void*)&LowSetVP,(void*)&LowSetSC};
 void* originals[]={(void*)originalP,(void*)originalIP,(void*)originalUP,(void*)originalIUP,(void*)originalReset,(void*)lowSetRT,(void*)lowSetDS,(void*)lowSetVP,(void*)lowSetSC};
 for(unsigned i=0;i<9;i++)if(!Write(table+slots[i],hooks+i,4)){for(unsigned j=0;j<i;j++)Write(table+slots[j],originals+j,4);failed=true;return false;}
 device=d;InstallUIRenderer();Log("D3D9 hooks attached, shaders compiled.");return true;
}
// RsInitialiseRW tests the original RwBool immediately after this call.
static int __cdecl StartRenderWareWithUI(){
 using EngineStart=int(__cdecl*)();
 int result=reinterpret_cast<EngineStart>(0x7F2E70)();
 if(result && !failed){
  auto* renderer=*reinterpret_cast<IDirect3DDevice9**>(0xC97C28);
  if(renderer){
   sceneThread=GetCurrentThreadId();
   AttachDevice(renderer);
  }
 }
 return result;
}
static void InstallEarlyUI(){
 auto* site=reinterpret_cast<BYTE*>(0x619D2E);
 const BYTE original[]={0xE8,0x3D,0x91,0x1D,0x00,0x85,0xC0,0x74,0xE8};
 if(memcmp(site,original,sizeof(original))){
  Log("Early UI skipped: engine-start call changed");
  return;
 }
 DWORD offset=(DWORD)(uintptr_t)&StartRenderWareWithUI-0x619D33;
 if(!Write(site+1,&offset,sizeof(offset)))Log("Early UI hook unavailable");
}
static void Poll() {
 DWORD now=GetTickCount();
 static bool toggleDown=false,reloadDown=false;
 bool toggle=(GetAsyncKeyState(toggleKey)&0x8000)!=0;
 bool reload=(GetAsyncKeyState(reloadKey)&0x8000)!=0;
 if(toggle&&!toggleDown){enabled=!enabled;Log("Enabled=%d",enabled);}
 if(reload&&!reloadDown){
  Settings();Log("Settings reloaded");
  toggle=(GetAsyncKeyState(toggleKey)&0x8000)!=0;
  reload=(GetAsyncKeyState(reloadKey)&0x8000)!=0;
 }
 toggleDown=toggle;
 reloadDown=reload;
 if(now-lastReport>10000){lastReport=now;if(Conflict()){failed=true;Log("ERROR: conflicting module detected");}}
}
static void __cdecl Scene() {
 Poll();UpdateWeatherFog();frame++;if(seamFill&&enabled&&!failed)stitching::beginFrame();generatedThisFrame=0;TrimCache();
 auto*d=*(IDirect3DDevice9**)0xC97C28;
 if(!failed&&d)AttachDevice(d);
 sceneThread=GetCurrentThreadId();if(device&&lowBoundary)LowBegin(device);inScene=true;
 ((void(__cdecl*)())0x53DF40)();
 inScene=false;
}
static void __cdecl Effects() {
 inEffects=true;((void(__cdecl*)())0x53E170)();inEffects=false;if(device)LowEnd(device);
}
static void __cdecl BeforeHUD() {
 if(device)LowEnd(device);
 if(device&&enabled&&!failed&&postEnabled)PostProcess(device);
 ((void(__cdecl*)())0x53E230)();
}
static void Building(RenderCB original,void* entry,void* object,unsigned char type,unsigned int flags){
 auto*d=*(IDirect3DDevice9**)0xC97C28;
 if(!failed&&d&&AttachDevice(d)){if(!buildingDepth)buildingThread=GetCurrentThreadId();buildingDepth++;
  const auto previous=buildingScope;
  const auto prevStitch=stitching::scope;
  const bool buildingPipe=original==(RenderCB)0x5D6480||original==(RenderCB)0x5D77D0;
  const bool defaultPrelit=original==(RenderCB)0x756DF0&&(flags&8);
  if(seamFill&&(buildingPipe||defaultPrelit))
   buildingScope={(const BYTE*)entry,0};
  else buildingScope={};
  stitching::scope=seamFill&&buildingScope.entry&&type==1?stitching::Scope{(const BYTE*)object,(const BYTE*)entry}:stitching::Scope{};
  original(entry,object,type,flags);
  stitching::scope=prevStitch;buildingScope=previous;
  buildingDepth--;}
 else original(entry,object,type,flags);
}
static void __cdecl DN(void*a,void*b,unsigned char c,unsigned int d){Building((RenderCB)0x5D6480,a,b,c,d);}
static void __cdecl Plain(void*a,void*b,unsigned char c,unsigned int d){Building((RenderCB)0x5D77D0,a,b,c,d);}
#include "common_atomic.hpp"
extern "C" __declspec(dllexport) void InitializeASI(){
 if(InterlockedExchange(&initialized,1))return;
 startTick=GetTickCount64();
 wchar_t path[32768];GetModuleFileNameW(self,path,32768);folder=path;folder=folder.substr(0,folder.find_last_of(L"\\/")+1);ini=folder+L"PSXFX.SA.ini";logPath=folder+L"PSXFX.SA.log";
 logging=GetPrivateProfileIntW(L"PSX",L"Logging",0,ini.c_str())!=0;
 if(logging){FILE*f=_wfopen(logPath.c_str(),L"wb");if(f)fclose(f);Log("PSXFX.SA 0.32 / x86");}
 if((uintptr_t)GetModuleHandleW(nullptr)!=0x400000||!ExactExe()){Log("REFUSED: unsupported EXE; no patches installed");return;}
 if(memcmp((void*)0x757875,commonAtomicSignature,5)){Log("REFUSED: common atomic call changed; no patches installed");return;}
 if(Conflict()){Log("REFUSED: SkyGfx or multiplayer module; disable it and restart");return;}
 const BYTE sigDN[]={0x68,0x80,0x64,0x5d,0x00,0x57,0xe8,0xe2,0x0b,0x18,0x00};
 const BYTE sigPlain[]={0x68,0xd0,0x77,0x5d,0x00,0x57,0xe8,0xcb,0xf8,0x17,0x00};
 const BYTE sigDevice[]={0xe8,0xdb,0x1e,0,0,0x8b,0x54,0x24,0x18,0xa1,0x28,0x7c,0xc9,0};
 const BYTE sigCallback[]={0x51,0x0f,0xb6,0x44,0x24,0x10,0x8b,0x4c,0x24,0x0c,0x55,0x56,0x57,0x50,0x33,0xf6};
 if(memcmp((void*)0x5D6480,sigCallback,sizeof(sigCallback))||memcmp((void*)0x5D77D0,sigCallback,sizeof(sigCallback))||memcmp((void*)0x5D67F3,sigDN,sizeof(sigDN))||memcmp((void*)0x5D7B0A,sigPlain,sizeof(sigPlain))||memcmp((void*)0x7FA320,sigDevice,sizeof(sigDevice))){Log("REFUSED: hook signatures differ; no patches installed");return;}
 if(*(void**)0xC02C1C||*(void**)0xC02C68||*(void**)0xC02D24){Log("REFUSED: pipelines already initialized; use early InitializeASI loading");return;}
 const BYTE sceneSig[]={0xe8,0x7c,0xf4,0xff,0xff},hudSig[]={0xe8,0x19,0xf7,0xff,0xff},effectsSig[]={0xe8,0x98,0xf6,0xff,0xff};
 if(memcmp((void*)0x53EAD3,effectsSig,5)||memcmp((void*)0x53EABF,sceneSig,5)||memcmp((void*)0x53EB12,hudSig,5)){Log("REFUSED: scene/HUD calls changed; no patches installed");return;}
 if(!ValidateAtmosphere()){Log("REFUSED: atmosphere/shadow hook signatures changed; no patches installed");return;}
 Settings();void* dn=(void*)&DN;void* plain=(void*)&Plain;
 if(!Write((void*)0x5D67F4,&dn,4)){Log("ERROR: cannot patch DN callback");return;}
 if(!Write((void*)0x5D7B0B,&plain,4)){DWORD original=0x5D6480;Write((void*)0x5D67F4,&original,4);Log("ERROR: cannot patch plain callback; DN rolled back");return;}
 BYTE callScene[5]={0xe8},callHUD[5]={0xe8},callEffects[5]={0xe8};
 DWORD rel=(DWORD)(uintptr_t)&Scene-(0x53EABF+5);memcpy(callScene+1,&rel,4);
 rel=(DWORD)(uintptr_t)&BeforeHUD-(0x53EB12+5);memcpy(callHUD+1,&rel,4);
 rel=(DWORD)(uintptr_t)&Effects-(0x53EAD3+5);memcpy(callEffects+1,&rel,4);
 if(!Write((void*)0x53EAD3,callEffects,5)||!Write((void*)0x53EABF,callScene,5)||!Write((void*)0x53EB12,callHUD,5)){
  Write((void*)0x53EAD3,effectsSig,5);Write((void*)0x53EABF,sceneSig,5);Write((void*)0x53EB12,hudSig,5);
  DWORD oldDN=0x5D6480,oldPlain=0x5D77D0;Write((void*)0x5D67F4,&oldDN,4);Write((void*)0x5D7B0B,&oldPlain,4);
  Log("ERROR: scene/HUD patch failed; rolled back");return;
 }
 if(!InstallAtmosphere()){
  Write((void*)0x53EAD3,effectsSig,5);Write((void*)0x53EABF,sceneSig,5);Write((void*)0x53EB12,hudSig,5);
  DWORD oldDN=0x5D6480,oldPlain=0x5D77D0;Write((void*)0x5D67F4,&oldDN,4);Write((void*)0x5D7B0B,&oldPlain,4);
  Log("ERROR: atmosphere patch installation failed; rolled back");return;
 }
 BYTE commonCall[5]={0xe8};rel=(DWORD)(uintptr_t)&CommonAtomicBridge-(0x757875+5);memcpy(commonCall+1,&rel,4);
 if(!Write((void*)0x757875,commonCall,5)){failed=true;Log("ERROR: common atomic hook failed; effects disabled");return;}
 InstallEarlyUI();
 lowBoundary=InstallLowBoundary();
 if(!InstallMSAAHooks())Log("MSAA hooks unavailable");
 Log("Building + shared atomic render dispatch + scene/effects/HUD + atmosphere/shadows installed.");
}
static constexpr uintptr_t startupSite=0x619CF4,originalStartTarget=0x7F2F70;
static constexpr BYTE startupSignature[]={0xe8,0x77,0x92,0x1d,0x0,0x83,0xc4,0x4,0x85,0xc0,0x74,0x24};
#include "standalone_start.hpp"
static int __cdecl StartFromGame(void* openParams){
 standalone_start::run();
 using Original=int(__cdecl*)(void* openParams);
 return reinterpret_cast<Original>(originalStartTarget)(openParams);
}
#include "modloader_start.hpp"
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,void* reserved){
 if(reason==DLL_PROCESS_ATTACH){
  self=h;
  DisableThreadLibraryCalls(h);
  if(!ml_start::arm())standalone_start::arm(reinterpret_cast<void*>(&StartFromGame));
 }else if(reason==DLL_PROCESS_DETACH && !reserved){
  ml_start::restore();
  standalone_start::restore();
 }
 return TRUE;
}
