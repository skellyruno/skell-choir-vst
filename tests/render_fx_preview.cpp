#include "../Source/ChoirFX.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>
#include <iostream>
#include <algorithm>

static void u16(std::ofstream& s,uint16_t v){s.put((char)v);s.put((char)(v>>8));}
static void u32(std::ofstream& s,uint32_t v){u16(s,(uint16_t)v);u16(s,(uint16_t)(v>>16));}

int main(int argc,char** argv) {
    const int sr=48000;
    const int count=sr*10;
    const char* file=argc>1?argv[1]:"OmniChoir-FX-preview.wav";
    choir::ChoirEffect fx;fx.prepare(sr);
    choir::FXParams p;p.mix=.93f;p.master=1.2f;p.wet=.72f;p.ensemble=.69f;
    p.roomSeconds=2.4f;
    fx.setParameters(p);
    std::vector<float> samples((size_t)count*2,0.f);
    float phase=0;
    const int melody[]={60,62,64,67};
    for(int t=0;t<count;++t) {
        const float secs=t/(float)sr;
        const float inRepeat=std::fmod(secs,4.f);
        const int n=std::min(3,(int)inRepeat);
        const float pitch=std::exp2((melody[n]-69.f)/12.f)*440.f;
        phase+=pitch/sr;phase-=std::floor(phase);
        const float loc=inRepeat-n;
        const float edge=std::min(1.f,std::min(loc/.09f,(1.f-loc)/.16f));
        // Synthetic solo-vowel example, repeated first dry and then wet.
        const float dry=std::max(0.f,edge)*(.17f*std::sin(2*choir::pi*phase)+
                  .048f*std::sin(4*choir::pi*phase)+.016f*std::sin(6*choir::pi*phase));
        float outL=0,outR=0;
        fx.render(dry,dry,outL,outR);
        float gain=secs<4.f?1.65f:2.3f;
        if(secs<4.f)outL=outR=dry*gain;
        else if(secs>8.f){outL*=.5f;outR*=.5f;}
        else {outL*=gain;outR*=gain;}
        samples[(size_t)t*2]=outL;
        samples[(size_t)t*2+1]=outR;
    }
    float peak=0;
    for(const float x:samples)peak=std::max(peak,std::abs(x));
    const float scale=peak>0?.92f/peak:1;
    std::ofstream out(file,std::ios::binary);
    out.write("RIFF",4);u32(out,36+count*4);out.write("WAVEfmt ",8);
    u32(out,16);u16(out,1);u16(out,2);u32(out,sr);u32(out,sr*4);u16(out,4);u16(out,16);
    out.write("data",4);u32(out,count*4);
    for(float x:samples) {
        const int v=(int)std::lround(std::max(-1.f,std::min(1.f,x*scale))*32767.f);
        u16(out,(uint16_t)(int16_t)v);
    }
    std::cout<<"rendered "<<file<<" with peak "<<peak<<" (normalized)\n";
}
