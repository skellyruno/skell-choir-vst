#include "ChoirFX.h"
#include <algorithm>
#include <cmath>

namespace choir {
namespace {
float bound(float v,float low,float high) { return std::max(low,std::min(high,v)); }
float frac(float x) { return x-std::floor(x); }
int positiveModulo(int n,int d) { return (n%d+d)%d; }
}

void PitchTracker::prepare(float sampleRate) {
    downsample=std::max(1,(int)std::round(sampleRate/4000.f));
    rate=sampleRate/downsample;
    reset();
}
void PitchTracker::reset() {
    history.fill(0); divide=write=count=hop=missed=0;
    accumulator=level=0; currentMidi=-1;
}
bool PitchTracker::push(float sample) {
    accumulator += sample;
    if (++divide < downsample) return false;
    const float averaged = accumulator/downsample;
    divide=0; accumulator=0;
    history[(size_t)write]=averaged;
    write=(write+1)%capacity;
    count=std::min(capacity,count+1);
    if (++hop<256 || count<760) return false;
    hop=0;
    return analyse();
}
bool PitchTracker::analyse() {
    // Centered normalized autocorrelation; ~60-800 Hz pitch range.
    constexpr int windowSamples=720;
    const int minLag=std::max(4,(int)(rate/750.f));
    const int maxLag=std::min(88,(int)(rate/65.f));
    float average=0, energy=0;
    for (int n=0;n<windowSamples;++n) average+=history[(size_t)((write-1-n+capacity)%capacity)];
    average/=windowSamples;
    for (int n=0;n<windowSamples;++n) {
        const float v=history[(size_t)((write-1-n+capacity)%capacity)]-average;
        energy+=v*v;
    }
    level=std::sqrt(energy/windowSamples);
    if (level<.003f) {
        if (++missed>4) currentMidi=-1;
        return false;
    }
    float best=-1;
    int bestLag=0;
    float correlation[89] {};
    for (int lag=minLag;lag<=maxLag;++lag) {
        float sum=0,e1=0,e2=0;
        for (int n=0;n<windowSamples-lag;++n) {
            const float a=history[(size_t)((write-1-n+capacity)%capacity)]-average;
            const float b=history[(size_t)((write-1-n-lag+capacity)%capacity)]-average;
            sum+=a*b; e1+=a*a; e2+=b*b;
        }
        const float v=sum/std::sqrt(std::max(1.e-12f,e1*e2));
        correlation[lag]=v;
        if (v>best) { best=v; bestLag=lag; }
    }
    if (best<.68f || bestLag<=0) {
        if (++missed>4) currentMidi=-1;
        return false;
    }
    // Prefer a short-lag peak near the best score to avoid jumping an octave
    // on harmonic-rich singers. Then parabolic interpolation for sub-semitones.
    for (int lag=minLag+1;lag<bestLag;++lag) {
        if (correlation[lag]>=best*.96f && correlation[lag]>=correlation[lag-1]
            && correlation[lag]>=correlation[lag+1]) { bestLag=lag; break; }
    }
    float refined=(float)bestLag;
    if (bestLag>minLag && bestLag<maxLag) {
        const float a=correlation[bestLag-1], b=correlation[bestLag], c=correlation[bestLag+1];
        const float denom=a-2*b+c;
        if (std::abs(denom)>1.e-5f) refined+=bound(.5f*(a-c)/denom,-.5f,.5f);
    }
    const float midiNote=69.f+12.f*std::log2((rate/refined)/440.f);
    if (!std::isfinite(midiNote)) return false;
    if (currentMidi<0 || std::abs(currentMidi-midiNote)>9) currentMidi=midiNote;
    else currentMidi=.55f*currentMidi+.45f*midiNote;
    missed=0;
    return true;
}

void GrainVoice::reset(float offset) { phase=frac(offset); ratio=targetRatio=1.f; }
float GrainVoice::read(const std::vector<float>& history,int write,float samplesBack) {
    const int size=(int)history.size();
    float pos=write-samplesBack;
    while(pos<0) pos+=size;
    while(pos>=size) pos-=size;
    const int i=(int)pos, j=(i+1)%size;
    const float f=pos-i;
    return history[(size_t)i]*(1-f)+history[(size_t)j]*f;
}
float GrainVoice::process(const std::vector<float>& history,int write,float window,float minDelay) {
    ratio+=.0004f*(targetRatio-ratio);  // smooth tracked note changes
    phase=frac(phase+(ratio-1.f)/window);
    const float other=frac(phase+.5f);
    const float a=std::sin(pi*phase), b=std::sin(pi*other);
    const float s1=read(history,write,minDelay+(1-phase)*window);
    const float s2=read(history,write,minDelay+(1-other)*window);
    return a*a*s1+b*b*s2;
}

void ChoirEffect::prepare(double sampleRate) {
    sr=(float)std::max(16000.,sampleRate);
    window=.073f*sr;
    minDelay=.009f*sr;
    inputHistory.assign((size_t)(sr*.19f)+32,0.f);
    room.prepare(sr);
    tracker.prepare(sr);
    reset();
}
void ChoirEffect::reset() {
    std::fill(inputHistory.begin(),inputHistory.end(),0.f);
    for (int i=0;i<(int)layers.size();++i) layers[(size_t)i].reset(i*.14159265f);
    write=0; inputEnv=outputEnv=wetNow=0; lowPass.fill(0);
    tracker.reset(); room.reset();
    if (status==1) status=0;
    updateTargetRatios();
}
void ChoirEffect::setSavedScanRoot(int root,bool ready) {
    rootPC=positiveModulo(root,12);
    status=ready?2:0;
    updateTargetRatios();
}
void ChoirEffect::beginScan() {
    pitchHistogram.fill(0);
    scanSamples=matchedFrames=0; progress=0; status=1;
}
void ChoirEffect::setParameters(const FXParams& params) {
    p=params;
    room.configure(p);
    updateStage();
    updateTargetRatios();
}
void ChoirEffect::updateStage() {
    for (int i=0;i<sections;++i) {
        const float dx=p.x[(size_t)i]-p.micX;
        const float dy=p.y[(size_t)i]-p.micY;
        const float distance=std::sqrt(dx*dx+dy*dy+1.8f*1.8f);
        // Microphone choice affects perceived width/directivity, not just pan.
        const float side=bound(dx/std::max(2.0f,distance),-1.f,1.f);
        const float widthScale[6]={.85f,1.3f,1.05f,1.f,1.15f,1.65f};
        const float spread=bound(side*widthScale[std::max(0,std::min(5,p.micMode))]*(.85f+.15f*p.width),-.98f,.98f);
        const float pan=(spread+1.f)*.25f*pi;
        const float attenuation=bound(3.f/(2.f+distance),.2f,.95f)*p.gain[(size_t)i];
        panL[(size_t)i]=std::cos(pan)*attenuation;
        panR[(size_t)i]=std::sin(pan)*attenuation;
        tone[(size_t)i]=1.f-std::exp(-2*pi*bound((i<2?5500.f:3500.f)*(1.f-.025f*distance),800.f,8500.f)/sr);
    }
}
void ChoirEffect::updateTargetRatios() {
    // Relative voicings: S fifth up, A third up, T fourth down, B octave down.
    // In Scan mode, scale snapping follows the detected dominant pitch-class;
    // this is NOT a reliable full musical key/chord transcription.
    const int semis[3][4]={{7,4,-5,-12},{7,3,-5,-12},{0,0,0,0}};
    constexpr int majorMask=(1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11);
    constexpr int minorMask=(1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10);
    const int harmony=std::max(0,std::min(2,p.harmony));
    const int keyMask=harmony==1?minorMask:majorMask;
    const float current=tracker.midi();
    const int melody=current>=0?(int)std::round(current):60;
    for (int i=0;i<sections;++i) {
        int interval=semis[harmony][i];
        if (p.mode==1 && status==2 && harmony!=2 && current>=0) {
            const int wanted=melody+interval;
            int best=100, error=99;
            for(int n=wanted-3;n<=wanted+3;++n) {
                const int relative=positiveModulo(n-rootPC,12);
                if ((keyMask & (1<<relative))==0) continue;
                const int delta=std::abs(n-wanted);
                if(delta<error) { best=n; error=delta; }
            }
            if(best!=100) interval=best-melody;
        }
        for(int singer=0;singer<singers;++singer) {
            const float detune=(singer==0?-.09f:.09f)*p.ensemble;
            const float shifted=bound((float)interval+detune,-18.f,12.f);
            layers[(size_t)(i*singers+singer)].targetRatio=std::exp2(shifted/12.f);
        }
    }
}
void ChoirEffect::render(float inL,float inR,float& outL,float& outR) {
    const float mono=.5f*(inL+inR);
    const bool pitchUpdated=tracker.push(mono);
    if (status==1) {
        if (pitchUpdated && tracker.midi()>=0) {
            ++pitchHistogram[(size_t)positiveModulo((int)std::round(tracker.midi()),12)];
            ++matchedFrames;
        }
        ++scanSamples;
        progress=bound(scanSamples/(4.f*sr),0.f,1.f);
        if (progress>=1.f) {
            if (matchedFrames>=4) {
                int best=0;
                for(int i=1;i<12;++i) if(pitchHistogram[(size_t)i]>pitchHistogram[(size_t)best])best=i;
                rootPC=best; status=2;
            } else status=3;
            updateTargetRatios();
        }
    }
    if(pitchUpdated && p.mode==1 && status==2) updateTargetRatios();
    // Voice gate, low level noise exclusion. Do not gate room tails.
    inputEnv+=.0012f*(std::abs(mono)-inputEnv);
    const float threshold=.0010f+.013f*(1.f-p.sensitivity);
    const float goal=inputEnv>threshold?1.f:0.f;
    outputEnv+=(goal>outputEnv?.004f:.0006f)*(goal-outputEnv);
    const float pitchedInput=mono*outputEnv;
    if(!inputHistory.empty())inputHistory[(size_t)write]=pitchedInput;
    float choirL=0,choirR=0;
    for(int i=0;i<sections;++i) {
        float voice=0;
        for(int j=0;j<singers;++j)
            voice+=layers[(size_t)(i*singers+j)].process(inputHistory,write,window,minDelay);
        voice*=.5f;
        // Section-specific high-frequency rolloff evokes choir distance / voice body.
        lowPass[(size_t)i]+=tone[(size_t)i]*(voice-lowPass[(size_t)i]);
        voice=.72f*lowPass[(size_t)i]+.28f*voice;
        choirL+=voice*panL[(size_t)i];
        choirR+=voice*panR[(size_t)i];
    }
    if (++write >= (int)inputHistory.size()) write=0;
    choirL*=.65f; choirR*=.65f;
    room.process(choirL,choirR,roomL,roomR);
    const float processedL=choirL+roomL*p.wet;
    const float processedR=choirR+roomR*p.wet;
    wetNow+=.002f*((p.choirOn?p.mix:0.f)-wetNow);
    const float dry=1.f-wetNow;
    // Output remains dry when mix=0, useful for host automation and bypass.
    outL=std::tanh(p.master*(dry*inL+wetNow*processedL));
    outR=std::tanh(p.master*(dry*inR+wetNow*processedR));
}
} // namespace choir
