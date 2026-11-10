#pragma once
#include "stitch_math.hpp"
namespace stitch {
struct FogRadius {
 V eye{};
 float squared=-1.f;
 void update(bool enabled,float end,V camera){
  eye=camera;squared=-1.f;
  if(enabled && std::isfinite(camera.x) && std::isfinite(camera.y) &&
     std::isfinite(camera.z) && std::isfinite(end)){
   float radius=std::max(0.f,end);
   squared=radius*radius;
  }
 }
 bool outside(V lo,V hi)const{
  if(squared<0.f)return false;
  auto gap=[](float x,float a,float b){return std::max({a-x,0.f,x-b});};
  V d{gap(eye.x,lo.x,hi.x),gap(eye.y,lo.y,hi.y),gap(eye.z,lo.z,hi.z)};
  return dot(d,d)>squared;
 }
 bool outsideContact(V alo,V ahi,V blo,V bhi,float tolerance)const{
  V lo{std::max(alo.x,blo.x)-tolerance,std::max(alo.y,blo.y)-tolerance,std::max(alo.z,blo.z)-tolerance};
  V hi{std::min(ahi.x,bhi.x)+tolerance,std::min(ahi.y,bhi.y)+tolerance,std::min(ahi.z,bhi.z)+tolerance};
  return outside(lo,hi);
 }
};
}
