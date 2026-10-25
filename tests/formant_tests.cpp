#undef NDEBUG   // these tests use assert(); a Release build must still check
#include "../Source/FormantChoir.h"
#include "../Source/ChoirFX.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace choir;
static const double kPi = 3.14159265358979323846, sr = 48000;

// an independent vowel synthesiser for the lead (cascade of resonators on a pulse train), not the engine under test
struct Reference
{
    struct Res { double a1, a2, b0, z1 = 0, z2 = 0; };
    std::vector<Res> res; double phase = 0;
    void setVowel(const double (*f)[2], int n) { res.clear(); for (int i = 0; i < n; ++i) { double w = 2 * kPi * f[i][0] / sr, r = std::exp(-kPi * f[i][1] / sr); res.push_back({ -2 * r * std::cos(w), r * r, 1 - 2 * r * std::cos(w) + r * r }); } }
    double next(double hz)
    {
        phase += hz / sr; double x = 0; if (phase >= 1) { phase -= 1; x = 1.0 * sr / hz * .02; }
        for (auto& r : res) { double y = r.b0 * x - r.a1 * r.z1 - r.a2 * r.z2; r.z2 = r.z1; r.z1 = y; x = y; }
        return x;
    }
};
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double) (b - a)); }
static double bandEnergy(const std::vector<float>& v, size_t from, double lo, double hi)
{
    const int n = 8192; double e = 0;
    for (double f = lo; f < hi; f += 20.0) { double re = 0, im = 0; for (int i = 0; i < n; ++i) { double w = .5 - .5 * std::cos(2 * kPi * i / n), ph = 2 * kPi * f * i / sr; re += v[from + (size_t) i] * w * std::cos(ph); im += v[from + (size_t) i] * w * std::sin(ph); } e += re * re + im * im; }
    return e;
}
struct Run { std::vector<float> out[16]; std::vector<float> lead; };
static Run run(double seconds, double leadHz, double leadLevel, const FormantControl& base, const double (*vowel)[2], int nf, const float* ratios, bool gateAlways = true)
{
    FormantChoir fc; fc.prepare(sr); Reference ref; ref.setVowel(vowel, nf); Run r; const size_t N = (size_t) (seconds * sr);
    for (auto& o : r.out) o.resize(N);
    FormantControl c = base; c.ratios = ratios; c.midi = (float) (69 + 12 * std::log2(leadHz / 440)); float o[16]; double g = 0;
    for (size_t i = 0; i < N; ++i)
    {
        double x = ref.next(leadHz) * leadLevel; r.lead.push_back((float) x);
        g += (gateAlways && i > 4800 ? .004 : -.004 * 0) * ((gateAlways && i > 4800 ? 1.0 : 0.0) - g); c.gate = (float) g;
        fc.process((float) x, c, o); for (int k = 0; k < 16; ++k) r.out[k][i] = o[k];
    }
    return r;
}
static const double A_t[4][2] = { { 650, 80 }, { 1080, 90 }, { 2650, 120 }, { 2900, 130 } };      // tenor "a"
static const double I_t[4][2] = { { 290, 40 }, { 1870, 90 }, { 2800, 100 }, { 3250, 120 } };      // tenor "i"

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    float ratios[16]; for (auto& r : ratios) r = 1.f;
    FormantControl base; base.humanize = .7f; base.vibrato = .6f; base.breath = .5f; base.vowel = 0.f; base.follow = 0.f;

    // 1. pitch: every voice sings the lead's note times its voicing ratio
    {
        float rr[16]; for (int k = 0; k < 16; ++k) rr[k] = (k / 4 == 1) ? 1.5f : (k / 4 == 3 ? .5f : 1.f);   // altos a fifth up, basses an octave down
        auto r = run(2.5, 220, .2, base, A_t, 4, rr);
        PitchTracker t; t.prepare((float) sr);
        const int checks[3] = { 0, 4, 12 }; const double want[3] = { 220, 330, 110 };
        for (int j = 0; j < 3; ++j)
        {
            t.reset(); double sum = 0; int n = 0;
            for (size_t i = 0; i < r.out[checks[j]].size(); ++i) { t.push(r.out[checks[j]][i]); if (i > 1.2 * sr && i % 480 == 0 && t.midi() > 0) { sum += 440 * std::pow(2, (t.midi() - 69) / 12.0); ++n; } }
            const double hz = sum / std::max(1, n); std::printf("[1] voice %2d sings %.1f Hz (wanted %.0f)\n", checks[j], hz, want[j]);
            assert(n > 20 && std::fabs(hz / want[j] - 1) < .03);
        }
    }
    // 2. vowels: 'a' and 'i' differ spectrally in the right direction
    {
        FormantControl a = base, i = base; a.vowel = 0.f; i.vowel = .5f; a.humanize = i.humanize = 0.f;
        auto ra = run(2.5, 150, .2, a, A_t, 4, ratios), ri = run(2.5, 150, .2, i, I_t, 4, ratios);
        const size_t at = (size_t) (1.5 * sr);
        const double ra_ = bandEnergy(ra.out[8], at, 1650, 2150) / bandEnergy(ra.out[8], at, 900, 1300);
        const double ri_ = bandEnergy(ri.out[8], at, 1650, 2150) / bandEnergy(ri.out[8], at, 900, 1300);
        std::printf("[2] tenor energy(1.65-2.15 kHz)/energy(0.9-1.3 kHz): 'a' %.3f, 'i' %.3f\n", ra_, ri_); assert(ri_ > 5 * ra_);
    }
    // 3. vowel follower: it recognises the lead's vowel (A and I, alto/tenor-ish tables)
    {
        struct Case { const char* n; const double (*f)[2]; int idx; double hz; };
        static const double E_t[4][2] = { { 400, 70 }, { 1700, 80 }, { 2600, 100 }, { 3200, 120 } };
        static const double O_t[4][2] = { { 400, 40 }, { 800, 80 }, { 2600, 100 }, { 2800, 120 } };
        static const double U_t[4][2] = { { 350, 40 }, { 600, 60 }, { 2700, 100 }, { 2900, 120 } };
        const Case cases[] = { { "A", A_t, 0, 130 }, { "E", E_t, 1, 130 }, { "I", I_t, 2, 130 }, { "O", O_t, 3, 130 }, { "U", U_t, 4, 130 },
                               { "A", A_t, 0, 200 }, { "E", E_t, 1, 200 }, { "I", I_t, 2, 200 } };
        int right = 0;
        for (auto& cs : cases)
        {
            VowelEstimator est; est.prepare(sr); Reference ref; ref.setVowel(cs.f, 4);
            for (int i = 0; i < (int) (1.0 * sr); ++i) est.push((float) (ref.next(cs.hz) * .3));
            int best = (int) (std::max_element(est.weights().begin(), est.weights().end()) - est.weights().begin());
            std::printf("[3] lead sings %s at %.0f Hz -> estimator says %c (F1 %.0f, F2 %.0f)\n", cs.n, cs.hz, "AEIOU"[best], est.f1(), est.f2());
            if (best == cs.idx) ++right;
        }
        std::printf("[3] vowel recognition: %d/8\n", right); assert(right >= 6);
    }
    // 4. loudness: the choir voice sits at about the lead's level, and follows swells
    {
        auto r = run(3.0, 180, .15, base, A_t, 4, ratios);
        const double lead = rms(r.lead, (size_t) (1.5 * sr), (size_t) (2.9 * sr)), voice = rms(r.out[8], (size_t) (1.5 * sr), (size_t) (2.9 * sr));
        std::printf("[4] lead rms %.4f, one tenor voice rms %.4f (ratio %.2f)\n", lead, voice, voice / lead); assert(voice / lead > .5 && voice / lead < 2.0);
        FormantChoir fc; fc.prepare(sr); Reference ref; ref.setVowel(A_t, 4); FormantControl c = base; c.ratios = ratios; c.midi = (float) (69 + 12 * std::log2(180.0 / 440)); c.gate = 1;
        std::vector<float> loud, soft; float o[16]; double lvl = .2;
        for (size_t i = 0; i < (size_t) (4 * sr); ++i)
        { if (i == (size_t) (2 * sr)) lvl = .05; float x = (float) (ref.next(180) * lvl); fc.process(x, c, o); (i > 1.4 * sr && i < 2 * sr ? loud : soft).push_back(o[8]); if (i < 2.6 * sr) soft.clear(); }
        const double a = rms(loud, 0, loud.size()), b = rms(soft, 0, soft.size());
        std::printf("[4] swell: lead drops 12 dB -> choir voice drops %.1f dB\n", 20 * std::log10(b / a)); assert(20 * std::log10(b / a) < -8 && 20 * std::log10(b / a) > -16);
    }
    // 5. performance layer
    {
        // vibrato: the voices wobble in pitch; with vibrato 0 they do not
        auto measure = [&](float vib)
        {
            FormantControl c = base; c.vibrato = vib; c.humanize = 1.f; auto r = run(3.0, 200, .2, c, A_t, 4, ratios);
            PitchTracker t; t.prepare((float) sr); std::vector<double> m;
            for (size_t i = 0; i < r.out[8].size(); ++i) { t.push(r.out[8][i]); if (i > 1.6 * sr && i % 240 == 0 && t.midi() > 0) m.push_back(t.midi()); }
            double lo = *std::min_element(m.begin(), m.end()), hi = *std::max_element(m.begin(), m.end()); return (hi - lo) * 100;
        };
        const double on = measure(1.f), off = measure(0.f);
        std::printf("[5] pitch excursion with vibrato 1.0: %.0f cents p-p, vibrato 0: %.0f cents p-p\n", on, off); assert(on > 2 * off + 10);
        // onset stagger: the singers do not all enter at the same instant
        auto r = run(1.0, 200, .2, base, A_t, 4, ratios);
        std::vector<double> enter; for (int k = 0; k < 16; ++k) { double peak = 0; for (size_t i = 0; i < r.out[k].size(); ++i) peak = std::max(peak, (double) std::fabs(r.out[k][i]));
            for (size_t i = 4800; i < r.out[k].size(); ++i) if (std::fabs(r.out[k][i]) > .25 * peak) { enter.push_back((i - 4800) / sr * 1000); break; } }
        std::sort(enter.begin(), enter.end()); std::printf("[5] entry times of the 16 singers: %.0f ms ... %.0f ms\n", enter.front(), enter.back()); assert(enter.back() - enter.front() > 6);
        // breath: a burst of noise at the start of a phrase that settles (energy BETWEEN harmonics, where a pure tone has none)
        auto between = [&](float breath, size_t from) { FormantControl c = base; c.breath = breath; c.humanize = 0.f; c.vibrato = 0.f;
            auto run2 = run(2.0, 200, .2, c, A_t, 4, ratios); const int n = 8192; double e = 0, h = 0;
            for (double f : { 1100.0, 1300.0, 900.0 }) { for (int pass = 0; pass < 2; ++pass) { double fr = pass == 0 ? f : std::round(f / 200) * 200; double re = 0, im = 0;
                for (int i = 0; i < n; ++i) { double w = .5 - .5 * std::cos(2 * kPi * i / n), ph = 2 * kPi * fr * i / sr; re += run2.out[8][from + (size_t) i] * w * std::cos(ph); im += run2.out[8][from + (size_t) i] * w * std::sin(ph); }
                (pass == 0 ? e : h) += re * re + im * im; } }
            return e / h; };
        const double early1 = between(1.f, (size_t) (.11 * sr)), late1 = between(1.f, (size_t) (1.5 * sr)), early0 = between(0.f, (size_t) (.11 * sr));
        std::printf("[5] between-harmonics / on-harmonic energy: breath 1 start %.4f, settled %.4f | breath 0 start %.4f\n", early1, late1, early0);
        assert(early1 > 8 * early0); assert(early1 > 3 * late1);
    }
    // 6. stability and cost
    {
        FormantChoir fc; fc.prepare(sr); FormantControl c = base; c.ratios = ratios; c.midi = 60; c.gate = 1; c.follow = 1; c.vowel = .3f; float o[16]; double sum = 0; Reference ref; ref.setVowel(I_t, 4);
        auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < (size_t) (10 * sr); ++i) { fc.process((float) (ref.next(150 + 80 * std::sin(i * 1e-4)) * .3), c, o); for (float v : o) { assert(std::isfinite(v)); sum += std::fabs(v); } }
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("[6] 10 s of audio, 16 voices + vowel follower: %.2f s = %.1fx realtime\n", sec, 10 / sec); assert(sum > 1);
    }
    std::printf("ALL FORMANT CHOIR TESTS PASSED\n");
}
