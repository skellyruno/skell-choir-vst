// Compact S/M should remain attached to singers at all stage sizes and edge placements.
#include "../Source/UI/StageControlLayout.h"
#include "test_support.h"
#include <cmath>
int main()
{
    CHECK(stagecontrols::kButton <= 10.f,"solo/mute buttons must be smaller than their original 11px size");
    CHECK(stagecontrols::kRowStep-stagecontrols::kButton >= 2.f,"solo/mute buttons need a visible gap");
    int cases=0;
    for(const float width : {730.f,765.f,860.f,1300.f,1500.f}) {
        const float left=7.f,right=width-35.f,ppm=(right-left)/21.7f,centre=width*.48f;
        const float l=centre-.705f*ppm,r=centre+.705f*ppm,size=width<900.f?12.f:19.f;
        const auto s=stagecontrols::beside(l,r,120.f,size,left,10.f,right,450.f,false);
        const auto m=stagecontrols::beside(l,r,120.f,size,left,10.f,right,450.f,true);
        CHECK(s.x>=r+.62f*size-.001f && s.x<r+.62f*size+2.f,"S too close or detached %.0f",width);
        CHECK(std::abs(s.x-m.x)<.01f && std::abs(m.y-s.y-stagecontrols::kRowStep)<.01f,"S/M must stack in one column");
        CHECK(s.x>=left && s.x+s.width<=right && m.y+m.height<=450.f,"buttons clip at %.0f",width);
        ++cases;
    }
    for(const float x : {-65.f,0.f,300.f,600.f,740.f})
        for(const float y : {5.f,45.f,380.f,460.f}) {
            const auto s=stagecontrols::beside(x-25.f,x+25.f,y,15.f,7.f,7.f,730.f,500.f,false);
            const auto m=stagecontrols::beside(x-25.f,x+25.f,y,15.f,7.f,7.f,730.f,500.f,true);
            CHECK(s.x>=10.f && s.x+s.width<=727.f && m.y>=37.f && m.y+m.height<=497.f,"edge clamp failed %.1f %.1f",x,y);
            ++cases;
        }
    const auto lead=stagecontrols::besideLead(360.f,220.f,7.f,7.f,735.f,500.f,false);
    CHECK(lead.x>370.f && lead.x<377.f,"lead S should hug the figure");
    std::printf("stage S/M: %d responsive and edge cases passed\n",cases);
}
