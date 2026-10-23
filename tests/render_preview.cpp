#include "../Source/ChoirDSP.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

static void u16(std::ofstream& o, uint16_t v) { const unsigned char b[2] = { (uint8_t)(v&255),(uint8_t)((v>>8)&255) }; o.write((const char*)b,2); }
static void u32(std::ofstream& o, uint32_t v) { for(int i=0;i<4;++i) o.put((char)(v>>(8*i))); }
int main(int argc, char** argv) {
    if (argc<2) { std::cerr << "Usage: render_preview output.wav\n"; return 1; }
    constexpr int sr=48000;
    choir::Instrument inst; inst.prepare(sr);
    choir::Params p; p.attack=.20f; p.release=1.1f; p.wet=.45f; p.vowel=0; p.roomSeconds=2.7f; p.ensemble=.7f;
    inst.setParameters(p);
    // Four-part C-major voicing, then change to a soft G major chord.
    const int first[4]={76,67,60,48}, second[4]={74,67,62,43};
    const int total=5*sr;
    std::vector<float> signal((size_t)total*2);
    for (int i=0; i<total; ++i) {
        if (i==0) for(int j=0;j<4;++j) inst.noteOn(j+2,first[j],.9f);
        if (i==sr*2) {
            for(int j=0;j<4;++j) inst.noteOff(j+2,first[j]);
            for(int j=0;j<4;++j) inst.noteOn(j+2,second[j],.76f);
        }
        if (i==sr*3) for(int j=0;j<4;++j) inst.noteOff(j+2,second[j]);
        float l,r; inst.render(l,r);
        signal[(size_t)i*2]=l; signal[(size_t)i*2+1]=r;
    }
    float peak=.0001f;
    for (auto x : signal) peak=std::max(peak,std::abs(x));
    const float scale=.78f/peak;
    std::ofstream out(argv[1],std::ios::binary);
    const uint32_t bytes=(uint32_t)signal.size()*2;
    out.write("RIFF",4);u32(out,36+bytes);out.write("WAVEfmt ",8);
    u32(out,16);u16(out,1);u16(out,2);u32(out,sr);u32(out,sr*4);u16(out,4);u16(out,16);
    out.write("data",4);u32(out,bytes);
    for(auto x : signal) { const int v=(int)std::round(std::clamp(x*scale,-1.f,1.f)*32767);u16(out,(uint16_t)(int16_t)v); }
    std::cout << "Generated " << argv[1] << "; peak before normalization=" << peak << "\n";
}
