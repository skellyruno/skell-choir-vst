#pragma once
// v0.6: the live effect with the faithful stage. JUCE-free so it is testable natively and reusable offline.
//
//   mono input --> ChoirEffect::processVoices (16 granular/LPC singer layers, mono each)
//              --> oc::ChoirStage  (a C++ port of choir-stage.js: mic arrays, per-singer delay + air absorption,
//                                   image-source early reflections, 8-line FDN, spot-mic follow, top-end shelf)
//              --> oc::Hall        (extra longer hall under the choir)
//   lead input --> oc::LeadStage   (the white lead, dry at home, heard where it stands once moved)
//   final      = ceiling( master * ( (1-mix) * lead * leadGain + mix * choir ) )
//
// The stage works in fixed 128-sample blocks, so the whole effect (the dry lead included, to stay time-aligned with
// the choir) is delayed by `latencySamples` and the host is told so.
#include "ChoirFX.h"
#include "Stage/Stage.h"
#include <memory>

namespace choir {

class StagedChoir {
public:
    static constexpr int block = 128;
    static constexpr int latencySamples = block;
    // v0.5 parameter semantics are kept; these map them onto the stage.
    static constexpr float kSectionUnity = 0.83f;   // section gain that now means "0 dB" (the v0.5 default)
    static constexpr float kWetUnity = 0.37f;       // ROOM RETURN that now means "reverb 1.0" (the v0.5 default)
    static constexpr float kVoiceTrim = 0.0466f;    // -26.6 dB: the stage is physically scaled; this puts the choir where v0.5 was (tests/calibrate_trim.cpp)

    void prepare(double sampleRate);
    void reset();
    void setParameters(const FXParams&);   // call from the audio thread, once per host block

    // inL/inR may alias outL/outR. Any n.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

    ChoirEffect& voices() { return fx; }
    const ChoirEffect& voices() const { return fx; }

private:
    void renderBlock();

    ChoirEffect fx;
    std::unique_ptr<oc::ChoirStage> stage;
    std::unique_ptr<oc::LeadStage> lead;
    std::unique_ptr<oc::Hall> hall;
    FXParams p;
    double sr = 48000;

    static constexpr int N = ChoirEffect::numVoices;
    std::vector<std::vector<float>> voiceBuf;     // [N][block]: what the singers sang this block
    std::vector<const float*> voicePtr;
    float inL_[block] {}, inR_[block] {}, outL_[block] {}, outR_[block] {};
    float choirL[block] {}, choirR[block] {}, leadL[block] {}, leadR[block] {};
    int pos = 0;
    float wetNow = 0, leadGainNow = 1;
};

} // namespace choir
