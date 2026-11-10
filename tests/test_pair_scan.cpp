#include "../src/pair_scan.hpp"
#include <vector>
#include <set>
#include <cassert>
#include <cstdio>
using namespace stitch;
struct Edge{Line world;};
using Edges=std::vector<Edge>;
using Pairs=std::set<std::pair<unsigned,unsigned>>;
Pairs run(const Edges& a,const Edges& b,bool self,unsigned slice,float tol,bool indexed=true){
 std::vector<float> prefix;float hi=-INFINITY;
 for(const auto& e:b){hi=std::max({hi,e.world.a.x,e.world.b.x});prefix.push_back(hi);}
 PairCursor c;Pairs seen;unsigned passes=0;
 for(;;){
  unsigned budget=slice;
  bool done=scan(c,a,b,self,tol,budget,[&](unsigned i,unsigned j,const Span& s){
   assert(s.a1>s.a0);assert(seen.emplace(i,j).second);
  },indexed?&prefix:nullptr);
  ++passes;if(done)break;assert(passes<100000);
 }
 return seen;
}
int main(int argc,char**){
 if(argc>1){
  V a{-.75f,-.5f,.2f},b{.75f,.5f,.2f},t=a+(b-a)*.36f;
  V left{-.75f,.75f,.2f},right{.75f,-.75f,.2f};
  Edges mesh={{{a,b,left}},{{a,t,right}},{{t,b,right}}};
  for(bool internal:{true,false}){
   Edges owner=internal?mesh:Edges{mesh[0]},neighbor=internal?mesh:Edges{mesh[1],mesh[2]};
   PairCursor c;bool done=false;
   while(!done){
    unsigned budget=1;
    done=scan(c,owner,neighbor,internal,.02f,budget,[&](unsigned i,unsigned j,const Span& s){
     printf("%d %u %u %.9g %.9g %.9g %.9g\n",int(internal),i,internal?j:j+1,s.a0,s.a1,s.b0,s.b1);
    });
   }
  }
  return 0;
 }
 Edges mesh={{{{0,0,0},{8,0,0},{4,2,0}}},{{{3,0,0},{0,0,0},{1,-2,0}}},
             {{{8,0,0},{3,0,0},{5,-2,0}}}};
 Edges sparse;
 for(unsigned i=0;i<100;i++){
  float x=3.f*i; sparse.push_back({{{x,0,0},{x+2,0,0},{x+1,1,0}}});
  sparse.push_back({{{x+1,0,0},{x+2,0,0},{x+1,-1,0}}});
 }
 for(bool self:{false,true})assert(run(sparse,sparse,self,1,.1f)==run(sparse,sparse,self,1,.1f,false));
 Pairs inside={{0,1},{0,2}};
 assert(run(mesh,mesh,true,1,.02f)==inside);
 Edges one={mesh[0]},neighbors={mesh[1],mesh[2]};
 for(auto& e:neighbors){e.world.a.y+=.014f;e.world.b.y+=.014f;}
 assert(run(one,neighbors,false,1,.006f).empty());
 Pairs adjacent={{0,0},{0,1}};
 assert(run(one,neighbors,false,1,.02f)==adjacent);
 assert(run(one,neighbors,false,1000,.02f)==adjacent);
 neighbors[1].world.inside={5,0,2}; // a wall meeting a road
 assert(run(one,neighbors,false,1,.02f)==adjacent);
 for(auto& e:neighbors){e.world.a.z=1;e.world.b.z=1;}
 assert(run(one,neighbors,false,1,.02f).empty());
 puts("PASS: intra-mesh T, adjacent offset tiles, right-angle fold, separate floors, exactly-once resume with one comparison per frame.");
}
