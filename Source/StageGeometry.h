#pragma once
// Where things are on the stage view, in one JUCE-free place so the maths can be tested natively.
//
// Two coordinates matter:
//   * the engine's: the lead stands at  mic + (leadX, leadY)  in the room (StagedChoir -> LeadStage: dx = leadX, dy = leadY - 0.4,
//     and the stage adds the 0.4 m home back). leadX = 0 and leadY = 0.4 is the lead at home, exactly dry. Nothing here changes that.
//   * the stage view's: the lead is DRAWN at  mic + kDisplayShift + (leadX, leadY),  i.e. the engine position moved a fixed 2.4 m
//     forward so her icon never sits on top of the mic icon.
// Dragging the lead sets leadX/leadY so that she lands under the pointer. Dragging the mic keeps the lead where she is on the
// stage by rewriting leadX/leadY. For that to always be possible the lead's parameters must cover every lead-minus-mic
// distance the view allows, so the parameter ranges (kOffsetX, kOffsetY) are wider than the stage.
#include <algorithm>

namespace choir {

struct StagePoint { float x = 0.f, y = 0.f; };

namespace StageGeometry {

constexpr StagePoint kDisplayShift { 0.f, 2.4f };   // how far the lead is drawn from the engine's position (metres)
constexpr float kHomeLeadY = 0.4f;                  // leadY when she stands at home (leadX = 0): the plugin's default

// what the view lets you drag to (metres, in the view's world)
constexpr float kMicXMax = 9.f, kMicYMin = -8.f, kMicYMax = 8.f;
constexpr float kLeadXMax = 10.f, kLeadYMin = -1.f, kLeadYMax = 12.5f;
// the ranges of the leadX / leadY parameters (PluginProcessor.cpp must declare the same)
constexpr float kOffsetXMin = -24.f, kOffsetXMax = 24.f, kOffsetYMin = -16.f, kOffsetYMax = 22.f;
// what the engine itself clamps the microphone to (Stage.h setListener, with the conductor's spot at 0, 0)
constexpr float kEngineMicXMax = 10.f, kEngineMicYMin = -10.f, kEngineMicYMax = 12.f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline StagePoint clampMic(StagePoint p)  { return { clampf(p.x, -kMicXMax, kMicXMax), clampf(p.y, kMicYMin, kMicYMax) }; }
inline StagePoint clampLead(StagePoint p) { return { clampf(p.x, -kLeadXMax, kLeadXMax), clampf(p.y, kLeadYMin, kLeadYMax) }; }
inline float clampOffsetX(float v) { return clampf(v, kOffsetXMin, kOffsetXMax); }
inline float clampOffsetY(float v) { return clampf(v, kOffsetYMin, kOffsetYMax); }

// where the lead is drawn, for a mic position and the leadX / leadY parameters
inline StagePoint leadOnStage(StagePoint mic, float leadX, float leadY)
{
    return { mic.x + kDisplayShift.x + leadX, mic.y + kDisplayShift.y + leadY };
}
// the leadX / leadY that put the lead at `lead` while the mic is at `mic` (the inverse of leadOnStage), kept inside the parameter ranges
inline StagePoint offsetFor(StagePoint lead, StagePoint mic)
{
    return { clampOffsetX(lead.x - mic.x - kDisplayShift.x), clampOffsetY(lead.y - mic.y - kDisplayShift.y) };
}
// where she is drawn when she is at home for this mic position
inline StagePoint home(StagePoint mic) { return leadOnStage(mic, 0.f, kHomeLeadY); }

// where the engine puts her in the room (the mic is clamped by the engine first)
inline StagePoint leadInRoom(StagePoint mic, float leadX, float leadY)
{
    const StagePoint m { clampf(mic.x, -kEngineMicXMax, kEngineMicXMax), clampf(mic.y, kEngineMicYMin, kEngineMicYMax) };
    return { m.x + leadX, m.y + leadY };
}
// the offset the engine receives (StagedChoir::setParameters passes leadY - 0.4)
inline StagePoint engineOffset(float leadX, float leadY) { return { leadX, leadY - kHomeLeadY }; }

} // namespace StageGeometry
} // namespace choir
