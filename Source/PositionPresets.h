#pragma once
// Independent placement layouts: only the four choir sections move. The lead,
// microphone, level, room acoustics and musical/automation settings are untouched.
#include "StagePresets.h"

namespace choir {
struct ChoirPosition {
    const char* name;
    float x[4], y[4];
};
inline constexpr ChoirPosition kChoirPositions[] = {
    { "Front Row",      {-4.2f,-1.4f, 1.4f, 4.2f}, {3.3f,3.3f,3.3f,3.3f} },
    { "Tight Cluster",   {-2.4f,-0.8f, 0.8f, 2.4f}, {4.3f,4.0f,4.0f,4.3f} },
    { "Wide Arc",        {-7.0f,-2.5f, 2.5f, 7.0f}, {6.0f,4.4f,4.4f,6.0f} },
    { "Back Gallery",    {-5.2f,-1.8f, 1.8f, 5.2f}, {9.3f,8.3f,8.3f,9.3f} },
    { "V Formation",     {-5.5f,-1.6f, 1.6f, 5.5f}, {4.7f,7.2f,7.2f,4.7f} },
    { "Split Sides",     {-6.1f,-3.2f, 3.2f, 6.1f}, {7.8f,4.2f,4.2f,7.8f} },
};
inline constexpr int kNumChoirPositions = (int)(sizeof(kChoirPositions)/sizeof(kChoirPositions[0]));
inline void applyChoirPositionOnly(const ChoirPosition& s, FXParams& p) {
    for(int i=0;i<4;++i) { p.x[(size_t)i]=s.x[i];p.y[(size_t)i]=s.y[i]; }
}
inline void applyRoomLayoutOnly(const StagePreset& s, FXParams& p) {
    for(int i=0;i<4;++i) { p.x[(size_t)i]=s.x[i];p.y[(size_t)i]=s.y[i]; }
}
} // namespace choir
