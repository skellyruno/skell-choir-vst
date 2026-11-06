#pragma once
// v0.8: CONSONANTS. Sixteen voices singing "s", "t" and "k" smear them: the pitch shifters turn a burst of noise into a washy cloud.
// This finds the consonants in the voice (broadband noise bursts: much of their energy is above ~3 kHz, unlike a sung vowel), so the
// stage can feed the choir the voice WITHOUT the consonant and pass the original consonant through cleanly instead.
// Calibrated on a real low male vocal (tests/data and the full takes it came from): the detector's high-frequency ratio is 0.05 for a sung
// vowel (99.9% of frames under 0.21) and 0.47 for a consonant (median; 10th percentile 0.30). A second cue guards against bright vowels:
// a consonant has little energy below ~1.2 kHz, a sung vowel (however bright) has most of it there.
// JUCE-free; tested in tests/consonant_tests.cpp.
#include <algorithm>
#include <cmath>

namespace choir {

class ConsonantDetector
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        // two cascaded one-pole high-passes at ~3.2 kHz: 12 dB per octave, enough to separate noise bursts from sung vowels
        highPass = std::exp(-2.0 * 3.14159265358979323846 * 3200.0 / sr);
        envUp = 1.0 - std::exp(-1.0 / (.0005 * sr));       // all envelopes follow quickly upward (0.5 ms)...
        envDown = 1.0 - std::exp(-1.0 / (.008 * sr));      // ...and relax over 8 ms
        gateUp = 1.0 - std::exp(-1.0 / (.0015 * sr));      // the consonant score rises in 1.5 ms
        gateDown = 1.0 - std::exp(-1.0 / (.012 * sr));     // and falls in 12 ms
        low = std::exp(-2.0 * 3.14159265358979323846 * 1200.0 / sr);
        reset();
    }
    void reset() { lp1 = lp2 = lowState = 0; all = high = lowEnv = 0; score = 0; }

    // One sample in, the consonant amount out: 0 = sung or silent, 1 = clearly a consonant.
    float process(float x)
    {
        lp1 = (float) (highPass * lp1 + (1.0 - highPass) * x);  const float h1 = x - lp1;      // first high-pass
        lp2 = (float) (highPass * lp2 + (1.0 - highPass) * h1); const float h2 = h1 - lp2;     // second
        lowState = (float) (low * lowState + (1.0 - low) * x);
        const float a = std::fabs(x), b = std::fabs(h2), c = std::fabs(lowState);
        all += (float) ((a > all ? envUp : envDown) * (a - all));
        high += (float) ((b > high ? envUp : envDown) * (b - high));
        lowEnv += (float) ((c > lowEnv ? envUp : envDown) * (c - lowEnv));
        // the share of the energy that is high: a sung vowel measures 0.05 (0.21 at the very worst), a consonant 0.3 to 0.6
        const float ratio = high / std::max(all, 1.0e-5f), lowShare = lowEnv / std::max(all, 1.0e-5f);
        const float highScore = std::min(1.f, std::max(0.f, (ratio - .22f) / .18f));
        const float guard = std::min(1.f, std::max(0.f, (.88f - lowShare) / .30f));      // a vowel keeps most of its energy below 1.2 kHz, a consonant does not
        const float target = all > .003f ? highScore * guard : 0.f;
        score += (float) ((target > score ? gateUp : gateDown) * (target - score));
        return score;
    }
private:
    double sr = 44100, highPass = 0, envUp = 0, envDown = 0, gateUp = 0, gateDown = 0;
    double low = 0;
    float lp1 = 0, lp2 = 0, lowState = 0, all = 0, high = 0, lowEnv = 0, score = 0;
};

} // namespace choir
