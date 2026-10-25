#undef NDEBUG   // these tests use assert(); a Release build must still check
#include "../Source/ChoirFX.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <algorithm>

int main() {
    for(const double sr:{44100.,48000.,96000.}) {
        choir::ChoirEffect fx;
        choir::FXParams p; p.mix=1.f;p.wet=.25f;p.master=1.f;
        fx.prepare(sr);fx.setParameters(p);
        double rms=0, max=0;
        for(int i=0;i<(int)(sr*.65);++i) {
            float l=0,r=0;
            fx.render(0,0,l,r);
            assert(std::isfinite(l)&&std::isfinite(r));
            max=std::max(max,(double)std::max(std::abs(l),std::abs(r)));
        }
        assert(max<1.e-8);
        fx.beginScan();
        for(int i=0;i<(int)(sr*4.3);++i) {
            const float x=.18f*std::sin(2.f*choir::pi*261.625565f*i/(float)sr);
            float l,r;
            fx.render(x,x,l,r);
            assert(std::isfinite(l)&&std::isfinite(r));
            rms+=l*l+r*r;
            max=std::max(max,(double)std::max(std::abs(l),std::abs(r)));
        }
        rms=std::sqrt(rms/(2*sr*4.3));
        std::cout << "sample rate " << sr << " RMS " << rms
                  << " peak " << max << " scan " << fx.scanStatus()
                  << " root " << fx.scanRoot() << " midi " << fx.detectedMidi() << '\n';
        assert(fx.scanStatus()==2);
        assert(fx.scanRoot()==0); // input was C4
        assert(rms>.002 && max<1.001);
        // The unchanged original is audible with 0% choir mix.
        fx.reset();p.mix=0.f;p.master=1.f;fx.setParameters(p);
        float l=0,r=0;
        for(int i=0;i<(int)(sr*.2);++i)fx.render(.10f,.10f,l,r);
        assert(std::abs(l-.099668f)<.003);
        fx.beginScan();
        for(int i=0;i<(int)(sr*4.01);++i)fx.render(0,0,l,r);
        assert(fx.scanStatus()==3); // silence does not invent a detected key
    }
    // Stage movement is reflected in the rendered stereo energy.
    choir::ChoirEffect lfx,rfx;
    lfx.prepare(48000);rfx.prepare(48000);
    choir::FXParams a; a.mix=1;a.wet=0;a.harmony=2;
    a.gain={1,0,0,0};a.x[0]=-8.f;
    choir::FXParams b=a;b.x[0]=8.f;
    lfx.setParameters(a);rfx.setParameters(b);
    double ll=0,lr=0,rl=0,rr=0;
    for(int i=0;i<28000;++i) {
        const float x=.2f*std::sin(2*choir::pi*220.f*i/48000.f);
        float L,R,U,V;
        lfx.render(x,x,L,R);rfx.render(x,x,U,V);
        ll+=L*L;lr+=R*R;rl+=U*U;rr+=V*V;
    }
    std::cout << "left/right ratios " << ll/std::max(1.e-9,lr) << ", " << rr/std::max(1.e-9,rl) << '\n';
    assert(ll>lr && rr>rl);
    std::cout << "ALL FX TESTS PASSED\n";
}
