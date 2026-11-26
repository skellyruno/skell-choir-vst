#include "test_support.h"
#include <chrono>
#include <numeric>
int main(int argc, char** argv) {
    ts::Audio audio;
    CHECK(argc==2 && ts::readWav16(argv[1],audio), "usage: bench vocal.wav");
    const int count=std::min<int>(audio.x.size(),3*audio.sr), block=256;
    for (int mode=0;mode<4;++mode) {
        auto p=ts::pluginDefaults();
        if(mode==1) p.lockGrains=true;
        if(mode==2) p.pitchSync=true;
        if(mode==3) {p.pitchSync=true;p.vibratoSpread=.7f;p.phraseBreath=.7f;p.sectionTone={1,0,2,0};p.voiceBlend=.35f;}
        choir::StagedChoir stage; stage.prepare(audio.sr);stage.setParameters(p);
        std::vector<float> l(count),r(count);std::vector<double> times;
        for(int pos=0;pos<count;pos+=block) {
            int n=std::min(block,count-pos);auto a=std::chrono::steady_clock::now();
            stage.process(audio.x.data()+pos,audio.x.data()+pos,l.data()+pos,r.data()+pos,n);
            auto b=std::chrono::steady_clock::now();times.push_back(std::chrono::duration<double,std::milli>(b-a).count());
        }
        double total=std::accumulate(times.begin(),times.end(),0.0),peak=0;
        for(int i=0;i<count;++i) {CHECK(std::isfinite(l[i])&&std::isfinite(r[i]),"nonfinite output");peak=std::max(peak,(double)std::max(std::abs(l[i]),std::abs(r[i])));}
        std::sort(times.begin(),times.end());
        std::printf("mode=%d sr=%d block=%d audio=%g sec cpu=%g%% median=%g ms p95=%g ms max=%g ms peak=%g\n",mode,audio.sr,block,count/(double)audio.sr,total/(1000.*count/audio.sr)*100,times[times.size()/2],times[(size_t)(.95*(times.size()-1))],times.back(),peak);
    }
    auto p=ts::pluginDefaults();p.mix=0;p.levelMatch=false;p.consonants=0;
    choir::StagedChoir stage; stage.prepare(audio.sr);stage.setParameters(p);
    std::vector<float> in(2048),l(2048),r(2048);in[0]=.25f;stage.process(in.data(),in.data(),l.data(),r.data(),(int)in.size());
    auto first=std::find_if(l.begin(),l.end(),[](float x){return std::abs(x)>1e-6f;})-l.begin();
    CHECK(first==choir::StagedChoir::latencySamples,"dry impulse latency %td != reported %d",first,choir::StagedChoir::latencySamples);
    std::printf("dry impulse delay=%td samples (%.3f ms)\n",first,1000.*first/audio.sr);
}
