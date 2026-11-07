// Per-draw source texture override. Native shaders, bones and material passes are retained.
struct TextureOverride {
 IDirect3DDevice9*d;IDirect3DBaseTexture9*original=nullptr;IDirect3DTexture9*small=nullptr;
 DWORD filters[3]{};bool ready=false,changed=false,native=false;
 explicit TextureOverride(IDirect3DDevice9*dev,bool fallback=false):d(dev){
  native=fallback;
  if(native&&(!nativeTextureFallback||textureMode!=2||!textureSize))return;
  if(inEffects||(!native&&!point&&(textureMode!=1||textureSize==0))||!ObjectDeclaration(d))return;
  if(FAILED(d->GetTexture(0,&original))||!original||original->GetType()!=D3DRTYPE_TEXTURE)return;
  for(int k=0;k<3;k++)if(FAILED(d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters+k)))return;
  ready=true;if((native||textureMode==1)&&textureSize)small=SmallTexture(d,static_cast<IDirect3DTexture9*>(original));
 }
 void Apply(){
  if(!ready||(native&&!small))return;
  changed=true;
  if(small&&FAILED(d->SetTexture(0,small)))small=nullptr;
  if(native&&small)nativeTextureDraws++;
  if(point||small){d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(0,D3DSAMP_MIPFILTER,small?D3DTEXF_NONE:D3DTEXF_POINT);textureDraws++;}
 }
 ~TextureOverride(){if(changed){d->SetTexture(0,original);for(int k=0;k<3;k++)d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters[k]);}Release(original);}
};
