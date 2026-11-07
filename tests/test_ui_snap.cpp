#include "../src/ui_snap_math.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
 psx_ui::Copier copy; psx_ui::Vertex v[4]={{1.1f,-1.1f,.2f,1,0x12345678,.1f,.9f},{3.1f,7.9f,.3f,.5f,0xfefefefe,.2f,.6f}};
 psx_ui::Vertex original[4];memcpy(original,v,sizeof(v));bool changed=false;int calls=0;
 int ret=copy.DrawCopy(v,4,4.f,[&](auto p){
  ++calls;assert(p!=v);assert(p[0].x==0&&p[0].y==0&&p[1].x==4&&p[1].y==8);
  for(int i=0;i<4;i++)assert(memcmp((char*)&p[i]+8,(char*)&v[i]+8,20)==0);
  auto saved=p[1];bool nested=true;
  assert(copy.DrawCopy(v,4,4.f,[&](auto q){assert(q==v);return 9;},nested)==9);
  assert(!nested&&memcmp(&p[1],&saved,sizeof(saved))==0);return -3;
 },changed);
 assert(ret==-3&&calls==1&&changed&&!copy.busy&&memcmp(v,original,sizeof(v))==0);
 for(int n:{-1,0,4097}){copy.DrawCopy(v,n,4.f,[&](auto p){assert(p==v);return 0;},changed);assert(!changed);}
 copy.DrawCopy(nullptr,4,4.f,[&](auto p){assert(p==nullptr);return 0;},changed);assert(!changed);
 copy.DrawScaled(v,4,.25f,.5f,[&](auto p){
  assert(p!=v&&p[0].x==v[0].x*.25f&&p[1].y==v[1].y*.5f);
  for(int i=0;i<4;i++)assert(memcmp((char*)&p[i]+8,(char*)&v[i]+8,20)==0);
  return 1;
 });assert(memcmp(v,original,sizeof(v))==0);
 v[0].x=std::numeric_limits<float>::quiet_NaN();copy.DrawCopy(v,4,4.f,[&](auto p){assert(p==v);return 0;},changed);assert(!changed);
 std::cout<<"PASS UI copy: grid, retained UV/color/Z/RHW, untouched input, reentry, limits, return values\n";
}
