// Per-draw pixel state only. Leaves blend/alpha test, depth and original vertex pipeline intact.
struct RasterState {
 IDirect3DDevice9*d;DWORD filters[3]{};float constants[4][4]{};
 bool filtersSaved=false,shaderChanged=false;
 explicit RasterState(IDirect3DDevice9*dev):d(dev){}
 void RestoreShader(){if(shaderChanged){d->SetPixelShader(nullptr);d->SetPixelShaderConstantF(0,&constants[0][0],4);shaderChanged=false;}}
 ~RasterState(){RestoreShader();if(filtersSaved)for(int k=0;k<3;k++)d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters[k]);}
 bool Transformed(){
  IDirect3DVertexDeclaration9*decl=nullptr;if(FAILED(d->GetVertexDeclaration(&decl))||!decl)return false;
  D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1];UINT n=MAXD3DDECLLENGTH+1;HRESULT hr=decl->GetDeclaration(elements,&n);Release(decl);
  if(FAILED(hr))return false;
  for(UINT i=0;i<n&&elements[i].Stream!=0xff;i++)if(elements[i].Usage==D3DDECLUSAGE_POSITIONT)return true;
  return false;
 }
 bool Texture(bool allowTarget){
  IDirect3DBaseTexture9*t=nullptr;if(FAILED(d->GetTexture(0,&t))||!t)return false;
  D3DSURFACE_DESC desc{};bool ok=t->GetType()==D3DRTYPE_TEXTURE&&SUCCEEDED(static_cast<IDirect3DTexture9*>(t)->GetLevelDesc(0,&desc));
  Release(t);return ok&&(allowTarget||!(desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL)));
 }
 bool Point(){
  if(!filtersSaved){for(int k=0;k<3;k++)if(FAILED(d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),filters+k)))return false;filtersSaved=true;}
  for(int k=0;k<3;k++)if(FAILED(d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)(D3DSAMP_MAGFILTER+k),D3DTEXF_POINT)))return false;
  return true;
 }
 void PointHUD(){if(hudPoint&&Transformed()&&Texture(true)&&Point())hudDraws++;}
 static bool Arg(DWORD value,float&out){
  DWORD base=value&D3DTA_SELECTMASK;
  if(value&~(D3DTA_SELECTMASK|D3DTA_COMPLEMENT|D3DTA_ALPHAREPLICATE))return false;
  if(base==D3DTA_DIFFUSE||base==D3DTA_CURRENT)out=0;
  else if(base==D3DTA_TEXTURE)out=1;
  else if(base==D3DTA_TFACTOR)out=2;
  else return false;
  if(value&D3DTA_ALPHAREPLICATE)out+=4;
  if(value&D3DTA_COMPLEMENT)out+=8;
  return true;
 }
 bool Stage(D3DTEXTURESTAGESTATETYPE op,D3DTEXTURESTAGESTATETYPE arg1,D3DTEXTURESTAGESTATETYPE arg2,float*out){
  DWORD operation=TS(d,0,op);if(operation<D3DTOP_SELECTARG1||operation>D3DTOP_ADD)return false;
  out[0]=(float)operation;out[1]=out[2]=0;
  if(operation!=D3DTOP_SELECTARG2&&!Arg(TS(d,0,arg1),out[1]))return false;
  if(operation!=D3DTOP_SELECTARG1&&!Arg(TS(d,0,arg2),out[2]))return false;
  return true;
 }
 bool Grid(){
  if(!spritePixel||TS(d,1,D3DTSS_COLOROP)!=D3DTOP_DISABLE||TS(d,0,D3DTSS_RESULTARG)!=D3DTA_CURRENT)return false;
  // SM3 pixel/vertex linkage and fog are not guessed. Preserve unsupported shader paths.
  IDirect3DVertexShader9*vs=nullptr;IDirect3DPixelShader9*ps=nullptr;
  HRESULT a=d->GetVertexShader(&vs),b=d->GetPixelShader(&ps);bool custom=vs||ps;Release(vs);Release(ps);
  if(FAILED(a)||FAILED(b)||custom||RS(d,D3DRS_FOGENABLE)||RS(d,D3DRS_SPECULARENABLE))return false;
  if(TS(d,0,D3DTSS_TEXCOORDINDEX)!=0||TS(d,0,D3DTSS_TEXTURETRANSFORMFLAGS)!=D3DTTFF_DISABLE)return false;
  IDirect3DVertexDeclaration9*decl=nullptr;if(FAILED(d->GetVertexDeclaration(&decl))||!decl)return false;
  D3DVERTEXELEMENT9 elems[MAXD3DDECLLENGTH+1];UINT n=MAXD3DDECLLENGTH+1;HRESULT hr=decl->GetDeclaration(elems,&n);Release(decl);
  if(FAILED(hr))return false;
  bool uv=false,color=false;
  for(UINT i=0;i<n&&elems[i].Stream!=0xff;i++){
   if(elems[i].UsageIndex)continue;
   if(elems[i].Usage==D3DDECLUSAGE_TEXCOORD&&elems[i].Type==D3DDECLTYPE_FLOAT2)uv=true;
   if(elems[i].Usage==D3DDECLUSAGE_COLOR&&elems[i].Type==D3DDECLTYPE_D3DCOLOR)color=true;
  }
  if(!uv||!color)return false;
  float c[4][4]{};c[0][0]=(float)texturePixelSize;
  if(!Stage(D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,c[1])||!Stage(D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,c[2]))return false;
  Color(RS(d,D3DRS_TEXTUREFACTOR),c[3]);
  if(FAILED(d->GetPixelShaderConstantF(0,&constants[0][0],4)))return false;
  shaderChanged=true;
  if(FAILED(d->SetPixelShader(spritePixel))||FAILED(d->SetPixelShaderConstantF(0,&c[0][0],4))){RestoreShader();return false;}
  return true;
 }
 bool World(){
  // Render-target sprites belong to the game's framebuffer effects, not source artwork.
  if(!Texture(false))return false;
  bool sprite=Transformed()||inEffects;
  if(sprite?effectsPoint:point)Point();
  if(textureMode==2&&texturePixelSize>1&&(!sprite||effectsGrid)){
   if(Grid()){spriteGridDraws++;return true;}else gridSkipped++;
  }
  return false;
 }
};
