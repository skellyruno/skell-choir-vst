#pragma once
// v0.8: KEY and SCALE. You choose them (nothing is detected). With a scale selected, the choir sings diatonic harmony: each section
// takes the scale note a fixed number of scale steps from the note you sing, so the harmony never leaves the key. With
// "Chromatic" (the default) nothing is snapped and the intervals are the fixed ones of v0.7 (+7, +4, -5, -12 semitones).
// JUCE-free; tested in tests/key_scale_tests.cpp.
#include <array>
#include <cmath>

namespace choir {

struct ScaleDef { const char* name; int count; int notes[12]; };   // semitones above the root

inline constexpr ScaleDef kScales[] = {
    { "Chromatic (off)",   12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    { "Major",              7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "Natural Minor",      7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Harmonic Minor",     7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "Dorian",             7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Mixolydian",         7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "Major Pentatonic",   5, { 0, 2, 4, 7, 9 } },
    { "Minor Pentatonic",   5, { 0, 3, 5, 7, 10 } },
};
inline constexpr int kNumScales = (int) (sizeof(kScales) / sizeof(kScales[0]));
inline constexpr const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

inline int floorDiv(int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
inline int clampScale(int scale) { return scale < 0 ? 0 : (scale >= kNumScales ? kNumScales - 1 : scale); }

// the MIDI note at an absolute scale-step index (0 = the root in octave 0; the root pitch class is added, so index 0 is note `root`)
inline int scaleNote(int root, int scale, int index)
{
    const auto& s = kScales[clampScale(scale)];
    const int octave = floorDiv(index, s.count), degree = index - octave * s.count;
    return root + 12 * octave + s.notes[degree];
}
// the scale-step index of the scale note nearest to `note` (ties go to the lower note)
inline int nearestIndex(int note, int root, int scale)
{
    const auto& s = kScales[clampScale(scale)];
    const int octave = floorDiv(note - root, 12);
    int best = octave * s.count, bestError = 1 << 20;
    for (int i = (octave - 1) * s.count; i <= (octave + 2) * s.count; ++i)
    {
        const int error = std::abs(scaleNote(root, scale, i) - note);
        if (error < bestError) { bestError = error; best = i; }
    }
    return best;
}
inline bool inScale(int note, int root, int scale) { return scaleNote(root, scale, nearestIndex(note, root, scale)) == note; }

// The fixed intervals of v0.7, in semitones, for soprano, alto, tenor, bass: a fifth up, a third up, a fourth down, an octave down.
inline constexpr std::array<int,4> kFixedIntervals { 7, 4, -5, -12 };

// The scale steps each section moves: the fixed interval spread over the scale (7 notes: +4, +2, -3, -7; 5 notes: +3, +2, -2, -5).
inline int stepsFor(int section, int scale)
{
    const int n = kScales[clampScale(scale)].count;
    return (int) std::lround((double) kFixedIntervals[(size_t) section] * n / 12.0);
}

// The interval (in semitones, from the sung `melody` note) each section should sing so that it lands on a note of the scale.
// The sung note is first taken to its nearest scale note, so a note a little off the scale does not push the harmony off it too.
inline std::array<int,4> harmonyIntervals(int melody, int root, int scale)
{
    std::array<int,4> out = kFixedIntervals;
    if (clampScale(scale) == 0) return out;                                  // Chromatic: the fixed intervals, untouched
    const int reference = nearestIndex(melody, root, scale);
    for (int s = 0; s < 4; ++s)
        out[(size_t) s] = scaleNote(root, scale, reference + stepsFor(s, scale)) - melody;
    return out;
}

} // namespace choir
