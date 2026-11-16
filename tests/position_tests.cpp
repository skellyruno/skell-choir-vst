// Independent choir formations: position-only edits never touch room, lead, mic, gain or automation.
#include "../Source/PositionPresets.h"
#include "../Source/StylePresets.h"
#include "test_support.h"
#include <cmath>
#include <set>
using namespace choir;

int main() {
    std::set<std::string> names;
    for (int i=0;i<kNumChoirPositions;++i) {
        const auto& pos=kChoirPositions[i];
        CHECK(names.insert(pos.name).second,"duplicate choir layout: %s",pos.name);
        FXParams p=ts::pluginDefaults();
        p.x={-8.f,-5.f,5.f,8.f}; p.micX=3.5f; p.micY=-3.f; p.leadX=1.2f; p.leadY=2.3f;
        p.roomSeconds=8.f; p.roomSize=.9f; p.hall=.4f;
        const FXParams before=p;
        applyChoirPositionOnly(pos,p);
        for(int k=0;k<4;++k) {
            CHECK(pos.x[k]>=kRangeX.lo && pos.x[k]<=kRangeX.hi && pos.y[k]>=kRangeY.lo && pos.y[k]<=kRangeY.hi,"layout %s has out-of-range coords",pos.name);
            CHECK(p.x[(size_t)k]==pos.x[k] && p.y[(size_t)k]==pos.y[k],"layout %s did not move section %d",pos.name,k);
            CHECK(p.gain[(size_t)k]==before.gain[(size_t)k],"layout %s changed a gain",pos.name);
        }
        CHECK(p.micX==before.micX && p.micY==before.micY && p.leadX==before.leadX && p.leadY==before.leadY && p.leadGain==before.leadGain,"layout %s moved listener or lead",pos.name);
        CHECK(p.roomSize==before.roomSize && p.roomSeconds==before.roomSeconds && p.hall==before.hall && p.mix==before.mix && p.width==before.width,"layout %s changed room or sound",pos.name);
    }
    for(int i=0;i<kNumStagePresets;++i) {
        FXParams p=ts::pluginDefaults();p.micX=-2.5f;p.leadY=.5f;
        const FXParams before=p;
        applyRoomLayoutOnly(kStagePresets[i],p);
        for(int k=0;k<4;++k)
            CHECK(p.x[(size_t)k]==kStagePresets[i].x[k] && p.y[(size_t)k]==kStagePresets[i].y[k],"room %s formation mismatch",kStagePresets[i].name);
        CHECK(p.micX==before.micX && p.leadY==before.leadY,"room layout moved listener/lead");
    }
    std::printf("POSITION TESTS PASSED: %d formations, %d room layouts; positions only\n",kNumChoirPositions,kNumStagePresets);
}
