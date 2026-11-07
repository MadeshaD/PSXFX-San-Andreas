#include "../src/stitch_identity.hpp"
#include <cassert>
#include <iostream>
using namespace stitch;
int main(){
 unsigned oldCount=0,newCount=0,skipped=0;
 for(unsigned i=0;i<10000;++i){float x=float(i)*4;
  Line wall{{x,0,0},{x+2,0,0},{x,0,1}};
  Face adjacent{{wall.a,wall.b,{x,1,0}}};FaceSpan span;
  assert(faceContact(wall,adjacent,.02f,span));++oldCount;
  assert(sameFaceEdge(wall.a,wall.b,adjacent));++skipped;
  Face reversed{{adjacent.p[2],adjacent.p[1],adjacent.p[0]}};
  assert(sameFaceEdge(wall.b,wall.a,reversed));
  Face split{{{x,-1,0},{x+2,-1,0},{x,1,0}}};
  assert(faceContact(wall,split,.02f,span));++oldCount;
  assert(!sameFaceEdge(wall.a,wall.b,split));++newCount;
  V middle=wall.a+(wall.b-wall.a)*.5f;
  assert(!sameEdge(wall.a,wall.b,wall.a,middle));
  Line offset=wall;offset.a.z=offset.b.z=.001f;
  assert(faceContact(offset,adjacent,.02f,span));
  assert(!sameFaceEdge(offset.a,offset.b,adjacent));
 }
 assert(newCount+skipped==oldCount);
 std::cout<<"PASS: 10000 identical shared-edge inserts eliminated; 10000 edge/face contacts, split edges and nonzero offsets retained. Synthetic fixture, not an FPS measurement.\n";
}
