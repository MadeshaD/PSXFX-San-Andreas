#pragma once
#include "ui_snap_math.hpp"
using UIVertex=psx_ui::Vertex;
using UIPrimitive=int(__cdecl*)(int,UIVertex*,int);
using UIIndexed=int(__cdecl*)(int,UIVertex*,int,unsigned short*,int);
using UILine=int(__cdecl*)(UIVertex*,int,int,int);
using UITriangle=int(__cdecl*)(UIVertex*,int,int,int,int);
static UIPrimitive uiOriginalPrimitive=nullptr;
static UIIndexed uiOriginalIndexed=nullptr;
static UILine uiOriginalLine=nullptr;
static UITriangle uiOriginalTriangle=nullptr;
static psx_ui::Copier uiCopier;
static bool uiRendererInstalled=false;
template<class Draw> static int UIDraw(UIVertex* vertices,int count,Draw draw) {
 if(sceneThread!=GetCurrentThreadId()||uiCopier.busy)return draw(vertices);
 float sx=1,sy=1;if(LowScale(sx,sy))return uiCopier.DrawScaled(vertices,count,sx,sy,draw);
 // Scene is not called while paused. Permit toggle/reload on the pause map too.
 if(!lowActive&&!inScene&&!inEffects&&!buildingDepth&&!internalDraw){
  static DWORD lastUIPoll=0;DWORD now=GetTickCount();
  if(now-lastUIPoll>=30){lastUIPoll=now;Poll();}
 }
 bool world=inScene||inEffects||buildingDepth||internalDraw;
 if(!enabled||failed||!uiSnap||world)return draw(vertices);
 // RsGlobal.maximumHeight. No per-draw D3D readback or allocation.
 int h=*(const int*)0xC17048;
 if(h<=0||h>32768)return draw(vertices);
 float step=std::max(1.f,float(h)/float(uiSnapHeight));
 bool changed=false;return uiCopier.DrawCopy(vertices,count,step,draw,changed);
}
static int __cdecl UIPrimitives(int type,UIVertex* v,int n){return UIDraw(v,n,[&](UIVertex* p){return uiOriginalPrimitive(type,p,n);});}
static int __cdecl UIIndices(int type,UIVertex* v,int n,unsigned short* idx,int ni){return UIDraw(v,n,[&](UIVertex* p){return uiOriginalIndexed(type,p,n,idx,ni);});}
static int __cdecl UILines(UIVertex* v,int n,int a,int b){return UIDraw(v,n,[&](UIVertex* p){return uiOriginalLine(p,n,a,b);});}
static int __cdecl UITriangles(UIVertex* v,int n,int a,int b,int c){return UIDraw(v,n,[&](UIVertex* p){return uiOriginalTriangle(p,n,a,b,c);});}
static bool UIExecutable(void* p){
 MEMORY_BASIC_INFORMATION m{};
 return p&&VirtualQuery(p,&m,sizeof(m))==sizeof(m)&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static void InstallUIRenderer(){
 if(uiRendererInstalled)return;
 auto* globals=*(BYTE**)0xC97B24;
 if(!globals){Log("UI snap: RW globals unavailable");return;}
 // RwGlobals.dOpenDevice at +0x10. Slots verified by EXE RW wrappers.
 void** slots=(void**)(globals+0x28);
 void* originals[4];memcpy(originals,slots,sizeof(originals));
 for(auto p:originals)if(!UIExecutable(p)){Log("UI snap: invalid RW callback; skipped");return;}
 uiOriginalLine=(UILine)originals[0];uiOriginalTriangle=(UITriangle)originals[1];
 uiOriginalPrimitive=(UIPrimitive)originals[2];uiOriginalIndexed=(UIIndexed)originals[3];
 void* hooks[]={(void*)&UILines,(void*)&UITriangles,(void*)&UIPrimitives,(void*)&UIIndices};
 for(unsigned i=0;i<4;i++)if(!Write(slots+i,hooks+i,4)){
  for(unsigned j=0;j<i;j++)Write(slots+j,originals+j,4);
  Log("UI snap: callback patch failed; rolled back");return;
 }
 uiRendererInstalled=true;Log("UI snap: RenderWare Im2D callbacks attached");
}
