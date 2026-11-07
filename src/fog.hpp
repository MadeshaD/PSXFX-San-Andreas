// Distance fog overrides only existing fog-enabled object draws, never HUD/sky sprites.
struct DistanceFogState {
 IDirect3DDevice9*d;DWORD saved[5]{};unsigned changedMask=0;
 static constexpr D3DRENDERSTATETYPE states[5]={D3DRS_FOGTABLEMODE,D3DRS_FOGVERTEXMODE,D3DRS_FOGSTART,D3DRS_FOGEND,D3DRS_RANGEFOGENABLE};
 explicit DistanceFogState(IDirect3DDevice9*dev,bool allowed):d(dev){
  if(!allowed)return;
  DWORD on=0;if(FAILED(d->GetRenderState(D3DRS_FOGENABLE,&on))||!on)return;
  IDirect3DPixelShader9*ps=nullptr;if(FAILED(d->GetPixelShader(&ps)))return;
  bool custom=ps!=nullptr;Release(ps);if(custom)return;
  for(int i=0;i<5;i++)if(FAILED(d->GetRenderState(states[i],saved+i)))return;
  DWORD start,end;memcpy(&start,&fogStart,4);memcpy(&end,&fogEnd,4);
  const DWORD values[5]={D3DFOG_LINEAR,D3DFOG_NONE,start,end,FALSE};
  for(unsigned i=0;i<5;++i){
   if(saved[i]==values[i])continue;
   if(FAILED(d->SetRenderState(states[i],values[i]))){Restore();return;}
   changedMask|=1u<<i;
  }
 }
 void Restore(){
  for(unsigned i=0;i<5&&changedMask;++i){
   if(changedMask&(1u<<i))d->SetRenderState(states[i],saved[i]);
  }
  changedMask=0;
 }
 ~DistanceFogState(){Restore();}
};
