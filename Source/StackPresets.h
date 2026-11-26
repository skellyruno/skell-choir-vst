#pragma once
// v0.9: STACKS. A style (StylePresets.h) is a whole sound in a room; a stack is WHO sings and how tightly: which sections are in, the harmony, and how loose
// and individual the singers are. It never touches the room, the stage, the lead level, OUTPUT or the PROCESS mode, so it can be tried on top of any style.
// JUCE-free so the numbers are tested natively (tests/stack_tests.cpp).
#include <array>

namespace choir {

struct StackPreset
{
    const char* name;
    const char* about;
    std::array<bool, 4> sings;       // soprano, alto, tenor, bass: false = that section is muted
    int harmony;                     // 0 major, 1 minor, 2 unison
    float mix, ensemble, humanize, voiceBlend, choirBreath, vibratoSpread;
};

inline constexpr StackPreset kStackPresets[] = {
    { "Gospel 16",     "All sixteen singers, loose and individual, each with a different vibrato.",
      { true, true, true, true },     0, .80f, .85f, .85f, .20f, .45f, .70f },
    { "Duet",          "Just soprano and alto: two close, tight voices on top of you.",
      { true, true, false, false },   0, .65f, .30f, .40f, 0.f,  .30f, .20f },
    { "Men's Section", "Tenors and basses only: a low, dark section under your voice.",
      { false, false, true, true },   0, .75f, .55f, .60f, .15f, .40f, .40f },
    { "Whisper Choir", "Everyone, breathy and soft, with a little synthesised air.",
      { true, true, true, true },     0, .60f, .50f, .90f, .60f, .95f, .30f },
    { "Unison Wall",   "Sixteen singers on your note: thick and tight, almost no vibrato spread.",
      { true, true, true, true },     2, .85f, .90f, .40f, .10f, .35f, .10f },
};
inline constexpr int kNumStackPresets = (int) (sizeof(kStackPresets) / sizeof(kStackPresets[0]));

} // namespace choir
