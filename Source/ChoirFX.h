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
    float humanize = 0.7f;        // individual pitch/timing/timbre diversity
    float formant = 0.5f;         // singer-specific spectral character, approximate
    float leadX = 0.f, leadY = 0.4f, leadGain = 1.f;
    float hall = 0.f;                // v0.6: extra hall under the choir (0 = off), 0..0.5
    float hallSeconds = 2.4f;        // its length
};

class PitchTracker {
public:
    void prepare(float sampleRate);
    void reset();
    // Returns true when a NEW valid pitch frame was measured (every ~8 ms while voiced).
    bool push(float sample);
    float midi() const { return currentMidi; }
    float rms() const { return level; }
    float confidence() const { return conf; }   // 0..1, 1 = perfectly periodic
    float latencySeconds() const { return .5f*(half+maxLag)/rate; }   // centre of the analysis window, behind 'now'
    float framePeriodSeconds() const { return (float)hopSamples/rate; }
private:
    // YIN (de Cheveigne & Kawahara 2002) on a ~16 kHz decimated copy: difference function, cumulative-mean
    // normalisation, absolute threshold, parabolic interpolation, 3-frame median.
    static constexpr int capacity = 2048;
    std::array<float,capacity> history {};
    std::array<float,1024> linear {};
    std::array<float,300> cmnd {};
    std::array<float,3> recent {};
    int downsample = 3, divide = 0, write = 0, count = 0, hop = 0, recentCount = 0;
    int hopSamples = 128, half = 512, minLag = 16, maxLag = 246, holdHops = 20;
    float accumulator = 0.f, rate = 16000.f, level = 0.f, currentMidi = -1.f, conf = 0.f;
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
    void processPair(const std::vector<float>& original,const std::vector<float>& excitation,
                     int write,float window,float minDelay,float& raw,float& exc);
    float ratio = 1.f, targetRatio = 1.f;
private:
    float phase = 0.f;
    static float read(const std::vector<float>& data, int write, float samplesBack);
};

class ChoirEffect {
public:
    static constexpr int voicesPerSection = 4;
    static constexpr int numVoices = sections*voicesPerSection;   // 16 independent singer layers

    void prepare(double sampleRate);
    void reset();
    void setParameters(const FXParams&);
    void beginScan(); // audio thread only
    void setSavedScanRoot(int root, bool complete);
    void render(float inL, float inR, float& outL, float& outR);
    // v0.6: the 16 singer layers as separate MONO streams, before any stage processing (no panning, room, mix or lead).
    // One call per input sample; out[0..numVoices-1], section-major (S0..S3, A0..A3, T0..T3, B0..B3). This is what
    // the faithful stage (Source/Stage) places on the virtual stage; render() above keeps the v0.5 simplified stage
    // for the offline tools.
    void processVoices(float mono, float* out);
    // Parameters for the voice path only: no stage panning, no v0.5 room. Singer tone is set as if standing at
    // the microphones: distance darkening belongs to the stage now.
    void setVoiceParameters(const FXParams&);
    // Offline mode locks actual SATB pitches to an edited full-take score.
    // This is driven by the export worker, never from the realtime UI.
    void setScoreVoicing(int melody,const std::array<int,4>& pitches);
    void clearScoreVoicing();
    int scanStatus() const { return status; } // 0 idle, 1 sampling, 2 ready, 3 no voiced input
    float scanProgress() const { return progress; }
    int scanRoot() const { return rootPC; }
    float detectedMidi() const { return tracker.midi(); }
    float inputRms() const { return tracker.rms(); }
private:
    static constexpr int singers = voicesPerSection; // distinct timing, detune, tone, stereo per section
    std::array<GrainVoice, sections*singers> layers;
    std::array<float,sections*singers> singerL {}, singerR {}, singerTone {}, singerLP {}, singerHP {}, singerBaseSemis {};
    // Independent, seeded per-singer expressive wandering. Deterministic across
    // renders and host buffer sizes; control-rate updates never allocate.
    std::array<float,sections*singers> singerPitchWalk {}, singerLevelWalk {}, singerTimeWalk {};
    std::array<float,sections*singers> singerLevelGain {}, singerDelaySeconds {};
    std::array<uint32_t,sections*singers> singerSeeds {};
    static float nextVariation(uint32_t& seed);
    std::array<float,sections> panL {}, panR {}, tone {}, lowPass {};
    std::array<int, 12> pitchHistogram {};
    std::vector<float> inputHistory,excitationHistory;
    static constexpr int lpcOrder=10;
    std::array<float,lpcOrder> lpc {};
    std::array<std::array<float,lpcOrder>,sections*singers> formantState {};
    int formantWrite=0, lpcCounter=0;
    void updateLPC();
    Room room;
    PitchTracker tracker;
    FXParams p;
    float sr = 48000.f, inputEnv = 0, outputEnv = 0, wetNow = 0;
    float window = 3072, minDelay = 480, roomL = 0, roomR = 0;
    int write = 0, status = 0, rootPC = 0, scanSamples = 0, matchedFrames = 0;
    int lfoStep = 0;
    bool scoreVoicing = false;
    int scoreMelody = 60;
    std::array<int,4> scorePitches {67,64,55,48};
    float leadFilterL = 0, leadFilterR = 0, motionSeconds = 0;
    float progress = 0;
    void updateTargetRatios();
    void updateStage();
    void updateTone(const std::array<float,sections>& distance);
};
} // namespace choir
