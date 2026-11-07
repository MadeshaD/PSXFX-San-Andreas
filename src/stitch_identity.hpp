#pragma once
#include "face_contact.hpp"
namespace stitch {
inline bool samePoint(V a,V b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
inline bool sameEdge(V a,V b,V c,V d){return (samePoint(a,c)&&samePoint(b,d))||(samePoint(a,d)&&samePoint(b,c));}
// An entire shared edge has identical endpoint snaps under the same transform.
// No tolerance: near-coincident and split edges must remain candidates.
inline bool sameFaceEdge(V a,V b,const Face&f){
 for(int i=0;i<3;++i)if(sameEdge(a,b,f.p[i],f.p[(i+1)%3]))return true;
 return false;
}
}
