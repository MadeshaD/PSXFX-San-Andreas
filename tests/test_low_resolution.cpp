// Execute production low-resolution lifecycle against a small fake D3D device.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <vector>
#include <memory>
#define STDMETHODCALLTYPE
#define __cdecl
using DWORD=uint32_t;using UINT=unsigned;using LONG=int32_t;using BYTE=unsigned char;using HRESULT=int;using D3DFORMAT=int;
constexpr int D3DFMT_UNKNOWN=0,D3DFMT_D24S8=75,D3DFMT_D24X4S4=79,D3DFMT_D15S1=73,D3DMULTISAMPLE_NONE=0,D3DUSAGE_RENDERTARGET=1,D3DPOOL_DEFAULT=0,D3DTEXF_POINT=1,TRUE=1;
constexpr DWORD D3DCLEAR_ZBUFFER=2,D3DCLEAR_STENCIL=4;
#define SUCCEEDED(x) ((x)>=0)
#define FAILED(x) ((x)<0)
struct RECT{LONG left,top,right,bottom;};
struct D3DVIEWPORT9{DWORD X,Y,Width,Height;float MinZ,MaxZ;};
struct D3DSURFACE_DESC{UINT Width=0,Height=0;D3DFORMAT Format=0;int MultiSampleType=0;};
struct IDirect3DSurface9{D3DSURFACE_DESC desc;int refs=1;void AddRef(){++refs;}void Release(){assert(refs>0);--refs;}HRESULT GetDesc(D3DSURFACE_DESC*p){*p=desc;return 0;}};
struct IDirect3DTexture9{IDirect3DSurface9*surface;int refs=1;void Release(){assert(refs>0);if(!--refs)surface->Release();}HRESULT GetSurfaceLevel(UINT,IDirect3DSurface9**p){*p=surface;surface->AddRef();return 0;}};
static std::vector<std::unique_ptr<IDirect3DSurface9>> surfaces;
static std::vector<std::unique_ptr<IDirect3DTexture9>> textures;
struct IDirect3DDevice9{
 IDirect3DSurface9*rt=nullptr,*ds=nullptr;D3DVIEWPORT9 vp{0,0,1920,1080,0,1};RECT sc{0,0,1920,1080};
 int failAt=0,operations=0;unsigned clears=0,copies=0;
 bool error(){return ++operations==failAt;}
 HRESULT GetRenderTarget(DWORD,IDirect3DSurface9**p){*p=rt;rt->AddRef();return 0;}
 HRESULT GetDepthStencilSurface(IDirect3DSurface9**p){*p=ds;if(ds)ds->AddRef();return 0;}
 HRESULT GetViewport(D3DVIEWPORT9*p){*p=vp;return 0;}
 HRESULT GetScissorRect(RECT*p){*p=sc;return 0;}
 HRESULT CreateTexture(UINT w,UINT h,UINT,DWORD,D3DFORMAT f,int,IDirect3DTexture9**p,void*){
  if(error())return -1;surfaces.push_back(std::make_unique<IDirect3DSurface9>());auto*s=surfaces.back().get();s->desc={w,h,f,0};
  textures.push_back(std::make_unique<IDirect3DTexture9>());auto*t=textures.back().get();t->surface=s;*p=t;return 0;
 }
 HRESULT CreateDepthStencilSurface(UINT w,UINT h,D3DFORMAT f,int,int,int,IDirect3DSurface9**p,void*){
  if(error())return -1;surfaces.push_back(std::make_unique<IDirect3DSurface9>());*p=surfaces.back().get();(*p)->desc={w,h,f,0};return 0;
 }
 HRESULT StretchRect(IDirect3DSurface9*,void*,IDirect3DSurface9*,void*,int){if(error())return -1;++copies;return 0;}
 HRESULT Clear(DWORD,void*,DWORD flags,DWORD,float,int){assert(flags==(D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL));if(error())return -1;++clears;return 0;}
};
static IDirect3DDevice9*device;static bool lowActive=false,internalDraw=false,enabled=true,failed=false,lowBroken=false,lowWarned=false;
static int renderHeight=240,frameLowHeight=0;static DWORD sceneThread=1;
static void*copyPixel=(void*)1;static DWORD GetCurrentThreadId(){return 1;}
static void Log(const char*,...){}static bool Write(void*,const void*,size_t){return true;}
template<class T>void Release(T*&p){if(p){p->Release();p=nullptr;}}
static bool NoMRT(IDirect3DDevice9*){return true;}
struct FullState{bool valid=true;explicit FullState(IDirect3DDevice9*){}};
static bool quadOK=true;
static bool Quad(IDirect3DDevice9*d,IDirect3DTexture9*t,void*,UINT w,UINT h){assert(d->rt!=t->surface);assert(w==1920&&h==1080);return quadOK;}
#include "../src/low_resolution.hpp"
static HRESULT RT(IDirect3DDevice9*d,DWORD,IDirect3DSurface9*s){if(d->error())return -1;d->rt=s;return 0;}
static HRESULT DS(IDirect3DDevice9*d,IDirect3DSurface9*s){if(d->error())return -1;d->ds=s;return 0;}
static HRESULT VP(IDirect3DDevice9*d,const D3DVIEWPORT9*p){if(d->error())return -1;d->vp=*p;return 0;}
static HRESULT SC(IDirect3DDevice9*d,const RECT*p){if(d->error())return -1;d->sc=*p;return 0;}
int main(){
 (void)&BeforeNativePost;(void)&InstallLowBoundary;
 lowSetRT=RT;lowSetDS=DS;lowSetVP=VP;lowSetSC=SC;
 IDirect3DSurface9 main{{1920,1080,21,0}},depth{{1920,1080,D3DFMT_D24S8,0}},aux{{128,128,21,0}};
 IDirect3DDevice9 d;device=&d;d.rt=&main;d.ds=&depth;
 auto restored=[&]{assert(d.rt==&main&&d.ds==&depth&&d.vp.Width==1920&&d.vp.Height==1080&&d.sc.right==1920&&!lowActive&&main.refs==1&&depth.refs==1);};
 LowBegin(&d);assert(lowActive&&d.rt==lowSurface&&d.ds==lowDepth&&d.vp.Width==427&&d.vp.Height==240&&d.clears==1);
 float x,y;assert(LowScale(x,y)&&std::abs(x-427.f/1920)<1e-7&&std::abs(y-240.f/1080)<1e-7);
 D3DVIEWPORT9 full{0,0,1920,1080,0,1};RECT fullsc{0,0,1920,1080};
 LowSetRT(&d,0,&main);LowSetDS(&d,&depth);LowSetVP(&d,&full);LowSetSC(&d,&fullsc);
 assert(d.rt==lowSurface&&d.ds==lowDepth&&d.vp.Height==240&&d.sc.right==427);
 LowSetRT(&d,0,&aux);assert(d.rt==&aux&&!LowScale(x,y));LowSetVP(&d,&full);assert(d.vp.Height==1080);
 LowSetRT(&d,0,&main);LowEnd(&d);restored();assert(lowFrames==1&&frameLowHeight==240);
 ClearLowResolution();restored();
 for(int step=1;step<=9;step++){
  d.operations=0;d.failAt=step;lowBroken=false;LowBegin(&d);assert(!lowActive&&lowBroken);restored();d.failAt=0;ClearLowResolution();
 }
 lowBroken=false;main.desc.MultiSampleType=4;LowBegin(&d);restored();assert(frameLowHeight==0);main.desc.MultiSampleType=0;
 enabled=false;LowBegin(&d);restored();enabled=true;
 renderHeight=0;LowBegin(&d);restored();renderHeight=240;
 lowBroken=false;LowBegin(&d);ClearLowResolution();restored();
 lowBroken=false;quadOK=false;LowBegin(&d);LowEnd(&d);restored();assert(lowBroken&&frameLowHeight==0);ClearLowResolution();quadOK=true;
 lowBroken=false;renderHeight=360;LowBegin(&d);assert(lowW==640&&lowH==360);LowEnd(&d);restored();ClearLowResolution();
 for(auto&s:surfaces)assert(s->refs==0);for(auto&t:textures)assert(t->refs==0);
 puts("PASS production low-resolution lifecycle: allocation, small color/depth/viewport, camera rebinds,");
 puts("auxiliary target bypass, scissor scaling, composite boundary, nine injected setup failures,");
 puts("MSAA/disabled/zero-height bypass, active cleanup/reset and balanced references.");
 puts("NOT TESTED: real D3D9/RenderWare cache behavior, Windows driver, GTA visual results.");
}
