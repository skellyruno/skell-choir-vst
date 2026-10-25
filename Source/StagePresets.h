#pragma once
// Stage presets: where the choir stands, where YOU stand, what room it is in. They set the placement and room
// parameters only (never the musical ones: harmony, mix, lead level, output), so trying a room never changes what
// is sung or how loud the plugin is overall.
#include "ChoirFX.h"
#include <cstddef>

namespace choir {

struct StagePreset
{
    const char* name;
    const char* about;
    float x[4], y[4], gain[4];            // soprano, alto, tenor, bass
    float micX, micY, width; int micMode;  // YOU: offset from the conductor's spot, mic spacing, array (0 AB 1 ORTF 2 XY 3 MS 4 Decca 5 Binaural)
    float roomSize, roomSeconds, absorption, diffusion, wet;
    float hall, hallSeconds;               // the extra hall under the choir (0 = off)
    float ensemble, humanize;
};

// ranges of the matching plugin parameters, for validation and for clamping loaded data
struct ParamRange { float lo, hi; };
inline constexpr ParamRange kRangeX { -9.f, 9.f }, kRangeY { .7f, 11.f }, kRangeGain { 0.f, 1.5f }, kRangeMicX { -9.f, 9.f }, kRangeMicY { -8.f, 8.f },
    kRangeWidth { .1f, 3.f }, kRangeUnit { 0.f, 1.f }, kRangeRoomSeconds { .2f, 10.f }, kRangeWet { 0.f, 1.5f }, kRangeHall { 0.f, .5f }, kRangeHallSeconds { .5f, 8.f };

inline constexpr StagePreset kStagePresets[] = {
    { "Default", "The v0.5 stage: a mid-sized hall, ORTF at the conductor's spot.",
      { -5.1f, -1.7f, 1.7f, 5.1f }, { 6.f, 5.2f, 5.2f, 6.f }, { .83f, .83f, .83f, .83f },
      0.f, 0.f, .6f, 1, .52f, 2.4f, .4f, .7f, .37f, 0.f, 2.4f, .58f, .75f },
    { "Cathedral", "A huge stone nave: the choir far away in the apse, a very long tail, a wide spaced pair.",
      { -6.4f, -2.2f, 2.2f, 6.4f }, { 9.5f, 8.4f, 8.4f, 9.5f }, { .9f, .88f, .86f, .9f },
      0.f, -2.5f, 1.4f, 0, .96f, 6.2f, .18f, .85f, .5f, .24f, 5.8f, .62f, .8f },
    { "Concert Hall", "A good shoebox hall from the stalls: balanced direct sound and a warm 2.8 s tail.",
      { -4.8f, -1.6f, 1.6f, 4.8f }, { 7.6f, 6.8f, 6.8f, 7.6f }, { .85f, .83f, .83f, .85f },
      0.f, -.5f, .7f, 1, .76f, 2.8f, .34f, .75f, .4f, .12f, 3.0f, .55f, .75f },
    { "Studio Booth", "Dry and close: a small treated room, coincident pair, tight blend.",
      { -2.2f, -.75f, .75f, 2.2f }, { 3.2f, 2.8f, 2.8f, 3.2f }, { .8f, .8f, .8f, .8f },
      0.f, .8f, .25f, 2, .12f, .55f, .8f, .5f, .2f, 0.f, 2.4f, .4f, .65f },
    { "Chapel", "A small stone chapel: intimate, close, a bright 1.8 s ring.",
      { -3.4f, -1.2f, 1.2f, 3.4f }, { 4.6f, 4.0f, 4.0f, 4.6f }, { .84f, .82f, .82f, .84f },
      0.f, .2f, .45f, 3, .34f, 1.8f, .3f, .8f, .4f, .09f, 2.0f, .5f, .8f },
    { "Gospel Stage", "A wide front-of-stage arc, close and present, a bright live room.",
      { -7.2f, -2.6f, 2.6f, 7.2f }, { 4.2f, 3.4f, 3.4f, 4.2f }, { .86f, .84f, .84f, .86f },
      0.f, 0.f, .9f, 4, .5f, 1.6f, .45f, .6f, .33f, .07f, 1.8f, .66f, .85f },
    { "Stadium Anthem", "A vast open space: a very wide choir, far back, long late reflections.",
      { -8.4f, -2.8f, 2.8f, 8.4f }, { 10.5f, 9.5f, 9.5f, 10.5f }, { .92f, .9f, .9f, .92f },
      0.f, -3.5f, 2.0f, 4, 1.f, 3.9f, .3f, .5f, .5f, .3f, 3.6f, .75f, .85f },
    { "Headphone Choir", "A binaural head in the middle of the choir: close, enveloping, minimal room.",
      { -3.8f, -1.3f, 1.3f, 3.8f }, { 2.4f, 1.8f, 1.8f, 2.4f }, { .8f, .8f, .8f, .8f },
      0.f, 0.f, .4f, 5, .3f, 1.2f, .5f, .75f, .28f, .05f, 1.5f, .6f, .8f },
};
inline constexpr int kNumStagePresets = (int) (sizeof(kStagePresets) / sizeof(kStagePresets[0]));

// copies a preset's placement and room settings onto a parameter set (nothing else is touched)
inline void applyStagePreset(const StagePreset& s, FXParams& p)
{
    for (int i = 0; i < 4; ++i) { p.x[(size_t) i] = s.x[i]; p.y[(size_t) i] = s.y[i]; p.gain[(size_t) i] = s.gain[i]; }
    p.micX = s.micX; p.micY = s.micY; p.width = s.width; p.micMode = s.micMode;
    p.roomSize = s.roomSize; p.roomSeconds = s.roomSeconds; p.absorption = s.absorption; p.diffusion = s.diffusion; p.wet = s.wet;
    p.hall = s.hall; p.hallSeconds = s.hallSeconds; p.ensemble = s.ensemble; p.humanize = s.humanize;
}

} // namespace choir
