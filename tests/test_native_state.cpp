// Portable fake-device tests executing the production RasterState guard.
// This tests control flow/state restoration, NOT real D3D9 driver behavior.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include "../src/native_shader.hpp"
using DWORD=uint32_t;using UINT=unsigned;using HRESULT=int;
#define FAILED(hr) ((hr)<0)
#define SUCCEEDED(hr) ((hr)>=0)
using D3DTEXTURESTAGESTATETYPE=int;using D3DSAMPLERSTATETYPE=int;
constexpr int D3DSAMP_MAGFILTER=5,D3DTEXF_POINT=1;
constexpr int D3DDECLUSAGE_POSITIONT=9,D3DDECLUSAGE_POSITION=0,D3DDECLUSAGE_TEXCOORD=5,D3DDECLUSAGE_COLOR=10,D3DDECLTYPE_FLOAT2=1,D3DDECLTYPE_D3DCOLOR=4;
constexpr unsigned MAXD3DDECLLENGTH=64;
constexpr int D3DTSS_COLOROP=1,D3DTSS_COLORARG1=2,D3DTSS_COLORARG2=3,D3DTSS_ALPHAOP=4,D3DTSS_ALPHAARG1=5,D3DTSS_ALPHAARG2=6,D3DTSS_RESULTARG=28,D3DTSS_TEXCOORDINDEX=11,D3DTSS_TEXTURETRANSFORMFLAGS=24;
constexpr DWORD D3DTA_DIFFUSE=0,D3DTA_CURRENT=1,D3DTA_TEXTURE=2,D3DTA_TFACTOR=3,D3DTA_SELECTMASK=15,D3DTA_COMPLEMENT=16,D3DTA_ALPHAREPLICATE=32;
constexpr int D3DTOP_MODULATE=4,D3DTOP_DISABLE=1,D3DTOP_SELECTARG1=2,D3DTOP_SELECTARG2=3,D3DTOP_ADD=7,D3DTTFF_DISABLE=0;
constexpr int D3DRS_FOGENABLE=28,D3DRS_SPECULARENABLE=29,D3DRS_TEXTUREFACTOR=60;
constexpr int D3DRS_CLIPPLANEENABLE=152,D3DRS_RANGEFOGENABLE=48,D3DRS_FOGTABLEMODE=35,D3DRS_FOGVERTEXMODE=140,D3DRS_FOGCOLOR=34,D3DRS_FOGSTART=36,D3DRS_FOGEND=37,D3DRS_FOGDENSITY=38;
constexpr int FALSE=0;
constexpr DWORD D3DUSAGE_RENDERTARGET=1,D3DUSAGE_DEPTHSTENCIL=2,D3DRTYPE_TEXTURE=3;
struct D3DVERTEXELEMENT9{unsigned Stream,Usage,UsageIndex,Type;};
struct D3DSURFACE_DESC{DWORD Usage=0;UINT Width=256,Height=256;};
struct D3DVIEWPORT9{DWORD X=0,Y=0,Width=640,Height=480;};
struct Ref{int refs=1;void Release(){--refs;}void AddRef(){++refs;}};
struct IDirect3DVertexDeclaration9:Ref{
 bool transformed=true;
 HRESULT GetDeclaration(D3DVERTEXELEMENT9*e,UINT*n){e[0]={0,transformed?9u:0u,0,1};e[1]={0,5,0,1};e[2]={0,10,0,4};e[3]={255,0,0,0};*n=4;return 0;}
};
struct IDirect3DBaseTexture9:Ref{DWORD GetType(){return D3DRTYPE_TEXTURE;}};
struct IDirect3DTexture9:IDirect3DBaseTexture9{DWORD usage=0;HRESULT GetLevelDesc(UINT,D3DSURFACE_DESC*d){d->Usage=usage;return 0;}};
struct IDirect3DVertexShader9:Ref{
 std::vector<uint32_t> code;
 HRESULT GetFunction(void*p,UINT*n){if(p)memcpy(p,code.data(),code.size()*4);*n=code.size()*4;return 0;}
};struct IDirect3DPixelShader9:Ref{};
struct D3DCAPS9{struct{int NumTemps=32,DynamicFlowControlDepth=24;}VS20Caps;};
struct IDirect3DDevice9{
 IDirect3DDevice9(){adapted.refs=0;}
 IDirect3DVertexDeclaration9 decl;IDirect3DTexture9 texture;
 IDirect3DVertexShader9 adapted;IDirect3DVertexShader9*vs=nullptr;IDirect3DPixelShader9*ps=nullptr;
 DWORD sampler[3]={2,3,2};float constants[20]{};bool failUpload=false,rejectSM3=false,rejectAll=false;unsigned creates=0;D3DCAPS9 caps;
 std::map<int,DWORD>states{{28,0},{29,0},{60,0xffffffff}};
 std::map<int,DWORD>stage{{1,4},{2,2},{3,0},{4,4},{5,2},{6,0},{28,1},{11,0},{24,0}};
 HRESULT GetVertexDeclaration(IDirect3DVertexDeclaration9**p){*p=&decl;return 0;}
 HRESULT GetTexture(UINT,IDirect3DBaseTexture9**p){*p=&texture;return 0;}
 HRESULT GetSamplerState(UINT,int s,DWORD*p){*p=sampler[s-5];return 0;}
 HRESULT SetSamplerState(UINT,int s,DWORD v){sampler[s-5]=v;return 0;}
 HRESULT GetVertexShader(IDirect3DVertexShader9**p){*p=vs;if(vs)vs->AddRef();return 0;}
 HRESULT SetVertexShader(IDirect3DVertexShader9*p){vs=p;return 0;}
 HRESULT GetDeviceCaps(D3DCAPS9*p){*p=caps;return 0;}
 HRESULT CreateVertexShader(const DWORD*code,IDirect3DVertexShader9**p){creates++;if(rejectAll||(rejectSM3&&code[0]==0xfffe0300u)){*p=nullptr;return -1;}*p=&adapted;adapted.AddRef();return 0;}
 HRESULT GetViewport(D3DVIEWPORT9*p){*p={};return 0;}
 HRESULT SetRenderState(int s,DWORD v){states[s]=v;return 0;}
 HRESULT GetPixelShader(IDirect3DPixelShader9**p){*p=ps;return 0;}
 HRESULT SetPixelShader(IDirect3DPixelShader9*p){ps=p;return 0;}
 HRESULT GetPixelShaderConstantF(UINT,float*p,UINT){memcpy(p,constants,sizeof(constants));return 0;}
 HRESULT SetPixelShaderConstantF(UINT,const float*p,UINT){if(failUpload){failUpload=false;return -1;}memcpy(constants,p,sizeof(constants));return 0;}
};
template<class T>void Release(T*&p){if(p){p->Release();p=nullptr;}}
DWORD TS(IDirect3DDevice9*d,DWORD stage,int s){return stage==1?1:d->stage[s];}
DWORD RS(IDirect3DDevice9*d,int s){return d->states[s];}
void Color(DWORD c,float*p){p[0]=float((c>>16)&255)/255;p[1]=float((c>>8)&255)/255;p[2]=float(c&255)/255;p[3]=float(c>>24)/255;}
bool hudPoint=true,effectsGrid=true,effectsPoint=true,inEffects=false,point=true;
int textureMode=2,texturePixelSize=3;
unsigned long long hudDraws=0,spriteGridDraws=0,gridSkipped=0;
IDirect3DPixelShader9 gridShader,customShader;IDirect3DPixelShader9*spritePixel=&gridShader;

float uvSteps=0,affineColor=0;
static float UVGrid(unsigned){return uvSteps;}
unsigned carDepth=0;float vehicleBrightness=1.25f;
bool nativeGeometry=true,extendGeometry=true,nativeSnapFallback=true;float snap=.5f,affine=.35f,maxTexels=8;
int vertexSnapType=0,affineType=0,clipSnapPrecision=32;
bool isSkin=false;
int mode=0,height=240;IDirect3DPixelShader9*pixel=&gridShader;
bool ObjectDeclaration(IDirect3DDevice9*d,bool*skin=nullptr){if(skin)*skin=isSkin;return !d->decl.transformed;}
float FloatState(DWORD v){float f;memcpy(&f,&v,4);return f;}
void Log(const char*,...){}
static IDirect3DVertexShader9*NativeReassemble(IDirect3DDevice9*,const std::vector<uint32_t>&){return nullptr;}
static float SnapAmount(){return (mode==0||mode==1)?snap:0.f;}
#include "../src/native_geometry.hpp"
int main(){
 using namespace native_shader;IDirect3DVertexShader9 original;
 original.code={0xfffe0201};
 Emit(original.code,31,{0x80000000,D(1,0)});Emit(original.code,31,{0x8000000a,D(1,4)});Emit(original.code,31,{0x80000005,D(1,5)});
 Emit(original.code,20,{D(4,0),S(1,0),S(2,0)});Emit(original.code,1,{D(5,0),S(1,4)});Emit(original.code,1,{D(6,0,3),S(1,5)});original.code.push_back(65535);
 IDirect3DDevice9 d;d.decl.transformed=false;d.vs=&original;d.states[D3DRS_FOGENABLE]=1;d.states[D3DRS_FOGTABLEMODE]=3;
 for(int k=0;k<20;k++)d.constants[k]=float(k)+.25f;
 auto restored=[&](){assert(d.vs==&original&&d.ps==nullptr&&d.states[D3DRS_FOGENABLE]==1);assert(d.sampler[0]==2&&d.sampler[1]==3&&d.sampler[2]==2);for(int k=0;k<20;k++)assert(d.constants[k]==float(k)+.25f);};
 {NativeGeometryState n(&d);assert(n.Prepare());assert(d.vs==&d.adapted&&d.ps==pixel&&d.states[D3DRS_FOGENABLE]==0);assert(d.constants[19]==1.f);}restored();
 carDepth=1;{NativeGeometryState n(&d);assert(n.Prepare());assert(d.constants[19]==vehicleBrightness);}restored();carDepth=0;
 assert(nativeVariants.size()==1&&original.refs==2);
 d.failUpload=true;{NativeGeometryState n(&d);assert(!n.Prepare());}restored();
 nativeGeometry=false;{NativeGeometryState n(&d);assert(!n.Prepare());}restored();nativeGeometry=true;
 d.ps=&customShader;{NativeGeometryState n(&d);assert(!n.Prepare());assert(d.ps==&customShader);}d.ps=nullptr;restored();
 inEffects=true;{NativeGeometryState n(&d);assert(!n.Prepare());}inEffects=false;restored();
 d.decl.transformed=true;{NativeGeometryState n(&d);assert(!n.Prepare());}d.decl.transformed=false;restored();
 d.texture.usage=D3DUSAGE_RENDERTARGET;{NativeGeometryState n(&d);assert(!n.Prepare());}d.texture.usage=0;restored();
 isSkin=true;{NativeGeometryState n(&d);assert(n.Prepare()&&n.skinned);assert(nativeVariants.back().snap==128);assert(d.constants[12]==affine);}restored();isSkin=false;
 for(int st=0;st<3;st++)for(int at=0;at<3;at++){
  vertexSnapType=st;affineType=at;
  for(int category=0;category<3;category++){
   carDepth=category==1;isSkin=category==2;
   {NativeGeometryState n(&d);assert(n.Prepare());assert(d.constants[12]==affine&&d.constants[18]==float(at));
    assert(nativeVariants.back().snapType==unsigned(st));}
   restored();
  }
 }
 carDepth=0;isSkin=false;vertexSnapType=0;affineType=0;
 assert(nativeVariants.size()==3);
 clipSnapPrecision=64;{NativeGeometryState n(&d);assert(n.Prepare());}restored();assert(nativeVariants.size()==4);clipSnapPrecision=32;
 ClearNative();assert(original.refs==1&&nativeVariants.empty()&&d.adapted.refs==0);
 // All affine types now honor the zero texel cap.
 snap=0;maxTexels=0;textureMode=0;
 for(int at:{0,1,2}){affineType=at;{NativeGeometryState n(&d);assert(!n.Prepare());}restored();}
 affineType=0;
 snap=.5f;maxTexels=8;textureMode=2;ClearNative();
 d.rejectSM3=true;unsigned calls=d.creates;
 {NativeGeometryState n(&d);assert(n.Prepare()&&n.positionOnly);assert(d.ps==nullptr&&d.states[D3DRS_FOGENABLE]==1);for(int k=0;k<20;k++)assert(d.constants[k]==float(k)+.25f);}restored();
 assert(d.creates==calls+2);{NativeGeometryState n(&d);assert(n.Prepare()&&n.positionOnly);}restored();assert(d.creates==calls+2);ClearNative();
 d.caps.VS20Caps.NumTemps=12;calls=d.creates;{NativeGeometryState n(&d);assert(!n.Prepare());}restored();assert(d.creates==calls+1);ClearNative();d.caps.VS20Caps.NumTemps=32;
 d.rejectAll=true;calls=d.creates;{NativeGeometryState n(&d);assert(!n.Prepare());}restored();assert(d.creates==calls+2);{NativeGeometryState n(&d);assert(!n.Prepare());}restored();assert(d.creates==calls+2);ClearNative();
 puts("PASS: rejected SM3 -> VS2.x snap fallback, unchanged native PS/fog/constants, cached accept/reject, capability gate.");
 puts("PASS: production native geometry state/cache: adaptation selection, original VS/PS/constants/fog/filters restored,");
 puts("      failed constant upload rollback, cache references released, opt-out/custom-PS/HUD/effects/RT gates.");
 puts("NOT TESTED: real D3D9 CreateVertexShader, driver validation, GTA render passes.");
}
