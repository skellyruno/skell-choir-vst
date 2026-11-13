// SWELL (the choir follows how loud you sing against your own average) and PULSE (tempo-synced breathing).
#include "test_support.h"

using namespace ts;
static const double PI = 3.14159265358979323846;

// alternating loud and soft phrases of a steady vowel: 3 s each, `loud` and `soft` in dBFS RMS, no gaps
static std::vector<float> phrases(int sr, double seconds, double loudDb, double softDb, double f0 = 196.0)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const bool loud = (int) ((double) i / sr / 3.0) % 2 == 0; const double db = loud ? loudDb : softDb;
        ph += f0 / sr; double v = 0; for (int h = 1; h <= 6; ++h) v += std::sin(2 * PI * h * ph) / h;
        x[i] = (float) (std::pow(10.0, db / 20.0) * v / 1.2);
    }
    return x;
}
static double windowDb(const std::vector<float>& y, int sr, double from, double to)
{
    double s = 0; size_t n = 0; for (size_t i = (size_t) (from * sr); i < (size_t) (to * sr) && i < y.size(); ++i) { s += (double) y[i] * y[i]; ++n; }
    return dB(std::sqrt(s / (double) std::max<size_t>(n, 1)));
}
static std::vector<float> choirOnly(const std::vector<float>& x, int sr, choir::FXParams p)
{
    p.mix = 1.f; p.leadGain = 0.f; p.levelMatch = false; p.master = 1.f; p.consonants = 0.f;
    return render(x, sr, p).l;
}

int main()
{
    const int sr = 44100;

    // 1. SWELL ------------------------------------------------------------------------------------------------------
    {
        const auto x = phrases(sr, 24.0, -14.0, -26.0);           // a 12 dB difference between loud and soft
        auto p = pluginDefaults();
        double spread[3];
        for (int k = 0; k < 3; ++k)
        {
            p.swell = k == 0 ? 0.f : (k == 1 ? .5f : 1.f);
            const auto y = choirOnly(x, sr, p);
            // the second and third loud / soft pairs, once the average has settled: 12-15 s loud, 15-18 s soft, measured away from the edges
            const double loud = windowDb(y, sr, 12.8, 14.8), soft = windowDb(y, sr, 15.8, 17.8);
            spread[k] = loud - soft;
            std::printf("[1] SWELL %.1f: the loud phrase is %.1f dB over the soft one in the choir (the input differs by 12.0 dB)\n", (double) p.swell, spread[k]);
        }
        CHECK(std::fabs(spread[0] - 12.0) < 1.5, "with SWELL off the choir should follow the input 1:1 (%.1f dB for 12.0)", spread[0]);
        CHECK(spread[1] > spread[0] + 2.0 && spread[2] > spread[1] + 2.0, "SWELL must exaggerate the dynamics step by step (%.1f, %.1f, %.1f dB)", spread[0], spread[1], spread[2]);
        CHECK(spread[2] < 12.0 + 14.0, "SWELL must stay bounded (%.1f dB)", spread[2]);
        // SWELL off is exactly the old sound
        auto off = pluginDefaults(); off.swell = 0.f; auto same = off; same.pulseRate = 0;
        const auto a = choirOnly(x, sr, off), b = choirOnly(x, sr, same); double worst = 0;
        for (size_t i = 0; i < a.size(); ++i) worst = std::max(worst, (double) std::fabs(a[i] - b[i]));
        CHECK(worst == 0.0, "SWELL 0 and PULSE off must not touch the sound");
        // no speed or surprises: a steady singer is not changed by SWELL
        const auto steady = phrases(sr, 12.0, -18.0, -18.0); auto s1 = pluginDefaults(); s1.swell = 1.f;
        const double d = windowDb(choirOnly(steady, sr, s1), sr, 8.0, 11.0) - windowDb(choirOnly(steady, sr, off), sr, 8.0, 11.0);
        std::printf("[1] a steady singer: SWELL 1 changes the choir level by %+.2f dB\n", d);
        CHECK(std::fabs(d) < 1.0, "SWELL must leave a steady singer alone (%+.2f dB)", d);
    }

    // 2. PULSE: the choir breathes once per period, locked to the host's position ------------------------------------------------------
    {
        const auto x = phrases(sr, 8.0, -18.0, -18.0);
        auto p = pluginDefaults(); p.pulseRate = 4; p.pulseDepth = 1.f; p.bpm = 120.0; p.ppq = 0.0;     // one beat = 0.5 s
        const auto y = choirOnly(x, sr, p);
        // short-time level every 10 ms between 2 s and 7 s
        const int hop = sr / 100; std::vector<double> env;
        for (int i = 2 * 100; i < 7 * 100; ++i) env.push_back(windowDb(y, sr, i / 100.0, i / 100.0 + .02));
        double lo = 1e9, hi = -1e9; for (double v : env) { lo = std::min(lo, v); hi = std::max(hi, v); }
        // the period: the lag (in 10 ms steps) with the strongest autocorrelation of the level in dB
        double mean = 0; for (double v : env) mean += v / (double) env.size();
        int bestLag = 0; double best = -1e9;
        for (int lag = 20; lag <= 80; ++lag) { double c = 0; for (size_t i = 0; i + (size_t) lag < env.size(); ++i) c += (env[i] - mean) * (env[i + (size_t) lag] - mean); if (c > best) { best = c; bestLag = lag; } }
        std::printf("[2] PULSE one beat at 120 bpm, depth 1: level swings %.1f dB, period %.2f s (expected 0.50 s)\n", hi - lo, bestLag / 100.0);
        CHECK(hi - lo > 20.0, "depth 1 should make the choir nearly vanish between beats (%.1f dB swing)", hi - lo);
        CHECK(std::abs(bestLag - 50) <= 2, "the pulse period is %.2f s, not 0.50 s", bestLag / 100.0);
        // depth 0 and rate off change nothing
        auto flat = pluginDefaults(); flat.pulseRate = 4; flat.pulseDepth = 0.f; flat.bpm = 120.0; flat.ppq = 0.0;
        auto none = pluginDefaults(); none.pulseRate = 0;
        const auto f = choirOnly(x, sr, flat), n = choirOnly(x, sr, none); double worst = 0;
        for (size_t i = 0; i < f.size(); ++i) worst = std::max(worst, (double) std::fabs(f[i] - n[i]));
        CHECK(worst < 1e-5, "depth 0 should be the same as off (differs by %g)", worst);
        (void) hop;

        // it follows the host: starting a quarter beat later moves the pulse by a quarter beat (0.125 s)
        auto later = p; later.ppq = .25;
        const auto y2 = choirOnly(x, sr, later);
        std::vector<double> env2; for (int i = 2 * 100; i < 7 * 100; ++i) env2.push_back(windowDb(y2, sr, i / 100.0, i / 100.0 + .02));
        int shift = 0; double bestMatch = 1e9;
        for (int s = 0; s < 50; ++s) { double e = 0; for (size_t i = 0; i + 50 < env.size(); ++i) { const double d = env[i + (size_t) s] - env2[i]; e += d * d; } if (e < bestMatch) { bestMatch = e; shift = s; } }
        // starting 0.25 beats later means the pattern we hear is 0.25 beats (0.125 s = 12-13 steps) earlier: env2[i] == env[i + 12.5]
        std::printf("[2] a host position a quarter beat later moves the pulse by %d steps of 10 ms (expected 12 or 13)\n", shift);
        CHECK(shift >= 11 && shift <= 14, "the pulse does not follow the host position (shifted %d steps)", shift);
    }
    std::printf("ALL SWELL AND PULSE TESTS PASSED\n");
}
