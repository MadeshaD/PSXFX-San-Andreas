#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
using BYTE=unsigned char;using DWORD=uint32_t;
bool active=true,noCarReflections=true;
bool AtmosphereActive(){return active;}
#include "../src/vehicle_materials.hpp"
int main(){
 alignas(8) BYTE entry[0x18+0x40+3*0x24]{};
 alignas(8) BYTE materialA[32]{},materialB[32]{};
 auto*a=(DWORD*)(materialA+0x10);auto*b=(DWORD*)(materialB+0x10);*a=0x87654327;*b=0x12345672;
 *(DWORD*)(entry+0x1c)=3;
 void*materials[]={materialA,materialB,materialA};for(int k=0;k<3;k++)memcpy(entry+0x18+0x40+k*0x24+8,materials+k,sizeof(void*));
 {CarMaterials scope(entry);assert(*a==0x87654320&&*b==0x12345670);assert(scope.saved.size()==2);
  {CarMaterials nested(entry);assert(*a==0x87654320);}assert(*a==0x87654320);}
 assert(*a==0x87654327&&*b==0x12345672);
 noCarReflections=false;{CarMaterials scope(entry);assert(*a==0x87654327);}noCarReflections=true;
 active=false;{CarMaterials scope(entry);assert(*a==0x87654327);}active=true;
 *(DWORD*)(entry+0x1c)=4097;{CarMaterials scope(entry);assert(scope.saved.empty());}
 puts("PASS: production car material guard: only three env/spec bits cleared, exact restoration, shared-material deduplication,");
 puts("      nested scopes, opt-out, disabled-plugin and mesh-count gates. No game renderer execution.");
}
