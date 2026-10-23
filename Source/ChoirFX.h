#pragma once
#include "ChoirDSP.h"
#include <array>
#include <cstdint>
#include <vector>

namespace choir {

// Audio-effect parameters. The four stage positions, microphone array and room
// are shared with the supplied OmniChoir demo's physical stage concept.
struct FXParams : Params {
    bool choirOn = true;             // button bypasses wet choir return
    int mode = 0;                  // 0 Live, 1 Scan/Harmony
    int harmony = 0;               // 0 Major, 1 Minor, 2 Unison (ensemble)
    float mix = 0.78f;             // blend of choir vs. original input
    float sensitivity = 0.65f;     // voice gate sensitivity
};

class PitchTracker {
public:
    void prepare(float sampleRate);
    void reset();
    // Returns true when a NEW valid pitch frame was measured.
    bool push(float sample);
    float midi() const { return currentMidi; }
    float rms() const { return level; }
private:
    static constexpr int capacity = 1024;
    std::array<float,capacity> history {};
    int downsample = 12, divide = 0, write = 0, count = 0, hop = 0;
    float accumulator = 0.f, rate = 4000.f, level = 0.f, currentMidi = -1.f;
    int missed = 0;
    bool analyse();
};

// Two overlapping Hann-window pitch grains per layer. Variable-delay resampling:
// a stylistic real-time harmonizer, not formant-preserving neural voice synthesis.
// Processing has a variable 10-90 ms grain delay; no deterministic fixed latency.
class GrainVoice {
public:
    void reset(float phaseOffset);
    float process(const std::vector<float>& history, int write, float window, float minDelay);
    float ratio = 1.f, targetRatio = 1.f;
private:
    float phase = 0.f;
    static float read(const std::vector<float>& data, int write, float samplesBack);
};

class ChoirEffect {
public:
    void prepare(double sampleRate);
    void reset();
    void setParameters(const FXParams&);
    void beginScan(); // audio thread only
    void setSavedScanRoot(int root, bool complete);
    void render(float inL, float inR, float& outL, float& outR);
    int scanStatus() const { return status; } // 0 idle, 1 sampling, 2 ready, 3 no voiced input
    float scanProgress() const { return progress; }
    int scanRoot() const { return rootPC; }
    float detectedMidi() const { return tracker.midi(); }
    float inputRms() const { return tracker.rms(); }
private:
    static constexpr int singers = 2; // slightly detuned per SATB section
    std::array<GrainVoice, sections*singers> layers;
    std::array<float,sections> panL {}, panR {}, tone {}, lowPass {};
    std::array<int, 12> pitchHistogram {};
    std::vector<float> inputHistory;
    Room room;
    PitchTracker tracker;
    FXParams p;
    float sr = 48000.f, inputEnv = 0, outputEnv = 0, wetNow = 0;
    float window = 3072, minDelay = 480, roomL = 0, roomR = 0;
    int write = 0, status = 0, rootPC = 0, scanSamples = 0, matchedFrames = 0;
    float progress = 0;
    void updateTargetRatios();
    void updateStage();
};
} // namespace choir
