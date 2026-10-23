#pragma once
#include "../ChoirFX.h"
#include <array>
#include <cstdint>
#include <vector>

namespace choir {
// Seconds, MIDI note numbers, editable SATB assignment. Each event is an
// isolated monophonic phrase/note estimate, NOT inferred source stems/chords.
struct NoteEvent {
    double start = 0, end = 0;
    int melody = 60;
    std::array<int,4> satb {67,64,55,48};
    float confidence = 0;
};
struct FullTakeScore {
    std::vector<NoteEvent> notes;
    int root = 0;
    bool minor = false;
    double duration = 0;
};
class FullTakeScanner {
public:
    void prepare(double sampleRate);
    void push(float mono);            // call sequentially for the ENTIRE take
    FullTakeScore finish(bool minor); // called once, after streaming the file
private:
    PitchTracker tracker;
    double rate = 48000;
    int64_t sample = 0, lastFrame = 0, activeStart = -1;
    int activePitch = -1, activeCount = 0, frames = 0;
    std::array<int,12> histogram {};
    std::vector<NoteEvent> notes;
    void observe(int midi, int64_t time);
    void close(int64_t time);
};
void assignSATB(FullTakeScore& score);
// User edits in single-semitone increments; independent of re-analysis.
bool transposeNote(FullTakeScore&,size_t eventIndex,int section,int semitones);
}
