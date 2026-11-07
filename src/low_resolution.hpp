#pragma once
// Main-view offscreen rasterization. Auxiliary targets are not redirected.
using SetRTFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,IDirect3DSurface9*);
using SetDSFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,IDirect3DSurface9*);
using SetVPFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const D3DVIEWPORT9*);
using SetSCFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*);
static SetRTFn lowSetRT=nullptr;static SetDSFn lowSetDS=nullptr;
static SetVPFn lowSetVP=nullptr;static SetSCFn lowSetSC=nullptr;
static IDirect3DTexture9* lowTexture=nullptr;
static IDirect3DSurface9 *lowSurface=nullptr,*lowDepth=nullptr,*lowMain=nullptr,*lowMainDepth=nullptr;
static D3DVIEWPORT9 lowOriginalVP{};static RECT lowOriginalSC{};
static UINT lowW=0,lowH=0;static D3DFORMAT lowFormat=D3DFMT_UNKNOWN,lowDepthFormat=D3DFMT_UNKNOWN;
static unsigned long long lowFrames=0,lowSkipped=0;
static bool LowTarget(IDirect3DDevice9*d){
 if(!lowActive||internalDraw||d!=device||GetCurrentThreadId()!=sceneThread)return false;
 IDirect3DSurface9*s=nullptr;if(FAILED(d->GetRenderTarget(0,&s)))return false;
 bool result=s==lowSurface;Release(s);return result;
}
static bool LowScale(float&x,float&y){
 if(!device||!LowTarget(device))return false;
 x=float(lowW)/lowOriginalVP.Width;y=float(lowH)/lowOriginalVP.Height;return true;
}
static HRESULT STDMETHODCALLTYPE LowSetRT(IDirect3DDevice9*d,DWORD i,IDirect3DSurface9*s){
 if(lowActive&&!internalDraw&&d==device&&GetCurrentThreadId()==sceneThread&&i==0&&s==lowMain)s=lowSurface;
 return lowSetRT(d,i,s);
}
static HRESULT STDMETHODCALLTYPE LowSetDS(IDirect3DDevice9*d,IDirect3DSurface9*s){
 if(s==lowMainDepth&&LowTarget(d))s=lowDepth;
 return lowSetDS(d,s);
}
static HRESULT STDMETHODCALLTYPE LowSetVP(IDirect3DDevice9*d,const D3DVIEWPORT9*p){
 if(p&&LowTarget(d)&&p->X==0&&p->Y==0&&p->Width==lowOriginalVP.Width&&p->Height==lowOriginalVP.Height){
  D3DVIEWPORT9 v=*p;v.Width=lowW;v.Height=lowH;return lowSetVP(d,&v);
 }
 return lowSetVP(d,p);
}
static HRESULT STDMETHODCALLTYPE LowSetSC(IDirect3DDevice9*d,const RECT*p){
 if(p&&LowTarget(d)){
  RECT r{LONG(std::floor(double(p->left)*lowW/lowOriginalVP.Width)),LONG(std::floor(double(p->top)*lowH/lowOriginalVP.Height)),LONG(std::ceil(double(p->right)*lowW/lowOriginalVP.Width)),LONG(std::ceil(double(p->bottom)*lowH/lowOriginalVP.Height))};
  return lowSetSC(d,&r);
 }
 return lowSetSC(d,p);
}
static bool LowRestore(IDirect3DDevice9*d){
 // Use original methods: restoring a main surface must not redirect it again.
 bool ok=SUCCEEDED(lowSetDS(d,nullptr));
 ok=SUCCEEDED(lowSetRT(d,0,lowMain))&&ok;
 ok=SUCCEEDED(lowSetDS(d,lowMainDepth))&&ok;
 ok=SUCCEEDED(lowSetVP(d,&lowOriginalVP))&&ok;
 ok=SUCCEEDED(lowSetSC(d,&lowOriginalSC))&&ok;
 lowActive=false;return ok;
}
static void LowReleaseFrame(){Release(lowMainDepth);Release(lowMain);}
static void ClearLowResolution(){
 if(lowActive&&device)LowRestore(device);
 LowReleaseFrame();Release(lowDepth);Release(lowSurface);Release(lowTexture);
 lowW=lowH=0;lowFormat=lowDepthFormat=D3DFMT_UNKNOWN;frameLowHeight=0;
}
static void LowEnd(IDirect3DDevice9*d){
 if(!lowActive)return;
 bool restored=LowRestore(d),ok=false;
 if(restored){
  internalDraw=true;
  {FullState state(d);if(state.valid)ok=Quad(d,lowTexture,copyPixel,lowOriginalVP.Width,lowOriginalVP.Height);}
  internalDraw=false;
 }
 if(ok){++lowFrames;frameLowHeight=int(lowH);}
 else {++lowSkipped;Log("Low resolution: composite/restore failed; disabling until reload");lowBroken=true;}
 LowReleaseFrame();
}
static void LowBegin(IDirect3DDevice9*d){
 frameLowHeight=0;
 if(lowActive)LowEnd(d);
 if(!enabled||failed||lowBroken||renderHeight==0||!copyPixel||!NoMRT(d))return;
 if(FAILED(d->GetRenderTarget(0,&lowMain))||FAILED(d->GetDepthStencilSurface(&lowMainDepth))||!lowMainDepth){LowReleaseFrame();return;}
 D3DSURFACE_DESC a{},b{};
 if(FAILED(lowMain->GetDesc(&a))||FAILED(lowMainDepth->GetDesc(&b))||FAILED(d->GetViewport(&lowOriginalVP))||FAILED(d->GetScissorRect(&lowOriginalSC))){LowReleaseFrame();return;}
 // MSAA requires a separately resolved seed and compatible depth handling.
 // Refuse it explicitly rather than silently downsampling a finished frame.
 if(a.MultiSampleType!=D3DMULTISAMPLE_NONE||b.MultiSampleType!=D3DMULTISAMPLE_NONE||lowOriginalVP.X||lowOriginalVP.Y||lowOriginalVP.Width!=a.Width||lowOriginalVP.Height!=a.Height){
  ++lowSkipped;if(!lowWarned){Log("Low resolution bypass: disable MSAA; main viewport must cover target");lowWarned=true;}LowReleaseFrame();return;
 }
 UINT h=std::min(UINT(renderHeight),a.Height),w=std::max(1u,UINT(std::floor(double(a.Width)*h/a.Height+.5)));
 if(h==a.Height){LowReleaseFrame();return;}
 if(!lowTexture||w!=lowW||h!=lowH||a.Format!=lowFormat||b.Format!=lowDepthFormat){
  Release(lowDepth);Release(lowSurface);Release(lowTexture);lowW=w;lowH=h;lowFormat=a.Format;lowDepthFormat=b.Format;
  if(FAILED(d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,a.Format,D3DPOOL_DEFAULT,&lowTexture,nullptr))||FAILED(lowTexture->GetSurfaceLevel(0,&lowSurface))||FAILED(d->CreateDepthStencilSurface(w,h,b.Format,D3DMULTISAMPLE_NONE,0,TRUE,&lowDepth,nullptr))){
   lowBroken=true;Log("Low resolution allocation failed; original rendering retained");LowReleaseFrame();return;
  }
  Log("Low resolution target: %ux%u (output %ux%u)",w,h,a.Width,a.Height);
 }
 // Preserve the sky/background already painted before RenderScene. Geometry
 // itself is rasterized in the small color/depth buffers, not copied afterward.
 bool ok=SUCCEEDED(d->StretchRect(lowMain,nullptr,lowSurface,nullptr,D3DTEXF_POINT));
 if(ok){
  D3DVIEWPORT9 vp{0,0,w,h,lowOriginalVP.MinZ,lowOriginalVP.MaxZ};RECT sc{LONG(std::floor(double(lowOriginalSC.left)*w/a.Width)),LONG(std::floor(double(lowOriginalSC.top)*h/a.Height)),LONG(std::ceil(double(lowOriginalSC.right)*w/a.Width)),LONG(std::ceil(double(lowOriginalSC.bottom)*h/a.Height))};
  ok=SUCCEEDED(lowSetDS(d,nullptr))&&SUCCEEDED(lowSetRT(d,0,lowSurface))&&SUCCEEDED(lowSetDS(d,lowDepth))&&SUCCEEDED(lowSetVP(d,&vp))&&SUCCEEDED(lowSetSC(d,&sc));
  DWORD flags=D3DCLEAR_ZBUFFER;if(b.Format==D3DFMT_D24S8||b.Format==D3DFMT_D24X4S4||b.Format==D3DFMT_D15S1)flags|=D3DCLEAR_STENCIL;
  if(ok)ok=SUCCEEDED(d->Clear(0,nullptr,flags,0,1.f,0));
 }
 if(!ok){LowRestore(d);LowReleaseFrame();lowBroken=true;++lowSkipped;Log("Low resolution setup failed; rolled back to original target");return;}
 lowActive=true;
}
// RenderEffects ends with a tail jump to CPostEffects::Render. Finish the
// offscreen world before those effects read the game's full-size camera raster.
static void __cdecl BeforeNativePost(){if(device)LowEnd(device);((void(__cdecl*)())0x7046E0)();}
static bool InstallLowBoundary(){
 const BYTE signature[]={0xe9,0xb4,0x64,0x1c,0x00};
 if(memcmp((void*)0x53E227,signature,5)){Log("Low resolution unavailable: native post boundary changed");return false;}
 BYTE patch[5]={0xe9};DWORD rel=(DWORD)(uintptr_t)&BeforeNativePost-(0x53E227+5);memcpy(patch+1,&rel,4);
 return Write((void*)0x53E227,patch,5);
}
