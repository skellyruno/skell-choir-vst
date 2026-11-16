// Regress audio-thread allocation when moving singers/mic or changing room.
#include "../Source/Stage/Stage.h"
#include "test_support.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
namespace {
thread_local bool countAllocations=false;
std::atomic<unsigned> allocations{0};
}
void* operator new(std::size_t n) {
    if(countAllocations) allocations.fetch_add(1,std::memory_order_relaxed);
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* p) noexcept {std::free(p);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n) {
    if(countAllocations) allocations.fetch_add(1,std::memory_order_relaxed);
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* p) noexcept {std::free(p);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
int main()
{
    constexpr int block=128,sections=4;
    auto cfg=oc::makeDefaultStage(48000.,4);
    cfg.engineBlock=block;
    std::vector<oc::Source> sources;
    for(int sec=0;sec<sections;++sec) {
        oc::Source source;source.section=sec;
        for(int member=0;member<4;++member) source.members.push_back(sec*4+member);
        sources.push_back(std::move(source));
    }
    oc::ChoirStage choir(cfg,std::move(sources));
    float input[sections][block],left[block]={},right[block]={};
    const float* ins[sections]={input[0],input[1],input[2],input[3]};
    for(int s=0;s<sections;++s)for(int i=0;i<block;++i)
        input[s][i]=.08f*std::sin((float)i*(.031f+.009f*s));
    choir.process(ins,left,right);
    allocations.store(0,std::memory_order_relaxed);
    countAllocations=true;
    for(int step=0;step<60;++step) {
        choir.setSectionOffset(step%sections,((step%13)-6)*.22,((step%17)-8)*.18);
        choir.setListener(((step%7)-3)*.21,((step%11)-5)*.11);
        auto room=cfg.room;
        room.size=.25+.01*(step%49);
        room.seconds=1.1+.015*(step%37);
        choir.setRoom(room);
        choir.process(ins,left,right);
    }
    countAllocations=false;
    CHECK(allocations.load(std::memory_order_relaxed)==0,"stage allocated %u times while moving the choir",allocations.load());
    for(int i=0;i<block;++i)
        CHECK(std::isfinite(left[i])&&std::isfinite(right[i]),"nonfinite audio at %d",i);
    std::printf("stage DSP: 60 moves, 0 audio-thread allocations, finite output\n");
}
