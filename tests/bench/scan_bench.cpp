// Exploratory benchmark (prints numbers). tests/tracker_tests.cpp is the asserting version.
// Scores FullTakeScanner against ground truth on synthetic sung takes (vibrato, scoops, consonant gaps, noise).
#ifndef SEED
#define SEED 23
#endif
#ifndef SCOOP
#define SCOOP 1
#endif
#include "../../Source/Offline/FullTakeScan.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include <algorithm>
struct Truth{double a,b;int midi;};
int main(){
  const double sr=48000; std::mt19937 rng(SEED); std::uniform_real_distribution<double> U(0,1); std::normal_distribution<double> G(0,1);
  double sumOn=0,sumOff=0; int matched=0,spurious=0,missed=0,total=0,wrongPitch=0; std::vector<double> onErr,offErr;
  for(int take=0;take<8;++take){
    double t=0.3; double ph=0; std::vector<Truth> truth; std::vector<float> x; double lastMidi=60;
    choir::FullTakeScanner sc; sc.prepare(sr);
    int notes=14;
    std::vector<std::pair<double,int>> plan; // duration, midi
    double cur=60; for(int n=0;n<notes;++n){ int step=(int)std::round(G(rng)*2.2); cur=std::min(76.0,std::max(48.0,cur+step)); if(step==0)cur+=1; plan.push_back({0.25+U(rng)*0.6,(int)cur}); }
    auto pushSamples=[&](int n,double f,double env,bool voiced){ for(int i=0;i<n;++i){ double v=0; if(voiced){ ph+=2*3.14159265358979323846*f/sr; for(int h=1;h<=14;++h) v+=std::sin(h*ph)/std::pow(h,0.95); v*=0.12*env; } v+=0.004*G(rng); sc.push((float)v); } };
    pushSamples((int)(0.3*sr),0,0,false);
    double clock=0.3;
    for(auto&p:plan){
      double dur=p.first; int midi=p.second; double hz=440*std::pow(2,(midi-69)/12.0);
      int len=(int)(dur*sr); double vp=U(rng)*6.28;
      truth.push_back({clock,clock+dur,midi});
      for(int i=0;i<len;i++){ double tt=i/sr; double scoop=SCOOP*(tt<0.08?-(0.08-tt)/0.08*120:0); double cents=scoop+30*std::sin(2*3.14159265358979323846*5.5*tt+vp)*std::min(1.0,tt/0.2);
        double f=hz*std::pow(2,cents/1200); double env=std::min({1.0,i/(0.025*sr),(len-i)/(0.04*sr)}); pushSamples(1,f,env,true);} 
      double gap=0.05+U(rng)*0.12; pushSamples((int)(gap*sr),0,0,false); clock+=dur+gap;
    }
    pushSamples((int)(0.5*sr),0,0,false);
    auto score=sc.finish(false);
    std::vector<bool> used(score.notes.size(),false);
    for(auto&tr:truth){ ++total; int best=-1; double bo=0; for(size_t i=0;i<score.notes.size();++i){ if(used[i])continue; double ov=std::min(tr.b,score.notes[i].end)-std::max(tr.a,score.notes[i].start); if(ov>0.5*(tr.b-tr.a)&&ov>bo){bo=ov;best=(int)i;} }
      if(best<0){++missed;continue;} used[best]=true; if(score.notes[best].melody!=tr.midi){++wrongPitch;} else {++matched; onErr.push_back(std::fabs(score.notes[best].start-tr.a)*1000); offErr.push_back(std::fabs(score.notes[best].end-tr.b)*1000);} }
    for(size_t i=0;i<score.notes.size();++i) if(!used[i]) ++spurious;
  }
  auto med=[](std::vector<double> v){std::sort(v.begin(),v.end()); return v.empty()?-1.0:v[v.size()/2];};
  auto mean=[](std::vector<double>&v){double s=0;for(double d:v)s+=d;return v.empty()?-1:s/v.size();};
  std::printf("truth notes %d | correct pitch+time %d (%.0f%%) | wrong pitch %d | missed %d | spurious extra notes %d\n",total,matched,100.0*matched/total,wrongPitch,missed,spurious);
  std::printf("onset error ms: mean %.0f median %.0f | offset error ms: mean %.0f median %.0f\n",mean(onErr),med(onErr),mean(offErr),med(offErr));
}
