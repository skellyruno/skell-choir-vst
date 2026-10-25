#undef NDEBUG   // these tests use assert(); a Release build must still check
#include "../Source/Offline/FullTakeScan.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    const double sr=48000;
    choir::FullTakeScanner scanner;scanner.prepare(sr);
    // Three successive notes with silence. The scan takes the WHOLE take,
    // rather than the four-second dominant-key sample used in live mode.
    const int notes[]={60,64,67};
    for(int n:notes) {
        const float hz=440.f*std::exp2((n-69.f)/12.f);
        for(int i=0;i<(int)sr;++i)
            scanner.push(.2f*std::sin(2*choir::pi*hz*i/(float)sr));
        for(int i=0;i<(int)(sr*.33);++i)scanner.push(0.f);
    }
    auto result=scanner.finish(false);
    std::cout<<"full scan: "<<result.notes.size()<<" notes, root "<<result.root
             <<" duration "<<result.duration<<"s\n";
    for(const auto& note:result.notes)
        std::cout<<note.start<<"-"<<note.end<<" melody="<<note.melody
                 <<" SATB="<<note.satb[0]<<","<<note.satb[1]<<","<<note.satb[2]
                 <<","<<note.satb[3]<<"\n";
    assert(result.duration>3.9);
    assert(result.notes.size()>=3);
    assert(result.notes.front().melody==60);
    const int original=result.notes.front().satb[0];
    assert(choir::transposeNote(result,0,0,2));
    assert(result.notes.front().satb[0]==original+2);
    assert(!choir::transposeNote(result,10000,0,5));
    assert(!choir::transposeNote(result,0,7,5));
    choir::ChoirEffect fx;fx.prepare(sr);
    choir::FXParams p;p.mix=1.f;fx.setParameters(p);
    fx.setScoreVoicing(result.notes[0].melody,result.notes[0].satb);
    float l=0,r=0;
    for(int i=0;i<100000;++i) {
        const float x=.1f*std::sin(2*choir::pi*261.62557f*i/(float)sr);
        fx.render(x,x,l,r);
        assert(std::isfinite(l)&&std::isfinite(r));
    }
    fx.clearScoreVoicing();
    std::cout<<"full-take score tests PASS\n";
}
