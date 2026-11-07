#include "../src/stitch_math.hpp"
#include <cassert>
#include <iostream>
using namespace stitch;
int main(){
 Line a;while(std::cin>>a.a.x>>a.a.y>>a.a.z>>a.b.x>>a.b.y>>a.b.z>>a.inside.x>>a.inside.y>>a.inside.z){
  V d=a.b-a.a;V middle=a.a+d*.375f;
  V across=middle-(a.inside-a.a-d*(dot(a.inside-a.a,d)/dot(d,d)));
  Line b{middle,a.a,across};Span s;
  assert(overlap(a,b,s));assert(std::abs(s.a0)<.001&&std::abs(s.a1-.375)<.001);
  assert(std::abs(s.b0-1)<.001&&std::abs(s.b1)<.001);
  b={a.b,middle,across};assert(overlap(a,b,s)&&std::abs(s.a1-1)<.001);
  b.inside=a.inside;assert(!overlap(a,b,s));
 }
 Line x{{0,0,0},{5,0,0},{1,1,0}},parallel{{1,0,.1f},{3,0,.1f},{2,-1,.1f}};
 Span span;assert(!overlap(x,parallel,span));
 Line crossing{{2,-1,0},{2,1,0},{3,0,0}};assert(!overlap(x,crossing,span));
 Line point{{5,0,0},{7,0,0},{5,-1,0}};assert(!overlap(x,point,span));
}
