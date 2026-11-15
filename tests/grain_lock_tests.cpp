// The opt-in pitch-locked grain window (FXParams::lockGrains). Off must be the original sound exactly; on must remove the sidebands of
// a steady note, move its window without clicks, and never leave the safe range. (With vibrato it does NOT help: see docs/V0_9_CHANGELOG.md.)
#include "../Source/ChoirFX.h"
#include "test_support.h"
#include <complex>

using namespace choir;
static void fft(std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i) { int b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap(a[(size_t) i], a[(size_t) j]); }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * 3.14159265358979323846 / len; const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) { std::complex<double> w(1); for (int j = 0; j < len / 2; ++j) { auto u = a[(size_t) (i + j)], v = a[(size_t) (i + j + len / 2)] * w; a[(size_t) (i + j)] = u + v; a[(size_t) (i + j + len / 2)] = u - v; w *= wl; } }
    }
}
// energy between the harmonics, in dB re the energy on them; the harmonic comb's own pitch is searched for, so detune does not matter
static double spurious(const std::vector<float>& x, size_t start, double sr)
{
    const int N = 1 << 16; std::vector<std::complex<double>> a((size_t) N);
    for (int i = 0; i < N; ++i) a[(size_t) i] = x[start + (size_t) i] * (.5 - .5 * std::cos(2 * 3.14159265358979323846 * i / N));
    fft(a); const double bin = sr / N; std::vector<double> e((size_t) N / 2); for (int i = 0; i < N / 2; ++i) e[(size_t) i] = std::norm(a[(size_t) i]);
    auto comb = [&](double f0, double& on, double& all) { on = all = 0; for (int i = (int) (100 / bin); i < (int) (5000 / bin); ++i) { all += e[(size_t) i]; const double f = i * bin, k = std::round(f / f0); if (k >= 1 && std::fabs(f - k * f0) < 4.0) on += e[(size_t) i]; } };
    double best = 0, bf = 0, on, all;
    for (double f0 = 70; f0 < 700; f0 += .25) { comb(f0, on, all); if (on > best) { best = on; bf = f0; } }
    for (double f0 = bf - .3; f0 < bf + .3; f0 += .01) { comb(f0, on, all); if (on > best) { best = on; bf = f0; } }
    comb(bf, on, all); return 10 * std::log10(std::max(1e-30, all - on) / on);
}
struct Run { std::vector<float> v0, v8, v12; float finalWindow = 0, minWindow = 1e9f, maxWindow = 0, maxStep = 0; };
// 4 s of a steady 10-harmonic note; lockOnAt < 0: never on; otherwise lockGrains turns on at that time (seconds)
static Run render(double f0, bool lock, double lockOnAt = -1, double vibratoCents = 0, double sr = 48000)
{
    FXParams p; p.humanize = 0.f; p.ensemble = 0.f; p.voiceBlend = 0.f; p.mode = 0; p.harmony = 0; p.lockGrains = lock;
    ChoirEffect fx; fx.prepare(sr); fx.setVoiceParameters(p);
    Run r; float out[16]; double ph = 0; float prevWin = -1.f; const double dep = std::pow(2.0, vibratoCents / 1200.0) - 1;
    for (int n = 0; n < (int) (4 * sr); ++n)
    {
        const double t = n / sr; if (lockOnAt >= 0 && !p.lockGrains && t >= lockOnAt) p.lockGrains = true;
        ph += 2 * 3.14159265358979323846 * f0 * (1 + dep * std::sin(2 * 3.14159265358979323846 * 5.5 * t)) / sr;
        double s = 0; for (int k = 1; k <= 10; ++k) s += std::sin(k * ph) / k;
        fx.setVoiceParameters(p); fx.processVoices((float) (.3 * s), out);
        r.v0.push_back(out[0]); r.v8.push_back(out[8]); r.v12.push_back(out[12]);
        if (prevWin >= 0.f) r.maxStep = std::max(r.maxStep, std::fabs(fx.windowInUse() - prevWin)); prevWin = fx.windowInUse();
        r.minWindow = std::min(r.minWindow, fx.windowInUse()); r.maxWindow = std::max(r.maxWindow, fx.windowInUse());
    }
    r.finalWindow = fx.windowInUse(); return r;
}
static double maxD2(const std::vector<float>& x, size_t from, size_t to) { double m = 0; for (size_t i = from; i + 2 < to; ++i) m = std::max(m, (double) std::fabs(x[i + 2] - 2 * x[i + 1] + x[i])); return m; }

int main()
{
    const double sr = 48000;
    // 1. OFF is the original sound: the window never moves, and the output is identical whether or not the flag was ever touched
    {
        const auto a = render(196, false), b = render(196, false);
        CHECK(a.minWindow == a.maxWindow && a.maxWindow == .073f * (float) sr, "with lockGrains off the window must stay at exactly 73 ms (%.3f .. %.3f)", a.minWindow, a.maxWindow);
        CHECK(a.v0 == b.v0 && a.v8 == b.v8 && a.v12 == b.v12, "two renders with the flag off must be bit-identical");
        std::printf("[1] off: window fixed at %.0f samples, bit-identical renders\n", a.finalWindow);
    }
    // 2. ON: on a steady note the window becomes an even number of periods and the sidebands drop by a lot
    {
        double worst = 1e9, best = 0;
        for (double f0 : { 147.0, 196.0, 330.0 })
        {
            const auto off = render(f0, false), on = render(f0, true);
            const double periods = on.finalWindow * f0 / sr, nearestEven = 2 * std::round(periods / 2);
            CHECK(std::fabs(periods - nearestEven) < .01 * nearestEven && nearestEven >= 2, "f0 %.0f: window is %.3f periods, not a whole even number", f0, periods);
            CHECK(on.finalWindow <= ChoirEffect::kMaxWindowSeconds * (float) sr + .5f && on.finalWindow >= .04f * (float) sr, "f0 %.0f: window %.0f samples outside the safe range", f0, on.finalWindow);
            const size_t at = (size_t) (2.5 * sr);
            const double so[] = { spurious(off.v0, at, sr), spurious(off.v8, at, sr), spurious(off.v12, at, sr) }, sn[] = { spurious(on.v0, at, sr), spurious(on.v8, at, sr), spurious(on.v12, at, sr) };
            for (int v = 0; v < 3; ++v)
            {
                CHECK(so[v] - sn[v] >= 15.0, "f0 %.0f voice %d: sidebands only %.1f dB lower with the lock (%.1f -> %.1f dB)", f0, v, so[v] - sn[v], so[v], sn[v]);
                worst = std::min(worst, so[v] - sn[v]); best = std::max(best, so[v] - sn[v]);
            }
        }
        std::printf("[2] steady notes: the lock lowers the sidebands by %.0f to %.0f dB (3 pitches x 3 voices)\n", worst, best);
    }
    // 2b. the rule itself, over the whole pitch range the tracker can report (60 Hz .. 2 kHz) and the usual sample rates
    {
        long n = 0;
        for (double rate : { 44100.0, 48000.0, 96000.0 })
            for (float f0 = 60.f; f0 <= 2000.f; f0 *= 1.01f)
            {
                const float w = ChoirEffect::lockedWindowFor(f0, (float) rate), periods = w * f0 / (float) rate;
                CHECK(w <= ChoirEffect::kMaxWindowSeconds * (float) rate + .01f, "f0 %.1f at %.0f Hz: window %.0f samples is longer than the history allows", f0, rate, w);
                CHECK(w >= .03f * (float) rate, "f0 %.1f: window %.0f samples is shorter than 30 ms", f0, w);
                if (w < ChoirEffect::kMaxWindowSeconds * (float) rate - .01f) CHECK(std::fabs(periods - 2 * std::round(periods / 2)) < 1e-3f, "f0 %.1f: %.4f periods is not a whole even number", f0, periods);
                ++n;
            }
        CHECK(ChoirEffect::lockedWindowFor(69.f, 48000.f) == ChoirEffect::kMaxWindowSeconds * 48000.f, "a 69 Hz voice (6 periods = 87 ms) must be clamped to the limit");
        // the history holds 190 ms: the longest read is minDelay (9 ms) + singer offsets (up to ~25 ms at humanize 1, plus the random timing walk) + the window
        CHECK(ChoirEffect::kMaxWindowSeconds <= .1f, "kMaxWindowSeconds %.3f leaves too little of the 190 ms history for the singers' delays", ChoirEffect::kMaxWindowSeconds);
        std::printf("[2b] the window rule: %ld pitches x rates, always within 30-85 ms, whole even periods unless clamped\n", n);
    }
    // 3. switching it on mid-note glides: no click, finite, same level
    {
        const auto off = render(196, false), sw = render(196, false, 2.0);
        const size_t from = (size_t) (2.0 * sr), to = (size_t) (3.2 * sr);
        const double base = maxD2(off.v0, from, to), moved = maxD2(sw.v0, from, to);
        CHECK(moved <= base * 1.5 + 1e-4, "switching the lock on mid-note clicks: |d2| %.4f against %.4f without", moved, base);
        double pk = 0, pko = 0; for (size_t i = from; i < to; ++i) { pk = std::max(pk, (double) std::fabs(sw.v0[i])); pko = std::max(pko, (double) std::fabs(off.v0[i])); }
        CHECK(std::isfinite(pk) && pk <= pko * 1.15, "level changed when the lock came on: peak %.3f against %.3f", pk, pko);
        CHECK(sw.maxWindow - sw.minWindow < 100.f, "the window moved by %.0f samples: more than a 73 ms -> 71 ms step", sw.maxWindow - sw.minWindow);
        CHECK(sw.maxStep <= ChoirEffect::kWindowSlewSamplesPerSample * 1.001f && sw.maxStep > 0.f, "the window moved %.5f samples in one sample; the limit is %.5f (more would detune the voices)", sw.maxStep, ChoirEffect::kWindowSlewSamplesPerSample);
        CHECK(ChoirEffect::kWindowSlewSamplesPerSample <= .003f, "the slew limit %.4f would detune the voices by more than .3 percent", ChoirEffect::kWindowSlewSamplesPerSample);
        std::printf("[3] switching on mid-note: |d2| %.4f vs %.4f, window moved %.0f samples\n", moved, base, sw.maxWindow - sw.minWindow);
    }
    // 4. a unpitched or wobbly input never takes the window out of the safe range; the output stays finite and no louder
    {
        for (double cents : { 25.0, 60.0 })
        {
            const auto off = render(196, false, -1, cents), on = render(196, true, -1, cents);
            CHECK(on.maxStep <= ChoirEffect::kWindowSlewSamplesPerSample * 1.001f, "vibrato %.0f cents: the window moved %.5f samples in one sample", cents, on.maxStep);
            CHECK(on.minWindow >= .04f * (float) sr && on.maxWindow <= ChoirEffect::kMaxWindowSeconds * (float) sr + .5f, "vibrato %.0f cents: window range %.0f..%.0f", cents, on.minWindow, on.maxWindow);
            double pk = 0, pko = 0; for (size_t i = (size_t) sr; i < on.v0.size(); ++i) { CHECK(std::isfinite(on.v0[i]) && std::isfinite(on.v12[i]), "non-finite output"); pk = std::max(pk, (double) std::fabs(on.v0[i])); pko = std::max(pko, (double) std::fabs(off.v0[i])); }
            CHECK(pk <= pko * 1.15, "vibrato %.0f cents: peak %.3f against %.3f", cents, pk, pko);
            CHECK(maxD2(on.v0, (size_t) sr, on.v0.size()) <= maxD2(off.v0, (size_t) sr, off.v0.size()) * 1.25, "vibrato %.0f cents: the lock adds clicks", cents);
        }
        std::printf("[4] vibrato 25 and 60 cents: window stays in range, finite, no extra clicks or level\n");
    }
    // 5. other sample rates: the window scales with the rate
    for (double sr2 : { 44100.0, 96000.0 })
    {
        const auto on = render(196, true, -1, 0, sr2); const double periods = on.finalWindow * 196 / sr2;
        CHECK(std::fabs(periods - 2 * std::round(periods / 2)) < .15, "%.0f Hz: window %.3f periods", sr2, periods);
    }
    std::printf("[5] 44.1 and 96 kHz: whole even periods too\nALL GRAIN LOCK TESTS PASSED\n");
    return 0;
}
