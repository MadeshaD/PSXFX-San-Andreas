// Render-thread timings. Submission includes the D3D driver call, not GPU completion.
namespace seam_profile {
enum Stage { build, search, prepare, submit };
static LONGLONG ticks[4]{};
static unsigned hits=0,firstFrame=0;
struct Timer {
 LONGLONG* dest=nullptr;
 LARGE_INTEGER start{};
 explicit Timer(Stage s){
  if(logging && QueryPerformanceCounter(&start))dest=&ticks[s];
 }
 ~Timer(){stop();}
 void stop(){
  if(dest){LARGE_INTEGER now;QueryPerformanceCounter(&now);*dest+=now.QuadPart-start.QuadPart;dest=nullptr;}
 }
};
static void report(){
 if(logging && firstFrame!=frame){
  LARGE_INTEGER frequency;
  if(QueryPerformanceFrequency(&frequency)){
   double scale=1000./double(frequency.QuadPart)/double(frame-firstFrame);
   Log("SeamFill CPU ms/frame: build=%.3f search=%.3f prepare=%.3f submit=%.3f cachedPairs=%u",
       ticks[build]*scale,ticks[search]*scale,ticks[prepare]*scale,ticks[submit]*scale,hits);
  }
 }
 firstFrame=frame;std::fill_n(ticks,4,0);hits=0;
}
}
