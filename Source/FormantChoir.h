#pragma once
// A second voice engine: source-filter singers. Where the granular harmonizer re-plays the lead, these 16 voices are
// SYNTHESISED: a glottal-like source through five vowel formants (separate tables for soprano, alto, tenor, bass),
// following the lead's pitch, loudness and (optionally) its vowel. A performance layer gives each singer its own
// vibrato, scoop-in, breath, onset timing and swell so the choir breathes instead of holding a static pad.
//
// JUCE-free, one call per input sample.
#include "Stage/Common.h"
#include <array>
#include <cstdint>
#include <vector>

namespace choir {

constexpr int kVowels = 5;   // A E I O U

// Estimates the lead's vowel from its spectral envelope (LPC at ~10 kHz, formants F1/F2) and returns soft weights
// over the five vowels. Approximate by design: it follows the lead's mouth shape, it does not transcribe lyrics.
class VowelEstimator {
public:
    void prepare(double sampleRate);
    void reset();
    void push(float sample);                              // call every input sample
    const std::array<float, kVowels>& weights() const { return w; }
    float f1() const { return formant1; }
    float f2() const { return formant2; }
    bool valid() const { return haveEstimate; }
private:
    void analyse();
    int decimate = 4, phaseCount = 0, filled = 0, sinceAnalysis = 0;
    double rate = 12000, accumulator = 0, previous = 0;
    std::array<float, 512> frame {}; std::array<float, 512> ring {};
    int ringWrite = 0;
    float formant1 = 0, formant2 = 0, energyFloor = 1e-5f;
    bool haveEstimate = false;
    std::array<float, kVowels> w { 1.f, 0.f, 0.f, 0.f, 0.f };
};

struct FormantControl
{
    float midi = -1.f;               // the lead's pitch, -1 when unknown (the last good pitch is held)
    const float* ratios = nullptr;   // 16 per-voice pitch ratios from the harmonizer's voicing (smoothed)
    float gate = 0.f;                // 0..1 the lead is singing (the harmonizer's voice gate)
    float humanize = .7f, vibrato = .6f, breath = .5f;
    float vowel = 0.f;               // manual vowel 0..1 across A E I O U
    float follow = 0.f;              // 0..1 how much the lead's own vowel steers the choir
    float formantShift = 1.f;        // 1 = as tabulated; scales all formant frequencies
};

class FormantChoir {
public:
    static constexpr int numVoices = 16, perSection = 4;
    void prepare(double sampleRate);
    void reset();
    // in: the lead sample (loudness, vowel, breath); out[16]: one sample per singer
    void process(float in, const FormantControl& c, float* out);
    const VowelEstimator& estimator() const { return vowelEstimator; }
    // per-voice gain that sets how loud the choir voice is relative to the lead's own level
    static constexpr float kLevel = 0.8f;
private:
    struct Voice
    {
        double phase = 0, vibPhase = 0, vibRate = 5.2, vibDepth = 14, vibDelay = .25, scoopCents = 0, scoopTime = .06;
        double vibClock = 10, scoopClock = 10, rms = 1e-3;
        double attack = .04, release = .18, env = 0, lp = 0, breathEnv = 0, jitter = 0, jitterTarget = 0, logHz = 5.5;
        double formantFreq[5] {}, formantAmp[5] {};
        oc::Biquad res[5], whisper[3];
        double onsetDelay = 0, gateTime = 0;
        uint32_t seed = 1;
        float noiseLp = 0;
    };
    static double rnd(uint32_t& s) { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0; }
    void retune(Voice& v, int section);
    double shift = 1.0, tunedShift = 0.0;
    std::array<float, kVowels> tunedWeights { -1.f, -1.f, -1.f, -1.f, -1.f };
    double sr = 48000;
    std::array<Voice, numVoices> voices;
    VowelEstimator vowelEstimator;
    std::array<float, kVowels> weights { 1.f, 0.f, 0.f, 0.f, 0.f }, weightsSmoothed { 1.f, 0.f, 0.f, 0.f, 0.f };
    double levelEnv = 0, quietTime = 10, sinceVoiced = 10, heldMidi = 60, loudHold = 0;
    int tick = 0;
    bool wasGated = false;
};

} // namespace choir
