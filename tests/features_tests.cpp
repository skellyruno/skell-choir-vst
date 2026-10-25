#undef NDEBUG   // these tests use assert(); a Release build must still check
// End-to-end tests of the v0.6 feature set: formant choir, IR hall, stems, meters, A/B.
#include "../Source/StagedChoir.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
using namespace choir;
static const double kPi = 3.14159265358979323846;

static std::vector<float> sung(double sr, double seconds, double hz = 196.0, double level = .22)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i) { double t = i / sr; ph += 2 * kPi * hz * (1 + .004 * std::sin(2 * kPi * 5.3 * t)) / sr; double s = 0; for (int h = 1; h <= 10; ++h) s += std::sin(h * ph) / h; x[i] = (float) (level * s * (std::fmod(t, 2.0) < 1.7 ? 1.0 : 0.0)); }
    return x;
}
struct Out { std::vector<float> l, r, stem[5][2]; };
static Out run(StagedChoir& sc, const std::vector<float>& x, int chunk = 512, bool stems = false)
{
    Out o; o.l.resize(x.size()); o.r.resize(x.size()); if (stems) for (auto& s : o.stem) { s[0].resize(x.size()); s[1].resize(x.size()); }
    for (size_t i = 0; i < x.size(); i += (size_t) chunk)
    {
        const int n = (int) std::min<size_t>((size_t) chunk, x.size() - i);
        if (stems) { StagedChoir::StemOut so[5]; for (int s = 0; s < 5; ++s) { so[s].left = &o.stem[s][0][i]; so[s].right = &o.stem[s][1][i]; } sc.process(&x[i], &x[i], &o.l[i], &o.r[i], n, so); }
        else sc.process(&x[i], &x[i], &o.l[i], &o.r[i], n);
    }
    return o;
}
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double) (b - a)); }
static bool finite(const std::vector<float>& v) { for (float f : v) if (!std::isfinite(f)) return false; return true; }
static FXParams base() { FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; p.gain = { .83f, .83f, .83f, .83f }; return p; }

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const double sr = 48000;
    // ---- 1. formant choir through the whole chain
    {
        auto x = sung(sr, 6); double lvl[3]; int i = 0;
        for (float blend : { 0.f, .5f, 1.f })
        {
            StagedChoir sc; sc.prepare(sr); auto p = base(); p.voiceBlend = blend; p.choirVowel = .0f; sc.setParameters(p);
            auto o = run(sc, x); assert(finite(o.l) && finite(o.r)); lvl[i] = rms(o.l, (size_t) (2 * sr), (size_t) (5.5 * sr));
            float pk = 0; for (float f : o.l) pk = std::max(pk, std::fabs(f)); assert(pk <= 1.0001f); ++i;
        }
        std::printf("[1] choir level (dB re harmonizer only): blend .5 %+.1f dB, blend 1 %+.1f dB\n", 20 * std::log10(lvl[1] / lvl[0]), 20 * std::log10(lvl[2] / lvl[0]));
        assert(std::fabs(20 * std::log10(lvl[2] / lvl[0])) < 4.0);
    }
    // ---- 2. impulse-response hall
    {
        // the IR is loaded at 44.1 kHz into a 48 kHz engine: must be resampled and still land where it should
        std::vector<float> imp(44100, 0.f); imp[441] = 1.f;      // a click at 10 ms
        auto res = oc::resampleCubic(imp.data(), (int) imp.size(), 44100.0, 48000.0);
        size_t pk = std::max_element(res.begin(), res.end(), [](float a, float b) { return std::fabs(a) < std::fabs(b); }) - res.begin();
        std::printf("[2] 44.1k->48k resample: 10 ms click lands at %.3f ms (length %zu)\n", pk / 48.0, res.size()); assert(std::fabs(pk / 48.0 - 10.0) < .05 && res.size() == 48000);
        std::vector<float> sine(4410); for (size_t k = 0; k < sine.size(); ++k) sine[k] = (float) std::sin(2 * kPi * 1000 * k / 44100.0);
        auto rs = oc::resampleCubic(sine.data(), (int) sine.size(), 44100.0, 48000.0); double e = 0; for (size_t k = 8; k + 8 < rs.size(); ++k) e = std::max(e, std::fabs(rs[k] - std::sin(2 * kPi * 1000 * k / 48000.0)));
        std::printf("[2] resampled 1 kHz sine max error %.4f\n", e); assert(e < .01);

        // loudness: a unit-energy-normalised IR sits within 1.5 dB of the algorithmic hall on noise
        std::mt19937 rng(1); std::normal_distribution<float> G(0, 1);
        const int n = (int) (2.6 * sr); std::vector<float> ir((size_t) n); double en = 0; for (int k = 0; k < n; ++k) { ir[(size_t) k] = G(rng) * std::pow(10.0f, -3.0f * k / 48000 / 2.4f); en += (double) ir[(size_t) k] * ir[(size_t) k]; }
        const float sc0 = (float) std::sqrt(StagedChoir::kIrEnergy / en); for (auto& v : ir) v *= sc0;
        double g[2];
        for (int mode = 0; mode < 2; ++mode)
        {
            oc::Hall h(sr, 128); h.set(2.4, 1.0); if (mode) h.adoptConvolver(std::make_unique<oc::ConvPair>(ir.data(), ir.data(), n, 1024));
            std::vector<float> l(128), r(128), ol(128), orr(128); double ei = 0, eo = 0;
            for (int b = 0; b < (int) (sr * 14 / 128); ++b) { for (int k = 0; k < 128; ++k) { l[(size_t) k] = G(rng); r[(size_t) k] = G(rng); } std::fill(ol.begin(), ol.end(), 0.f); std::fill(orr.begin(), orr.end(), 0.f); h.process(l.data(), r.data(), ol.data(), orr.data());
                if (b > (int) (sr * 5 / 128)) for (int k = 0; k < 128; ++k) { ei += l[(size_t) k] * l[(size_t) k]; eo += ol[(size_t) k] * ol[(size_t) k]; } }
            g[mode] = std::sqrt(eo / ei);
        }
        std::printf("[2] hall noise gain: algorithmic %.3f, impulse response %.3f (%+.2f dB)\n", g[0], g[1], 20 * std::log10(g[1] / g[0])); assert(std::fabs(20 * std::log10(g[1] / g[0])) < 1.5);

        // end to end: load an IR, hear a different tail; clear it, the algorithmic hall returns; swaps while running stay clean
        auto x = sung(sr, 8); std::vector<float> sparse((size_t) (1.5 * sr), 0.f); for (int k = 0; k < 6; ++k) sparse[(size_t) (k * 4000 + 100)] = (k % 2 ? -.5f : .5f);   // an odd, obviously different IR
        StagedChoir sc; sc.prepare(sr); auto p = base(); p.hall = .35f; sc.setParameters(p);
        std::vector<float> o1(x.size()), o2(x.size()), oa(x.size()), ob(x.size());
        double maxStep = 0; float prev = 0; bool ok = true;
        for (size_t i = 0; i < x.size(); i += 256)
        {
            if (i == (size_t) (2 * sr)) sc.setImpulseResponse(sparse.data(), nullptr, (int) sparse.size(), sr);
            if (i == (size_t) (4 * sr)) sc.setImpulseResponse(ir.data(), ir.data(), (int) ir.size(), sr);
            if (i == (size_t) (6 * sr)) sc.setImpulseResponse(nullptr, nullptr, 0, sr);
            const int m = (int) std::min<size_t>(256, x.size() - i); sc.process(&x[i], &x[i], &oa[i], &ob[i], m);
            for (int k = 0; k < m; ++k) { if (!std::isfinite(oa[i + (size_t) k])) ok = false; maxStep = std::max(maxStep, (double) std::fabs(oa[i + (size_t) k] - prev)); prev = oa[i + (size_t) k]; }
        }
        std::printf("[2] IR swapped in, replaced and cleared while running: finite=%d, largest sample-to-sample step %.3f\n", ok, maxStep); assert(ok); assert(maxStep < .8);
        sc.collectGarbage(); assert(sc.hasImpulseResponse() == false);
    }
    // ---- 3. stems
    {
        auto x = sung(sr, 5); StagedChoir sc; sc.prepare(sr); auto p = base(); p.gain = { .83f, 0.f, 0.f, 0.f }; p.mix = 1.f; sc.setParameters(p);
        auto o = run(sc, x, 384, true); double e[5]; for (int s = 0; s < 5; ++s) { assert(finite(o.stem[s][0]) && finite(o.stem[s][1])); e[s] = rms(o.stem[s][0], (size_t) (2 * sr), (size_t) (4.5 * sr)); }
        std::printf("[3] stem RMS with only the sopranos singing: S %.4f A %.4f T %.4f B %.4f lead %.4f\n", e[0], e[1], e[2], e[3], e[4]);
        assert(e[0] > 30 * e[1] && e[0] > 30 * e[2] && e[0] > 30 * e[3]); assert(e[4] > 1e-3);
        // the soprano stem and the main output are the same singers through the same hall: they should agree closely in time
        double num = 0, d1 = 0, d2 = 0; for (size_t i = (size_t) (2 * sr); i < (size_t) (4.5 * sr); ++i) { num += o.l[i] * o.stem[0][0][i]; d1 += o.l[i] * o.l[i]; d2 += o.stem[0][0][i] * o.stem[0][0][i]; }
        const double c = num / std::sqrt(d1 * d2 + 1e-18); std::printf("[3] main output vs soprano stem correlation (aligned): %.3f\n", c); assert(c > .85);
        // asking for stems must not change the main output
        StagedChoir sc2; sc2.prepare(sr); sc2.setParameters(p); auto o2 = run(sc2, x, 384, false);
        double d = 0; for (size_t i = 0; i < x.size(); ++i) d = std::max(d, (double) std::fabs(o.l[i] - o2.l[i])); std::printf("[3] main output with and without stems differs by at most %.2e\n", d); assert(d < 1e-6);
    }
    // ---- 4. meters
    {
        auto x = sung(sr, 5); StagedChoir sc; sc.prepare(sr); auto p = base(); p.gain = { .83f, 0.f, 0.f, 0.f }; sc.setParameters(p); auto o = run(sc, x);
        const float s0 = sc.meters.section[0].load(), s1 = sc.meters.section[1].load();
        std::printf("[4] meters: section 0 %.4f vs section 1 %.5f; lead peak %.3f; width %.3f; correlation %.3f\n", s0, s1, sc.meters.leadIn.load(), sc.meters.width.load(), sc.meters.correlation.load());
        assert(s0 > 20 * s1); assert(sc.meters.leadIn.load() > .1f); assert(sc.meters.width.load() > .05f); assert(sc.meters.correlation.load() < .95f);
        StagedChoir dry; dry.prepare(sr); auto q = base(); q.mix = 0.f; dry.setParameters(q); run(dry, x);
        std::printf("[4] dry mono lead: width %.4f, correlation %.4f\n", dry.meters.width.load(), dry.meters.correlation.load()); assert(dry.meters.width.load() < .01f && dry.meters.correlation.load() > .99f);
    }
    // ---- 5. A/B: the classic stage is the v0.5 engine exactly, delayed to match the faithful stage
    {
        auto x = sung(sr, 4); auto p = base(); p.mix = .78f; p.master = .9f; p.classicStage = true;
        ChoirEffect ref; ref.prepare(sr); ref.setParameters(p); std::vector<float> rl(x.size()), rr(x.size()); for (size_t i = 0; i < x.size(); ++i) ref.render(x[i], x[i], rl[i], rr[i]);
        StagedChoir sc; sc.prepare(sr); sc.setParameters(p); auto o = run(sc, x, 300);
        double d = 0; const size_t L = (size_t) StagedChoir::latencySamples; for (size_t i = 0; i + L < x.size(); ++i) d = std::max(d, (double) std::fabs(o.l[i + L] - rl[i]));
        std::printf("[5] classic stage vs v0.5 ChoirEffect::render, delayed %zu samples: max difference %.2e\n", L, d); assert(d < 1e-6);
        // switching faithful -> classic -> faithful over silence leaves nothing stale behind
        StagedChoir s2; s2.prepare(sr); auto q = base(); q.hall = .3f; s2.setParameters(q); auto y = sung(sr, 3); std::vector<float> sil((size_t) (3 * sr), 0.f); std::vector<float> a(y.size()), b(y.size());
        for (size_t i = 0; i < y.size(); i += 256) s2.process(&y[i], &y[i], &a[i], &b[i], (int) std::min<size_t>(256, y.size() - i));
        q.classicStage = true; s2.setParameters(q); for (size_t i = 0; i < 12000; i += 256) s2.process(&sil[i], &sil[i], &a[i], &b[i], 256);
        q.classicStage = false; s2.setParameters(q); std::vector<float> tail((size_t) (2 * sr)), tb(tail.size());
        for (size_t i = 0; i + 256 <= tail.size(); i += 256) s2.process(&sil[i], &sil[i], &tail[i], &tb[i], 256);
        const double late = rms(tail, (size_t) (1.2 * sr), (size_t) (1.9 * sr)); std::printf("[5] after faithful->classic->faithful over silence, output rms %.2e\n", late); assert(late < 1e-4 && finite(tail));
    }
    std::printf("ALL FEATURE TESTS PASSED\n");
}
