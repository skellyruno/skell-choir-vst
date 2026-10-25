#undef NDEBUG   // these tests use assert(); a Release build must still check
// Regression tests for the YIN pitch tracker and the full-take scanner, scored against synthetic ground truth.
#include "../Source/ChoirFX.h"
#include "../Source/Offline/FullTakeScan.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
static const double kPi = 3.14159265358979323846;

// ---- pitch tracker: steady and vibrato phrases, clean / noisy / weak fundamental
static void trackerCase(const char* name, double fundamental, double snrDb, double vibCents, double minWithin50, double maxOctave, double maxLockMs)
{
    const double sr = 48000; std::mt19937 rng(7); std::uniform_real_distribution<double> U(0, 1); std::normal_distribution<double> G(0, 1);
    choir::PitchTracker t; t.prepare((float) sr);
    long voiced = 0, ok = 0, oct = 0; std::vector<double> lock; double ph = 0;
    for (int n = 0; n < 40; ++n)
    {
        double midi = 43 + U(rng) * 36, hz = 440 * std::pow(2, (midi - 69) / 12), dur = .35 + U(rng) * .5, gap = .06 + U(rng) * .1;
        int len = (int) (dur * sr), glen = (int) (gap * sr); double vp = U(rng) * 6.28; bool locked = false;
        for (int i = 0; i < len + glen; ++i)
        {
            double x = 0, truth = -1;
            if (i < len)
            {
                double tt = i / sr, cents = vibCents * std::sin(2 * kPi * 5.5 * tt + vp) * std::min(1.0, tt / .15), f = hz * std::pow(2, cents / 1200);
                ph += 2 * kPi * f / sr; double env = std::min({ 1.0, i / (.03 * sr), (len - i) / (.05 * sr) });
                for (int h = 1; h <= 18; ++h) x += (h == 1 ? fundamental : 1.0) * std::sin(h * ph) / std::pow(h, .9);
                x *= .12 * env; truth = 69 + 12 * std::log2(f / 440);
            }
            x += std::pow(10, -snrDb / 20) * .096 * G(rng);
            t.push((float) x);
            if (i % 480 == 0 && truth > 0)
            {
                double m = t.midi(), e = (m - truth) * 100;
                if (i > .12 * sr) { ++voiced; if (m > 0) { if (std::fabs(e) < 50) ++ok; else if (std::fabs(std::fabs(e) - 1200) < 150 || std::fabs(std::fabs(e) - 700) < 150) ++oct; } }
                if (!locked && m > 0 && std::fabs(e) < 50) { locked = true; lock.push_back(i / sr * 1000); }
            }
        }
    }
    std::sort(lock.begin(), lock.end()); const double med = lock.empty() ? 1e9 : lock[lock.size() / 2];
    std::printf("[tracker] %-24s within 50c %5.1f%% | octave/fifth %4.1f%% | median lock %3.0f ms | locked %zu/40\n", name, 100.0 * ok / voiced, 100.0 * oct / voiced, med, lock.size());
    assert(100.0 * ok / voiced >= minWithin50); assert(100.0 * oct / voiced <= maxOctave); assert(med <= maxLockMs); assert(lock.size() >= 39);
}

// ---- scanner: sung phrases with consonant gaps, vibrato, noise, optional pitch scoops
static void scannerCase(const char* name, unsigned seed, double scoop)
{
    const double sr = 48000; std::mt19937 rng(seed); std::uniform_real_distribution<double> U(0, 1); std::normal_distribution<double> G(0, 1);
    struct Truth { double a, b; int midi; };
    std::vector<Truth> truth; double ph = 0, clock = .3; choir::FullTakeScanner sc; sc.prepare(sr);
    auto feed = [&](int n, double f, double env, bool voiced)
    { for (int i = 0; i < n; ++i) { double v = 0; if (voiced) { ph += 2 * kPi * f / sr; for (int h = 1; h <= 14; ++h) v += std::sin(h * ph) / std::pow(h, .95); v *= .12 * env; } v += .004 * G(rng); sc.push((float) v); } };
    feed((int) (.3 * sr), 0, 0, false); double cur = 60;
    for (int n = 0; n < 14; ++n)
    {
        int step = (int) std::round(G(rng) * 2.2); cur = std::min(76.0, std::max(48.0, cur + step)); if (step == 0) cur += 1;
        double dur = .25 + U(rng) * .6, hz = 440 * std::pow(2, (cur - 69) / 12.0), vp = U(rng) * 6.28; int len = (int) (dur * sr);
        truth.push_back({ clock, clock + dur, (int) cur });
        for (int i = 0; i < len; ++i)
        {
            double tt = i / sr, cents = scoop * (tt < .08 ? -(.08 - tt) / .08 * 120 : 0) + 30 * std::sin(2 * kPi * 5.5 * tt + vp) * std::min(1.0, tt / .2);
            feed(1, hz * std::pow(2, cents / 1200), std::min({ 1.0, i / (.025 * sr), (len - i) / (.04 * sr) }), true);
        }
        double gap = .05 + U(rng) * .12; feed((int) (gap * sr), 0, 0, false); clock += dur + gap;
    }
    feed((int) (.5 * sr), 0, 0, false);
    auto score = sc.finish(false); std::vector<bool> used(score.notes.size(), false); int ok = 0; std::vector<double> on, off;
    for (auto& tr : truth)
    {
        int best = -1; double bo = 0;
        for (size_t i = 0; i < score.notes.size(); ++i) { if (used[i]) continue; double ov = std::min(tr.b, score.notes[i].end) - std::max(tr.a, score.notes[i].start); if (ov > .5 * (tr.b - tr.a) && ov > bo) { bo = ov; best = (int) i; } }
        if (best < 0 || score.notes[(size_t) best].melody != tr.midi) continue;
        used[(size_t) best] = true; ++ok; on.push_back(std::fabs(score.notes[(size_t) best].start - tr.a) * 1000); off.push_back(std::fabs(score.notes[(size_t) best].end - tr.b) * 1000);
    }
    int spurious = 0; for (size_t i = 0; i < used.size(); ++i) if (!used[i]) ++spurious;
    std::sort(on.begin(), on.end()); std::sort(off.begin(), off.end());
    const double mOn = on.empty() ? 1e9 : on[on.size() / 2], mOff = off.empty() ? 1e9 : off[off.size() / 2];
    std::printf("[scanner] %-22s %d/14 notes correct | spurious %d | median onset err %.0f ms, offset err %.0f ms\n", name, ok, spurious, mOn, mOff);
    assert(ok >= 13); assert(spurious <= 1); assert(mOn < 15); assert(mOff < 15);
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    trackerCase("clean, no vibrato", 1.0, 60, 0, 99.0, .5, 60);
    trackerCase("clean, vibrato +-35c", 1.0, 60, 35, 90.0, .5, 60);
    trackerCase("20 dB SNR", 1.0, 20, 0, 98.0, 1.0, 70);
    trackerCase("weak fundamental", .12, 40, 0, 98.0, 1.0, 70);
    scannerCase("scoops, seed 23", 23, 1.0);
    scannerCase("no scoops, seed 99", 99, 0.0);
    scannerCase("scoops, seed 12345", 12345, 1.0);
    std::printf("ALL TRACKER/SCANNER TESTS PASSED\n");
}
