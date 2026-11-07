#include "../src/seam_visibility.hpp"
#include <cassert>
#include <algorithm>
#include <random>
using stitch::Clip;
int main(){
 std::mt19937 rng(28);
 std::uniform_real_distribution<float> d(-3,3),depth(.01f,20.f);
 unsigned culled=0;
 for(int type=0;type<3;++type)for(int sample=0;sample<25000;++sample){
  unsigned mask=63;
  Clip points[5];
  for(Clip &p:points){
   p.w=depth(rng);p.x=d(rng)*p.w;p.y=d(rng)*p.w;p.z=d(rng)*p.w;
   mask &= stitch::clipMask(p,type==2?-.25f:2.f/320,type==2?-.25f:2.f/240);
   auto snap=[&](float a,float cells){
    if(type==2)return std::floor(a*4)/4;
    if(type==1)return std::floor(a/p.w*cells*.5f)*2.f/cells*p.w;
    return std::round(a/p.w*cells*.5f)*2.f/cells*p.w;
   };
   p.x=snap(p.x,320);p.y=snap(p.y,240);
  }
  if(mask){++culled;unsigned after=63;for(auto p:points)after&=stitch::clipMask(p,0,0);assert((after&mask)==mask);}
 }
 assert(culled>50);
 assert(stitch::clipMask({1.001f,0,.5f,1},.01f,.01f)==0);
 assert(stitch::clipMask({9,0,0,-1},.01f,.01f)==0);
}
