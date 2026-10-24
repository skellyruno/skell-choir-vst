#include "FullTakeScan.h"
#include <algorithm>
#include <cmath>

namespace choir {
namespace {
#ifndef OC_ON_MS
#define OC_ON_MS 0
#endif
#ifndef OC_END_MS
#define OC_END_MS 0
#endif
constexpr double kOnsetExtraSeconds=OC_ON_MS/1000.0, kEndExtraSeconds=OC_END_MS/1000.0, kEndHoldSeconds=.045, kScoopSeconds=.15;
constexpr int kPendingFrames=3, kMinFrames=8;
int wrap(int x) {return (x%12+12)%12;}
int snap(int note,int root,bool minor) {
    const int major[7]={0,2,4,5,7,9,11};
    const int minorScale[7]={0,2,3,5,7,8,10};
    int best=note, error=100;
    for(int d=-3;d<=3;++d) {
        const int candidate=note+d;
        const int pc=wrap(candidate-root);
        bool in=false;
        for(int degree: minor?minorScale:major) if(pc==degree)in=true;
        if(in&&std::abs(d)<error) {best=candidate;error=std::abs(d);}
    }
    return best;
}
}
void FullTakeScanner::prepare(double sr) {
    rate=std::max(16000.0,sr);tracker.prepare((float)rate);
    sample=lastFrame=0;activeStart=-1;activePitch=-1;
    pendingPitch=-1;pendingCount=0;pendingTime=0;env=peakEnv=0;lastBelow=noteFloor=0;
    activeCount=frames=0; histogram.fill(0);notes.clear();
}
void FullTakeScanner::close(int64_t time) {
    if(activeStart<0)return;
    const double start=(double)activeStart/rate,end=(double)time/rate;
    // Thresholds are in time, not frames: the tracker now reports a pitch every ~8 ms.
    if(end-start>=.16 && activeCount>=kMinFrames) {
        NoteEvent e; e.start=start;e.end=end;e.melody=activePitch;
        e.confidence=std::min(1.f,(float)activeCount*tracker.framePeriodSeconds()/.24f);
        if(!notes.empty() && notes.back().melody==e.melody && e.start-notes.back().end<.16) {
            notes.back().end=e.end;
            notes.back().confidence=std::max(e.confidence,notes.back().confidence);
        } else notes.push_back(e);
    }
    noteFloor=time;
    activeStart=-1;activePitch=-1;activeCount=0;pendingPitch=-1;pendingCount=0;
}
void FullTakeScanner::observe(int midi,int64_t time) {
    if(midi<35||midi>94) {close(time);return;}
    ++histogram[(size_t)wrap(midi)];
    if(activePitch<0) {activePitch=midi;activeStart=time;activeCount=1;return;}
    if(midi==activePitch) {++activeCount;return;}
    close(time);
    activePitch=midi;activeStart=time;activeCount=1;
}
void FullTakeScanner::push(float mono) {
    if(!std::isfinite(mono))mono=0.f;
    const bool updated=tracker.push(mono);
    ++sample;
    {   // 3 ms envelope follower; the 'silence' threshold follows the recent peak (~2 s memory)
        const double k=1.0-std::exp(-1.0/(.003*rate));
        env+=k*(std::fabs((double)mono)-env);
        peakEnv=std::max(env,peakEnv*(1.0-1.0/(2.0*rate)));
        if(env<std::max(.003,.08*peakEnv))lastBelow=sample;
    }
    // A reported pitch describes the middle of the tracker's analysis window; a note is first reported a little
    // after it starts (the window must fill with it), and last reported a little before the window empties.
    const int64_t onsetLag=(int64_t)((tracker.latencySeconds()+kOnsetExtraSeconds)*rate);
    if(updated && tracker.midi()>=0.f) {
        const float m=tracker.midi();
        // Hysteresis: vibrato around a semitone boundary must not chop one note into several.
        int pitch=(activePitch>=0 && std::fabs(m-(float)activePitch)<.65f)?activePitch:(int)std::lround(m);
        lastFrame=sample;
        if(activePitch>=0 && pitch!=activePitch) {
            if(pitch==pendingPitch) ++pendingCount; else {pendingPitch=pitch;pendingCount=1;pendingTime=sample;}
            if(pendingCount>=kPendingFrames) {
                const int64_t when=std::max<int64_t>(0,pendingTime-onsetLag);
                if(when-activeStart<(int64_t)(kScoopSeconds*rate)) {
                    // a note this young changing pitch is the singer settling in (a scoop or a slow lock), not a
                    // new note: correct its pitch and keep where it started
                    activePitch=pitch;
                } else observe(pitch,when);
                pendingPitch=-1;pendingCount=0;
            }
        } else {
            pendingPitch=-1;pendingCount=0;
            int64_t when=std::max<int64_t>(0,sample-onsetLag);
            if(activePitch<0) {
                // a note after silence starts where the energy rose, if that is plausibly this note: after the
                // previous one ended and not more than 200 ms before the pitch-based estimate
                const int64_t rose=lastBelow;
                if(rose>=noteFloor && rose<=when+(int64_t)(.02*rate) && rose>=when-(int64_t)(.2*rate)) when=rose;
            }
            observe(pitch,when);
        }
    } else if(activePitch>=0 && sample-lastFrame>(int64_t)(kEndHoldSeconds*rate)) {
        // Voicing stopped: end the note where it last sounded. (A short consonant gap inside one sung pitch is
        // re-joined by close(): same-pitch notes less than 160 ms apart merge.)
        close(std::max<int64_t>(0,lastFrame-(int64_t)((tracker.latencySeconds()+kEndExtraSeconds)*rate)));
    }
}
FullTakeScore FullTakeScanner::finish(bool minor) {
    close(std::max<int64_t>(0,lastFrame-(int64_t)((tracker.latencySeconds()+kEndExtraSeconds)*rate)));
    FullTakeScore s; s.notes=std::move(notes);s.minor=minor;s.duration=sample/rate;
    int max=0;for(int i=1;i<12;++i)if(histogram[(size_t)i]>histogram[(size_t)max])max=i;
    s.root=max;
    assignSATB(s);
    return s;
}
void assignSATB(FullTakeScore& score) {
    for(auto& e:score.notes) {
        const int m=e.melody;
        const std::array<int,4> target {m+7,m+3+(score.minor?0:1),m-5,m-12};
        const std::array<int,4> min {60,55,47,35}, max {88,79,74,62};
        for(int part=0;part<4;++part) {
            int n=snap(target[(size_t)part],score.root,score.minor);
            while(n<min[(size_t)part])n+=12;
            while(n>max[(size_t)part])n-=12;
            e.satb[(size_t)part]=std::clamp(n,24,100);
        }
    }
}
bool transposeNote(FullTakeScore& score,size_t index,int section,int semitones) {
    if(index>=score.notes.size()||section<0||section>=4)return false;
    auto& n=score.notes[index].satb[(size_t)section];
    n=std::clamp(n+semitones,24,100);
    return true;
}
}
