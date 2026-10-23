#include "ExternalStemMixer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

namespace choir::stems {
namespace {
constexpr double pi=3.14159265358979323846;
uint16_t read16(std::istream& s) {
    unsigned char a=0,b=0; s.read((char*)&a,1);s.read((char*)&b,1);
    return uint16_t(a)|(uint16_t(b)<<8);
}
uint32_t read32(std::istream& s) {return uint32_t(read16(s))|(uint32_t(read16(s))<<16);}
void write16(std::ostream& s,uint16_t v) {char b[2]={(char)v,(char)(v>>8)};s.write(b,2);}
void write32(std::ostream& s,uint32_t v) {write16(s,(uint16_t)v);write16(s,(uint16_t)(v>>16));}
class Reader {
public:
    std::ifstream f;
    int channels=0,rate=0;
    uint64_t frames=0,dataOffset=0;
    std::vector<char> scratch;
    bool open(const std::string& file,std::string& error) {
        f.open(file,std::ios::binary);
        if(!f) {error="Missing or unreadable stem: "+file;return false;}
        char tag[4]{};f.read(tag,4);
        if(std::string(tag,4)!="RIFF") {error="Not a RIFF WAV: "+file;return false;}
        read32(f);f.read(tag,4);
        if(std::string(tag,4)!="WAVE") {error="Not a WAVE file: "+file;return false;}
        bool fmt=false,data=false;
        while(f && !data) {
            f.read(tag,4);if(f.gcount()!=4)break;
            const uint32_t length=read32(f);
            const uint64_t start=(uint64_t)f.tellg();
            if(std::string(tag,4)=="fmt " && length>=16) {
                const auto codec=read16(f);channels=(int)read16(f);
                rate=(int)read32(f);read32(f);read16(f);const int bits=read16(f);
                if(codec!=1 || (channels!=1&&channels!=2) || bits!=16 || rate<16000 || rate>192000) {
                    error="Need PCM16 mono/stereo 16-192kHz WAV: "+file;return false;
                }
                fmt=true;
            } else if(std::string(tag,4)=="data") {
                dataOffset=start;
                if(!fmt) {error="WAV data precedes fmt: "+file;return false;}
                frames=length/(2*channels);
                data=true;
            }
            if(!data)f.seekg((std::streamoff)(start+length+(length&1)),std::ios::beg);
        }
        if(!fmt || !data || frames==0 || !f) {error="Invalid/empty WAV: "+file;return false;}
        return true;
    }
    bool readMono(int64_t index,int count,std::vector<float>& dest) {
        dest.assign((size_t)count,0.f);
        const int64_t begin=std::max<int64_t>(0,index);
        const int64_t finish=std::min<int64_t>((int64_t)frames,index+count);
        if(finish<=begin)return true;
        const size_t bytes=(size_t)(finish-begin)*(size_t)channels*2;
        scratch.resize(bytes);
        f.clear();f.seekg((std::streamoff)(dataOffset+(uint64_t)begin*(uint64_t)channels*2));
        if(!f.read(scratch.data(),(std::streamsize)bytes))return false;
        for(int64_t i=begin;i<finish;++i) {
            const size_t at=(size_t)(i-begin)*channels*2;
            const auto lo=[&](size_t p){return (int16_t)(uint16_t(uint8_t(scratch[p])) |
                                          (uint16_t(uint8_t(scratch[p+1]))<<8));};
            const float sample=channels==1?lo(at)/32768.f:(lo(at)+lo(at+2))/65536.f;
            dest[(size_t)(i-index)]=sample;
        }
        return true;
    }
};
struct Loaded {
    Reader reader;
    int64_t delay=0;
    double gainL=0,gainR=0;
};
}

bool render(const Manifest& cfg,const std::string& destination,Result& result,
            std::string& error,const std::function<bool(float)>& progress) {
    result={};error.clear();
    if(cfg.voices.empty()||cfg.voices.size()>128) {error="Expected 1..128 voice stems";return false;}
    if(!std::isfinite(cfg.micX)||!std::isfinite(cfg.micY)||!std::isfinite(cfg.room)||
       cfg.room<0 || cfg.room>.8 || !std::isfinite(cfg.outputDb)) {
        error="Invalid microphone, room, or output level";return false;
    }
    std::vector<Loaded> loaded(cfg.voices.size());
    int rate=0;int64_t maxLength=0;
    for(size_t i=0;i<cfg.voices.size();++i) {
        const auto& voice=cfg.voices[i];auto& entry=loaded[i];
        { // Never truncate a source by allowing it to be the output destination.
            std::error_code ec;
            if(std::filesystem::exists(destination,ec) &&
               std::filesystem::equivalent(voice.file,destination,ec) && !ec) {
                error="Output cannot overwrite a source singer stem";return false;
            }
        }
        if(!entry.reader.open(voice.file,error))return false;
        if(!rate)rate=entry.reader.rate;
        if(entry.reader.rate!=rate){error="Stem sample rates differ; resample before mixing";return false;}
        if(!std::isfinite(voice.x)||!std::isfinite(voice.y)||!std::isfinite(voice.gainDb)||
            !std::isfinite(voice.offsetMs)||voice.offsetMs<0 ||voice.offsetMs>60000) {
            error="Invalid position, level or onset offset";return false;
        }
        const double dx=voice.x-cfg.micX,dy=voice.y-cfg.micY;
        const double distance=std::hypot(dx,dy);
        const double position=std::clamp(1.4*std::atan2(dx,std::max(2.,std::abs(dy)))/(pi/2),-.97,.97);
        const double phi=(position+1)*pi/4;
        const double gain=std::pow(10.,(voice.gainDb+cfg.outputDb)/20.)*3./(3.+distance);
        entry.gainL=voice.section=="lead"?gain/std::sqrt(2.):std::cos(phi)*gain;
        entry.gainR=voice.section=="lead"?gain/std::sqrt(2.):std::sin(phi)*gain;
        entry.delay=(int64_t)std::llround(rate*(voice.offsetMs/1000.+distance/343.));
        maxLength=std::max(maxLength,entry.delay+(int64_t)entry.reader.frames);
    }
    const int echo1=cfg.room>0?(int)std::llround(rate*.075):0;
    const int echo2=cfg.room>0?(int)std::llround(rate*.163):0;
    const int64_t nframes=maxLength+(cfg.room>0?(int64_t)std::llround(rate*.24):0);
    if(nframes<=0 || nframes>(int64_t)(std::numeric_limits<uint32_t>::max()-44)/4) {
        error="Output exceeds 4 GB RIFF/WAV limit";return false;
    }
    constexpr int block=2048;
    std::vector<float> a,b,c;
    std::vector<double> left(block),right(block);
    double peak=0;
    // First pass detects peak, second writes with a shared safety scale. Both
    // passes use identical sums; this avoids track-length-dependent clipping.
    for(int pass=0;pass<2;++pass) {
        std::ofstream writer;
        if(pass==1) {
            writer.open(destination,std::ios::binary|std::ios::trunc);
            if(!writer){error="Cannot create WAV output";return false;}
            writer.write("RIFF",4);write32(writer,36+(uint32_t)(nframes*4));
            writer.write("WAVEfmt ",8);write32(writer,16);write16(writer,1);
            write16(writer,2);write32(writer,(uint32_t)rate);write32(writer,(uint32_t)rate*4);
            write16(writer,4);write16(writer,16);
            writer.write("data",4);write32(writer,(uint32_t)(nframes*4));
        }
        for(int64_t start=0;start<nframes;start+=block) {
            if(progress && !progress(.5f*pass+.5f*(float)((double)start/nframes))) {
                error="Stem render cancelled";
                writer.close();if(pass==1)std::filesystem::remove(destination);
                return false;
            }
            const int n=(int)std::min<int64_t>(block,nframes-start);
            std::fill(left.begin(),left.begin()+n,0.);
            std::fill(right.begin(),right.begin()+n,0.);
            for(auto& voice:loaded) {
                const int64_t offset=start-voice.delay;
                if(!voice.reader.readMono(offset,n,a) ||
                   (cfg.room>0 && (!voice.reader.readMono(offset-echo1,n,b)||
                                   !voice.reader.readMono(offset-echo2,n,c)))) {
                    error="Could not read stem samples";
                    writer.close();if(pass==1)std::filesystem::remove(destination);
                    return false;
                }
                for(int k=0;k<n;++k) {
                    left[(size_t)k]+=voice.gainL*a[(size_t)k];
                    right[(size_t)k]+=voice.gainR*a[(size_t)k];
                    if(cfg.room>0) {
                        left[(size_t)k]+=cfg.room*(.20*voice.gainR*b[(size_t)k]+.12*voice.gainL*c[(size_t)k]);
                        right[(size_t)k]+=cfg.room*(.20*voice.gainL*b[(size_t)k]+.14*voice.gainR*c[(size_t)k]);
                    }
                }
            }
            if(pass==0) {
                for(int k=0;k<n;++k)peak=std::max(peak,std::max(std::abs(left[k]),std::abs(right[k])));
            } else {
                const double gain=peak>.975?.975/peak:1.;
                for(int k=0;k<n;++k) {
                    const auto quant=[&](double v){return (int16_t)std::lround(std::clamp(v*gain,-1.,1.)*32767.);};
                    write16(writer,(uint16_t)quant(left[(size_t)k]));
                    write16(writer,(uint16_t)quant(right[(size_t)k]));
                }
            }
        }
        if(pass==1 && !writer) {
            error="Disk write failed";writer.close();std::filesystem::remove(destination);return false;
        }
    }
    result.sampleRate=rate;result.voiceCount=(int)loaded.size();
    result.seconds=nframes/(double)rate;result.peak=peak;result.safetyGain=peak>.975?.975/peak:1.;
    if(progress)progress(1.f);
    return true;
}
}
