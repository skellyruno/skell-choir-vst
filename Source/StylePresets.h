#pragma once
// v0.7: STYLES (a whole sound in one click), the CHOIR SIZE macro and SURPRISE ME. JUCE-free, so the numbers are tested natively
// (tests/style_tests.cpp). A full style recalls room acoustics, choir layout, microphone/lead placements
// and singing parameters. It does not change lead level, output, AUTO LEVEL, or per-section solo/mute.
#include "PositionPresets.h"
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
    float micX = 0.f, micY = 0.f, leadX = 0.f, leadY = .4f;  // all four positions are part of a full style
};

inline constexpr StylePreset kStylePresets[] = {
    { "Natural Choir", "The default sound: a mid-sized hall, a warm blended choir.",
      0, 0, .78f, .5f,  .58f, .75f, .5f,   0.f,  0.f,  0.f,  .5f, .6f,   0.f, 0.f, 0.f, .4f },
    { "Gregorian Chant", "Everyone sings your note in a huge stone nave: pure, steady, slow to bloom.",
      1, 2, .85f, .8f,  .8f,  .55f, .6f,   .55f, 0.f,  .4f,  .5f, .15f,  0.f,-.7f,-.8f,1.2f },
    { "Gospel Choir", "A wide, close, lively arc with a big warm vibrato and a loose, human feel.",
      5, 0, .8f,  .55f, .75f, .85f, .55f,  .2f,  .3f,  .5f,  .4f, .7f,   0.f, 0.f, 1.0f, .6f },
    { "Cinematic Pad", "A vast hall and a huge, smooth, synthetic-leaning wall of voices.",
      6, 0, .9f,  .9f,  .85f, .5f,  .5f,   .7f,  .45f, .2f,  .6f, .5f,  0.f,-.8f, 0.f, 1.8f },
    { "Lo-fi Church", "A small stone chapel, minor and a little loose, breathy and close.",
      4, 1, .7f,  .3f,  .45f, .9f,  .35f,  .1f,  .2f,  .3f,  .7f, .35f,  0.f, .2f,-.5f, .6f },
    { "Intimate Quartet", "Four singers in a dry room, tight and close: nothing big, everything clear.",
      3, 0, .6f,  .2f,  .25f, .55f, .5f,   0.f,  0.f,  0.f,  .3f, .5f,   0.f, .8f, 0.f, .4f },
    { "Midnight Cathedral", "Dark minor harmonies in a lofty cathedral with gently drifting voices.",
      1, 1, .84f, .7f, .65f, .62f, .55f, .35f, .25f, .3f, .5f, .38f, 0.f, -.3f, -.2f, .8f },
    { "Stadium Anthem", "A bright and broad full-choir anthem with open room reflections.",
      6, 0, .84f, .78f, .75f, .65f, .55f, .3f, .3f, .35f, .4f, .62f, 0.f, -.3f, .4f, .7f },
    { "Close Harmony", "A dry, clear, intimate blend for tight arrangements.",
      3, 0, .77f, .32f, .38f, .52f, .5f, .1f, .05f, .15f, .33f, .4f, 0.f, .35f, 0.f, .45f },
    { "Ethereal Bloom", "Airy, floating voices gathered in an expansive marble dome.",
      13, 2, .83f, .72f, .68f, .55f, .56f, .5f, .55f, .25f, .55f, .32f, .2f, -.25f, -.25f, 1.0f },
    { "Underground Echo", "A shadowy chamber with slow, breathy minor swells.",
      9, 1, .81f, .6f, .55f, .72f, .53f, .3f, .4f, .25f, .62f, .35f, 0.f, -.2f, .2f, .75f },
};
inline constexpr int kNumStylePresets = (int) (sizeof(kStylePresets) / sizeof(kStylePresets[0]));

inline float clamp01(float v) { return std::max(0.f, std::min(1.f, v)); }
inline float clampTo(float v, ParamRange r) { return std::max(r.lo, std::min(r.hi, v)); }

// Full style: matching acoustics, singer arrangement, microphone and lead positions, and how they sing.
inline void applyStyle(const StylePreset& s, FXParams& p, float& size)
{
    applyRoomOnly(kStagePresets[s.stage], p);
    applyRoomLayoutOnly(kStagePresets[s.stage], p);
    p.micX = s.micX; p.micY = s.micY; p.leadX = s.leadX; p.leadY = s.leadY;
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
    s.micX = kStagePresets[s.stage].micX; s.micY = kStagePresets[s.stage].micY;
    s.leadX = randomIn(state, -.7f, .7f); s.leadY = randomIn(state, .4f, 1.1f);
    return s;
}

} // namespace choir
