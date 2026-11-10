#pragma once
#include <cmath>
#include <algorithm>
namespace stitch {
struct V {float x,y,z;};
inline V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V operator*(V a,float b){return {a.x*b,a.y*b,a.z*b};}
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct Line{V a,b,inside;};
struct Span{float a0,a1,b0,b1;};
// Match collinear world-space intervals. Adjacent triangle interiors must lie
// on different sides, allowing perpendicular folds while rejecting superimposed faces.
inline bool overlap(const Line& a,const Line& b,Span& s,float tolerance=.006f){
 V u=a.b-a.a,v=b.b-b.a;
 float uu=dot(u,u),vv=dot(v,v);
 if(uu<.0001f||vv<.0001f)return false;
 float uv=dot(u,v);
 // Shallowly converging borders can share only a portion of a segment.
 if(uv*uv<.9924f*uu*vv)return false;
 float t0=dot(b.a-a.a,u)/uu,t1=dot(b.b-a.a,u)/uu;
 float lo=std::max(0.f,std::min(t0,t1)),hi=std::min(1.f,std::max(t0,t1));
 if(hi<=lo||(hi-lo)*(hi-lo)*uu<.0001f)return false;
 V p=a.a+u*lo,q=a.a+u*hi;
 float l=std::clamp(dot(p-b.a,v)/vv,0.f,1.f),h=std::clamp(dot(q-b.a,v)/vv,0.f,1.f);
 V e=p-(b.a+v*l),f=q-(b.a+v*h);
 float limit=tolerance*tolerance;
 if(dot(e,e)>limit||dot(f,f)>limit){
  // Clip the span to the tube around the opposite line. The longitudinal
  // projection has already been restricted to the opposite segment.
  V delta=f-e;
  float length2=dot(delta,delta);
  if(length2<=1.e-16f)return false;
  float closest=-dot(e,delta)/length2;
  V radial=e+delta*closest;
  float left=limit-dot(radial,radial);
  if(left<=0)return false;
  float reach=std::sqrt(left/length2);
  float begin=std::max(0.f,closest-reach),end=std::min(1.f,closest+reach);
  if(end<=begin)return false;
  float span=hi-lo,other=h-l;
  hi=lo+span*end;lo+=span*begin;
  h=l+other*end;l+=other*begin;
  if((hi-lo)*(hi-lo)*uu<.0001f)return false;
 }
 V ia=a.inside-a.a,ib=b.inside-b.a;
 ia=ia-u*(dot(ia,u)/uu);ib=ib-u*(dot(ib,u)/uu);
 float sideLengths=std::sqrt(dot(ia,ia)*dot(ib,ib));
 if(sideLengths<1.e-8f||dot(ia,ib)>=.15f*sideLengths)return false;
 s={lo,hi,l,h};return true;
}
}
