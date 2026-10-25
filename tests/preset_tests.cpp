#undef NDEBUG   // these tests use assert(); a Release build must still check
#include "../Source/StagedChoir.h"
#include "../Source/StagePresets.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace choir;
static const double kPi = 3.14159265358979323846;
static bool inside(float v, ParamRange r) { return v >= r.lo && v <= r.hi; }
int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const double sr = 48000;
    // 1. every value of every preset is inside the real parameter range
    for (int i = 0; i < kNumStagePresets; ++i)
    {
        const auto& s = kStagePresets[i];
        for (int k = 0; k < 4; ++k) { assert(inside(s.x[k], kRangeX)); assert(inside(s.y[k], kRangeY)); assert(inside(s.gain[k], kRangeGain)); }
        assert(inside(s.micX, kRangeMicX) && inside(s.micY, kRangeMicY) && inside(s.width, kRangeWidth));
        assert(s.micMode >= 0 && s.micMode <= 5);
        assert(inside(s.roomSize, kRangeUnit) && inside(s.absorption, kRangeUnit) && inside(s.diffusion, kRangeUnit) && inside(s.ensemble, kRangeUnit) && inside(s.humanize, kRangeUnit));
        assert(inside(s.roomSeconds, kRangeRoomSeconds) && inside(s.wet, kRangeWet) && inside(s.hall, kRangeHall) && inside(s.hallSeconds, kRangeHallSeconds));
        assert(std::strlen(s.name) > 2 && std::strlen(s.about) > 10);
        for (int j = i + 1; j < kNumStagePresets; ++j) assert(std::strcmp(s.name, kStagePresets[j].name) != 0);
        // sections keep their order left to right (S A T B) so the choir is not mirrored
        assert(s.x[0] < s.x[1] && s.x[1] < s.x[2] && s.x[2] < s.x[3]);
    }
    std::printf("[1] %d presets, all values within the parameter ranges\n", kNumStagePresets);
    // 2. every preset renders: audible, bounded, level near the default, distinct from the others
    std::vector<float> x((size_t) (6 * sr)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i) { double t = i / sr; ph += 2 * kPi * 196 * (1 + .004 * std::sin(2 * kPi * 5.3 * t)) / sr; double s = 0; for (int h = 1; h <= 10; ++h) s += std::sin(h * ph) / h; x[i] = (float) (.22 * s); }
    double ref = 0; std::printf("[2] %-16s %9s %9s %8s %7s\n", "preset", "level dB", "peak", "width", "tail");
    double widthMin = 1, widthMax = 0, tailMin = 1e9, tailMax = 0;
    for (int i = 0; i < kNumStagePresets; ++i)
    {
        FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; applyStagePreset(kStagePresets[i], p);
        StagedChoir sc; sc.prepare(sr); sc.setParameters(p);
        std::vector<float> l(x.size() + (size_t) (3 * sr)), r(l.size()), in(l.size(), 0.f); std::copy(x.begin(), x.end(), in.begin());
        for (size_t k = 0; k < in.size(); k += 512) { int n = (int) std::min<size_t>(512, in.size() - k); sc.process(&in[k], &in[k], &l[k], &r[k], n); }
        double e = 0, tail = 0; float pk = 0; for (size_t k = (size_t) (2 * sr); k < (size_t) (5.5 * sr); ++k) { e += (double) l[k] * l[k]; pk = std::max(pk, std::fabs(l[k])); } e = std::sqrt(e / (3.5 * sr));
        for (size_t k = (size_t) (7 * sr); k < (size_t) (7.5 * sr); ++k) tail += (double) l[k] * l[k];
        tail = std::sqrt(tail / (.5 * sr));
        for (float v : l) assert(std::isfinite(v));
        const double db = 20 * std::log10(e);
        if (i == 0) ref = db;
        const float w = sc.meters.width.load();
        std::printf("[2] %-16s %+8.1f %9.3f %8.3f %7.5f\n", kStagePresets[i].name, db, pk, w, tail);
        assert(pk <= 1.0001f); assert(e > 1e-3); assert(std::fabs(db - ref) < 9.0);
        widthMin = std::min(widthMin, (double) w); widthMax = std::max(widthMax, (double) w); tailMin = std::min(tailMin, tail); tailMax = std::max(tailMax, tail);
    }
    std::printf("[2] across presets: stereo width %.2f-%.2f, 2-s-after tail %.5f-%.5f\n", widthMin, widthMax, tailMin, tailMax);
    assert(tailMax > 5 * tailMin);       // the rooms really differ: the longest tail rings far more than the driest
    std::printf("ALL PRESET TESTS PASSED\n");
}
