#pragma once
#include <cmath>
namespace atmosphere {
inline int ShadowQuality(int original,bool active,bool simple){return active&&simple?0:original;}
inline void ScaleRGB(float*rgb,float scale){for(int i=0;i<3;i++)if(std::isfinite(rgb[i]))rgb[i]*=scale;}
}
