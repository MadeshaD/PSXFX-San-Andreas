// GPU-only texture copies and a pre-HUD post pass. No writes to game textures/files.
template<class T> static void Release(T*& p){if(p){p->Release();p=nullptr;}}
struct FullState {
 IDirect3DDevice9*d;IDirect3DStateBlock9*block=nullptr;IDirect3DSurface9*rt=nullptr,*depth=nullptr;bool valid=false;
 explicit FullState(IDirect3DDevice9*dev):d(dev){
  if(FAILED(d->GetRenderTarget(0,&rt)))return;
  HRESULT hr=d->GetDepthStencilSurface(&depth);if(FAILED(hr)&&hr!=D3DERR_NOTFOUND)return;
  if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&block)))return;
  valid=SUCCEEDED(block->Capture());
 }
 ~FullState(){if(valid){d->SetDepthStencilSurface(nullptr);d->SetRenderTarget(0,rt);d->SetDepthStencilSurface(depth);block->Apply();}Release(depth);Release(rt);Release(block);}
};
static bool NoMRT(IDirect3DDevice9*d){
 IDirect3DSurface9*extra=nullptr;HRESULT hr=d->GetRenderTarget(1,&extra);
 bool ok=!extra&&(hr==D3DERR_NOTFOUND||hr==D3DERR_INVALIDCALL||SUCCEEDED(hr));Release(extra);return ok;
}
static bool Quad(IDirect3DDevice9*d,IDirect3DTexture9*source,IDirect3DPixelShader9*ps,UINT w,UINT h,const float*constants=nullptr){
 // Every mutated state is covered by FullState in the caller, including stream 0 cleared by DrawPrimitiveUP.
 if(FAILED(d->SetVertexShader(nullptr))||FAILED(d->SetPixelShader(ps))||FAILED(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1))||FAILED(d->SetTexture(0,source)))return false;
 if(FAILED(d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0))||FAILED(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE))||FAILED(d->SetStreamSourceFreq(0,1)))return false;
 const std::pair<D3DRENDERSTATETYPE,DWORD> states[]={
  {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},
  {D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_SEPARATEALPHABLENDENABLE,FALSE},
  {D3DRS_FOGENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_LIGHTING,FALSE},
  {D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_CLIPPLANEENABLE,0},{D3DRS_SRGBWRITEENABLE,FALSE},
  {D3DRS_COLORWRITEENABLE,15},{D3DRS_FILLMODE,D3DFILL_SOLID},
  {D3DRS_WRAP0,0},{D3DRS_VERTEXBLEND,D3DVBF_DISABLE},{D3DRS_INDEXEDVERTEXBLENDENABLE,FALSE},
  {D3DRS_MULTISAMPLEMASK,0xffffffff}};
 for(auto state:states)if(FAILED(d->SetRenderState(state.first,state.second)))return false;
 for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})if(FAILED(d->SetSamplerState(0,state,D3DTEXF_POINT)))return false;
 if(FAILED(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE))||FAILED(d->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,0))||FAILED(d->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,0))||FAILED(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0)))return false;
 if(FAILED(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP))||FAILED(d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP)))return false;
 if(constants&&FAILED(d->SetPixelShaderConstantF(0,constants,3)))return false;
 D3DVIEWPORT9 vp{0,0,w,h,0,1};if(FAILED(d->SetViewport(&vp)))return false;
 // D3D9 raster centers are integer positions: the half-pixel offset is intentional.
 struct V{float x,y,z,rhw,u,v;};
 V v[]={{-.5f,-.5f,0,1,0,0},{float(w)-.5f,-.5f,0,1,1,0},{-.5f,float(h)-.5f,0,1,0,1},{float(w)-.5f,float(h)-.5f,0,1,1,1}};
 return SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(V)));
}
struct CachedTexture{IDirect3DTexture9*source;IDirect3DTexture9*small;unsigned lastFrame;uint64_t bytes;};
static std::vector<CachedTexture> textureCache;
static IDirect3DTexture9*screenCopy=nullptr;
static D3DSURFACE_DESC screenDesc{};
static void ClearResources(){
 stitching::clear();
 ClearLowResolution();
 ClearNative();
 for(auto&c:textureCache){Release(c.source);Release(c.small);}textureCache.clear();Release(screenCopy);screenDesc={};
}
static void TrimCache(){
 for(auto it=textureCache.begin();it!=textureCache.end();){
  if(frame-it->lastFrame>120){Release(it->source);Release(it->small);it=textureCache.erase(it);}else ++it;
 }
}
static IDirect3DTexture9*SmallTexture(IDirect3DDevice9*d,IDirect3DTexture9*source){
 for(auto&c:textureCache)if(c.source==source){c.lastFrame=frame;return c.small;}
 if(textureSize<1||generatedThisFrame>=4||!copyPixel||!NoMRT(d))return nullptr;
 D3DSURFACE_DESC desc{};if(FAILED(source->GetLevelDesc(0,&desc)))return nullptr;
 if(desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL|D3DUSAGE_DYNAMIC))return nullptr;
 UINT w=desc.Width,h=desc.Height;if(!w||!h)return nullptr;
 while(std::max(w,h)>(UINT)textureSize){w=std::max(1u,w/2);h=std::max(1u,h/2);}
 if(w==desc.Width&&h==desc.Height)return nullptr;
 // Bound both cached targets and retained originals. Estimate uncompressed mip-chain bytes conservatively.
 uint64_t bytes=uint64_t(desc.Width)*desc.Height*6+uint64_t(w)*h*4;
 constexpr uint64_t budget=64ull*1024*1024;
 if(bytes>budget)return nullptr;
 for(;;){
  uint64_t total=bytes;for(auto&c:textureCache)total+=c.bytes;
  if(total<=budget&&textureCache.size()<256)break;
  auto it=std::min_element(textureCache.begin(),textureCache.end(),[](const CachedTexture&a,const CachedTexture&b){return a.lastFrame<b.lastFrame;});
  if(it==textureCache.end()||it->lastFrame==frame)return nullptr;
  Release(it->source);Release(it->small);textureCache.erase(it);
 }
 generatedThisFrame++;
 IDirect3DTexture9*small=nullptr;IDirect3DSurface9*target=nullptr;
 bool ok=false;
 {
  FullState state(d);
  if(state.valid&&SUCCEEDED(d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&small,nullptr))&&SUCCEEDED(small->GetSurfaceLevel(0,&target))){
   internalDraw=true;
   ok=SUCCEEDED(d->SetDepthStencilSurface(nullptr))&&SUCCEEDED(d->SetRenderTarget(0,target))&&Quad(d,source,copyPixel,w,h);
   internalDraw=false;
  }
 }
 Release(target);
 if(!ok){Release(small);return nullptr;}
 source->AddRef();textureCache.push_back({source,small,frame,bytes});return small;
}
static bool ObjectDeclaration(IDirect3DDevice9*d,bool*skinned=nullptr){
 if(skinned)*skinned=false;
 IDirect3DVertexDeclaration9*decl=nullptr;if(FAILED(d->GetVertexDeclaration(&decl))||!decl)return false;
 D3DVERTEXELEMENT9 elems[MAXD3DDECLLENGTH+1];UINT n=MAXD3DDECLLENGTH+1;
 HRESULT hr=decl->GetDeclaration(elems,&n);Release(decl);if(FAILED(hr))return false;
 bool pos=false,weights=false,indices=false;
 for(UINT i=0;i<n&&elems[i].Stream!=0xff;i++){
  if(elems[i].UsageIndex==0){weights|=elems[i].Usage==D3DDECLUSAGE_BLENDWEIGHT;indices|=elems[i].Usage==D3DDECLUSAGE_BLENDINDICES;}
  if(elems[i].Usage==D3DDECLUSAGE_POSITIONT)return false;
  if(elems[i].Usage==D3DDECLUSAGE_POSITION&&elems[i].UsageIndex==0)pos=true;
 }
 if(skinned)*skinned=weights&&indices;
 return pos;
}
#include "texture_override.hpp"
static void PostProcess(IDirect3DDevice9*d){
 if(!postPixel||!NoMRT(d))return;
 FullState state(d);if(!state.valid)return;
 D3DSURFACE_DESC desc{};if(FAILED(state.rt->GetDesc(&desc)))return;
 D3DVIEWPORT9 vp{};if(FAILED(d->GetViewport(&vp))||vp.X||vp.Y||vp.Width!=desc.Width||vp.Height!=desc.Height)return;
 if(!screenCopy||desc.Width!=screenDesc.Width||desc.Height!=screenDesc.Height||desc.Format!=screenDesc.Format){
  Release(screenCopy);screenDesc=desc;
  if(FAILED(d->CreateTexture(desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,&screenCopy,nullptr)))return;
 }
 IDirect3DSurface9*target=nullptr;if(FAILED(screenCopy->GetSurfaceLevel(0,&target)))return;
 HRESULT hr=d->StretchRect(state.rt,nullptr,target,nullptr,D3DTEXF_NONE);Release(target);
 if(FAILED(hr)){static bool logged=false;if(!logged){Log("Postprocess copy failed %08lx; original frame retained",(unsigned long)hr);logged=true;}return;}
 float gridH=frameLowHeight?float(frameLowHeight):0.f;
 float constants[12]={float((1<<colorBits)-1),saturation,dither,contrast,gridH?floorf(gridH*desc.Width/desc.Height+.5f):0,gridH,float(desc.Width),float(desc.Height),float(ditherType),0,0,0};
 internalDraw=true;
 Quad(d,screenCopy,postPixel,desc.Width,desc.Height,constants);
 internalDraw=false;
}
using ResetFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
static ResetFn originalReset=nullptr;
static HRESULT STDMETHODCALLTYPE Reset(IDirect3DDevice9*d,D3DPRESENT_PARAMETERS*p){
 if(d==device)ClearResources();
 if(d==device&&p){
  ((void(__cdecl*)(UINT))0x7F84F0)(1);
  p->MultiSampleType=D3DMULTISAMPLE_NONE;p->MultiSampleQuality=0;
 }
 HRESULT hr=originalReset(d,p);
 if(d==device&&SUCCEEDED(hr)){lowBroken=false;lowWarned=false;}
 return hr;
}
