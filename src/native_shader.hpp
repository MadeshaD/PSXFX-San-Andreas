// Restricted legacy D3D9 VS -> SM3 adapter. No new constants: bone registers remain untouched.
// Reject unknown instructions/outputs rather than guessing their semantics.
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
namespace native_shader {
using U=uint32_t;
inline unsigned Type(U t){return ((t>>28)&7)|((t>>8)&24);}
inline U Reg(unsigned type,unsigned n){return 0x80000000u|((type&7)<<28)|((type&24)<<8)|n;}
inline U D(unsigned type,unsigned n,unsigned mask=15){return Reg(type,n)|(mask<<16);}
inline U S(unsigned type,unsigned n,unsigned sw=0xe4){return Reg(type,n)|(sw<<16);}
inline U R(unsigned n,unsigned component=4){return S(0,n,component<4?component*0x55:0xe4);}
inline void Emit(std::vector<U>&o,U op,std::initializer_list<U> args){o.push_back(op|(U(args.size())<<24));o.insert(o.end(),args.begin(),args.end());}
struct Result {std::vector<U> code;std::string reason;bool relative=false;unsigned instructions=0;};
inline Result Convert(const std::vector<U>&in,unsigned width,unsigned height,unsigned snap256,unsigned snapType=0,unsigned clipPrecision=32){
 Result out;auto fail=[&](const char*s){out.code.clear();out.reason=s;return out;};
 if(in.empty()||(in[0]!=0xfffe0101u&&in[0]!=0xfffe0200u&&in[0]!=0xfffe0201u))return fail("unsupported VS version");
 if(!width||!height||width>65535||height>65535||snap256>256||snapType>2||clipPrecision<1||clipPrecision>4096)return fail("grid bounds");
 bool old=in[0]==0xfffe0101u,ended=false;unsigned posMask=0,uvMask=0,colorMask=0,inputMask=0,readInputs=0;
 std::vector<U> declarations,defs,body;
 for(size_t i=1;i<in.size();){
  U token=in[i++];unsigned op=token&65535;
  if(op==0xffff){ended=true;if(i!=in.size())return fail("trailing tokens");break;}
  if(op==0xfffe){unsigned n=(token>>16)&0x7fff;if(n>in.size()-i)return fail("comment bounds");i+=n;continue;}
  if(token&0xf0ff0000u)return fail("instruction modifiers/flow unsupported");
  if(op==31){
   if(i+2>in.size()||(!old&&((token>>24)&15)!=2))return fail("dcl length");
   U usage=in[i++],reg=in[i++];unsigned n=reg&0x7ff;
   if(Type(reg)!=1||n>=16||(inputMask&(1u<<n)))return fail("input declaration");
   inputMask|=1u<<n;Emit(declarations,31,{usage,reg});continue;
  }
  if(op==81){
   if(i+5>in.size()||(!old&&((token>>24)&15)!=5)||Type(in[i])!=2||(in[i]&0x7ff)>=256)return fail("def bounds");
   defs.push_back(81u|(5u<<24));defs.insert(defs.end(),in.begin()+i,in.begin()+i+5);i+=5;continue;
  }
  unsigned operands=0;
  switch(op){
   case 0:break;
   case 1:case 6:case 7:case 14:case 15:case 16:case 19:case 35:case 36:case 46:case 78:operands=2;break;
   case 2:case 3:case 5:case 8:case 9:case 10:case 11:case 12:case 13:case 17:case 20:case 21:case 22:case 23:case 24:case 32:case 33:operands=3;break;
   case 4:case 18:operands=4;break;
   default:return fail(("unsupported opcode "+std::to_string(op)+" at word "+std::to_string(i-1)).c_str());
  }
  if(!operands){if(!old&&((token>>24)&15))return fail("nop length");continue;}
  size_t start=i;if(i+operands>in.size())return fail("operand bounds");
  U dest=in[i++];unsigned dt=Type(dest),dn=dest&0x7ff,mask=(dest>>16)&15;
  if(!(dest&0x80000000u)||!mask||(dest&0x0fe02000u))return fail("destination modifiers");
  if(dt==0){if(dn>=20)return fail("temporary collision");}
  else if(dt==3){if(dn||!(op==1||op==46))return fail("address destination");}
  else if(dt==4){if(dn>1)return fail("point size output");if(!dn){posMask|=mask;dest=(dest&0x0fff0000)|Reg(0,20);}else dest=(dest&0x0fff0000)|Reg(0,23);}
  else if(dt==5){if(dn>1)return fail("color output");if(!dn){colorMask|=mask;dest=(dest&0x0fff0000)|Reg(0,22);}else dest=(dest&0x0fff0000)|Reg(0,23);}
  else if(dt==6){if(dn)return fail("multiple texture outputs");uvMask|=mask;dest=(dest&0x0fff0000)|Reg(0,21);}
  else return fail("destination type");
  std::vector<U> args{dest};
  for(unsigned k=1;k<operands;k++){
   if(i>=in.size())return fail("source bounds");
   U s=in[i++];unsigned t=Type(s),n=s&0x7ff,mod=(s>>24)&15;
   if(!(s&0x80000000u)||!((t==0&&n<20)||(t==1&&n<16)||(t==2&&n<256))||(mod!=0&&mod!=1&&mod!=11&&mod!=12))return fail("source type/modifier");
   if(t==1)readInputs|=1u<<n;
   args.push_back(s);
   if(s&0x2000){
    if(t!=2)return fail("non-constant relative source");
    out.relative=true;
    if(old)args.push_back(S(3,0,0));
    else {if(i>=in.size())return fail("relative bounds");U a=in[i++];if(Type(a)!=3||(a&0x7ff)||((a>>24)&15))return fail("relative address");args.push_back(a);}
   }
  }
  if(!old&&i-start!=((token>>24)&15))return fail("instruction length");
  if(old&&dt==3){
   // VS1.1 MOV a0 and SM3 MOVA both convert using round-to-nearest.
   if(args.size()!=2)return fail("address source");
   Emit(body,46,{dest,args[1]});
  }else if(old&&op==78){
   if(args.size()!=2)return fail("expp relative source");
   Emit(body,19,{D(0,29,2),args[1]});Emit(body,3,{D(0,29,1),args[1],R(29,1)});
   Emit(body,14,{D(0,29,1),R(29,0)});Emit(body,14,{D(0,29,4),args[1]});Emit(body,13,{D(0,29,8),args[1],args[1]});Emit(body,1,{dest,R(29)});
  }else {body.push_back((op==78?14u:op)|(U(args.size())<<24));body.insert(body.end(),args.begin(),args.end());}
  if(++out.instructions>300)return fail("instruction budget");
 }
 if(!ended||posMask!=15||(uvMask&3)!=3||colorMask!=15||!inputMask||(readInputs&~inputMask))return fail("missing position/color/UV/declarations");
 out.code={0xfffe0300u};out.code.insert(out.code.end(),declarations.begin(),declarations.end());
 // Link exactly to PSMain's POSITION, COLOR0, TEXCOORD0/1/2.
 for(auto spec:{std::vector<unsigned>{0,0,0,15},{10,0,1,15},{5,0,2,7},{5,1,3,7},{5,2,4,3},{5,3,5,7}})
  Emit(out.code,31,{0x80000000u|spec[0]|(spec[1]<<16),D(6,spec[2],spec[3])});
 out.code.insert(out.code.end(),defs.begin(),defs.end());out.code.insert(out.code.end(),body.begin(),body.end());auto&o=out.code;
 Emit(o,1,{D(0,21,12),R(20,3)}); // Define unused UV lanes conservatively.
 // Derive 1/0/0.5 from the final finite position; do not reserve any c-register.
 Emit(o,13,{D(0,27,1),R(20,3),R(20,3)});Emit(o,1,{D(0,27,14),R(27,0)});Emit(o,3,{D(0,27,2),R(27,0),R(27,0)});
 Emit(o,2,{D(0,27,4),R(27,0),R(27,0)});Emit(o,6,{D(0,27,4),R(27,2)});
 if(snap256){
  Emit(o,1,{D(0,24),R(27,1)});unsigned numbers[]={snapType==2?clipPrecision:width,snapType==2?clipPrecision:height,snap256,256};
  for(int bit=15;bit>=0;bit--){Emit(o,2,{D(0,24),R(24),R(24)});unsigned mask=0;for(unsigned k=0;k<4;k++)if(numbers[k]&(1u<<bit))mask|=1u<<k;if(mask)Emit(o,2,{D(0,24,mask),R(24),R(27,0)});}
  Emit(o,6,{D(0,24,8),R(24,3)});Emit(o,5,{D(0,24,4),R(24,2),R(24,3)});
  Emit(o,5,{D(0,27,8),R(24,3),R(27,2)});Emit(o,5,{D(0,27,8),R(27,3),R(27,2)});
  Emit(o,1,{D(0,25),R(27,1)});Emit(o,1,{D(0,26),R(27,1)});
  Emit(o,41u|(1u<<16),{R(20,3),R(27,3)}); // IFC GT; do not move vertices behind/near the eye.
  if(snapType==0){
  Emit(o,6,{D(0,25,8),R(20,3)});Emit(o,5,{D(0,25,3),R(20),R(25,3)});
  Emit(o,5,{D(0,25,1),R(25),R(27,2)});Emit(o,5,{D(0,25,2),R(25)^0x01000000u,R(27,2)});
  Emit(o,2,{D(0,25,3),R(25),R(27,2)});Emit(o,5,{D(0,25,3),R(25),R(24)});
  Emit(o,2,{D(0,25,3),R(25),R(27,2)});Emit(o,19,{D(0,26,3),R(25)});Emit(o,3,{D(0,25,3),R(25),R(26)});
  Emit(o,6,{D(0,26,1),R(24,0)});Emit(o,6,{D(0,26,2),R(24,1)});
  Emit(o,5,{D(0,25,3),R(25),R(26)});Emit(o,3,{D(0,25,3),R(25),R(27,2)});Emit(o,2,{D(0,25,3),R(25),R(25)});
  Emit(o,1,{D(0,25,2),R(25)^0x01000000u});Emit(o,5,{D(0,25,3),R(25),R(20,3)});
  }else{
   Emit(o,1,{D(0,25,3),R(20)});
   Emit(o,1,{D(0,26,3),R(24)});
   if(snapType==1){
    Emit(o,6,{D(0,25,8),R(20,3)});Emit(o,5,{D(0,25,3),R(25),R(25,3)});
    Emit(o,5,{D(0,26,3),R(26),R(27,2)});
   }
   Emit(o,5,{D(0,25,3),R(25),R(26)});
   Emit(o,19,{D(0,28,3),R(25)});Emit(o,3,{D(0,25,3),R(25),R(28)});
   Emit(o,6,{D(0,28,1),R(26,0)});Emit(o,6,{D(0,28,2),R(26,1)});
   Emit(o,5,{D(0,25,3),R(25),R(28)});
   if(snapType==1)Emit(o,5,{D(0,25,3),R(25),R(20,3)});
  }
  Emit(o,3,{D(0,25,3),R(25),R(20)});Emit(o,4,{D(0,20,3),R(25),R(24,2),R(20)});Emit(o,43,{});
 }
 Emit(o,1,{D(6,0),R(20)});Emit(o,1,{D(6,1)|0x00100000u,R(22)});
 Emit(o,5,{D(6,2,3),R(21),R(20,3)});Emit(o,1,{D(6,2,4),R(20,3)});
 Emit(o,1,{D(6,3,3),R(27,1)});Emit(o,1,{D(6,3,4),R(20,3)});Emit(o,1,{D(6,4,3),R(21)});// Saturate before weighting, matching the ordinary diffuse color output.
 Emit(o,1,{D(0,22)|0x00100000u,R(22)});
 Emit(o,5,{D(6,5,7),R(22),R(20,3)});
 o.push_back(0xffff);
 return out;
}
// Fallback for VS2.x hardware with >=29 temps and dynamic branching.
// Preserve the original version, declarations, color/fog/UV outputs and constants.
// Only position writes are redirected before appending the existing snap arithmetic.
inline Result PositionOnly(const std::vector<U>&in,unsigned width,unsigned height,unsigned snap256,unsigned snapType=0,unsigned clipPrecision=32){
 Result out;if(in.empty()||in[0]!=0xfffe0201u||!snap256){out.reason="position fallback requires VS2.x and nonzero snap";return out;}
 auto full=Convert(in,width,height,snap256,snapType,clipPrecision);if(full.code.empty())return full;
 out.code.push_back(in[0]);unsigned slots=0;
 for(size_t i=1;i<in.size();){
  U token=in[i++];unsigned op=token&65535,n=(token>>24)&15;
  if(op==65535)break;
  if(op==65534){n=(token>>16)&0x7fff;out.code.push_back(token);out.code.insert(out.code.end(),in.begin()+i,in.begin()+i+n);i+=n;continue;}
  out.code.push_back(token);
  for(unsigned k=0;k<n;k++){
   U arg=in[i++];if(k==0&&op!=31&&op!=81&&Type(arg)==4&&(arg&2047)==0)arg=(arg&0x0fff0000)|Reg(0,20);
   out.code.push_back(arg);
  }
  if(op!=31&&op!=81)slots+=(op==20||op==22)?4:(op==21||op==23||op==16||op==32||op==36)?3:(op==24||op==18||op==33)?2:1;
 }
 const U marker[]={13u|(3u<<24),D(0,27,1),R(20,3),R(20,3)};
 auto begin=std::search(full.code.begin(),full.code.end(),std::begin(marker),std::end(marker));
 if(begin==full.code.end()){out.code.clear();out.reason="missing snap epilogue";return out;}
 size_t i=begin-full.code.begin();
 for(;i<full.code.size();){
  U token=full.code[i];unsigned op=token&65535,n=(token>>24)&15;
  if(op==1&&n==2&&full.code[i+1]==D(6,0))break;
  out.code.insert(out.code.end(),full.code.begin()+i,full.code.begin()+i+1+n);i+=1+n;slots+=op==41?3:1;
 }
 Emit(out.code,1,{D(4,0),R(20)});out.code.push_back(65535);slots++;
 if(slots>256){out.code.clear();out.reason="VS2.x instruction budget";return out;}
 out.instructions=slots;out.relative=full.relative;return out;
}

}
