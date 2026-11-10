#include "../src/face_contact.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstdint>
using BYTE=unsigned char;
using stitch::V;
struct Bounds{V lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};void add(V p){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
 bool touches(const Bounds&o)const{return lo.x<=o.hi.x+.02f&&hi.x+.02f>=o.lo.x&&lo.y<=o.hi.y+.02f&&hi.y+.02f>=o.lo.y&&lo.z<=o.hi.z+.02f&&hi.z+.02f>=o.lo.z;}};
#include "../src/face_tree.hpp"
using namespace stitch;
int main(){
 Line wall{{0,0,0},{2,0,0},{0,0,1}};
 Face f{{{0,-1,0},{2,-1,0},{0,1,0}}},g{{{2,-1,0},{2,1,0},{0,1,0}}};
 FaceSpan a,b;assert(faceContact(wall,f,.02f,a));assert(faceContact(wall,g,.02f,b));
 assert(std::abs(a.t0)<1e-6f&&std::abs(a.t1-.5f)<1e-6f);
 assert(std::abs(b.t0-.5f)<1e-6f&&std::abs(b.t1-1)<1e-6f);
 auto verify=[&](Face f,FaceSpan s){for(int i=0;i<2;++i){float t=i?s.t1:s.t0;Weight w=i?s.w1:s.w0;V p=f.p[0]+(f.p[1]-f.p[0])*w.b+(f.p[2]-f.p[0])*w.c;V e=p-(wall.a+(wall.b-wall.a)*t);assert(dot(e,e)<1e-10f);}};
 verify(f,a);verify(g,b);
 Line far=wall;far.a.z=far.b.z=.1f;assert(!faceContact(far,f,.02f,a));
 Line flat=wall;flat.inside={0,1,0};assert(!faceContact(flat,f,.02f,a));
 Face shoulder{{{0,.04f,0},{2,.04f,0},{0,-2,.03f}}};
 assert(faceContact(flat,shoulder,.1f,a,true));
 assert(!faceContact(flat,shoulder,.1f,a,false));
 assert(!faceContact(flat,f,.02f,a,true)); // broad coplanar overlay
 Line angled{{0,-.02f,0},{4,-.22f,0},{1,-2,0}};
 Line road{{0,0,0},{4,0,0},{1,2,0}};
 Span partial;assert(overlap(road,angled,partial,.1f));
 assert(partial.a1>.35f && partial.a1<.45f);
 Face bad{{{0,0,0},{1,0,0},{2,0,0}}};assert(!faceContact(wall,bad,.02f,a));
 Face reversed{{f.p[2],f.p[1],f.p[0]}};assert(faceContact(wall,reversed,.02f,a));verify(reversed,a);
 Line boundary{{0,-1,0},{2,-1,0},{0,-1,1}};assert(faceContact(boundary,f,.02f,a));
 FaceTree tree;
 for(unsigned i=0;i<1000;++i){Face q=f;for(auto&p:q.p)p.x+=float(i)*4;tree.surfaces.push_back({q,q,nullptr});}
 tree.surfaces.push_back({g,g,nullptr});tree.build(0,unsigned(tree.surfaces.size()));
 unsigned brute=0,found=0,visited=0;
 for(const auto&q:tree.surfaces)if(faceContact(wall,q.world,.02f,a))++brute;
 Bounds box;box.add(wall.a);box.add(wall.b);
 for(unsigned j=0;j<tree.nodes.size();){const auto&n=tree.nodes[j];++visited;if(!box.touches(n.box)){j=n.after;continue;}++j;if(n.face!=~0u&&faceContact(wall,tree.surfaces[n.face].world,.02f,a))++found;}
 assert(found==brute&&found==2&&visited<80);
 std::cout<<"PASS: production BVH matches brute force over 1001 triangles, visiting "<<visited<<" nodes.\n";
 std::cout<<"PASS: continuous wall contact across two floor triangles, barycentric endpoints, reversed winding, boundary contact, coplanar/distant/degenerate rejection.\n";
}
