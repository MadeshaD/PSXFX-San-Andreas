#include "../src/seam_fog.hpp"
#include <cassert>
int main(){
 stitch::FogRadius f;
 f.update(true,145,{100,0,0});
 assert(f.outside({250,0,0},{800,20,20}));
 assert(!f.outside({240,0,0},{800,20,20}));
 assert(!f.outside({245,0,0},{246,0,0}));
 assert(f.outside({210,110,0},{230,130,0}));
 assert(f.outsideContact({0,0,0},{400,10,10},{300,0,0},{800,10,10},.1f));
 f.update(true,260,{100,0,0});
 assert(!f.outsideContact({0,0,0},{400,10,10},{300,0,0},{800,10,10},.1f));
 f.update(false,145,{0,0,0});
 assert(!f.outside({9999,0,0},{10000,0,0}));
}
