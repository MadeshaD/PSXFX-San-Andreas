#pragma once
#include "stitch_math.hpp"
#include <vector>
namespace stitch {
struct PairCursor {unsigned i=0,j=0;};
// Edges must be sorted by their minimum world X. The cursor always points
// to the next unexamined pair, including when the frame budget runs out.
template<class Edges,class Emit>
bool scan(PairCursor& c,const Edges& owner,const Edges& other,bool internal,
          float tolerance,unsigned& budget,Emit emit,const std::vector<float>* prefixMax=nullptr){
 while(c.i<owner.size()){
  if(c.j==0 && prefixMax){
   const auto& line=owner[c.i].world;
   float lower=std::min(line.a.x,line.b.x)-tolerance;
   c.j=unsigned(std::lower_bound(prefixMax->begin(),prefixMax->end(),lower)-prefixMax->begin());
  }
  if(internal)c.j=std::max(c.j,c.i+1);
  if(c.j>=other.size()){++c.i;c.j=0;continue;}
  if(!budget)return false;
  --budget;
  const Line& a=owner[c.i].world;
  unsigned j=c.j++;
  const Line& b=other[j].world;
  if(std::min(b.a.x,b.b.x)>std::max(a.a.x,a.b.x)+tolerance){++c.i;c.j=0;continue;}
  if(std::max(b.a.x,b.b.x)<std::min(a.a.x,a.b.x)-tolerance)continue;
  Span span;
  if(overlap(a,b,span,tolerance))emit(c.i,j,span);
 }
 return true;
}
}
