#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace psx_ui {
struct Vertex { float x,y,z,rhw; std::uint32_t color; float u,v; };
static_assert(sizeof(Vertex)==28,"RenderWare D3D9 Im2D layout");
// Only XY changes. Original shared sprite buffers are never modified.
struct Copier {
 std::array<Vertex,4096> buffer{};
 bool busy=false;
 template<class Draw> int DrawScaled(Vertex* source,int count,float sx,float sy,Draw draw) {
  if(busy||!source||count<=0||count>4096)return draw(source);
  busy=true;struct Guard {bool& b;~Guard(){b=false;}} guard{busy};
  memcpy(buffer.data(),source,sizeof(Vertex)*count);
  for(int i=0;i<count;i++){buffer[i].x*=sx;buffer[i].y*=sy;}
  return draw(buffer.data());
 }
 template<class Draw> int DrawCopy(Vertex* source,int count,float step,Draw draw,bool& changed) {
  changed=false;
  if(busy||!source||count<=0||count>4096||!std::isfinite(step)||step<1.f)return draw(source);
  for(int i=0;i<count;i++)if(!std::isfinite(source[i].x)||!std::isfinite(source[i].y))return draw(source);
  busy=true;
  struct Guard { bool& b; ~Guard(){b=false;} } guard{busy};
  memcpy(buffer.data(),source,sizeof(Vertex)*count);
  for(int i=0;i<count;i++) {
   buffer[i].x=std::floor(source[i].x/step+.5f)*step;
   buffer[i].y=std::floor(source[i].y/step+.5f)*step;
  }
  changed=true;return draw(buffer.data());
 }
};
}
