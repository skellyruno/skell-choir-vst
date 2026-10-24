// Exploratory benchmark (prints numbers). tests/tracker_tests.cpp is the asserting version.
// Benchmarks choir::PitchTracker on synthetic sung phrases: accuracy, octave errors, dropouts, false voicing, lock time.
#ifndef VIBC
#define VIBC 35
#endif
#include "../../Source/ChoirFX.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include <algorithm>
int main(int argc,char**argv){
  const double sr=48000; int N=argc>1?atoi(argv[1]):1; (void)N;
  std::mt19937 rng(7); std::uniform_real_distribution<double> U(0,1); std::normal_distribution<double> G(0,1);
  struct Cfg{const char* name; double fundamental; double snrDb;};
  Cfg cfgs[]={{"clean",1.0,60},{"noisy 20dB",1.0,20},{"weak fundamental",0.12,40},{"weak+noisy",0.12,20}};
  for(auto&c:cfgs){
    choir::PitchTracker t; t.prepare((float)sr);
    long voiced=0,ok=0,oct=0,drop=0,falsePos=0,silent=0; std::vector<double> errs; std::vector<double> lock; double ph=0;
    const int notes=60; double tcur=0; 
    for(int n=0;n<notes;++n){
      double midi=43+U(rng)*36; double hz=440*std::pow(2,(midi-69)/12); double dur=0.35+U(rng)*0.5; double gap=0.06+U(rng)*0.1;
      int len=(int)(dur*sr), glen=(int)(gap*sr); double vibPh=U(rng)*6.28; bool locked=false; int sinceStart=0;
      for(int i=0;i<len+glen;++i){
        double x=0, truth=-1;
        if(i<len){ double tt=i/sr; double cents=VIBC*std::sin(2*3.14159265358979323846*5.5*tt+vibPh)*std::min(1.0,tt/0.15); double f=hz*std::pow(2,cents/1200); ph+=2*3.14159265358979323846*f/sr;
          double env=std::min({1.0,i/(0.03*sr),(len-i)/(0.05*sr)}); for(int h=1;h<=18;++h) x+=(h==1?c.fundamental:1.0)*std::sin(h*ph)/std::pow(h,0.9); x*=0.12*env; truth=69+12*std::log2(f/440);
        }
        double noise=std::pow(10,-c.snrDb/20)*0.12*G(rng)*0.8; x+=noise; (void)truth;
        t.push((float)x);
        if(i%480==0){ // every 10 ms
          double m=t.midi();
          if(truth>0 && i>0.12*sr){ ++voiced; if(m<0)++drop; else { double e=(m-truth)*100; errs.push_back(std::fabs(e)); if(std::fabs(e)<50)++ok; else if(std::fabs(std::fabs(e)-1200)<150||std::fabs(std::fabs(e)-700)<150)++oct; } }
          if(truth>0 && !locked && m>0 && std::fabs((m-truth)*100)<50){locked=true; lock.push_back(i/sr*1000);} 
          if(truth<0 && i>=len+0.1*sr*0 ){ ++silent; if(m>0&&i>len+(int)(0.08*sr)) ++falsePos; }
        }
      }
    }
    std::sort(errs.begin(),errs.end()); double mean=0; for(double e:errs)mean+=e; mean/=std::max<size_t>(1,errs.size()); double p50=errs[errs.size()/2],p90=errs[errs.size()*9/10],p99=errs[errs.size()*99/100];
    std::printf("   |err| cents: mean %.1f  median %.1f  p90 %.1f  p99 %.1f\n",mean,p50,p90,p99);
    std::sort(lock.begin(),lock.end()); double med=lock.empty()?-1:lock[lock.size()/2];
    std::printf("%-18s within 50c %5.1f%% | octave/fifth errs %4.1f%% | dropouts %4.1f%% | false voicing in gaps %4.1f%% | median lock %.0f ms (%zu/%d notes)\n",
      c.name,100.0*ok/voiced,100.0*oct/voiced,100.0*drop/voiced,100.0*falsePos/std::max(1L,silent),med,lock.size(),notes);
  }
}
