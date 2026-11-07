// Execute the production per-draw texture guard with a mock device/cache.
#include <cassert>
#include <cstdio>
#include <cstdint>
using DWORD=uint32_t;using D3DSAMPLERSTATETYPE=int;
#define FAILED(x) ((x)<0)
constexpr int D3DSAMP_MAGFILTER=5,D3DSAMP_MINFILTER=6,D3DSAMP_MIPFILTER=7;
constexpr int D3DTEXF_POINT=1,D3DTEXF_NONE=0,D3DRTYPE_TEXTURE=3;
struct IDirect3DBaseTexture9 {int refs=1;int GetType(){return 3;}void Release(){--refs;}};
struct IDirect3DTexture9:IDirect3DBaseTexture9{};
struct IDirect3DDevice9 {
 IDirect3DTexture9 source,target;IDirect3DBaseTexture9*bound=&source;
 DWORD filters[3]={2,3,2};bool object=true,failTexture=false,failRead=false;
 int GetTexture(int,IDirect3DBaseTexture9**out){*out=bound;++bound->refs;return 0;}
 int SetTexture(int,IDirect3DBaseTexture9*t){if(failTexture){failTexture=false;return -1;}bound=t;return 0;}
 int GetSamplerState(int,int k,DWORD*v){if(failRead)return -1;*v=filters[k-5];return 0;}
 int SetSamplerState(int,int k,DWORD v){filters[k-5]=v;return 0;}
};
template<class T>void Release(T*&t){if(t){t->Release();t=nullptr;}}
bool ObjectDeclaration(IDirect3DDevice9*d){return d->object;}
bool cacheAvailable=true;int cacheCalls=0;
IDirect3DTexture9*SmallTexture(IDirect3DDevice9*d,IDirect3DTexture9*){++cacheCalls;return cacheAvailable?&d->target:nullptr;}
bool inEffects=false,point=true,nativeTextureFallback=true;
int textureMode=2,textureSize=128;
unsigned long long nativeTextureDraws=0,textureDraws=0;
#include "../src/texture_override.hpp"
int main(){
 IDirect3DDevice9 d;
 auto restored=[&](){assert(d.bound==&d.source&&d.source.refs==1);assert(d.filters[0]==2&&d.filters[1]==3&&d.filters[2]==2);};
 {TextureOverride t(&d,true);t.Apply();assert(d.bound==&d.target&&d.filters[2]==0);}restored();assert(nativeTextureDraws==1);
 {TextureOverride t(&d);t.Apply();assert(d.bound==&d.source);}restored();assert(cacheCalls==1);
 cacheAvailable=false;{TextureOverride t(&d,true);t.Apply();assert(d.bound==&d.source&&d.filters[0]==2);}restored();cacheAvailable=true;
 nativeTextureFallback=false;{TextureOverride t(&d,true);t.Apply();restored();}nativeTextureFallback=true;
 d.object=false;{TextureOverride t(&d,true);t.Apply();restored();}d.object=true;
 inEffects=true;{TextureOverride t(&d,true);t.Apply();restored();}inEffects=false;
 textureMode=0;{TextureOverride t(&d,true);t.Apply();restored();}textureMode=2;
 textureSize=0;{TextureOverride t(&d,true);t.Apply();restored();}textureSize=128;
 d.failRead=true;{TextureOverride t(&d,true);t.Apply();assert(d.bound==&d.source);}restored();d.failRead=false;
 d.failTexture=true;{TextureOverride t(&d,true);t.Apply();assert(d.bound==&d.source);}restored();assert(nativeTextureDraws==1);
 textureMode=1;{TextureOverride t(&d);t.Apply();assert(d.bound==&d.target);}restored();
 puts("PASS: production texture guard: fallback and legacy paths, exact texture/filter restoration,");
 puts("      reference release, cache miss, opt-out, HUD/effects gates, read/set failures.");
 puts("NOT TESTED: GTA skin/car materials, real GPU texture cache, D3D9 runtime.");
}
