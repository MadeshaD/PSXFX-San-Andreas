// Copies only static boundary data. RenderWare addresses in keys identify a
// visible instance; no saved engine pointer is dereferenced on a later frame.
#include "face_contact.hpp"
#include <map>
#include <deque>
#include "pair_scan.hpp"
#include <array>
#include <tuple>
#include <set>
#include "seam_visibility.hpp"
#include "stitch_identity.hpp"
namespace stitching {
using stitch::V;
struct Tri{uint16_t i[3],material;};
struct UV{float u,v;};
template<class T> T read(const BYTE*p,unsigned off){T x;memcpy(&x,p+off,sizeof(x));return x;}
struct Bounds{V lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
 void add(V p){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
 hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
 bool touches(const Bounds& o)const{return lo.x<=o.hi.x+seamTolerance&&hi.x+seamTolerance>=o.lo.x&&lo.y<=o.hi.y+seamTolerance&&hi.y+seamTolerance>=o.lo.y&&lo.z<=o.hi.z+seamTolerance&&hi.z+seamTolerance>=o.lo.z;}
};
#include "face_tree.hpp"
struct Endpoint{V pos,normal;UV uv;uint16_t index;};
struct Edge{Endpoint a,b;stitch::Line world;const BYTE*mat;Bounds bounds;};
struct Patch{unsigned neighbor,edge,other;stitch::Span span;bool face=false;stitch::Weight w0{},w1{};};
struct Instance{unsigned id=0,seen=0,born=0,innerCount=0,outerCount=0; D3DMATRIX world{};Bounds bounds;std::vector<Edge> edges;FaceTree tree;std::vector<Patch> patches;std::set<unsigned> neighbors;std::map<const BYTE*,std::vector<unsigned>> materialPatches;std::map<const BYTE*,unsigned> renderedMaterials;};
struct Scope{const BYTE* atomic;const BYTE* entry;};
static Scope scope{};
using Key=std::array<uint32_t,24>;
static std::map<Key,Instance> instances;
static std::map<unsigned,Instance*> byID;
struct Work{unsigned owner,neighbor;stitch::PairCursor cursor;bool faces=false;};
static std::deque<Work> pending;
static void resumePairs();
static unsigned nextID=0,builtThisFrame=0,totalEdges=0,totalFaces=0;
static void addPatch(Instance&a,Patch p){if(p.neighbor!=a.id)a.neighbors.insert(p.neighbor);a.materialPatches[a.edges[p.edge].mat].push_back(unsigned(a.patches.size()));a.patches.push_back(p);}
static IDirect3DVertexDeclaration9* declaration=nullptr;
static void clear(){pending.clear();byID.clear();instances.clear();totalEdges=0;totalFaces=0;Release(declaration);}
static void beginFrame(){
 builtThisFrame=0;
 bool evicted=false;
 for(auto i=instances.begin();i!=instances.end();){
  if(frame-i->second.seen>12){evicted=true;byID.erase(i->second.id);totalEdges-=unsigned(i->second.edges.size());totalFaces-=unsigned(i->second.tree.surfaces.size());i=instances.erase(i);}
  else ++i;
 }
 if(evicted)for(auto& item:instances){
  Instance& inst=item.second;
  bool lostNeighbor=false;
  for(auto id=inst.neighbors.begin();id!=inst.neighbors.end();){
   if(byID.count(*id)){++id;continue;}
   id=inst.neighbors.erase(id);lostNeighbor=true;
  }
  if(!lostNeighbor)continue;
  const auto end=std::remove_if(inst.patches.begin(),inst.patches.end(),
      [](const Patch& p){return !byID.count(p.neighbor);});
  bool changed=end!=inst.patches.end();
  inst.outerCount-=unsigned(inst.patches.end()-end);
  inst.patches.erase(end,inst.patches.end());
  if(changed){inst.materialPatches.clear();for(unsigned k=0;k<inst.patches.size();++k)inst.materialPatches[inst.edges[inst.patches[k].edge].mat].push_back(k);}
 }
 resumePairs();
}
using stitch::Clip;
static Clip transform(V p,const D3DMATRIX&m){
 auto at=[&](int j){return p.x*m.m[0][j]+p.y*m.m[1][j]+p.z*m.m[2][j]+m.m[3][j];};
 return {at(0),at(1),at(2),at(3)};
}
static V worldPoint(V p,const D3DMATRIX&m){Clip c=transform(p,m);return {c.x,c.y,c.z};}
struct Geo{const BYTE* ptr;const Tri*tri;const V*pos,*norm;const UV*uv;const DWORD*colors;const BYTE*const*mats;int nt,nv,nm;};
static bool geometry(Geo& g){
 if(!scope.atomic||scope.atomic[0]!=1||!scope.entry)return false;
 const BYTE*p=read<const BYTE*>(scope.atomic,0x18);
 if(!p||p[0]!=8)return false; // rpGEOMETRY
 g.ptr=p;g.nt=read<int>(p,0x10);g.nv=read<int>(p,0x14);g.nm=read<int>(p,0x24);
 if(g.nt<=0||g.nt>24000||g.nv<=0||g.nv>65535||g.nm<=0||g.nm>4096||read<int>(p,0x18)!=1)return false;
 const BYTE*morph=read<const BYTE*>(p,0x5c);
 if(!morph||read<const BYTE*>(morph,0)!=p)return false;
 g.pos=read<const V*>(morph,0x14);g.norm=read<const V*>(morph,0x18);
 g.tri=read<const Tri*>(p,0x2c);g.uv=read<const UV*>(p,0x34);
 g.colors=read<const DWORD*>(p,0x30);g.mats=read<const BYTE*const*>(p,0x20);
 return g.pos&&g.tri&&g.uv&&g.mats;
}
static bool finite(V p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
static void boundaries(Instance& inst,const Geo& g){
 // Canonicalize positions before counting edges, so a UV split inside one
 // model does not become an exterior edge.
 std::map<std::array<float,3>,unsigned> unique;
 std::vector<unsigned> canon(g.nv);
 for(int i=0;i<g.nv;++i){V p=g.pos[i];if(!finite(p))return;
  canon[i]=unique.emplace(std::array<float,3>{p.x,p.y,p.z},unsigned(unique.size())).first->second;}
 struct Use{V normal{};bool crease=false;unsigned count=0;uint16_t a=0,b=0,c=0,material=0;};
 std::map<std::pair<unsigned,unsigned>,Use> counts;
 for(int i=0;i<g.nt;++i){const Tri&t=g.tri[i];
  if(t.i[0]>=g.nv||t.i[1]>=g.nv||t.i[2]>=g.nv)return;
  for(int j=0;j<3;++j){auto a=t.i[j],b=t.i[(j+1)%3];if(canon[a]==canon[b])continue;
   auto& u=counts[std::minmax(canon[a],canon[b])];
   V normal=stitch::cross(g.pos[b]-g.pos[a],g.pos[t.i[(j+2)%3]]-g.pos[a]);
   if(u.count==1){float d=stitch::dot(u.normal,normal);u.crease=d*d<.75f*stitch::dot(u.normal,u.normal)*stitch::dot(normal,normal);}
   if(++u.count==1){u.normal=normal;u.a=a;u.b=b;u.c=t.i[(j+2)%3];u.material=t.material;}
  }
 }
 if(g.nt<=8192&&totalFaces+unsigned(g.nt)<=160000){
  inst.tree.surfaces.reserve(g.nt);inst.tree.nodes.reserve(2*g.nt-1);
  for(int i=0;i<g.nt;++i){const Tri&t=g.tri[i];if(t.material>=g.nm)continue;
   Surface f{};f.mat=g.mats[t.material];for(int j=0;j<3;++j){f.local.p[j]=g.pos[t.i[j]];f.world.p[j]=worldPoint(f.local.p[j],inst.world);inst.bounds.add(f.world.p[j]);}
   inst.tree.surfaces.push_back(f);
  }
  if(!inst.tree.surfaces.empty())inst.tree.build(0,unsigned(inst.tree.surfaces.size()));
 }
 auto end=[&](uint16_t i){return Endpoint{g.pos[i],g.norm?g.norm[i]:V{},g.uv[i],i};};
 for(const auto& item:counts){const auto& u=item.second;
  if((u.count!=1&&!(u.count==2&&u.crease))||u.material>=g.nm)continue;
  stitch::Line l{worldPoint(g.pos[u.a],inst.world),worldPoint(g.pos[u.b],inst.world),worldPoint(g.pos[u.c],inst.world)};
  Bounds edgeBounds;edgeBounds.add(l.a);edgeBounds.add(l.b);
  inst.edges.push_back({end(u.a),end(u.b),l,g.mats[u.material],edgeBounds});inst.bounds.add(l.a);inst.bounds.add(l.b);
  if(inst.edges.size()>4096){inst.edges.clear();return;}
 }
 std::sort(inst.edges.begin(),inst.edges.end(),[](const Edge&a,const Edge&b){
  return std::min(a.world.a.x,a.world.b.x)<std::min(b.world.a.x,b.world.b.x);});
}
static bool scanFaces(Instance&a,const Instance&b,stitch::PairCursor&c,unsigned&budget,unsigned&count){
 while(c.i<a.edges.size()){
  if(c.j>=b.tree.nodes.size()){++c.i;c.j=0;continue;}
  if(!budget)return false;
  --budget;
  const auto&e=a.edges[c.i];const Bounds& box=e.bounds;
  const auto&node=b.tree.nodes[c.j];
  if(!box.touches(node.box)){c.j=node.after;continue;}
  ++c.j;if(node.face==~0u)continue;
  // Only same-instance local coordinates qualify. This avoids assuming that
  // separately transformed instances have identical floating-point clip positions.
  if(a.id==b.id&&stitch::sameFaceEdge(e.a.pos,e.b.pos,b.tree.surfaces[node.face].local)){continue;}
  stitch::FaceSpan span;
  if(stitch::faceContact(e.world,b.tree.surfaces[node.face].world,seamTolerance,span)){
   addPatch(a,{b.id,c.i,node.face,{span.t0,span.t1,0,0},true,span.w0,span.w1});++count;
  }
 }
 return true;
}
static void resumePairs(){
 unsigned remaining=32000;
 while(remaining&&!pending.empty()){
  Work work=pending.front();pending.pop_front();
  auto first=byID.find(work.owner),second=byID.find(work.neighbor);
  if(first==byID.end()||second==byID.end())continue;
  Instance& a=*first->second;
  const Instance& b=*second->second;
  bool internal=a.id==b.id;
  unsigned& count=internal?a.innerCount:a.outerCount;
  if(count>=4096){continue;}
  unsigned slice=std::min({remaining,2048u,4096u-count});
  unsigned budget=slice;
  bool finished=work.faces?scanFaces(a,b,work.cursor,budget,count):stitch::scan(work.cursor,a.edges,b.edges,a.id==b.id,seamTolerance,budget,
   [&](unsigned i,unsigned j,const stitch::Span& span){
    if(internal&&stitch::sameEdge(a.edges[i].a.pos,a.edges[i].b.pos,b.edges[j].a.pos,b.edges[j].b.pos)){return;}
    addPatch(a,{b.id,i,j,span});++count;
   });
  remaining-=slice-budget;
  if(!finished)pending.push_back(work);
 }
}
static Instance* lookup(const Geo& g,const D3DMATRIX&w){
 Key key{};key[0]=(DWORD)(uintptr_t)scope.atomic;key[1]=(DWORD)(uintptr_t)g.ptr;
 key[2]=(DWORD)(uintptr_t)scope.entry;key[3]=(DWORD)(uintptr_t)g.pos;
 key[4]=read<DWORD>(scope.entry,0x18);memcpy(key.data()+5,&w,64);key[21]=g.nv;key[22]=g.nt;key[23]=(DWORD)(uintptr_t)g.tri;
 auto i=instances.find(key);if(i!=instances.end()){i->second.seen=frame;return &i->second;}
 if(builtThisFrame>=6||instances.size()>=512||totalEdges>=65536)return nullptr;
 ++builtThisFrame;
 auto& n=instances[key];n.id=++nextID;n.world=w;n.seen=n.born=frame;boundaries(n,g);
 totalEdges+=unsigned(n.edges.size());totalFaces+=unsigned(n.tree.surfaces.size());
 if(!n.edges.empty()){
  pending.push_back({n.id,n.id,{}});
  if(!n.tree.nodes.empty())pending.push_back({n.id,n.id,{},true});
 }
 for(const auto& item:byID){const auto&o=*item.second;if(!n.bounds.touches(o.bounds))continue;
  if(!n.edges.empty()&&!o.edges.empty())pending.push_back({n.id,o.id,{}});
  if(!n.edges.empty()&&!o.tree.nodes.empty())pending.push_back({n.id,o.id,{},true});
  if(!o.edges.empty()&&!n.tree.nodes.empty())pending.push_back({o.id,n.id,{},true});
 }
 byID[n.id]=&n;return &n;
}
struct Vertex{V pos,normal;DWORD color;UV uv;Clip a,b,c;float t,u;};
static bool ensureDecl(IDirect3DDevice9*d){
 if(declaration)return true;
 D3DVERTEXELEMENT9 e[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},
 {0,24,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},{0,28,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},
 {0,36,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_TEXCOORD,3},{0,52,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_TEXCOORD,4},
 {0,68,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_TEXCOORD,6},{0,84,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,5},D3DDECL_END()};
 static_assert(sizeof(Vertex)==92,"bridge declaration");
 return SUCCEEDED(d->CreateVertexDeclaration(e,&declaration));
}
static DWORD mixColor(const DWORD* colors,const Edge&e,float t){
 if(!colors)return 0xffffffffu;
 DWORD a=colors[e.a.index],b=colors[e.b.index];
 auto byte=[&](int k){float x=(a>>(8*k))&255,y=(b>>(8*k))&255;return DWORD(x+(y-x)*t+.5f);};
 return (byte(3)<<24)|(byte(0)<<16)|(byte(1)<<8)|byte(2);
}
// DrawPrimitiveUP clears stream zero. Save just the bindings changed by this draw.
struct Bindings {
 IDirect3DDevice9* d; IDirect3DVertexShader9* vs=nullptr;
 IDirect3DVertexDeclaration9* decl=nullptr; IDirect3DVertexBuffer9* vb=nullptr;
 UINT offset=0,stride=0;DWORD fvf=0,cull=0;bool valid=false;
 explicit Bindings(IDirect3DDevice9*dev):d(dev){
  valid=SUCCEEDED(d->GetVertexShader(&vs))&&SUCCEEDED(d->GetVertexDeclaration(&decl))&&
        SUCCEEDED(d->GetFVF(&fvf))&&SUCCEEDED(d->GetStreamSource(0,&vb,&offset,&stride))&&
        SUCCEEDED(d->GetRenderState(D3DRS_CULLMODE,&cull));
 }
 ~Bindings(){
  if(valid){
   d->SetStreamSource(0,vb,offset,stride);
   if(fvf)d->SetFVF(fvf);else d->SetVertexDeclaration(decl);
   d->SetVertexShader(vs);d->SetRenderState(D3DRS_CULLMODE,cull);
  }
  Release(vb);Release(decl);Release(vs);
 }
};
static void draw(IDirect3DDevice9*d,const MeshDraw& mesh,const D3DMATRIX&w,const D3DMATRIX&view,const D3DMATRIX&proj,bool hasColor,bool hasNormal,float marginX,float marginY){
 if(!scope.atomic||!seamFill||!stitchVS||!mesh.known||snap<=0||(mode!=0&&mode!=1)||inEffects||carDepth)return;
 if(!RS(d,D3DRS_ZENABLE)||!RS(d,D3DRS_ZWRITEENABLE)||RS(d,D3DRS_ALPHABLENDENABLE)||RS(d,D3DRS_STENCILENABLE))return;
 auto mat=ScopedMaterial(mesh);if(!mat)return;
 Geo g{};if(!geometry(g)){return;}
 Instance*n=lookup(g,w);if(!n)return;
 n->renderedMaterials[mat]=frame;
 if(n->patches.empty()||n->born==frame)return;
 auto batch=n->materialPatches.find(mat);if(batch==n->materialPatches.end())return;
 // Retain capacity between calls. DrawPrimitiveUP consumes the data synchronously.
 static std::vector<Vertex> verts;verts.clear();verts.reserve(batch->second.size()*6);
 
 unsigned previousID=0,previousEdge=~0u;
 const Instance* neighbor=nullptr;
 D3DMATRIX wvpB{};
 Clip aa{},ab{};unsigned ownerMask=0;
 auto wvpA=Mul(Mul(w,view),proj);
 for(unsigned patchIndex:batch->second){const auto&p=n->patches[patchIndex];
  const Edge&e=n->edges[p.edge];if(e.mat!=mat)continue;
  if(previousID!=p.neighbor){
   auto it=byID.find(p.neighbor);
   neighbor=it==byID.end()?nullptr:it->second;
   if(neighbor&&(frame-neighbor->seen>1||neighbor->born==frame))neighbor=nullptr;
   if(neighbor)wvpB=neighbor==n?wvpA:Mul(Mul(neighbor->world,view),proj);
   previousID=p.neighbor;
  }
  if(!neighbor)continue;
  const auto& other=*neighbor;
  const BYTE*targetMat=p.face?other.tree.surfaces[p.other].mat:other.edges[p.other].mat;
  auto visible=other.renderedMaterials.find(targetMat);
  if(visible==other.renderedMaterials.end()||frame-visible->second>1)continue;
  if(previousEdge!=p.edge){
   aa=transform(e.a.pos,wvpA);ab=transform(e.b.pos,wvpA);
   ownerMask=stitch::clipMask(aa,marginX,marginY)&stitch::clipMask(ab,marginX,marginY);
   previousEdge=p.edge;
  }
  Clip ba,bb,bc;
  if(p.face){const auto&f=other.tree.surfaces[p.other].local;ba=transform(f.p[0],wvpB);bb=transform(f.p[1],wvpB);bc=transform(f.p[2],wvpB);}
  else{const auto&f=other.edges[p.other];ba=transform(f.a.pos,wvpB);bb=transform(f.b.pos,wvpB);bc=ba;}
  if(ownerMask&&(ownerMask&stitch::clipMask(ba,marginX,marginY)&stitch::clipMask(bb,marginX,marginY)&stitch::clipMask(bc,marginX,marginY)))continue;
  auto vtx=[&](float t,Clip a,Clip b,Clip c,float u,float v){
   return Vertex{e.a.pos+(e.b.pos-e.a.pos)*t,hasNormal?e.a.normal+(e.b.normal-e.a.normal)*t:V{},
    mixColor(hasColor?g.colors:nullptr,e,t),{e.a.uv.u+(e.b.uv.u-e.a.uv.u)*t,e.a.uv.v+(e.b.uv.v-e.a.uv.v)*t},a,b,c,u,v};};
  auto a=vtx(p.span.a0,aa,ab,aa,p.span.a0,0),b=vtx(p.span.a1,aa,ab,aa,p.span.a1,0);
  auto c=a,f1=b;
  c.a=f1.a=ba;c.b=f1.b=bb;c.c=f1.c=bc;
  c.t=p.face?p.w0.b:p.span.b0;c.u=p.face?p.w0.c:0;
  f1.t=p.face?p.w1.b:p.span.b1;f1.u=p.face?p.w1.c:0;
  verts.insert(verts.end(),{a,b,c,c,b,f1});
  
  
 }
 if(verts.empty()||!ensureDecl(d))return;
 Bindings state(d);if(!state.valid)return;
 if(FAILED(d->SetVertexDeclaration(declaration))||FAILED(d->SetVertexShader(stitchVS))||FAILED(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE)))return;
 float saved[4];
 if(seamDebug){
  if(FAILED(d->GetPixelShaderConstantF(4,saved,1)))return;
  float debug[4]={saved[0],saved[1],saved[2],-1};
  if(FAILED(d->SetPixelShaderConstantF(4,debug,1)))return;
 }
 originalUP(d,D3DPT_TRIANGLELIST,UINT(verts.size()/3),verts.data(),sizeof(Vertex));
 if(seamDebug)d->SetPixelShaderConstantF(4,saved,1);
}
}
