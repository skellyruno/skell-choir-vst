#pragma once
// MIDI chord mode (v0.7): which section sings which held key. JUCE-free, so it is tested natively (tests/midi_chord_tests.cpp).
// The audio thread calls noteOn / noteOff / sustain as messages arrive and reads sections() once per block.
#include <algorithm>
#include <array>
#include <bitset>

namespace choir {

class ChordTracker
{
public:
    void noteOn(int note)  { if (valid(note)) { down.set((size_t) note); sustained.reset((size_t) note); } }
    void noteOff(int note)
    {
        if (!valid(note)) return;
        down.reset((size_t) note);
        if (pedal) sustained.set((size_t) note);     // the pedal keeps it sounding
    }
    void sustain(bool on)
    {
        pedal = on;
        if (!on) sustained.reset();                  // pedal up: everything not physically held stops
    }
    void allOff() { down.reset(); sustained.reset(); }
    bool any() const { return (down | sustained).any(); }

    // Soprano, alto, tenor, bass -> the MIDI note each sings, or -1 when nothing is held.
    // The held keys are sorted and spread over the four sections, the lowest to the bass and the highest to the soprano:
    //   1 key : all four sing it            2 keys: bass+tenor the lower, alto+soprano the higher
    //   3 keys: bass lowest, tenor+alto the middle one, soprano highest
    //   4 keys: one each                    5+ keys: the lowest and highest are kept, the others spread evenly between them
    std::array<int,4> sections() const
    {
        std::array<int,4> out {{ -1, -1, -1, -1 }};
        int notes[128]; int n = 0;
        const auto held = down | sustained;
        for (int k = 0; k < 128; ++k) if (held.test((size_t) k)) notes[n++] = k;
        if (n == 0) return out;
        for (int s = 0; s < 4; ++s)                  // s = 0 soprano .. 3 bass
        {
            const int fromBottom = 3 - s;            // 0 for the bass, 3 for the soprano
            const int index = n == 1 ? 0 : (int) ((float) fromBottom * (float) (n - 1) / 3.f + .5f);
            out[(size_t) s] = notes[std::min(n - 1, index)];
        }
        return out;
    }
private:
    static bool valid(int note) { return note >= 0 && note < 128; }
    std::bitset<128> down, sustained;
    bool pedal = false;
};

} // namespace choir
