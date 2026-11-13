// Moving the microphone away from the choir must sound like moving away: the direct voice gets quieter and the room (the tail) takes over,
// so the tail-to-direct ratio rises with distance; coming closer does the opposite. The tail alone does not simply fade with distance (it
// is the room's diffuse field). Source/Stage/Stage.h kTailDistanceExponent. At the preset's own mic spot nothing changes (golden tests).
#include "../Source/StagedChoir.h"
#include "../Source/StagePresets.h"
#include "test_support.h"
using namespace choir;
struct Result { double directDb, tailDb; };
static Result measure(int preset, float micY)
{
    const double sr = 48000; const int B = 512;
    StagedChoir sc; sc.prepare(sr); FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; applyStagePreset(kStagePresets[preset], p); p.micY = micY; sc.setParameters(p);
    std::vector<float> in(B), l(B), r(B); double ph = 0, e1 = 0, e2 = 0; long n1 = 0, n2 = 0;
    for (int k = 0; k < (int) (3 * sr / B); ++k)
    {
        for (int i = 0; i < B; ++i) { const double t = (k * B + i) / sr; double s = 0; ph += 2 * 3.14159265358979 * 196 / sr; if (t < 1.0) for (int h = 1; h <= 10; ++h) s += std::sin(h * ph) / h; in[(size_t) i] = (float) (.22 * s); }
        sc.setParameters(p); sc.process(in.data(), in.data(), l.data(), r.data(), B);
        for (int i = 0; i < B; ++i) { const double t = (k * B + i) / sr, v = l[(size_t) i] * l[(size_t) i] + r[(size_t) i] * r[(size_t) i]; if (t > .5 && t < 1.0) { e1 += v; ++n1; } else if (t > 1.3 && t < 1.8) { e2 += v; ++n2; } }
    }
    const double d = 10 * std::log10(e1 / n1 + 1e-12), t = 10 * std::log10(e2 / n2 + 1e-12);
    return { d, t - d };
}
int main()
{
    for (int preset : { 0, 1 })
    {
        const char* name = kStagePresets[preset].name;
        const Result far = measure(preset, -8.f), mid = measure(preset, -4.f), home = measure(preset, 0.f), close = measure(preset, 4.f);
        std::printf("  %-10s ratio(tail-direct, dB): far %.1f  mid %.1f  home %.1f  close %.1f   direct dB: far %.1f home %.1f\n", name, far.tailDb, mid.tailDb, home.tailDb, close.tailDb, far.directDb, home.directDb);
        if (preset == 0) CHECK(far.directDb < home.directDb - 2.0, "%s: the voice (level while singing; in a wet hall the room dominates it, so only the dry default is held to this) should be quieter from 8 m further away (%.1f vs %.1f dB)", name, far.directDb, home.directDb);
        CHECK(far.tailDb > mid.tailDb + 1.0 && mid.tailDb > home.tailDb + 1.0, "%s: the tail should take over as the mic moves away (far %.1f mid %.1f home %.1f)", name, far.tailDb, mid.tailDb, home.tailDb);
        CHECK(far.tailDb > home.tailDb + 4.0, "%s: 8 m further away should add at least 4 dB of room against the voice (%.1f)", name, far.tailDb - home.tailDb);
        CHECK(close.tailDb < home.tailDb, "%s: coming closer should be drier (%.1f vs %.1f)", name, close.tailDb, home.tailDb);
    }
    std::printf("ALL MIC DISTANCE TESTS PASSED\n");
    return 0;
}
