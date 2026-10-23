#include "../Source/Offline/ExternalStemMixer.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs=std::filesystem;
static void u16(std::ostream& out,uint16_t v) {
    out.put((char)(v&255));out.put((char)((v>>8)&255));
}
static void u32(std::ostream& out,uint32_t v) {u16(out,(uint16_t)v);u16(out,(uint16_t)(v>>16));}
static void makeWav(const fs::path& path,int sr,int pitch,int frames) {
    std::ofstream f(path,std::ios::binary);
    f.write("RIFF",4);u32(f,36+frames*2);f.write("WAVEfmt ",8);
    u32(f,16);u16(f,1);u16(f,1);u32(f,sr);u32(f,sr*2);u16(f,2);u16(f,16);
    f.write("data",4);u32(f,frames*2);
    for(int i=0;i<frames;++i)
        u16(f,(uint16_t)(int16_t)std::lround(7200*std::sin(6.2831853*pitch*i/sr)));
}
static std::vector<int16_t> samples(const fs::path& p) {
    std::ifstream f(p,std::ios::binary);f.seekg(44);
    std::vector<int16_t> v;
    while(true) {
        char a,b;if(!f.get(a)||!f.get(b))break;
        v.push_back((int16_t)(uint16_t((uint8_t)a)|(uint16_t((uint8_t)b)<<8)));
    }
    return v;
}
int main() {
    const auto d=fs::temp_directory_path()/"omnichoir_v05_stem_tests";
    fs::create_directories(d);
    makeWav(d/"s.wav",48000,440,4800);
    makeWav(d/"b.wav",48000,220,4800);
    choir::stems::Manifest m;
    m.room=.25;m.outputDb=-3;
    m.voices={{(d/"s.wav").string(),"soprano",-5,6,9,-7},
              {(d/"b.wav").string(),"bass",5,6,15,-7}};
    choir::stems::Result result;
    std::string error;
    const auto output=(d/"mix.wav").string();
    assert(choir::stems::render(m,output,result,error));
    assert(result.sampleRate==48000 && result.voiceCount==2 && result.seconds>.3);
    assert(result.peak>0 && result.safetyGain>0);
    const auto audio=samples(output);
    assert(audio.size()>10000 && audio.size()%2==0);
    int different=0,nonzero=0;
    for(size_t i=0;i<audio.size();i+=2) {
        different+=(audio[i]!=audio[i+1]);
        nonzero+=(audio[i]!=0||audio[i+1]!=0);
    }
    assert(different>400 && nonzero>1000);
    const auto cancelled=(d/"cancelled.wav").string();
    assert(!choir::stems::render(m,cancelled,result,error,[](float x){return x<.66f;}));
    assert(error=="Stem render cancelled" && !fs::exists(cancelled));
    makeWav(d/"other.wav",44100,120,4410);
    m.voices[1].file=(d/"other.wav").string();
    assert(!choir::stems::render(m,output,result,error));
    assert(error.find("sample rates differ")!=std::string::npos);
    m.voices.pop_back();
    assert(!choir::stems::render(m,m.voices[0].file,result,error));
    assert(error.find("overwrite")!=std::string::npos);
    fs::remove_all(d);
    std::cout<<"offline native external-stem mixer PASS\n";
}
