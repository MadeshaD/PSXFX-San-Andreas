#include <initializer_list>
#include <cassert>
#include <cstdint>
#include <cstring>
using BYTE=unsigned char;
#define __thiscall
#define __fastcall
static bool seamFill=true,enabled=true,failed=false;
static bool Write(void*,const void*,size_t){return false;}
#include "../src/seam_owner.hpp"
static void* intact(const BYTE*){return nullptr;}
static void* damaged(const BYTE* info){return const_cast<BYTE*>(info);}
int main(){
 using Query=void*(*)(const BYTE*);
 Query vt[]={nullptr,nullptr,intact};auto table=vt;
 BYTE entity[64]{},info[32]{},atomic[100]{1},clump[16]{2},other[16]{2};
 memcpy(info,&table,sizeof(table));
 int16_t noObject=-1;memcpy(info+16,&noObject,2);
 entity[0x36]=1;
 assert(seam_owner::eligible(entity,info));
 for(BYTE type:{2,3,4,5}){entity[0x36]=type;assert(!seam_owner::eligible(entity,info));}
 entity[0x36]=1;info[16]=0;info[17]=0;assert(!seam_owner::eligible(entity,info));
 memcpy(info+16,&noObject,2);vt[2]=damaged;assert(!seam_owner::eligible(entity,info));
 vt[2]=intact;entity[0x1d]=2;assert(!seam_owner::eligible(entity,info));
 entity[0x1d]=0;info[19]=0x58;assert(!seam_owner::eligible(entity,info));
 assert(!seam_owner::owns(atomic));seam_owner::root=atomic;assert(seam_owner::owns(atomic));
 auto parent=clump;memcpy(atomic+0x3c,&parent,sizeof(parent));
 seam_owner::root=clump;assert(seam_owner::owns(atomic));
 seam_owner::root=other;assert(!seam_owner::owns(atomic));
}
