#pragma once
#include <cmath>
namespace stitch {
struct Clip{float x,y,z,w;};
// A union of source segments/triangles lies in their convex hull. VertexSnap
// leaves z,w unchanged. XY slack covers a whole grid step and numeric noise.
// A negative margin denotes a clip-space (rather than NDC) displacement.
inline unsigned clipMask(Clip p,float marginX,float marginY){
 if(!(p.w>0.f))return 0;
 float slack=.0001f*(1.f+p.w);
 float x=p.w+slack+(marginX<0.f?-marginX:marginX*p.w);
 float y=p.w+slack+(marginY<0.f?-marginY:marginY*p.w);
 return (p.x< -x?1u:0u)|(p.x>x?2u:0u)|(p.y< -y?4u:0u)|(p.y>y?8u:0u)|
        (p.z< -slack?16u:0u)|(p.z>p.w+slack?32u:0u);
}
}
