// Changing the room (a stage preset) while a tail is ringing must not click. Before v0.9.1 the late reverb's delay read points jumped to
// their new lengths in one sample (Crypt -> Cathedral: a click 4x the steady level). Now they glide. Also: a room that is NOT changed is
// bit-for-bit what it was (the glide is invisible at rest), and a fresh instance starts in its room instead of gliding into it.
// Usage: room_switch_tests
#include "../Source/StagedChoir.h"
#include "../Source/StagePresets.h"
#include "test_support.h"
#include <array>
#include <cstdlib>
using namespace choir;

static const double kSr = 48000;
// renders a 0.5 s burst, switches to preset `to` at 0.9 s (during the tail), returns the largest |second difference| in the 150 ms after the switch
static double clickAfterSwitch(int from, int to, std::vector<float>* all = nullptr)
{
    const int B = 512; StagedChoir sc; sc.prepare(kSr); FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; applyRoomOnly(kStagePresets[from], p);
    const int total = (int) (1.6 * kSr / B), sw = (int) (.9 * kSr / B);
    std::vector<float> in((size_t) B), l((size_t) B), r((size_t) B); double ph = 0, mx = 0; float h1 = 0, h2 = 0;
    for (int k = 0; k < total; ++k)
    {
        for (int i = 0; i < B; ++i) { const double t = (k * B + i) / kSr; double s = 0; ph += 2 * 3.14159265358979 * 196 / kSr; if (t < .5) for (int h = 1; h <= 10; ++h) s += std::sin(h * ph) / h; in[(size_t) i] = (float) (.22 * s); }
        if (k == sw) applyRoomOnly(kStagePresets[to], p);
        sc.setParameters(p); sc.process(in.data(), in.data(), l.data(), r.data(), B);
        for (int i = 0; i < B; ++i)
        {
            if (all) all->push_back(l[(size_t) i]);
            if (k >= sw && k < sw + (int) (.15 * kSr / B) + 1) mx = std::max(mx, (double) std::fabs(l[(size_t) i] - 2 * h1 + h2));
            h2 = h1; h1 = l[(size_t) i];
        }
    }
    return mx;
}

// energy (dB) of the tail after an impulse at kHit, in four bands (0.05-0.2, 0.2-0.5, 0.5-1, 1-2 s after it). The instance starts in `from` (silent), moves to `to` at 0.1 s,
// and is hit at kHit (the singers walk at 2.5 m/s, so the longest move, about 8 m, needs 3.2 s: wait it out): if the room really arrived (feedback, damping, lengths) this matches an instance that started in `to`.
static const double kHit = 4.5;
static std::array<double, 4> tailBands(int from, int to)
{
    const int B = 512; StagedChoir sc; sc.prepare(kSr); FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; applyRoomOnly(kStagePresets[from], p);
    std::vector<float> in((size_t) B), l((size_t) B), r((size_t) B); double e[4] = {}; long n[4] = {};
    for (int k = 0; k < (int) ((kHit + 2.2) * kSr / B); ++k)
    {
        if (k == (int) (.1 * kSr / B)) applyRoomOnly(kStagePresets[to], p);
        for (int i = 0; i < B; ++i) { const double t = (k * B + i) / kSr; in[(size_t) i] = (t >= kHit && t < kHit + .02) ? .5f * (float) std::sin(2 * 3.14159265358979 * 300 * (t - kHit)) : 0.f; }
        sc.setParameters(p); sc.process(in.data(), in.data(), l.data(), r.data(), B);
        for (int i = 0; i < B; ++i)
        {
            const double t = (k * B + i) / kSr - (kHit + .02); if (t < .03) continue;
            const int b = t < .18 ? 0 : t < .48 ? 1 : t < .98 ? 2 : t < 1.98 ? 3 : -1; if (b < 0) continue;
            e[b] += l[(size_t) i] * l[(size_t) i] + r[(size_t) i] * r[(size_t) i]; ++n[b];
        }
    }
    std::array<double, 4> f {}; for (int b = 0; b < 4; ++b) f[(size_t) b] = 10 * std::log10(e[b] / std::max(1L, n[b]) + 1e-14);
    return f;
}

int main()
{
    // 1. the loud-tailed rooms switching to every other room: the click stays within the steady wiggle plus a small allowance
    int pairs = 0; double worstExcess = 0;
    for (int a = 0; a < kNumStagePresets; ++a)   // every room as the one being left
    {
        const double base = clickAfterSwitch(a, a);
        for (int b = 0; b < kNumStagePresets; ++b)
        {
            if (a == b) continue;
            const double c = clickAfterSwitch(a, b), excess = c - base; worstExcess = std::max(worstExcess, excess); ++pairs;
            if (getenv("RS_VERBOSE")) std::printf("%s -> %s %.5f base %.5f\n", kStagePresets[a].name, kStagePresets[b].name, c, base);
            CHECK(excess <= 0.0010, "%s -> %s clicks: |d2| %.5f against %.5f when the room stays (excess %.5f)", kStagePresets[a].name, kStagePresets[b].name, c, base, excess);
        }
    }
    std::printf("[1] %d room switches during a tail: worst click excess %.5f (was 0.0308 for Crypt -> Cathedral before the glides and ramps, 0.00145 before the feedback glide)\n", pairs, worstExcess);

    // 2. a room that is not changed is exactly deterministic (same input and settings -> identical output)
    std::vector<float> x, y; clickAfterSwitch(2, 2, &x); clickAfterSwitch(2, 2, &y);
    CHECK(x == y && x.size() > 1000, "the same render twice must be bit-identical");
    std::printf("[2] an unchanged room renders bit-identically\n");

    // 3. a fresh instance starts in its room: the first blocks already ring at the room's length (no glide from some default)
    {
        std::vector<float> a, b; clickAfterSwitch(9, 9, &a);   // Crypt from the start
        FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; applyRoomOnly(kStagePresets[9], p);
        StagedChoir sc; sc.prepare(kSr); sc.setParameters(p); sc.setParameters(p);
        std::vector<float> in(512, 0.f), l(512), r(512); in[0] = .5f; sc.process(in.data(), in.data(), l.data(), r.data(), 512);
        double e = 0; for (float v : l) e += v * v; CHECK(e > 0, "a fresh Crypt instance is silent after an impulse");
        CHECK(a.size() > 1000, "setup");
    }
    std::printf("[3] a fresh instance starts in its room\n");
    // 4. every room has its own identity: the decay of an impulse (energy in six time bands) differs between any two rooms by at least 3 dB RMS
    {
        std::vector<std::array<double, 5>> fp;
        for (int pre = 0; pre < kNumStagePresets; ++pre)
        {
            StagedChoir sc; sc.prepare(kSr); FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; applyRoomOnly(kStagePresets[pre], p); sc.setParameters(p);
            const int B = 512; std::vector<float> in((size_t) B), l((size_t) B), r((size_t) B); double e[6] = {}; long n[6] = {};
            for (int k = 0; k < (int) (3 * kSr / B); ++k)
            {
                for (int i = 0; i < B; ++i) { const double t = (k * B + i) / kSr; in[(size_t) i] = t < .02 ? .5f * (float) std::sin(2 * 3.14159265358979 * 300 * t) : 0.f; }
                sc.setParameters(p); sc.process(in.data(), in.data(), l.data(), r.data(), B);
                for (int i = 0; i < B; ++i) { const double t = (k * B + i) / kSr; if (t < .02) continue; const int b = t < .05 ? 0 : t < .2 ? 1 : t < .5 ? 2 : t < 1 ? 3 : t < 2 ? 4 : 5; e[b] += l[(size_t) i] * l[(size_t) i] + r[(size_t) i] * r[(size_t) i]; ++n[b]; }
            }
            std::array<double, 5> f {}; for (int b = 1; b <= 5; ++b) f[(size_t) (b - 1)] = std::max(-100.0, 10 * std::log10(e[b] / std::max(1L, n[b]) + 1e-14));
            fp.push_back(f);
        }
        double closest = 1e9;
        for (size_t a = 0; a < fp.size(); ++a) for (size_t b = a + 1; b < fp.size(); ++b)
        {
            double d = 0; for (size_t i = 0; i < 5; ++i) d += (fp[a][i] - fp[b][i]) * (fp[a][i] - fp[b][i]);
            d = std::sqrt(d / 5); closest = std::min(closest, d);
            CHECK(d >= 3.0, "%s and %s decay almost identically (%.2f dB RMS apart)", kStagePresets[a].name, kStagePresets[b].name, d);
        }
        std::printf("[4] all %d rooms are distinct; the closest pair is %.1f dB RMS apart\n", kNumStagePresets, closest);
    }
    // 5. the new room really ARRIVES: a switched instance rings like one that started in that room (the glides must end on their targets)
    {
        const int pairsToCheck[][2] = { { 3, 1 }, { 3, 9 }, { 5, 6 }, { 8, 9 }, { 9, 3 }, { 1, 5 } };
        double worst = 0;
        for (const auto& pr : pairsToCheck)
        {
            const auto fresh = tailBands(pr[1], pr[1]), moved = tailBands(pr[0], pr[1]);
            for (size_t b = 0; b < 4; ++b)
            {
                const double d = std::fabs(fresh[b] - moved[b]); worst = std::max(worst, d);
                CHECK(d <= 0.6, "%s -> %s: band %zu rings %.2f dB differently from an instance that started in the room (%.1f vs %.1f dB)", kStagePresets[pr[0]].name, kStagePresets[pr[1]].name, b, d, moved[b], fresh[b]);
            }
        }
        std::printf("[5] a room switched into rings like a fresh one: worst band difference %.2f dB over %d switches\n", worst, (int) (sizeof(pairsToCheck) / sizeof(pairsToCheck[0])));
    }
    std::printf("ALL ROOM SWITCH TESTS PASSED\n");
    return 0;
}
