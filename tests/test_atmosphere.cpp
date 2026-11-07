// Portable fake-device tests executing the production RasterState guard.
// This tests control flow/state restoration, NOT real D3D9 driver behavior.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <cstdio>
using DWORD=uint32_t;using UINT=unsigned;using HRESULT=int;
#define FAILED(hr) ((hr)<0)
#define SUCCEEDED(hr) ((hr)>=0)
using D3DRENDERSTATETYPE=int;
constexpr int D3DRS_FOGTABLEMODE=35,D3DRS_FOGVERTEXMODE=140,D3DRS_FOGSTART=36,D3DRS_FOGEND=37,D3DRS_RANGEFOGENABLE=48,D3DFOG_LINEAR=3,D3DFOG_NONE=0,FALSE=0;
using D3DTEXTURESTAGESTATETYPE=int;using D3DSAMPLERSTATETYPE=int;
constexpr int D3DSAMP_MAGFILTER=5,D3DTEXF_POINT=1;
constexpr int D3DDECLUSAGE_POSITIONT=9,D3DDECLUSAGE_POSITION=0,D3DDECLUSAGE_TEXCOORD=5,D3DDECLUSAGE_COLOR=10,D3DDECLTYPE_FLOAT2=1,D3DDECLTYPE_D3DCOLOR=4;
constexpr unsigned MAXD3DDECLLENGTH=64;
constexpr int D3DTSS_COLOROP=1,D3DTSS_COLORARG1=2,D3DTSS_COLORARG2=3,D3DTSS_ALPHAOP=4,D3DTSS_ALPHAARG1=5,D3DTSS_ALPHAARG2=6,D3DTSS_RESULTARG=28,D3DTSS_TEXCOORDINDEX=11,D3DTSS_TEXTURETRANSFORMFLAGS=24;
constexpr DWORD D3DTA_DIFFUSE=0,D3DTA_CURRENT=1,D3DTA_TEXTURE=2,D3DTA_TFACTOR=3,D3DTA_SELECTMASK=15,D3DTA_COMPLEMENT=16,D3DTA_ALPHAREPLICATE=32;
constexpr int D3DTOP_DISABLE=1,D3DTOP_SELECTARG1=2,D3DTOP_SELECTARG2=3,D3DTOP_ADD=7,D3DTTFF_DISABLE=0;
constexpr int D3DRS_FOGENABLE=28,D3DRS_SPECULARENABLE=29,D3DRS_TEXTUREFACTOR=60;
constexpr DWORD D3DUSAGE_RENDERTARGET=1,D3DUSAGE_DEPTHSTENCIL=2,D3DRTYPE_TEXTURE=3;
struct D3DVERTEXELEMENT9{unsigned Stream,Usage,UsageIndex,Type;};
struct D3DSURFACE_DESC{DWORD Usage=0;};
struct Ref{void Release(){}};
struct IDirect3DVertexDeclaration9:Ref{
 bool transformed=true;
 HRESULT GetDeclaration(D3DVERTEXELEMENT9*e,UINT*n){e[0]={0,transformed?9u:0u,0,1};e[1]={0,5,0,1};e[2]={0,10,0,4};e[3]={255,0,0,0};*n=4;return 0;}
};
struct IDirect3DBaseTexture9:Ref{DWORD GetType(){return D3DRTYPE_TEXTURE;}};
struct IDirect3DTexture9:IDirect3DBaseTexture9{DWORD usage=0;HRESULT GetLevelDesc(UINT,D3DSURFACE_DESC*d){d->Usage=usage;return 0;}};
struct IDirect3DVertexShader9:Ref{};struct IDirect3DPixelShader9:Ref{};
struct IDirect3DDevice9{
 IDirect3DVertexDeclaration9 decl;IDirect3DTexture9 texture;
 IDirect3DVertexShader9*vs=nullptr;IDirect3DPixelShader9*ps=nullptr;
 DWORD sampler[3]={2,3,2};float constants[16]{};bool failUpload=false;
 std::map<int,DWORD>states{{28,0},{29,0},{60,0xffffffff}};
 std::map<int,DWORD>stage{{1,4},{2,2},{3,0},{4,4},{5,2},{6,0},{28,1},{11,0},{24,0}};
 int failFogSet=0;
 HRESULT GetRenderState(int s,DWORD*p){*p=states[s];return 0;}
 HRESULT SetRenderState(int s,DWORD v){if(s==failFogSet){failFogSet=0;return -1;}states[s]=v;return 0;}
 HRESULT GetVertexDeclaration(IDirect3DVertexDeclaration9**p){*p=&decl;return 0;}
 HRESULT GetTexture(UINT,IDirect3DBaseTexture9**p){*p=&texture;return 0;}
 HRESULT GetSamplerState(UINT,int s,DWORD*p){*p=sampler[s-5];return 0;}
 HRESULT SetSamplerState(UINT,int s,DWORD v){sampler[s-5]=v;return 0;}
 HRESULT GetVertexShader(IDirect3DVertexShader9**p){*p=vs;return 0;}
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
#include "../src/raster.hpp"
float fogStart=60.f,fogEnd=260.f;

#include "../src/fog.hpp"
#include "../src/atmosphere_math.hpp"
int main(){
 IDirect3DDevice9 d;
 d.states[28]=1;d.states[35]=2;d.states[140]=1;d.states[36]=0x42c80000;d.states[37]=0x44fa0000;d.states[48]=1;
 const auto original=d.states;
 {DistanceFogState s(&d,true);assert(d.states[35]==3&&d.states[140]==0&&d.states[48]==0);float start,end;DWORD a=d.states[36],b=d.states[37];memcpy(&start,&a,4);memcpy(&end,&b,4);assert(start==60&&end==260);}
 assert(d.states==original);
 {DistanceFogState s(&d,false);assert(d.states==original);}
 d.failFogSet=37;{DistanceFogState s(&d,true);assert(d.states==original);}assert(d.states==original);
 d.ps=&customShader;{DistanceFogState s(&d,true);assert(d.states==original);}d.ps=nullptr;
 d.states[28]=0;auto off=d.states;{DistanceFogState s(&d,true);assert(d.states==off);}
 for(int quality=0;quality<4;quality++)for(bool active:{false,true})for(bool simple:{false,true})
  assert(atmosphere::ShadowQuality(quality,active,simple)==(active&&simple?0:quality));
 float color[4]={.8f,.4f,.2f,.5f};atmosphere::ScaleRGB(color,.5f);assert(color[0]==.4f&&color[1]==.2f&&color[2]==.1f&&color[3]==.5f);
 puts("PASS: production fog state apply/restore, partial-set failure rollback, disabled/custom-PS gates;");
 puts("      shadow quality isolation for all 16 combinations; RGB-only light scaling.");
 puts("NOT TESTED: native game call wrappers, real D3D9 driver, visual shadows/lighting/fog, gameplay.");
}
