#undef NDEBUG   // these tests use assert(); a Release build must still check
#include "../Source/ChoirDSP.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

struct Measure { double l2=0, r2=0, peak=0; };
Measure run(choir::Instrument& i, int n) {
    Measure result;
    for (int t=0;t<n;++t) {
        float l=0,r=0; i.render(l,r);
        assert(std::isfinite(l) && std::isfinite(r));
        result.l2 += l*l; result.r2 += r*r;
        result.peak = std::max(result.peak,(double)std::max(std::abs(l),std::abs(r)));
    }
    result.l2 = std::sqrt(result.l2/n); result.r2 = std::sqrt(result.r2/n);
    return result;
}
int main() {
    for (const double sr : { 44100.,48000.,96000. }) {
        choir::Instrument synth; synth.prepare(sr);
        choir::Params p; p.attack=.03f; p.release=.12f; p.wet=.25f;
        synth.setParameters(p);
        auto silence = run(synth,512);
        assert(silence.peak < 1e-8);
        synth.noteOn(1,60,.95f);
        const auto sound=run(synth,(int)(sr*.3));
        assert(sound.peak > .0001 && sound.peak < 1.001);
        assert(synth.activeVoices()>0);
        synth.noteOff(1,60);
        run(synth,(int)(sr*.9));
        assert(synth.activeVoices()==0);
        synth.noteOn(2,74,1); // explicit soprano part
        synth.sustain(2,true); synth.noteOff(2,74);
        run(synth,(int)(sr*.25));
        assert(synth.activeVoices()>0);
        synth.sustain(2,false);
        run(synth,(int)(sr*.4));
        assert(synth.activeVoices()==0);
        synth.pitchWheel(3,10000);
        synth.noteOn(3,65,1);
        run(synth,(int)(sr*.1));
        synth.allNotesOff();
        run(synth,(int)(sr*.25));
        assert(synth.activeVoices()==0);
        std::cout<<"DSP sample rate "<<sr<<" Hz OK; voice RMS "<<sound.l2<<" / "<<sound.r2<<"\n";
    }
    // A solo soprano on each side must be spatially distinct.
    choir::Instrument left,right; left.prepare(48000); right.prepare(48000);
    choir::Params a; a.wet=0; a.attack=.01f; a.micMode=0; a.x[0]=-6.f;
    choir::Params b=a; b.x[0]=6.f;
    left.setParameters(a); right.setParameters(b);
    left.noteOn(2,69,1); right.noteOn(2,69,1);
    const auto l=run(left,16000),r=run(right,16000);
    assert(l.l2>l.r2 && r.r2>r.l2);
    std::cout<<"stereo stage movement OK\nALL TESTS PASSED\n";
}
