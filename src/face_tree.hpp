// Included inside stitching, after Bounds. Immutable copied triangle BVH.
struct Surface{stitch::Face local,world;const BYTE*mat;};
struct FaceNode{Bounds box;unsigned after,face;};
struct FaceTree{
 std::vector<Surface> surfaces;std::vector<FaceNode> nodes;
 unsigned build(unsigned begin,unsigned end){
  unsigned id=unsigned(nodes.size());nodes.push_back({});Bounds box;
  for(unsigned i=begin;i<end;++i)for(V p:surfaces[i].world.p)box.add(p);
  nodes[id].box=box;nodes[id].face=~0u;
  if(end-begin==1)nodes[id].face=begin;
  else {V d=box.hi-box.lo;int axis=d.x>d.y?(d.x>d.z?0:2):(d.y>d.z?1:2);
   auto value=[&](const Surface& s){V c=s.world.p[0]+s.world.p[1]+s.world.p[2];return axis==0?c.x:axis==1?c.y:c.z;};
   unsigned mid=(begin+end)/2;std::nth_element(surfaces.begin()+begin,surfaces.begin()+mid,surfaces.begin()+end,[&](const Surface&a,const Surface&b){return value(a)<value(b);});
   build(begin,mid);build(mid,end);
  }
  nodes[id].after=unsigned(nodes.size());return id;
 }
};
