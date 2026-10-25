#pragma once
// The live effect with the faithful stage. JUCE-free so it is testable natively and reusable offline.
//
//   mono input --> ChoirEffect::processVoices  16 mono singers (granular harmonizer and/or formant choir)
//              --> oc::ChoirStage   a C++ port of choir-stage.js: mic arrays, per-singer delay + air absorption,
//                                   image-source early reflections, 8-line FDN, spot-mic follow, top-end shelf
//              --> oc::Hall         extra hall under the choir: algorithmic, or a loaded impulse response
//   lead input --> oc::LeadStage    the lead, dry at home, heard where it stands once moved
//   final      = ceiling( master * ( (1-mix) * lead * leadGain + mix * choir ) )
//
// Extras: per-section stem outputs, meters, and an A/B switch to the v0.5 classic stage.
//
// The stage works in fixed 128-sample blocks, so the whole effect (the dry lead included, to stay time-aligned with
// the choir) is delayed by `latencySamples` and the host is told so. The classic stage is delayed to match, so A/B
// never changes the latency the host was told.
#include "ChoirFX.h"
#include "Stage/Stage.h"
#include <atomic>
#include <memory>
#include <mutex>

namespace choir {

class StagedChoir {
public:
    static constexpr int block = 128;
    static constexpr int latencySamples = block;
    // v0.5 parameter semantics are kept; these map them onto the stage.
    static constexpr float kSectionUnity = 0.83f;   // section gain that now means "0 dB" (the v0.5 default)
    static constexpr float kWetUnity = 0.37f;       // ROOM RETURN that now means "reverb 1.0" (the v0.5 default)
    static constexpr float kVoiceTrim = 0.0466f;    // -26.6 dB: the stage is physically scaled; this puts the choir where v0.5 was (tests/calibrate_trim.cpp)
    static constexpr float kIrEnergy = 1.18f;       // loaded impulse responses are scaled to this energy: the same loudness as the algorithmic hall on noise
    static constexpr int numStems = 5;              // sopranos, altos, tenors, basses, lead

    struct StemOut { float* left = nullptr; float* right = nullptr; };

    // Readable from any thread (relaxed atomics, updated once per 128-sample block).
    struct Meters {
        std::atomic<float> section[4] { 0.f, 0.f, 0.f, 0.f };   // RMS of each section's singers, before the stage
        std::atomic<float> leadIn { 0.f };                      // peak of the lead input
        std::atomic<float> outPeakL { 0.f }, outPeakR { 0.f };  // output peaks (decay ~0.3 s)
        std::atomic<float> correlation { 1.f };                 // -1..1, +1 = mono, 0 = wide, negative = out of phase
        std::atomic<float> width { 0.f };                       // 0..1, side energy share of the output
    };
    Meters meters;

    StagedChoir() = default;
    ~StagedChoir();
    StagedChoir(const StagedChoir&) = delete; StagedChoir& operator=(const StagedChoir&) = delete;

    void prepare(double sampleRate);
    void reset();
    void setParameters(const FXParams&);   // call from the audio thread, once per host block

    // inL/inR may alias outL/outR. Any n. stems (optional, numStems entries) receive the section stems and the placed
    // lead, time-aligned with the main output; each section stem is that section heard alone through the same stage
    // (so the four do not sum exactly to the main mix, whose spot microphones and shelf act on all sections together).
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, const StemOut* stems = nullptr);

    // --- impulse-response hall (call from the message thread; safe while audio runs)
    // right may be null (mono IR). length 0 clears it. The IR is resampled to the engine rate, trimmed to
    // Convolver::kMaxSeconds, faded out and level-matched to the algorithmic hall.
    void setImpulseResponse(const float* left, const float* right, int length, double irSampleRate);
    bool hasImpulseResponse() const { std::lock_guard<std::mutex> g(irMutex); return !irLeft.empty(); }
    void collectGarbage();

    ChoirEffect& voices() { return fx; }
    const ChoirEffect& voices() const { return fx; }

private:
    void renderBlock();
    void applyTo(oc::ChoirStage& s, bool all, int onlySection);
    void adoptPendingImpulseResponse();
    void rebuildImpulseResponse();
    void retire(oc::ConvPair* p);

    ChoirEffect fx;
    std::unique_ptr<oc::ChoirStage> stage;
    std::unique_ptr<oc::ChoirStage> sectionStage[4];
    std::unique_ptr<oc::LeadStage> lead;
    std::unique_ptr<oc::Hall> hall;
    FXParams p;
    double sr = 48000;

    static constexpr int N = ChoirEffect::numVoices;
    std::vector<std::vector<float>> voiceBuf;     // [N][block]: what the singers sang this block
    std::vector<const float*> voicePtr;
    std::vector<const float*> sectionPtr[4];
    float inL_[block] {}, inR_[block] {}, outL_[block] {}, outR_[block] {};
    float classicL_[block] {}, classicR_[block] {};
    float choirL[block] {}, choirR[block] {}, leadL[block] {}, leadR[block] {};
    float stemL_[numStems][block] {}, stemR_[numStems][block] {}, stemOutL_[numStems][block] {}, stemOutR_[numStems][block] {};
    float scratchL[block] {}, scratchR[block] {};
    int pos = 0;
    float wetNow = 0, leadGainNow = 1;
    bool lastClassic = false, stemsWanted = false, lastStems = false;
    float corr = 1.f, wid = 0.f, peakL = 0.f, peakR = 0.f;

    // impulse response: the raw data (message thread) and the lock-free handoff to the audio thread
    std::vector<float> irLeft, irRight; double irRate = 48000;
    mutable std::mutex irMutex;   // message thread (load/clear) vs prepare(); never taken on the audio thread
    std::atomic<oc::ConvPair*> pendingIr { nullptr };
    std::atomic<bool> clearIr { false };
    std::atomic<oc::ConvPair*> graveyard[4] { nullptr, nullptr, nullptr, nullptr };
};

} // namespace choir
