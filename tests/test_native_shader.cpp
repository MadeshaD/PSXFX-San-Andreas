// Execute generated SM3 tokens in an independent scalar test interpreter.
// This is not a D3D9 shader validator or GPU/animation runtime test.
#include "../src/native_shader.hpp"
#include <array>
#include <cmath>
#include <cassert>
#include <random>
#include <cstdio>
#include <fstream>
using namespace native_shader;
using V=std::array<double,4>;
struct VM {
 std::array<V,32> r{};std::array<V,16> v{};std::array<V,256> c{};std::array<V,12> o{};V a{};
 V source(U t,U rel=0,unsigned offset=0){
  unsigned type=Type(t),n=(t&2047)+offset;
  if(t&8192)n+=(unsigned)a[(rel>>16)&3];
  V x{};if(type==0)x=r.at(n);else if(type==1)x=v.at(n);else if(type==2)x=c.at(n);else if(type==3)x=a;else assert(false);
  V q{};for(int k=0;k<4;k++){q[k]=x[(t>>(16+2*k))&3];unsigned mod=(t>>24)&15;if(mod==1)q[k]=-q[k];else if(mod==11)q[k]=fabs(q[k]);else if(mod==12)q[k]=-fabs(q[k]);else assert(mod==0);}return q;
 }
 void run(const std::vector<U>&code){
  assert(code.at(0)==0xfffe0300u);bool active=true;
  for(size_t i=1;i<code.size();){U token=code.at(i++);unsigned op=token&65535,n=(token>>24)&15;size_t end=i+n;
   if(op==65535)break;
   assert(end<=code.size());
   if(op==31){i=end;continue;}
   if(op==41){V x=source(code[i]),y=source(code[i+1]);active=x[0]>y[0];i=end;continue;}
   if(op==43){active=true;i=end;continue;}
   if(!active){i=end;continue;}
   U dst=code.at(i++);std::vector<V> args;std::vector<std::pair<U,U>> regs;
   while(i<end){U s=code.at(i++),rel=0;if(s&8192)rel=code.at(i++);args.push_back(source(s,rel));regs.push_back({s,rel});}
   V q{};auto x=args[0];V y=args.size()>1?args[1]:V{},z=args.size()>2?args[2]:V{};
   for(int k=0;k<4;k++)switch(op){
    case 1:q[k]=x[k];break;case 2:q[k]=x[k]+y[k];break;case 3:q[k]=x[k]-y[k];break;
    case 4:q[k]=x[k]*y[k]+z[k];break;case 5:q[k]=x[k]*y[k];break;
    case 6:q[k]=1/x[0];break;case 7:q[k]=1/sqrt(fabs(x[0]));break;
    case 8:case 9:for(int j=0;j<(op==8?3:4);j++)q[k]+=x[j]*y[j];break;
    case 10:q[k]=std::min(x[k],y[k]);break;case 11:q[k]=std::max(x[k],y[k]);break;
    case 12:q[k]=x[k]<y[k];break;case 13:q[k]=x[k]>=y[k];break;
    case 78:case 14:q[k]=exp2(x[0]);break;case 19:q[k]=x[k]-floor(x[k]);break;
    case 20:case 21:case 22:case 23:case 24:{V row=source(regs[1].first,regs[1].second,k);for(int j=0;j<(op>=22?3:4);j++)q[k]+=x[j]*row[j];break;}
    case 46:q[k]=std::round(x[k]);break;
    default:fprintf(stderr,"Unhandled VM opcode %u\n",op);assert(false);
   }
   V*target=nullptr;unsigned type=Type(dst),index=dst&2047;
   if(type==0)target=&r.at(index);else if(type==3)target=&a;else if(type==6)target=&o.at(index);else assert(false);
   for(int k=0;k<4;k++)if(dst&(1u<<(16+k)))(*target)[k]=(dst&0x00100000)?std::clamp(q[k],0.,1.):q[k];
   assert(i==end);
  }
 }
};
std::vector<U> fixture(bool skin,bool old){
 std::vector<U>b{old?0xfffe0101u:0xfffe0200u};
 for(unsigned n:{0u,1u,2u,4u,5u})Emit(b,31,{0x80000000u|(n==0?0:n==1?1:n==2?2:n==4?10:5),D(1,n)});
 if(skin){
  for(unsigned k=0;k<4;k++){
   Emit(b,old?1:46,{D(3,0,1),S(1,2,k*0x55)});
   Emit(b,21,{D(0,5,7),S(1,0),S(2,16)|8192,S(3,0,0)});
   if(!k)Emit(b,5,{D(0,3,7),R(5),S(1,1,k*0x55)});
   else Emit(b,4,{D(0,3,7),R(5),S(1,1,k*0x55),R(3)});
  }
  Emit(b,1,{D(0,3,8),S(1,0,255)});Emit(b,20,{D(4,0),R(3),S(2,0)});
 }else Emit(b,20,{D(4,0),S(1,0),S(2,0)});
 Emit(b,5,{D(5,0),S(1,4),S(2,4)});Emit(b,1,{D(6,0,3),S(1,5)});b.push_back(65535);
 if(old){std::vector<U>legacy{b[0]};for(size_t i=1;i<b.size();){U t=b[i++];if(t==65535){legacy.push_back(t);break;}unsigned n=(t>>24)&15;legacy.push_back(t&65535);for(unsigned j=0;j<n;j++){U arg=b[i++];legacy.push_back(arg);if((t&65535)!=31&&j>0&&(arg&8192)){i++;j++;}}}return legacy;}
 return b;
}
std::vector<U> LegacyForVM(std::vector<U> code){
 code[0]=0xfffe0300u;
 for(size_t i=1;i<code.size();){U t=code[i++];unsigned op=t&65535,n=(t>>24)&15;if(op==65535)break;
  if(op==65534){i+=(t>>16)&0x7fff;continue;}
  if(op!=31&&op!=81&&n){U&d=code[i];unsigned type=Type(d),index=d&2047;
   if(type==4)d=(d&0x0fff0000)|Reg(6,index?5:0);
   else if(type==5)d=(d&0x0fff0000)|Reg(6,index?6:1);
   else if(type==6)d=(d&0x0fff0000)|Reg(6,4+index);
  }i+=n;
 }return code;
}
int main(int argc,char**argv){
 std::mt19937 rng(31);std::uniform_real_distribution<double>d(-2,2),positive(.1,1);
 unsigned tested=0;
 for(unsigned snapType:{0u,1u,2u})for(unsigned version:{0xfffe0101u,0xfffe0200u,0xfffe0201u})for(bool skin:{false,true})for(unsigned snapQ:{0u,128u,256u}){
  bool old=version==0xfffe0101u;auto f=fixture(skin,old);f[0]=version;auto adapted=Convert(f,427,240,snapQ,snapType,37);
  if(adapted.code.empty())fprintf(stderr,"FAIL %s\n",adapted.reason.c_str());
  assert(!adapted.code.empty());
  if(argc>1&&skin&&!old&&snapQ==128){std::ofstream file(argv[1],std::ios::binary);file.write((char*)adapted.code.data(),adapted.code.size()*4);}
  if(argc>2){std::string name=std::string(argv[2])+"/"+std::to_string(version)+"_"+std::to_string(skin)+"_"+std::to_string(snapQ)+".bin";std::ofstream file(name,std::ios::binary);file.write((char*)adapted.code.data(),adapted.code.size()*4);}
  auto fallback=PositionOnly(f,427,240,snapQ,snapType,37);
  bool hasFallback=version==0xfffe0201u&&snapQ>0;
  assert(fallback.code.empty()!=hasFallback);
  if(hasFallback){assert(fallback.code[0]==version&&fallback.instructions<=256);
   // Original token stream unchanged except position destinations and final END.
   for(size_t i=1;i<f.size()-1;){U token=f[i];unsigned n=(token>>24)&15;assert(fallback.code[i]==token);i++;
    for(unsigned k=0;k<n;k++,i++){U expected=f[i];if(k==0&&(token&65535)!=31&&(token&65535)!=81&&Type(expected)==4&&(expected&2047)==0)expected=(expected&0x0fff0000)|Reg(0,20);assert(fallback.code[i]==expected);}
   }
   if(argc>2){std::string name=std::string(argv[2])+"/legacy_"+std::to_string(skin)+"_"+std::to_string(snapQ)+".bin";std::ofstream file(name,std::ios::binary);file.write((char*)fallback.code.data(),fallback.code.size()*4);}
  }
  for(int it=0;it<300;it++){
   VM vm;for(auto&c:vm.c)for(auto&x:c)x=d(rng);
   vm.v[0]={d(rng),d(rng),d(rng),1};vm.v[4]={positive(rng),positive(rng),positive(rng),positive(rng)};vm.v[5]={d(rng)*4,d(rng)*4,0,0};
   double sum=0;for(auto&x:vm.v[1]){x=positive(rng);sum+=x;}for(auto&x:vm.v[1])x/=sum;
   for(int k=0;k<4;k++)vm.v[2][k]=(rng()%32)*3+(it%2?.2:.8);
   V p=vm.v[0];
   if(skin){p={0,0,0,1};for(int k=0;k<4;k++)for(int j=0;j<3;j++)for(int t=0;t<4;t++)p[j]+=vm.v[1][k]*vm.v[0][t]*vm.c[16+(int)std::round(vm.v[2][k])+j][t];}
   V clip{};for(int j=0;j<4;j++)for(int k=0;k<4;k++)clip[j]+=p[k]*vm.c[j][k];
   V expected=clip;
   if(clip[3]>1./1024&&snapQ){for(int k=0;k<2;k++){
    double sign=k?-1:1,grid=k?240:427,target=0;
    if(snapType==0){double screen=(clip[k]/clip[3]*sign*.5+.5)*grid;target=(floor(screen+.5)/grid-.5)*2*sign*clip[3];}
    else if(snapType==1)target=floor(clip[k]/clip[3]*grid*.5)/(grid*.5)*clip[3];
    else target=floor(clip[k]*37)/37;
    expected[k]+=double(snapQ)/256*(target-clip[k]);
   }}
   if(hasFallback){VM legacy=vm;legacy.run(LegacyForVM(fallback.code));
    for(int k=0;k<4;k++){assert(fabs(legacy.o[0][k]-expected[k])<1e-9);assert(fabs(legacy.o[1][k]-vm.v[4][k]*vm.c[4][k])<1e-9);}
    for(int k=0;k<2;k++)assert(legacy.o[4][k]==vm.v[5][k]);
    assert(legacy.c==vm.c);
   }
   auto constants=vm.c;vm.run(adapted.code);assert(constants==vm.c);
   for(int k=0;k<4;k++){assert(fabs(vm.o[0][k]-expected[k])<1e-9);assert(fabs(vm.o[1][k]-std::clamp(vm.v[4][k]*vm.c[4][k],0.,1.))<1e-9);}
   for(int k=0;k<2;k++){assert(vm.o[4][k]==vm.v[5][k]);assert(fabs(vm.o[2][k]-vm.v[5][k]*clip[3])<1e-9);}
   for(int k=0;k<3;k++)assert(fabs(vm.o[5][k]-vm.o[1][k]*clip[3])<1e-9);
   assert(vm.o[2][2]==vm.o[0][3]&&vm.o[3][2]==vm.o[0][3]&&vm.o[3][0]==0&&vm.o[3][1]==0);tested++;
  }
 }
 auto good=fixture(true,false);
 for(size_t n=0;n<good.size();n++){auto bad=std::vector<U>(good.begin(),good.begin()+n);assert(Convert(bad,320,240,128).code.empty());}
 auto bad=good;bad[0]=0xfffe0300;assert(Convert(bad,320,240,128).code.empty());bad=good;bad[1]=0x01000028;assert(Convert(bad,320,240,128).code.empty());
 for(int n=0;n<10000;n++){std::vector<U>bad(rng()%100);for(auto&x:bad)x=rng();if(!bad.empty())bad[0]=0xfffe0200;assert(Convert(bad,320,240,128).code.empty());}
 printf("PASS: %u native vertex executions (VS1.1/2.0/2.1, rigid/4-bone skinning, 32-bone palette, 3 snap types, snap 0/.5/1).\n",tested);
 puts("PASS: original constants preserved, lighting RGBA, clip z/w, perspective/affine UV outputs, near/behind-camera bypass.");
 puts("PASS: 3600 VS2.x position-only executions; native color/UV/constants and original body preserved; <=256 slots.");
 puts("PASS: truncated/unsupported/random token streams rejected. NOT TESTED: actual game shader streams, D3D9 bytecode validation, GPU rendering.");
 return 0;
}
