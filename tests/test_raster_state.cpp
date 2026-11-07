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
int main(){
 IDirect3DDevice9 d;for(int i=0;i<16;i++)d.constants[i]=float(i)+.125f;
 auto check=[&](){assert(d.ps==nullptr);assert(d.sampler[0]==2&&d.sampler[1]==3&&d.sampler[2]==2);for(int i=0;i<16;i++)assert(d.constants[i]==float(i)+.125f);};
 {RasterState s(&d);s.PointHUD();assert(d.sampler[0]==1&&d.ps==nullptr);}check();assert(hudDraws==1);
 {RasterState s(&d);assert(s.World());assert(d.ps==&gridShader);assert(d.constants[0]==3);}check();assert(spriteGridDraws==1);
 d.failUpload=true;{RasterState s(&d);assert(!s.World());assert(d.ps==nullptr);for(int i=0;i<16;i++)assert(d.constants[i]==float(i)+.125f);}check();
 d.ps=&customShader;{RasterState s(&d);assert(!s.World());assert(d.ps==&customShader);}assert(d.ps==&customShader);d.ps=nullptr;check();
 d.states[28]=1;{RasterState s(&d);s.World();assert(d.ps==nullptr&&d.states[28]==1);}check();d.states[28]=0;
 d.texture.usage=1;{RasterState s(&d);s.World();assert(d.ps==nullptr&&d.sampler[0]==2);}check();
 {RasterState s(&d);s.PointHUD();assert(d.sampler[0]==1);}check();d.texture.usage=0;
 hudPoint=false;{RasterState s(&d);s.PointHUD();assert(d.sampler[0]==2);}check();hudPoint=true;
 d.decl.transformed=false;{RasterState s(&d);s.PointHUD();assert(d.sampler[0]==2);}check();
 d.stage[1]=26;{RasterState s(&d);assert(!s.World());assert(d.ps==nullptr);}check();
 puts("PASS: production RasterState on fake device: HUD filtering, grid apply/restore, upload failure rollback,");
 puts("      custom shader/fog preservation, RT exclusion, HUD toggle and transformed declaration selection.");
 puts("NOT TESTED: real Windows/D3D9 driver calls, runtime shader linkage, GTA rendering.");
}
