#pragma once
// v0.7: STYLES (a whole sound in one click), the CHOIR SIZE macro and SURPRISE ME. JUCE-free, so the numbers are tested natively
// (tests/style_tests.cpp). A style is a stage preset (where the singers stand and the room) plus how they sing; it never touches the
// lead level, the output, CHOIR MIX's loudness law (AUTO LEVEL) or the per-section solo/mute state.
#include "StagePresets.h"
#include <algorithm>
#include <cstdint>

namespace choir {

struct StylePreset
{
    const char* name;
    const char* about;
    int stage;                       // index into kStagePresets: placement and room
    int harmony;                     // 0 major, 1 minor, 2 unison
    float mix, size;                 // CHOIR MIX and the CHOIR SIZE macro
    float ensemble, humanize, formant;
    float voiceBlend, choirVowel, vowelFollow, choirBreath, vibrato;
};

inline constexpr StylePreset kStylePresets[] = {
    { "Natural Choir", "The default sound: a mid-sized hall, a warm blended choir.",
      0, 0, .78f, .5f,  .58f, .75f, .5f,   0.f,  0.f,  0.f,  .5f, .6f },
    { "Gregorian Chant", "Everyone sings your note in a huge stone nave: pure, steady, slow to bloom.",
      1, 2, .85f, .8f,  .8f,  .55f, .6f,   .55f, 0.f,  .4f,  .5f, .15f },
    { "Gospel Choir", "A wide, close, lively arc with a big warm vibrato and a loose, human feel.",
      5, 0, .8f,  .55f, .75f, .85f, .55f,  .2f,  .3f,  .5f,  .4f, .7f },
    { "Cinematic Pad", "A vast hall and a huge, smooth, synthetic-leaning wall of voices.",
      6, 0, .9f,  .9f,  .85f, .5f,  .5f,   .7f,  .45f, .2f,  .6f, .5f },
    { "Lo-fi Church", "A small stone chapel, minor and a little loose, breathy and close.",
      4, 1, .7f,  .3f,  .45f, .9f,  .35f,  .1f,  .2f,  .3f,  .7f, .35f },
    { "Intimate Quartet", "Four singers in a dry room, tight and close: nothing big, everything clear.",
      3, 0, .6f,  .2f,  .25f, .55f, .5f,   0.f,  0.f,  0.f,  .3f, .5f },
};
inline constexpr int kNumStylePresets = (int) (sizeof(kStylePresets) / sizeof(kStylePresets[0]));

inline float clamp01(float v) { return std::max(0.f, std::min(1.f, v)); }
inline float clampTo(float v, ParamRange r) { return std::max(r.lo, std::min(r.hi, v)); }

// the parameter values a style sets, on top of its stage preset (the stage preset sets placement, room, ensemble and humanize first)
inline void applyStyle(const StylePreset& s, FXParams& p, float& size)
{
    applyStagePreset(kStagePresets[s.stage], p);
    p.harmony = s.harmony; p.mix = s.mix; size = s.size;
    p.ensemble = s.ensemble; p.humanize = s.humanize; p.formant = s.formant;
    p.voiceBlend = s.voiceBlend; p.choirVowel = s.choirVowel; p.vowelFollow = s.vowelFollow; p.choirBreath = s.choirBreath; p.vibrato = s.vibrato;
}

// CHOIR SIZE: one knob that makes the choir read as bigger or smaller. 0.5 changes nothing; below it the choir is tighter, drier and
// closer, above it looser, wetter and wider. It is applied on top of the individual knobs, which keep their own meaning.
inline void applySize(FXParams& p, float size)
{
    const float d = clamp01(size) - .5f;
    p.ensemble = clamp01(p.ensemble + d * .6f);
    p.humanize = clamp01(p.humanize + d * .3f);
    p.wet = clampTo(p.wet + d * .5f, kRangeWet);
    p.roomSeconds = clampTo(p.roomSeconds * (1.f + d * .8f), kRangeRoomSeconds);
    p.roomSize = clamp01(p.roomSize + d * .4f);
    p.width = clampTo(p.width + d * .8f, kRangeWidth);
}

// SURPRISE ME: a random but musical style, drawn from ranges chosen to always sound like a choir. tests/style_tests.cpp range-checks 300 of
// them and renders 8 on a real voice (60 with OMNICHOIR_THOROUGH=1), checking level, peaks and the choir/lead balance. Deterministic per seed.
inline float randomIn(std::uint32_t& state, float lo, float hi)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return lo + (hi - lo) * (float) (state & 0xffffff) / (float) 0x1000000;
}
inline StylePreset randomStyle(std::uint32_t& state)
{
    if (state == 0) state = 0x9e3779b9u;
    StylePreset s = kStylePresets[0];
    s.name = "Surprise"; s.about = "A random, musical combination.";
    s.stage = (int) randomIn(state, 0.f, (float) kNumStagePresets - .001f);
    s.harmony = randomIn(state, 0.f, 1.f) < .8f ? (int) randomIn(state, 0.f, 1.999f) : 2;   // mostly major or minor, sometimes unison
    s.mix = randomIn(state, .7f, .9f);   // never lower: a low MIX plus a distant room plus synthetic voices could bury the choir under the lead
    s.size = randomIn(state, .2f, .85f);
    s.ensemble = randomIn(state, .35f, .85f);
    s.humanize = randomIn(state, .45f, .9f);
    s.formant = randomIn(state, .35f, .7f);
    s.voiceBlend = randomIn(state, 0.f, 1.f) < .65f ? randomIn(state, 0.f, .2f) : randomIn(state, .3f, .7f);
    s.choirVowel = randomIn(state, 0.f, 1.f);
    s.vowelFollow = randomIn(state, 0.f, .6f);
    s.choirBreath = randomIn(state, .2f, .7f);
    s.vibrato = randomIn(state, .2f, .8f);
    return s;
}

} // namespace choir
