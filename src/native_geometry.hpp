#include "native_shader.hpp"
struct NativeVariant {IDirect3DVertexShader9*source;IDirect3DVertexShader9*adapted;unsigned w,h,snap,snapType,clipPrecision;bool positionOnly;};
static std::vector<NativeVariant> nativeVariants;
static void ClearNative(){for(auto&e:nativeVariants){Release(e.source);Release(e.adapted);}nativeVariants.clear();}
static IDirect3DVertexShader9*AdaptNative(IDirect3DDevice9*d,IDirect3DVertexShader9*source,unsigned w,unsigned h,unsigned snapQ,bool&positionOnly){
 positionOnly=false;
 for(auto&e:nativeVariants)if(e.source==source&&e.w==w&&e.h==h&&e.snap==snapQ&&e.snapType==(unsigned)vertexSnapType&&e.clipPrecision==(unsigned)clipSnapPrecision){positionOnly=e.positionOnly;return e.adapted;}
 if(nativeVariants.size()>=128)return nullptr;
 UINT size=0;if(FAILED(source->GetFunction(nullptr,&size))||size<8||size>65536||size%4)return nullptr;
 std::vector<uint32_t> code(size/4);if(FAILED(source->GetFunction(code.data(),&size)))return nullptr;
 auto converted=native_shader::Convert(code,w,h,snapQ,vertexSnapType,clipSnapPrecision);IDirect3DVertexShader9*adapted=nullptr;
 if(!converted.code.empty()){
  HRESULT hr=d->CreateVertexShader(reinterpret_cast<const DWORD*>(converted.code.data()),&adapted);
  if(FAILED(hr)){
   adapted=NativeReassemble(d,converted.code);
   D3DCAPS9 caps{};
   if(!adapted&&nativeSnapFallback&&snapQ&&SUCCEEDED(d->GetDeviceCaps(&caps))&&caps.VS20Caps.NumTemps>=29&&caps.VS20Caps.DynamicFlowControlDepth>=1){
    auto fallback=native_shader::PositionOnly(code,w,h,snapQ,vertexSnapType,clipSnapPrecision);
    if(!fallback.code.empty()){
     hr=d->CreateVertexShader(reinterpret_cast<const DWORD*>(fallback.code.data()),&adapted);
     if(SUCCEEDED(hr))positionOnly=true;
    }
   }
  }
 }
 source->AddRef();nativeVariants.push_back({source,adapted,w,h,snapQ,(unsigned)vertexSnapType,(unsigned)clipSnapPrecision,positionOnly});
 return adapted;
}
struct NativeGeometryState {
 IDirect3DDevice9*d;IDirect3DVertexShader9*original=nullptr;float ps[5][4]{};DWORD fog=0,filters[3]{};bool changed=false,positionOnly=false,skinned=false;
 explicit NativeGeometryState(IDirect3DDevice9*dev):d(dev){}
 ~NativeGeometryState(){
  if(changed){d->SetVertexShader(original);if(!positionOnly){d->SetPixelShader(nullptr);d->SetPixelShaderConstantF(0,&ps[0][0],5);d->SetRenderState(D3DRS_FOGENABLE,fog);}for(int k=0;k<3;k++)d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters[k]);}
  Release(original);
 }
 bool Prepare(){
  if(!nativeGeometry||!extendGeometry||inEffects||!ObjectDeclaration(d,&skinned))return false;
  if(FAILED(d->GetVertexShader(&original))||!original)return false;
  IDirect3DPixelShader9*oldPS=nullptr;HRESULT result=d->GetPixelShader(&oldPS);bool custom=oldPS!=nullptr;Release(oldPS);
  if(FAILED(result)||custom||RS(d,D3DRS_SPECULARENABLE)||RS(d,D3DRS_CLIPPLANEENABLE)||RS(d,D3DRS_RANGEFOGENABLE))return false;
  if(TS(d,1,D3DTSS_COLOROP)!=D3DTOP_DISABLE||TS(d,0,D3DTSS_COLOROP)!=D3DTOP_MODULATE||TS(d,0,D3DTSS_ALPHAOP)!=D3DTOP_MODULATE)return false;
  for(auto pair:{std::pair<D3DTEXTURESTAGESTATETYPE,D3DTEXTURESTAGESTATETYPE>{D3DTSS_COLORARG1,D3DTSS_COLORARG2},{D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2}}){DWORD a=TS(d,0,pair.first),b=TS(d,0,pair.second);if(!((a==D3DTA_TEXTURE&&b==D3DTA_DIFFUSE)||(b==D3DTA_TEXTURE&&a==D3DTA_DIFFUSE)))return false;}
  IDirect3DBaseTexture9*texture=nullptr;if(FAILED(d->GetTexture(0,&texture))||!texture)return false;
  D3DSURFACE_DESC desc{};bool ok=texture->GetType()==D3DRTYPE_TEXTURE&&SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0,&desc));Release(texture);
  if(!ok||!desc.Width||!desc.Height||(desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL)))return false;
  float amount=SnapAmount(),aff=(mode==0||mode==2)?affine:0;
  if(uvSteps==0&&affineColor==0&&amount<=0&&(aff<=0||maxTexels<=0)&&!(textureMode==2&&texturePixelSize>1)&&!(carDepth&&vehicleBrightness!=1.f))return false;
  D3DVIEWPORT9 vp{};if(FAILED(d->GetViewport(&vp))||!vp.Width||!vp.Height)return false;
  unsigned h=std::min((unsigned)height,(unsigned)vp.Height),w=(unsigned)floorf(float(h)*vp.Width/vp.Height+.5f);
  auto*adapted=AdaptNative(d,original,w,h,(unsigned)floorf(amount*256.f+.5f),positionOnly);if(!adapted)return false;
  if(positionOnly){
   for(int k=0;k<3;k++)if(FAILED(d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters+k)))return false;
   changed=true;if(FAILED(d->SetVertexShader(adapted)))return false;
   if(point)for(int k=0;k<3;k++)if(FAILED(d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),D3DTEXF_POINT)))return false;
   return true;
  }
  float pc[5][4]{};Color(RS(d,D3DRS_FOGCOLOR),pc[0]);
  DWORD fogMode=RS(d,D3DRS_FOGTABLEMODE);if(!fogMode)fogMode=RS(d,D3DRS_FOGVERTEXMODE);
  fog=RS(d,D3DRS_FOGENABLE);if(fog&&(fogMode==0||fogMode>3))return false;pc[1][0]=fog&&fogMode?1.f:0.f;pc[1][1]=(float)fogMode;
  pc[1][2]=FloatState(RS(d,D3DRS_FOGSTART));pc[1][3]=FloatState(RS(d,D3DRS_FOGEND));pc[2][0]=FloatState(RS(d,D3DRS_FOGDENSITY));
  pc[0][3]=UVGrid(desc.Height);pc[2][2]=UVGrid(desc.Width);pc[2][3]=affineColor;
  pc[3][0]=aff;pc[3][1]=maxTexels;pc[3][2]=(float)desc.Width;pc[3][3]=(float)desc.Height;pc[4][0]=textureMode==2?(float)texturePixelSize:1.f;pc[4][2]=(float)affineType;pc[4][3]=carDepth?vehicleBrightness:1.f;
  if(FAILED(d->GetPixelShaderConstantF(0,&ps[0][0],5)))return false;
  for(int k=0;k<3;k++)if(FAILED(d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters+k)))return false;
  changed=true;
  if(FAILED(d->SetVertexShader(adapted))||FAILED(d->SetPixelShader(pixel))||FAILED(d->SetPixelShaderConstantF(0,&pc[0][0],5))||FAILED(d->SetRenderState(D3DRS_FOGENABLE,FALSE)))return false;
  if(point)for(int k=0;k<3;k++)if(FAILED(d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),D3DTEXF_POINT)))return false;
  return true;
 }
};
