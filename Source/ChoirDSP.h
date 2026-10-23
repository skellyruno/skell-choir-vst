#pragma once

#include <array>
#include <cstdint>
#include <vector>

// Native, dependency-free DSP core for the OmniChoir VST3 instrument.
// The uploaded web demo contains a stage renderer and playback of PRE-RENDERED
// voices, not its original vocal synthesis engine. This synthesizer is new,
// designed to make the project playable without the missing sound library.
namespace choir {
constexpr int sections = 4;
constexpr int polyphony = 32;
constexpr float pi = 3.14159265358979323846f;

struct Params {
    std::array<float, sections> gain { 0.82f, 0.78f, 0.78f, 0.78f };
    std::array<float, sections> x { -5.1f, -1.7f, 1.7f, 5.1f };
    std::array<float, sections> y { 6.0f, 5.2f, 5.2f, 6.0f };
    float micX = 0.0f, micY = 0.0f;
    int micMode = 1; // 0 AB, 1 ORTF, 2 XY, 3 MS, 4 Decca, 5 Binaural
    float width = 0.6f;
    float wet = 0.32f;
    float roomSeconds = 2.4f;
    float roomSize = 0.52f;
    float absorption = 0.4f;
    float diffusion = 0.7f;
    float master = 0.7f;
    float attack = 0.13f;
    float release = 0.6f;
    float breath = 0.12f;
    float ensemble = 0.48f;
    int vowel = 0; // 0 Ah, 1 Eh, 2 Ee, 3 Oh, 4 Oo
};

class Biquad {
public:
    void setBandpass(float hz, float q, float sr);
    float process(float x);
    void reset();
private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

class Room {
public:
    void prepare(float sampleRate);
    void configure(const Params& p);
    void reset();
    void process(float inL, float inR, float& outL, float& outR);
private:
    struct Line { std::vector<float> samples; int write = 0, length = 100; float low = 0.0f, feedback = 0.7f; };
    struct Allpass { std::vector<float> samples; int write = 0; float gain = 0.5f; float process(float x); void reset(); };
    std::array<Line, 8> lines;
    std::array<Allpass, 4> allpasses;
    float sr = 48000.0f, damping = 0.4f;
    float lastRT = -1, lastSize = -1, lastAbsorb = -1, lastDiffuse = -1;
};

class Instrument {
public:
    void prepare(double sampleRate);
    void reset();
    void setParameters(const Params& params);
    void noteOn(int midiChannel, int note, float velocity);
    void noteOff(int midiChannel, int note);
    void sustain(int midiChannel, bool down);
    void allNotesOff();
    void pitchWheel(int midiChannel, int value14);
    void render(float& left, float& right);
    int activeVoices() const;
private:
    struct Voice {
        bool active = false, keyDown = false, held = false;
        int channel = 1, note = 60, section = 0;
        float hz = 220, amp = 0, gain = 1, velocity = 1;
        float env = 0, delayL = 0, delayR = 0;
        float targetL = 0, targetR = 0, gainL = 0, gainR = 0;
        float currentGainL = 0, currentGainR = 0;
        std::array<float, 3> phase {};
        std::array<Biquad, 3> formants;
        std::vector<float> ring;
        int write = 0;
        uint32_t noise = 1;
        uint64_t age = 0;
    };
    std::array<Voice, polyphony> voices;
    std::array<bool, 16> sustainDown {};
    std::array<float, 16> bend {};
    Params p;
    Room room;
    float sr = 48000.0f, outputGain = 0.0f, wetNow = 0.0f;
    uint64_t voiceClock = 0;
    float rangeWeight(int section, int note) const;
    void spawn(int channel, int note, int section, float velocity, float weight);
    void updateSpatial(Voice& v);
    void retune(Voice& v);
    float generate(Voice& v);
    static float polyBlep(float t, float dt);
};
} // namespace choir
