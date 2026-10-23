// JUCE-free end-to-end proof for full-take scan, editable SATB CSV and export.
// Usage: omnichoir-batch scan input.wav arrangement.csv
//        omnichoir-batch render input.wav arrangement.csv choir.wav
// WAV input: little-endian 16-bit PCM or 32-bit float, mono/stereo.
#include "../Source/Offline/FullTakeScan.h"
#include "../Source/ChoirFX.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
uint16_t u16(std::istream& in) {unsigned char a[2];if(!in.read((char*)a,2))throw std::runtime_error("truncated wav");return (uint16_t)(a[0]|(a[1]<<8));}
uint32_t u32(std::istream& in) {unsigned char a[4];if(!in.read((char*)a,4))throw std::runtime_error("truncated wav");return (uint32_t)a[0]|((uint32_t)a[1]<<8)|((uint32_t)a[2]<<16)|((uint32_t)a[3]<<24);}
void w16(std::ostream& out,uint16_t v){unsigned char b[2]={(unsigned char)v,(unsigned char)(v>>8)};out.write((char*)b,2);}
void w32(std::ostream& out,uint32_t v){w16(out,(uint16_t)v);w16(out,(uint16_t)(v>>16));}
struct Audio {int channels=0, sampleRate=0;std::vector<float> samples;size_t frames()const{return samples.size()/(size_t)channels;}};
Audio load(const char* path) {
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot open input WAV");
    char header[4];in.read(header,4);if(std::string(header,4)!="RIFF")throw std::runtime_error("Only RIFF WAV supported");
    u32(in);in.read(header,4);if(std::string(header,4)!="WAVE")throw std::runtime_error("Missing WAVE header");
    Audio result;int bits=0,format=0;std::streampos dataPos=0;uint32_t dataBytes=0;
    while(in.read(header,4)) {
        const auto bytes=u32(in);const std::string chunk(header,4);const auto start=in.tellg();
        if(chunk=="fmt ") {
            format=u16(in);result.channels=(int)u16(in);result.sampleRate=(int)u32(in);
            u32(in);u16(in);bits=(int)u16(in);
        } else if(chunk=="data") {dataPos=start;dataBytes=bytes;}
        in.seekg(start+(std::streamoff)bytes+(std::streamoff)(bytes%2));
        if(!in && chunk!="data")break;
    }
    if(result.channels<1||result.channels>2||result.sampleRate<16000||result.sampleRate>192000
       ||!(format==1&&bits==16)&&!(format==3&&bits==32)||!dataBytes)
        throw std::runtime_error("WAV must be mono/stereo PCM16 or float32 at 16-192 kHz");
    in.clear();in.seekg(dataPos);
    result.samples.reserve(dataBytes/(size_t)(bits/8));
    const size_t samples=dataBytes/(size_t)(bits/8);
    for(size_t i=0;i<samples;++i) {
        float sample=0;
        if(bits==16)sample=(int16_t)u16(in)/32768.f;
        else {const uint32_t bits32=u32(in);static_assert(sizeof(float)==4);std::memcpy(&sample,&bits32,4);}
        result.samples.push_back(std::isfinite(sample)?std::clamp(sample,-1.f,1.f):0.f);
    }
    return result;
}
void save(const char* path,const Audio& audio) {
    std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("Cannot write WAV");
    const auto bytes=(uint64_t)audio.samples.size()*2;
    if(bytes>=0xffffffffull)throw std::runtime_error("Output larger than RIFF 4GB limit");
    out.write("RIFF",4);w32(out,(uint32_t)(36+bytes));out.write("WAVEfmt ",8);
    w32(out,16);w16(out,1);w16(out,(uint16_t)audio.channels);w32(out,(uint32_t)audio.sampleRate);
    w32(out,(uint32_t)(audio.sampleRate*audio.channels*2));w16(out,(uint16_t)(audio.channels*2));w16(out,16);
    out.write("data",4);w32(out,(uint32_t)bytes);
    for(float sample:audio.samples)w16(out,(uint16_t)(int16_t)std::lrint(std::clamp(sample,-.9999f,.9999f)*32767.f));
}
void writeScore(const char* path,const choir::FullTakeScore& score) {
    std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write arrangement CSV");
    out<<"start,end,melody,soprano,alto,tenor,bass,confidence\n";
    out.precision(8);
    for(const auto& e:score.notes)out<<e.start<<','<<e.end<<','<<e.melody<<','
       <<e.satb[0]<<','<<e.satb[1]<<','<<e.satb[2]<<','<<e.satb[3]<<','<<e.confidence<<'\n';
}
choir::FullTakeScore readScore(const char* path) {
    std::ifstream in(path);if(!in)throw std::runtime_error("Cannot read score CSV");
    choir::FullTakeScore score;std::string line;std::getline(in,line);
    while(std::getline(in,line)) {
        std::replace(line.begin(),line.end(),',',' ');
        choir::NoteEvent e;std::istringstream row(line);
        if(row>>e.start>>e.end>>e.melody>>e.satb[0]>>e.satb[1]
              >>e.satb[2]>>e.satb[3]>>e.confidence) {
            if(e.start<0||e.end<=e.start)throw std::runtime_error("Invalid note time in CSV");
            score.notes.push_back(e);
        }
    }
    std::sort(score.notes.begin(),score.notes.end(),[](const auto&a,const auto&b){return a.start<b.start;});
    return score;
}
void scan(const char* wav,const char* scorePath) {
    auto source=load(wav);choir::FullTakeScanner scanner;scanner.prepare(source.sampleRate);
    for(size_t frame=0;frame<source.frames();++frame) {
        float s=source.samples[frame*(size_t)source.channels];
        if(source.channels==2)s=.5f*(s+source.samples[frame*2+1]);
        scanner.push(s);
    }
    auto score=scanner.finish(false);writeScore(scorePath,score);
    std::cout<<"Scanned "<<score.duration<<" seconds; "<<score.notes.size()
             <<" editable notes; dominant pitch class "<<score.root<<"\n";
}
void render(const char* wav,const char* scorePath,const char* output) {
    auto input=load(wav);const auto score=readScore(scorePath);
    if(score.notes.empty())throw std::runtime_error("No usable notes in CSV; scan a monophonic vocal");
    Audio out;out.sampleRate=input.sampleRate;out.channels=2;out.samples.resize(input.frames()*2);
    choir::ChoirEffect fx;fx.prepare(input.sampleRate);choir::FXParams p;p.mix=1;p.master=.95f;
    fx.setParameters(p);size_t event=0,last=(size_t)-1;
    for(size_t frame=0;frame<input.frames();++frame) {
        const double t=frame/(double)input.sampleRate;
        while(event<score.notes.size()&&t>score.notes[event].end)++event;
        if(event<score.notes.size()&&t>=score.notes[event].start) {
            if(last!=event) {
                fx.setScoreVoicing(score.notes[event].melody,score.notes[event].satb);
                last=event;
            }
        } else if(last!=(size_t)-1) {fx.clearScoreVoicing();last=(size_t)-1;}
        const float a=input.samples[frame*(size_t)input.channels];
        const float b=input.channels==1?a:input.samples[frame*2+1];
        float l=0,r=0;fx.render(a,b,l,r);
        out.samples[frame*2]=l;out.samples[frame*2+1]=r;
    }
    save(output,out);
    std::cout<<"Exported choir-only stereo WAV: "<<input.frames()/(double)input.sampleRate<<" seconds\n";
}
}
int main(int argc,char** argv) {
    try {
        if(argc==4&&std::string(argv[1])=="scan")scan(argv[2],argv[3]);
        else if(argc==5&&std::string(argv[1])=="render")render(argv[2],argv[3],argv[4]);
        else {std::cerr<<"Usage:\n  omnichoir-batch scan vocal.wav score.csv\n"
            <<"  omnichoir-batch render vocal.wav score.csv choir.wav\n";return 2;}
    } catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
