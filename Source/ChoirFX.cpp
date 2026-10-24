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
    downsample=std::max(1,(int)std::round(sampleRate/16000.f));
    rate=sampleRate/downsample;
    half=(int)(rate*.032f);                          // 32 ms difference window: >2 periods down to 65 Hz
    maxLag=std::min((int)cmnd.size()-2,(int)(rate/65.f));
    minLag=std::max(4,(int)(rate/1000.f));
    hopSamples=std::max(1,(int)(rate*.008f));        // a new estimate every 8 ms
    holdHops=20;                                     // keep the last pitch ~160 ms through consonants and breaths
    reset();
}
void PitchTracker::reset() {
    history.fill(0); recent.fill(0); divide=write=count=hop=missed=recentCount=0;
    accumulator=level=conf=0; currentMidi=-1;
}
bool PitchTracker::push(float sample) {
    accumulator += sample;
    if (++divide < downsample) return false;
    const float averaged = accumulator/downsample;
    divide=0; accumulator=0;
    history[(size_t)write]=averaged;
    write=(write+1)%capacity;
    count=std::min(capacity,count+1);
    if (++hop<hopSamples || count<half+maxLag+8) return false;
    hop=0;
    return analyse();
}
bool PitchTracker::analyse() {
    const int len=half+maxLag;
    for (int i=0;i<len;++i) linear[(size_t)i]=history[(size_t)((write-len+i+capacity)%capacity)];
    float mean=0, energy=0;
    for (int i=len-half;i<len;++i) mean+=linear[(size_t)i];
    mean/=half;
    for (int i=len-half;i<len;++i) { const float v=linear[(size_t)i]-mean; energy+=v*v; }
    level=std::sqrt(energy/half);
    auto lost=[&]{ conf=0; if (++missed>holdHops) { currentMidi=-1; recentCount=0; } return false; };
    if (level<.003f) return lost();
    // difference function and its cumulative-mean normalisation
    cmnd[0]=1.f;
    double running=0;
    for (int tau=1;tau<=maxLag;++tau) {
        double sum=0;
        for (int j=0;j<half;++j) { const float d=linear[(size_t)j]-linear[(size_t)(j+tau)]; sum+=(double)d*d; }
        running+=sum;
        cmnd[(size_t)tau]=running>1.e-12?(float)(sum*tau/running):1.f;
    }
    // first dip under the threshold. The depth is judged on the parabola through the dip, not on the raw lag
    // samples: a period that falls between two lags (22.5 samples at 711 Hz) is otherwise shallower than its
    // integer multiple (45) and the tracker lands an octave low.
    int best=-1;
    for (int tau=minLag+1;tau<maxLag;++tau) {
        const float a=cmnd[(size_t)tau-1], b=cmnd[(size_t)tau], c=cmnd[(size_t)tau+1];
        if (!(b<=a && b<c)) continue;
        const float denom=a-2*b+c;
        const float depth=denom>1.e-6f?b-(a-c)*(a-c)/(8*denom):b;
        if (depth<.15f) { best=tau; break; }
    }
    if (best<0) {
        float m=1.e9f;
        for (int tau=minLag;tau<=maxLag;++tau) if (cmnd[(size_t)tau]<m) { m=cmnd[(size_t)tau]; best=tau; }
        if (m>.35f) return lost();
    }
    float refined=(float)best;
    if (best>minLag && best<maxLag) {
        const float a=cmnd[(size_t)best-1], b=cmnd[(size_t)best], c=cmnd[(size_t)best+1];
        const float denom=a-2*b+c;
        if (std::abs(denom)>1.e-6f) refined+=bound(.5f*(a-c)/denom,-.5f,.5f);
    }
    conf=bound(1.f-cmnd[(size_t)best],0.f,1.f);
    const float midiNote=69.f+12.f*std::log2((rate/refined)/440.f);
    if (!std::isfinite(midiNote)) return lost();
    // 3-frame median removes single-frame octave glitches without delaying real note changes by more than one hop
    recent[(size_t)(recentCount%3)]=midiNote; ++recentCount;
    float m3=midiNote;
    if (recentCount>=3) {
        std::array<float,3> s=recent; std::sort(s.begin(),s.end()); m3=s[1];
    }
    if (currentMidi<0 || std::abs(currentMidi-m3)>1.f) currentMidi=m3;   // a new note: lock at once
    else currentMidi=.5f*currentMidi+.5f*m3;                              // vibrato and drift: light smoothing
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

void GrainVoice::processPair(const std::vector<float>& original,
                             const std::vector<float>& excitation,int write,float window,
                             float minDelay,float& raw,float& exc) {
    ratio+=.0004f*(targetRatio-ratio);
    phase=frac(phase+(ratio-1.f)/window);
    const float other=frac(phase+.5f);
    const float a=std::sin(pi*phase),b=std::sin(pi*other);
    const float p1=minDelay+(1-phase)*window,p2=minDelay+(1-other)*window;
    raw=a*a*read(original,write,p1)+b*b*read(original,write,p2);
    exc=a*a*read(excitation,write,p1)+b*b*read(excitation,write,p2);
}

void ChoirEffect::updateLPC() {
    // Regularized 10th-order LPC source/filter analysis. The predictor models
    // the input's vocal-tract spectral envelope, then the granular engine
    // shifts its excitation; the same envelope is reapplied at the OUTPUT pitch.
    // This is conventional LPC formant compensation, not a neural voice model.
    if(inputHistory.size()<1024 || inputEnv<.002f)return;
    constexpr int frame=720;
    float corr[lpcOrder+1] {};
    const int size=(int)inputHistory.size();
    for(int lag=0;lag<=lpcOrder;++lag) {
        double sum=0;
        for(int n=0;n<frame;++n) {
            const float a=inputHistory[(size_t)((write-1-n+size)%size)];
            const float b=inputHistory[(size_t)((write-1-n-lag+size)%size)];
            sum+=(double)a*b;
        }
        corr[lag]=(float)(sum/frame);
    }
    corr[0]=corr[0]*1.035f+1.e-7f;
    float a[lpcOrder+1] {};
    double error=corr[0];
    if(error<1.e-8)return;
    for(int order=1;order<=lpcOrder;++order) {
        double k=corr[order];
        for(int j=1;j<order;++j)k+=a[j]*corr[order-j];
        k=-k/std::max(1.e-9,error);
        k=bound((float)k,-.89f,.89f);
        float next[lpcOrder+1] {};
        for(int j=1;j<order;++j)next[j]=a[j]+(float)k*a[order-j];
        next[order]=(float)k;
        for(int j=1;j<=order;++j)a[j]=next[j];
        error*=1.-k*k;
        if(error<=1.e-10)break;
    }
    for(int k=0;k<lpcOrder;++k) {
        // Dampen resonant poles slightly for reliable audio-thread behavior.
        const float target=a[k+1]*std::pow(.968f,(float)(k+1));
        lpc[(size_t)k]=.77f*lpc[(size_t)k]+.23f*target;
    }
}

float ChoirEffect::nextVariation(uint32_t& seed) {
    // An LCG is sufficient for slow independent modulation, not cryptography.
    // Using only unsigned arithmetic makes this repeatable and allocation-free.
    seed=seed*1664525u+1013904223u;
    return float(seed>>8)*(1.f/8388608.f)-1.f;
}

void ChoirEffect::prepare(double sampleRate) {
    sr=(float)std::max(16000.,sampleRate);
    window=.073f*sr;
    minDelay=.009f*sr;
    inputHistory.assign((size_t)(sr*.19f)+32,0.f);
    excitationHistory.assign(inputHistory.size(),0.f);
    room.prepare(sr);
    tracker.prepare(sr);
    reset();
}
void ChoirEffect::reset() {
    std::fill(inputHistory.begin(),inputHistory.end(),0.f);
    std::fill(excitationHistory.begin(),excitationHistory.end(),0.f);
    lpc.fill(0);for(auto& state:formantState)state.fill(0);
    formantWrite=lpcCounter=0;
    for (int i=0;i<(int)layers.size();++i) layers[(size_t)i].reset(i*.14159265f);
    write=lfoStep=0; motionSeconds=0; inputEnv=outputEnv=wetNow=0; lowPass.fill(0);
    singerLP.fill(0); singerHP.fill(0);leadFilterL=leadFilterR=0;
    singerPitchWalk.fill(0);singerLevelWalk.fill(0);singerTimeWalk.fill(0);
    singerLevelGain.fill(1.f);singerDelaySeconds.fill(0.f);
    for(int i=0;i<sections*singers;++i)
        singerSeeds[(size_t)i]=0xB5297A4Du + 1013904223u*(uint32_t)(i+1);
    tracker.reset(); room.reset();
    if (status==1) status=0;
    updateTargetRatios();
}
void ChoirEffect::setSavedScanRoot(int root,bool ready) {
    rootPC=positiveModulo(root,12);
    status=ready?2:0;
    updateTargetRatios();
}
void ChoirEffect::setScoreVoicing(int melody,const std::array<int,4>& notes) {
    scoreVoicing=true;scoreMelody=melody;scorePitches=notes;updateTargetRatios();
}
void ChoirEffect::clearScoreVoicing() {scoreVoicing=false;updateTargetRatios();}
void ChoirEffect::beginScan() {
    // Discard analysis history from before the user pressed Scan.
    tracker.reset();
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
        const float side=bound(dx/std::max(2.f,distance),-1.f,1.f);
        const float arrayWidth[6]={.85f,1.3f,1.05f,1.f,1.15f,1.65f};
        const float spread=bound(side*arrayWidth[std::max(0,std::min(5,p.micMode))]*(.85f+.15f*p.width),-.98f,.98f);
        const float attenuation=bound(3.f/(2.f+distance),.2f,.95f)*p.gain[(size_t)i];
        for(int singer=0;singer<singers;++singer) {
            const int k=i*singers+singer;
            const float deviation=(singer-1.5f)*(.025f+.11f*p.humanize);
            const float phi=(bound(spread+deviation,-.98f,.98f)+1.f)*.25f*pi;
            // The same section reaches multiple different positions/capsules.
            singerL[(size_t)k]=std::cos(phi)*attenuation/singers;
            singerR[(size_t)k]=std::sin(phi)*attenuation/singers;
            const float timbre=(singer-1.5f)*.11f*p.formant;
            const float hz=bound((i<2?5100.f:3350.f)*(1.f-.022f*distance)*(1.f+timbre),750.f,9000.f);
            singerTone[(size_t)k]=1.f-std::exp(-2*pi*hz/sr);
        }
    }
}
void ChoirEffect::updateTone(const std::array<float,sections>& distance) {
    for (int i=0;i<sections;++i)
        for(int singer=0;singer<singers;++singer) {
            const int k=i*singers+singer;
            const float timbre=(singer-1.5f)*.11f*p.formant;
            const float hz=bound((i<2?5100.f:3350.f)*(1.f-.022f*distance[(size_t)i])*(1.f+timbre),750.f,9000.f);
            singerTone[(size_t)k]=1.f-std::exp(-2*pi*hz/sr);
        }
}
void ChoirEffect::setVoiceParameters(const FXParams& params) {
    p=params;
    updateTone({0.f,0.f,0.f,0.f});
    updateTargetRatios();
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
        if (scoreVoicing) {
            interval=scorePitches[(size_t)i]-scoreMelody;
        } else if (p.mode==1 && status==2 && harmony!=2 && current>=0) {
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
            const int k=i*singers+singer;
            // Around +/- 14 cents with individual slow independent drift.
            const float detune=(singer-1.5f)*.095f*p.ensemble*p.humanize;
            singerBaseSemis[(size_t)k]=bound((float)interval+detune,-24.f,24.f);
        }
    }
}
void ChoirEffect::processVoices(float mono,float* voicesOut) {
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
    if(pitchUpdated && p.mode==1 && status==2 && !scoreVoicing) updateTargetRatios();
    if (++lfoStep>=256) {
        lfoStep=0;
        for(int i=0;i<sections*singers;++i) {
            // Independently wandering pitch, level and timing rather than a
            // periodic oscillator shared by a fake choir. These are controlled
            // by HUMANIZE and zero out when humanization is disabled.
            auto& seed=singerSeeds[(size_t)i];
            const float a=.022f; // slow ~0.6 s correlation at 48 kHz
            singerPitchWalk[(size_t)i]+=
                a*(nextVariation(seed)-singerPitchWalk[(size_t)i]);
            singerLevelWalk[(size_t)i]+=
                a*(nextVariation(seed)-singerLevelWalk[(size_t)i]);
            singerTimeWalk[(size_t)i]+=
                a*(nextVariation(seed)-singerTimeWalk[(size_t)i]);
            const float depth=bound(p.humanize,0.f,1.f);
            const float cents=depth*0.55f*singerPitchWalk[(size_t)i];
            singerLevelGain[(size_t)i]=1.f+depth*.30f*singerLevelWalk[(size_t)i];
            singerDelaySeconds[(size_t)i]=depth*.003f*singerTimeWalk[(size_t)i];
            layers[(size_t)i].targetRatio=
                std::exp2(bound(singerBaseSemis[(size_t)i]+cents,-24.f,24.f)/12.f);
        }
    }
    motionSeconds+=1.f/sr;
    // Voice gate, low level noise exclusion. Do not gate room tails.
    inputEnv+=.0012f*(std::abs(mono)-inputEnv);
    const float threshold=.0010f+.013f*(1.f-p.sensitivity);
    const float goal=inputEnv>threshold?1.f:0.f;
    outputEnv+=(goal>outputEnv?.004f:.0006f)*(goal-outputEnv);
    const float pitchedInput=mono*outputEnv;
    if(++lpcCounter>=2048) {lpcCounter=0;updateLPC();}
    float excitation=pitchedInput;
    const int historySize=(int)inputHistory.size();
    for(int j=0;j<lpcOrder;++j) {
        const int idx=(write-j-1+historySize)%historySize;
        excitation+=lpc[(size_t)j]*inputHistory[(size_t)idx];
    }
    excitation=bound(excitation,-2.f,2.f);
    inputHistory[(size_t)write]=pitchedInput;
    excitationHistory[(size_t)write]=excitation;
    for(int i=0;i<sections;++i) {
        for(int j=0;j<singers;++j) {
            const int k=i*singers+j;
            // Singer-specific phasing/delay and a mild, fixed spectral-envelope
            // distinction. This is an approximation, not neural identity cloning.
            const float extra=(3.f*j+2.f*i)*.0015f*sr*p.humanize;
            float x=0,exc=0;
            layers[(size_t)k].processPair(inputHistory,excitationHistory,write,
                                          window,minDelay+extra+singerDelaySeconds[(size_t)k]*sr,x,exc);
            float formed=exc;
            for(int j=0;j<lpcOrder;++j) {
                const int idx=(formantWrite-j-1+lpcOrder)%lpcOrder;
                formed-=lpc[(size_t)j]*formantState[(size_t)k][(size_t)idx];
            }
            formed=bound(formed,-1.6f,1.6f);
            formantState[(size_t)k][(size_t)formantWrite]=formed;
            // Blend the LPC-resynthesized, envelope-retaining component with
            // the unmodified harmonizer. Higher CHARACTER means more LPC.
            x=(1.f-.72f*p.formant)*x+(.72f*p.formant)*formed;
            singerLP[(size_t)k]+=singerTone[(size_t)k]*(x-singerLP[(size_t)k]);
            const float brightness=(j%2==0?.30f:.13f)*(1.f-.48f*p.formant);
            const float voice=(1.f-brightness)*singerLP[(size_t)k]+brightness*x;
            voicesOut[k]=voice*singerLevelGain[(size_t)k];
        }
    }
    formantWrite=(formantWrite+1)%lpcOrder;
    if (++write >= (int)inputHistory.size()) write=0;
}
void ChoirEffect::render(float inL,float inR,float& outL,float& outR) {
    const float mono=.5f*(inL+inR);
    std::array<float,numVoices> voice {};
    processVoices(mono,voice.data());
    float choirL=0,choirR=0;
    for(int k=0;k<numVoices;++k) {
        choirL+=voice[(size_t)k]*singerL[(size_t)k];
        choirR+=voice[(size_t)k]*singerR[(size_t)k];
    }
    choirL*=.65f; choirR*=.65f;
    room.process(choirL,choirR,roomL,roomR);
    const float processedL=choirL+roomL*p.wet;
    const float processedR=choirR+roomR*p.wet;
    wetNow+=.002f*((p.choirOn?p.mix:0.f)-wetNow);
    const float dry=1.f-wetNow;
    // Output remains dry when mix=0, useful for host automation and bypass.
    // Dragging the white lead alters where the direct voice sits relative to
    // the microphone. Home (0, 0.4 m) remains neutral for null-ish auditioning.
    const float leadSide=bound(p.leadX/12.f,-.8f,.8f);
    const float leftGain=1.f-.23f*std::max(0.f,leadSide);
    const float rightGain=1.f+.23f*std::min(0.f,leadSide);
    const float leadDistance=std::max(0.f,p.leadY-.4f);
    const float fade=1.f/(1.f+leadDistance*.045f);
    outL=std::tanh(p.master*(dry*inL*p.leadGain*fade*leftGain + wetNow*processedL));
    outR=std::tanh(p.master*(dry*inR*p.leadGain*fade*rightGain + wetNow*processedR));
}
} // namespace choir
