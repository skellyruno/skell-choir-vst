#pragma once
#include "ChoirDSP.h"
#include "FormantChoir.h"
#include "Scales.h"
#include <array>
#include <cstdint>
#include <vector>

namespace choir {

// Audio-effect parameters. The four stage positions, microphone array and room
// are shared with the supplied OmniChoir demo's physical stage concept.
struct FXParams : Params {
    bool choirOn = true;             // button bypasses wet choir return
    int mode = 0;                  // 0 Live, 1 Scan/Harmony, 2 MIDI chord (the choir sings the notes you hold, from your voice)
    int harmony = 0;               // 0 Major, 1 Minor, 2 Unison (ensemble)
    float mix = 0.78f;             // blend of choir vs. original input
    float sensitivity = 0.65f;     // voice gate sensitivity
    float humanize = 0.7f;        // individual pitch/timing/timbre diversity
    float formant = 0.5f;         // singer-specific spectral character, approximate
    float leadX = 0.f, leadY = 0.4f, leadGain = 1.f;
    float hall = 0.f;                // v0.6: extra hall under the choir (0 = off), 0..0.5
    float hallSeconds = 2.4f;        // its length
    // v0.6 second voice engine (FormantChoir): synthesised singers that follow the lead
    float voiceBlend = 0.f;          // 0 = the granular harmonizer only, 1 = the formant choir only
    float choirVowel = 0.f;          // manual vowel 0..1 across A E I O U (distinct from the legacy Params::vowel)
    float vowelFollow = 0.f;         // how much the lead's own vowel steers the choir
    float choirBreath = .5f;         // breath at phrase starts and a little air through the tone (distinct from Params::breath)
    float vibrato = .6f;             // vibrato depth of the synthesised singers
    bool classicStage = false;       // A/B: the v0.5 simplified stage instead of the faithful one
    float consonants = 0.f;          // v0.8, 0..1: how much of your own consonants (s, t, k...) pass through cleanly instead of being sung by the choir
    float swell = 0.f;               // v0.8, 0..1: the choir blooms when you sing louder than your own average and recedes when softer (faithful stage only)
    int pulseRate = 0;               // v0.8: tempo-synced breathing. 0 off, 1 two bars, 2 one bar, 3 half a bar, 4 one beat, 5 half a beat
    float pulseDepth = .5f;          // 0..1, how far the choir breathes down
    double ppq = -1.0;               // v0.8: the host's position in quarter notes at the start of this block (-1 = unknown)
    double bpm = 0.0;                // v0.8: the host tempo (0 = unknown, 120 is assumed)
    int keyRoot = 0;                 // v0.8: the key you choose, 0 = C .. 11 = B (nothing is detected)
    int scale = 0;                   // v0.8: index into kScales; 0 = Chromatic = off, the fixed v0.7 intervals
    bool lockGrains = false;         // v0.9 opt-in: the granular voices use a window of whole pitch periods (steady notes: far fewer sidebands). Off = the original sound, bit for bit
    bool pitchSync = false;          // opt-in (the STEADY NOTES button): the harmony voices shift with period-synchronous jumps instead of two free grains (see SyncVoice): clean through vibrato, ~30 ms less lag. Off = the original sound, bit for bit
    std::array<int,4> sectionTone {};  // v0.9: per section 0 normal, 1 bright, 2 dark (a gentle high shelf on that section's singers; 0 is bypassed exactly)
    float toneAmount = 1.f;          // v0.9: how strong the T keys are, 0..1.6 (1 = as designed: +3.4 / -3.4 dB at 4-7 kHz; 0 = no tone)
    float phraseBreath = 0.f;        // v0.9, 0..1: a soft inhale from the choir as you start singing after a pause (0 = off, bit-identical)
    float vibratoSpread = 0.f;       // v0.9, 0..1: each harmony singer copies your vibrato at their own depth (0 = every voice copies it exactly, the original sound)
    bool levelMatch = false;         // v0.7: keep the output loudness steady as CHOIR MIX moves (faithful stage only)
    std::array<int,4> midiNotes {{-1,-1,-1,-1}};   // v0.7, mode 2: the MIDI note each section sings (soprano..bass), -1 = no note held
    float choirLevelDb = 0.f;        // v0.6.1: choir level in dB, -12..+12, faithful stage only (the classic stage stays bit-identical to v0.5)
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
    float pendingMidi = -1.f; int pendingCount = 0, steady = 0;   // a big leap is believed only once it has stayed put (see analyse)
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
    float vibMul = 1.f;                       // a per-singer multiplier on the read speed that bends the copied vibrato (1 = off, exactly)
private:
    float phase = 0.f;
    static float read(const std::vector<float>& data, int write, float samplesBack);
};

// What a SyncVoice needs to know about the lead at this instant.
struct SyncContext {
    float sampleRate = 48000.f;
    float period = 0.f;                       // the lead's pitch period in samples, 0 = not clearly pitched (consonant, breath, noise)
    const std::vector<float>* mark = nullptr; // low-passed copy of the input history (same indexing): what the seams are aligned on
};

// Period-synchronous variable-delay pitch shifter (opt-in, FXParams::pitchSync).
//
// The two-grain shifter (GrainVoice) reads the input at speed `ratio` through two Hann grains half a window apart, so the two reads are
// out of phase and beat at the grain rate (~7-23 Hz), and its window cannot follow vibrato (see docs/V0_9_CHANGELOG.md, STEADY NOTES).
// This one keeps a single read pointer that moves at exactly `ratio` (so the pitch is exact, vibrato included) and, when the delay
// leaves its small window, JUMPS by a whole number of the singer's periods. Each jump is refined by cross-correlation (+-15% of a
// period) so the old and new read points carry the same waveform phase, then cross-faded over about one period. The two signals in the
// fade are therefore nearly identical and add instead of beating, whatever the pitch is doing. The delay stays within roughly
// 5..25 ms instead of 9..82 ms, so the choir also trails the lead by about 30 ms less. Unpitched input (a breath, an 's') is jumped by a
// fixed 6 ms with an equal-power fade. Like GrainVoice it resamples, so formants move with the pitch (CHARACTER's LPC path is unchanged).
class SyncVoice {
public:
    void reset(float seed01);
    void processPair(const std::vector<float>& original, const std::vector<float>& excitation, int write, float baseDelay,
                     const SyncContext& ctx, float& raw, float& exc);
    float ratio = 1.f, targetRatio = 1.f;
    float vibMul = 1.f;                       // a per-singer multiplier on the read speed that bends the copied vibrato (1 = off, exactly)
    int jumps() const { return jumpCount; }   // for tests: how many seams this voice has made
    int pitchedJumps() const { return pitchedCount; }   // ...how many of them were aligned on a pitched waveform
    double meanSeamRho() const { return pitchedCount ? rhoSum/pitchedCount : 0.0; }   // ...and how alike the two read points were (1 = identical)
private:
    void jump(const std::vector<float>& original, int write, float baseDelay, const SyncContext& ctx, float low, float span, float period, bool voiced, float fadeLen);
    double delay = 0, oldDelay = 0;
    float fade = 1.f, fadeInc = 0.f, seed = 0.f;
    float rho = 0.f;      // how alike the old and new read points are (0..1): the cross-fade keeps the POWER constant for that correlation
    bool started = false;
    int jumpCount = 0, pitchedCount = 0;
    double rhoSum = 0;
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
    float pitchConfidence() const { return tracker.confidence(); }   // 0..1, how clearly the lead is singing a pitch right now
private:
    static constexpr int singers = voicesPerSection; // distinct timing, detune, tone, stereo per section
    std::array<GrainVoice, sections*singers> layers;
    std::array<SyncVoice, sections*singers> syncLayers;   // the pitchSync engine; layers[] is untouched and bit-identical while syncMix is 0
    std::vector<float> markHistory;                       // input history low-passed at ~1 kHz, for aligning the seams
    float markLp1 = 0.f, markLp2 = 0.f, markCoef = .1f;
    float syncMix = 0.f, syncPeriod = 0.f;                // 0 = GrainVoice only, 1 = SyncVoice only (a 20 ms cross-fade between them when the button is pressed)
    bool fresh = true;                                    // no sample processed since reset(): a new setting takes effect at once
    float vibSlow = -1.f, vibDev = 0.f; bool vibActive = false; std::array<float,8192> vibRing {}; int vibPos = 0;   // the lead's slow pitch and its fast deviation from it (semitones), for vibratoSpread
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
    FormantChoir formant;
    float blendNow = 0.f;
    FXParams p;
    float sr = 48000.f, inputEnv = 0, outputEnv = 0, wetNow = 0;
    float window = 3072, minDelay = 480, roomL = 0, roomR = 0;
    float windowNow = 3072, lockedWindow = 3072;   // lockGrains: the window in use glides to a whole even number of pitch periods (see steerWindow)
    void steerWindow();
public:
    static constexpr float kMaxWindowSeconds = .085f, kWindowSlewSamplesPerSample = .002f;   // .2% pitch error at most while the window moves
    float windowInUse() const { return windowNow; }
    int syncJumps(int voice) const { return syncLayers[(size_t)voice].jumps(); }            // tests: seams the pitchSync engine has made (0 while it is off)
    int syncPitchedJumps(int voice) const { return syncLayers[(size_t)voice].pitchedJumps(); }
    double syncSeamCorrelation(int voice) const { return syncLayers[(size_t)voice].meanSeamRho(); }
    // the window (in samples) that makes the two grains' read points a whole number of periods apart: an even number of periods near 73 ms, never longer than the history allows
    static float lockedWindowFor(float f0, float sampleRate) {
        const float periods = std::max(2.f, 2.f * std::round(.073f * f0 * .5f));
        return std::min(kMaxWindowSeconds * sampleRate, periods / f0 * sampleRate);
    }
private:
    int write = 0, status = 0, rootPC = 0, scanSamples = 0, matchedFrames = 0;
    int lfoStep = 0;
    bool scoreVoicing = false;
    int scoreMelody = 60;
    int liveMelody = 60;   // the sung note the key-aware harmony is measured from (rounded, with hysteresis)
    bool keyedValid = false;   // the key-aware intervals have been computed at least once
    int midiMelody = 60;   // mode 2: the sung note the chord intervals are measured from (rounded, with hysteresis)
    std::array<int,4> scorePitches {67,64,55,48};
    float leadFilterL = 0, leadFilterR = 0, motionSeconds = 0;
    float progress = 0;
    void updateTargetRatios();
    void updateStage();
    void updateTone(const std::array<float,sections>& distance);
};
} // namespace choir
