// Supply original VS2.x token binaries extracted from the user's captures.
// Originals are not bundled. Independent CPU arithmetic check, not GPU validation.
#define main SyntheticFixturesMain
#include "test_native_shader.cpp"
#undef main
int main(int argc,char**argv){
 assert(argc>1);std::mt19937 rng(73);std::uniform_real_distribution<double> random(-1,1),positive(.1,1);
 unsigned count=0;
 for(int file=1;file<argc;file++){
  std::ifstream in(argv[file],std::ios::binary|std::ios::ate);assert(in);auto bytes=in.tellg();assert(bytes>0&&bytes%4==0);
  std::vector<U> original(size_t(bytes)/4);in.seekg(0);in.read((char*)original.data(),bytes);
  for(unsigned snapQ:{0u,128u,256u})for(int trial=0;trial<100;trial++){
   VM start;for(auto&v:start.v)for(auto&x:v)x=random(rng);
   for(auto&c:start.c)for(auto&x:c)x=random(rng);
   start.v[0][3]=1;
   double sum=0;for(auto&x:start.v[1]){x=positive(rng);sum+=x;}for(auto&x:start.v[1])x/=sum;
   for(auto&x:start.v[2])x=(rng()%32)*3;
   VM reference=start;reference.run(LegacyForVM(original));
   auto adapted=Convert(original,427,240,snapQ);assert(!adapted.code.empty());
   VM full=start;full.run(adapted.code);
   auto close=[](double a,double b){assert(std::isfinite(a)&&std::isfinite(b));assert(fabs(a-b)<1e-8*(1+fabs(b)));};
   V expected=reference.o[0];double w=expected[3];
   if(snapQ&&w>1./1024)for(int axis=0;axis<2;axis++){
    double sign=axis?-1:1,grid=axis?240:427;
    double screen=(expected[axis]/w*sign*.5+.5)*grid;
    double target=(floor(screen+.5)/grid-.5)*2*sign*w;
    expected[axis]+=(target-expected[axis])*snapQ/256.;
   }
   for(int k=0;k<4;k++){close(full.o[0][k],expected[k]);close(full.o[1][k],std::clamp(reference.o[1][k],0.,1.));}
   for(int k=0;k<2;k++){close(full.o[4][k],reference.o[4][k]);close(full.o[2][k],reference.o[4][k]*w);}
   assert(full.c==start.c);
   if(snapQ){auto fallback=PositionOnly(original,427,240,snapQ);assert(!fallback.code.empty());VM legacy=start;legacy.run(LegacyForVM(fallback.code));
    for(int k=0;k<4;k++){close(legacy.o[0][k],expected[k]);close(legacy.o[1][k],reference.o[1][k]);close(legacy.o[5][k],reference.o[5][k]);}
    for(int k=0;k<2;k++)close(legacy.o[4][k],reference.o[4][k]);
    assert(legacy.c==start.c);
   }
   count++;
  }
 }
 printf("PASS: %d captured shaders, %u randomized cases; skin transform, snap, color/UV/constants, legacy fog preserved.\n",argc-1,count);
 puts("NOT TESTED: Windows driver acceptance, in-game FPS, visible pedestrian effects.");
}
