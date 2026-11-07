#pragma once
#include "stitch_math.hpp"
namespace stitch {
inline V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct Face{V p[3];};
struct Weight{float b,c;};
struct FaceSpan{float t0,t1;Weight w0,w1;};
// Clip an edge against a triangle in world space, preserving face barycentrics.
inline bool faceContact(Line e,Face f,float tolerance,FaceSpan& out){
 V u=f.p[1]-f.p[0],v=f.p[2]-f.p[0],d=e.b-e.a,n=cross(u,v),en=cross(d,e.inside-e.a);
 float nn=dot(n,n),ee=dot(en,en),dd=dot(d,d),ne=dot(n,en);
 if(nn<1e-12f||ee<1e-12f||dd<.0001f||ne*ne>.8836f*nn*ee)return false;
 if(std::abs(dot(n,d))>.06f*std::sqrt(nn*dd))return false;
 float uu=dot(u,u),vv=dot(v,v),uv=dot(u,v),det=uu*vv-uv*uv;
 if(det<=1e-12f)return false;
 auto bary=[&](V p){p=p-f.p[0];return Weight{(dot(p,u)*vv-dot(p,v)*uv)/det,(dot(p,v)*uu-dot(p,u)*uv)/det};};
 Weight a=bary(e.a),b=bary(e.b);float lo=0,hi=1;
 auto clip=[&](float x,float y){if(x>=0&&y>=0)return true;if(x<0&&y<0)return false;float t=x/(x-y);if(x<0)lo=std::max(lo,t);else hi=std::min(hi,t);return hi>lo;};
 float da=dot(e.a-f.p[0],n)/std::sqrt(nn),db=dot(e.b-f.p[0],n)/std::sqrt(nn);
 if(!clip(a.b,b.b)||!clip(a.c,b.c)||!clip(1-a.b-a.c,1-b.b-b.c)||!clip(tolerance-da,tolerance-db)||!clip(tolerance+da,tolerance+db)||(hi-lo)*(hi-lo)*dd<.0001f)return false;
 auto at=[&](float t){return Weight{a.b+(b.b-a.b)*t,a.c+(b.c-a.c)*t};};
 // Boundary contacts are also accepted: a smooth internal face edge has no
 // open-edge representative in the original matcher.
 out={lo,hi,at(lo),at(hi)};return true;
}
}
