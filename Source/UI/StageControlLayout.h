#pragma once
// JUCE-free solo/mute layout, attached to the actual rendered singer silhouette.
#include <algorithm>
namespace stagecontrols {
struct Rect { float x, y, width, height; };
constexpr float kButton=10.f, kRowStep=12.f; // compact but legible; click targets stay padded
inline Rect beside(float leftTop,float rightTop,float topRowY,float figureSize,
                   float plotLeft,float plotTop,float plotRight,float plotBottom,bool mute)
{
    const float edge=.62f*figureSize;
    float x=rightTop+edge+.5f;
    if(x+kButton>plotRight-4.f) x=leftTop-edge-kButton-.5f;
    x=std::clamp(x,plotLeft+3.f,plotRight-kButton-3.f);
    const float top=std::clamp(topRowY-.74f*figureSize-1.f,
                                plotTop+30.f,plotBottom-kRowStep-kButton-3.f);
    return {x,top+(mute?kRowStep:0.f),kButton,kButton};
}
inline Rect besideLead(float x,float y,float plotLeft,float plotTop,
                       float plotRight,float plotBottom,bool mute)
{ return beside(x,x,y,20.f,plotLeft,plotTop,plotRight,plotBottom,mute); }
}
