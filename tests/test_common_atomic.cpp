#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <cstdio>
#define __stdcall
#define PSX_PORTABLE_COMMON_TEST
using RenderCB=void(*)(void*,void*,unsigned char,unsigned int);
static unsigned scopes=0,draws=0;static void*seenEntry;static void*seenObject;static unsigned seenType,seenFlags;
static void Draw(void*e,void*o,unsigned char t,unsigned f){draws++;seenEntry=e;seenObject=o;seenType=t;seenFlags=f;}
static void DN(void*e,void*o,unsigned char t,unsigned f){Draw(e,o,t,f);}
static void Plain(void*e,void*o,unsigned char t,unsigned f){Draw(e,o,t,f);}
static void Car(void*e,void*o,unsigned char t,unsigned f){Draw(e,o,t,f);}
static void Building(RenderCB cb,void*e,void*o,unsigned char t,unsigned f){scopes++;cb(e,o,t,f);}
#include "../src/common_atomic.hpp"
int main(){
 int entry=1,object=2;
 PSXCommonRender(&Draw,&entry,&object,1,0x2f);assert(scopes==1&&draws==1&&seenEntry==&entry&&seenObject==&object&&seenType==1&&seenFlags==0x2f);
 for(auto cb:{&DN,&Plain,&Car})PSXCommonRender(cb,&entry,&object,1,7);
 assert(scopes==1&&draws==4);
 PSXCommonRender(&Draw,&entry,&object,1,5);assert(scopes==2&&draws==5);
 puts("PASS: shared atomic dispatch forwards arguments, scopes additional callbacks once, bypasses existing building/car wrappers.");
}
