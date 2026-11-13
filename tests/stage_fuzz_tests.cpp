// Random room, mic and lead changes while audio runs (odd block sizes included): the output must stay finite and bounded, and must not fall
// into denormals. Every preset is applied, then parameters are thrown at the stage at random, including the extremes of every range.
// Usage: stage_fuzz_tests
#include "../Source/StagedChoir.h"
#include "../Source/StagePresets.h"
#include "test_support.h"
#include <random>
using namespace choir;
int main()
{
    const double sr = 48000; std::mt19937 rng(12345);
    auto uni = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    auto pick = [&](float a, float b) { return rng() % 4 == 0 ? (rng() % 2 ? a : b) : uni(a, b); };   // a quarter of the draws sit on a limit
    StagedChoir sc; sc.prepare(sr); FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .4f;
    double peak = 0; long samples = 0, denormals = 0; int changes = 0; double ph = 0; const int sizes[] = { 1, 7, 64, 100, 480, 512, 1024, 2048 };
    for (int round = 0; round < 90; ++round)
    {
        if (round % 3 == 0) applyStagePreset(kStagePresets[(size_t) (rng() % kNumStagePresets)], p);
        else
        {
            p.micX = pick(-9.f, 9.f); p.micY = pick(-8.f, 8.f); p.width = uni(.2f, 1.f); p.micMode = (int) (rng() % 6);
            p.leadX = pick(-24.f, 24.f); p.leadY = pick(-16.f, 22.f); p.leadGain = rng() % 5 == 0 ? 0.f : uni(0.f, 1.5f);
            p.roomSize = pick(.1f, 1.f); p.roomSeconds = pick(.2f, 8.f); p.absorption = uni(0.f, 1.f); p.diffusion = uni(0.f, 1.f); p.wet = pick(0.f, 1.f);
            p.hall = pick(0.f, .5f); p.hallSeconds = pick(.5f, 8.f);
            for (int s = 0; s < 4; ++s) { p.x[(size_t) s] = pick(-9.f, 9.f); p.y[(size_t) s] = pick(0.f, 12.f); p.gain[(size_t) s] = rng() % 6 == 0 ? 0.f : uni(0.f, 1.5f); }
        }
        ++changes;
        for (int b = 0; b < 6; ++b)
        {
            const int n = sizes[rng() % 8]; std::vector<float> in((size_t) n), l((size_t) n), r((size_t) n);
            for (int i = 0; i < n; ++i) { ph += 2 * 3.14159265358979 * (160 + 60 * std::sin(samples * 2e-5)) / sr; double s = 0; for (int h = 1; h <= 8; ++h) s += std::sin(h * ph) / h; in[(size_t) i] = round % 7 == 6 ? 0.f : (float) (.25 * s); ++samples; }
            sc.setParameters(p); sc.process(in.data(), in.data(), l.data(), r.data(), n);
            for (int i = 0; i < n; ++i)
                for (float v : { l[(size_t) i], r[(size_t) i] })
                {
                    CHECK(std::isfinite(v), "non-finite output after change %d (block %d, size %d)", changes, b, n);
                    if (std::fpclassify(v) == FP_SUBNORMAL) ++denormals;
                    peak = std::max(peak, (double) std::fabs(v));
                }
        }
    }
    std::printf("[1] %d random changes, %ld samples: peak %.3f, %ld denormal output samples\n", changes, samples, peak, denormals);
    CHECK(peak < 1.5, "output peaked at %.3f", peak);
    CHECK(denormals == 0, "%ld denormal samples reached the output", denormals);
    std::printf("ALL STAGE FUZZ TESTS PASSED\n");
    return 0;
}
