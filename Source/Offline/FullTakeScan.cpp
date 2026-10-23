#include "FullTakeScan.h"
#include <algorithm>
#include <cmath>

namespace choir {
namespace {
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
    activeCount=frames=0; histogram.fill(0);notes.clear();
}
void FullTakeScanner::close(int64_t time) {
    if(activeStart<0)return;
    const double start=(double)activeStart/rate,end=(double)time/rate;
    if(end-start>=.16 && activeCount>=3) {
        NoteEvent e; e.start=start;e.end=end;e.melody=activePitch;
        e.confidence=std::min(1.f,(float)activeCount/5.f);
        if(!notes.empty() && notes.back().melody==e.melody && e.start-notes.back().end<.16) {
            notes.back().end=e.end;
            notes.back().confidence=std::max(e.confidence,notes.back().confidence);
        } else notes.push_back(e);
    }
    activeStart=-1;activePitch=-1;activeCount=0;
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
    const bool updated=tracker.push(std::isfinite(mono)?mono:0.f);
    ++sample;
    if(updated && tracker.midi()>=0.f) {
        const int pitch=(int)std::lround(tracker.midi());
        // Compensate the analysis window (~180 ms) conservatively.
        observe(pitch,std::max<int64_t>(0,sample-(int64_t)(.09*rate)));
        lastFrame=sample;
    } else if(activePitch>=0 && sample-lastFrame>(int64_t)(.24*rate)) {
        close(lastFrame);
    }
}
FullTakeScore FullTakeScanner::finish(bool minor) {
    close(std::min(sample,lastFrame+(int64_t)(.07*rate)));
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
