// CONSONANTS: the detector separates sung vowels from noise bursts, and with it on the choir no longer smears "s", "t" and "k".
#include "test_support.h"
#include "../Source/Consonants.h"
#include <random>

using namespace ts;
static const double PI = 3.14159265358979323846;

// A phrase of vowel / consonant pairs. Returns the audio and the consonant windows [start, end) in samples.
struct Phrase { std::vector<float> x; std::vector<std::pair<size_t,size_t>> consonants; };
static Phrase makePhrase(int sr, int repeats, double f0 = 196.0, double rolloff = 2.0)
{
    std::mt19937 rng(7); std::normal_distribution<double> noise(0.0, 1.0);
    Phrase p; double ph = 0; double h1 = 0, h2 = 0, prev = 0, prev2 = 0;
    auto vowel = [&](double seconds, double amp)
    {
        const size_t n = (size_t) (seconds * sr); const size_t base = p.x.size();
        for (size_t i = 0; i < n; ++i)
        {
            ph += f0 * (1.0 + .01 * std::sin(2 * PI * 5.5 * (double) (base + i) / sr)) / sr;
            double v = 0; for (int h = 1; h <= 14; ++h) v += std::sin(2 * PI * h * ph) / std::pow((double) h, rolloff);    // rolloff 2: a real singing voice (-12 dB per octave); 1: an unnaturally bright one
            const double env = std::min(1.0, std::min((double) i / (.02 * sr), (double) (n - i) / (.02 * sr)));
            p.x.push_back((float) (amp * env * v / 1.6));
        }
    };
    auto burst = [&](double seconds, double amp)    // white noise through two differentiators: energy above ~3 kHz, as in an s or a t
    {
        const size_t n = (size_t) (seconds * sr), start = p.x.size();
        for (size_t i = 0; i < n; ++i)
        {
            const double w = noise(rng); const double d1 = w - prev; const double d2 = d1 - prev2; prev = w; prev2 = d1;
            const double env = std::min(1.0, std::min((double) i / (.002 * sr), (double) (n - i) / (.004 * sr)));
            p.x.push_back((float) (amp * env * d2 / 3.0));
        }
        p.consonants.push_back({ start, start + n });
    };
    auto silence = [&](double seconds) { for (size_t i = 0; i < (size_t) (seconds * sr); ++i) p.x.push_back(0.f); };
    (void) h1; (void) h2;
    for (int r = 0; r < repeats; ++r)
    {
        vowel(.45, .12); burst(.16, .10);           // "ah-s"
        vowel(.45, .12); burst(.012, .16); silence(.07);   // "ah-t": a short plosive burst, then the gap
        vowel(.45, .12); burst(.03, .12); silence(.08);    // "ah-k"
    }
    vowel(.4, .12);
    return p;
}
static std::vector<float> envelope(const std::vector<float>& y, int sr, double fcHz)   // a 4-pole high-pass, rectified and smoothed over 5 ms
{
    const double a = std::exp(-2 * PI * fcHz / sr); std::vector<float> out(y.size()); double l1 = 0, l2 = 0, l3 = 0, l4 = 0, e = 0; const double k = 1.0 - std::exp(-1.0 / (.005 * sr));
    for (size_t i = 0; i < y.size(); ++i)
    {
        l1 = a * l1 + (1 - a) * y[i]; const double h1 = y[i] - l1; l2 = a * l2 + (1 - a) * h1; const double h2 = h1 - l2;
        l3 = a * l3 + (1 - a) * h2; const double h3 = h2 - l3; l4 = a * l4 + (1 - a) * h3; const double h4 = h3 - l4;
        e += k * (std::fabs(h4) - e); out[i] = (float) e;
    }
    return out;
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const int sr = 44100;

    // 1. the detector: vowels are not consonants, bursts are --------------------------------------------------------------------
    {
        // a stress test first: a vowel far brighter than any singing voice (-6 dB per octave, 14 harmonics) must still not be taken for a consonant
        {
            const auto bright = makePhrase(sr, 1, 196.0, 1.0); choir::ConsonantDetector b; b.prepare(sr); double worst = 0;
            for (size_t i = (size_t) (.05 * sr); i < (size_t) (.40 * sr); ++i) worst = std::max(worst, (double) b.process(bright.x[i]));
            std::printf("[1] stress: an unnaturally bright vowel scores at most %.2f\n", worst);
            CHECK(worst < .25, "a very bright vowel scores %.2f as a consonant (limit 0.25)", worst);
        }
        const auto ph = makePhrase(sr, 2);
        choir::ConsonantDetector d; d.prepare(sr);
        std::vector<float> score(ph.x.size()); for (size_t i = 0; i < ph.x.size(); ++i) score[i] = d.process(ph.x[i]);
        // vowels: the middle of each vowel (away from the edges)
        double worstVowel = 0; size_t cursor = 0; int vowels = 0;
        auto inConsonant = [&](size_t i) { for (auto& c : ph.consonants) if (i + (size_t) (.03 * sr) >= c.first && i < c.second + (size_t) (.06 * sr)) return true; return false; };
        for (size_t i = (size_t) (.05 * sr); i + (size_t) (.05 * sr) < ph.x.size(); ++i) if (!inConsonant(i) && ph.x[i] != 0.f) { worstVowel = std::max(worstVowel, (double) score[i]); ++vowels; }
        (void) cursor;
        std::printf("[1] across %d vowel samples the consonant score never exceeds %.2f\n", vowels, worstVowel);
        CHECK(worstVowel < .15, "a bright sung vowel reaches a consonant score of %.2f (limit 0.15)", worstVowel);
        // bursts: how high and how fast
        for (size_t c = 0; c < ph.consonants.size(); ++c)
        {
            const auto& w = ph.consonants[c]; const size_t len = w.second - w.first;
            double peak = 0; size_t reached = 0;
            for (size_t i = w.first; i < w.second + (size_t) (.01 * sr); ++i) { peak = std::max(peak, (double) score[i]); if (!reached && score[i] > .5f) reached = i - w.first; }
            const double ms = reached ? 1000.0 * (double) reached / sr : 999.0;
            std::printf("[1] consonant %zu (%.0f ms long): score peaks at %.2f, passes 0.5 after %.1f ms\n", c, 1000.0 * (double) len / sr, peak, ms);
            CHECK(peak > .5, "consonant %zu is not found (peak score %.2f)", c, peak);
            CHECK(ms < .6 * 1000.0 * (double) len / sr + 2.0, "consonant %zu is found too late (%.1f ms)", c, ms);
        }
    }

    // 2. on real singing: consonants are a minority, and the same in the raw and the tuned take ---------------------------------
    {
        double share[2]; int k = 0;
        {   // the hard excerpt has the real consonants (around 19 s of the take): a minority of the samples, but not none
            Audio h; CHECK(readWav16(root + "/tests/data/vocal_raw_hard_9s.wav", h), "cannot read the hard fixture");
            choir::ConsonantDetector d; d.prepare(h.sr); const auto m = voicedMask(h.x, h.sr); size_t n = 0, hot = 0;
            for (size_t i = 0; i < h.x.size(); ++i) { const float s = d.process(h.x[i]); if (m[i]) { ++n; if (s > .5f) ++hot; } }
            const double pct = 100.0 * (double) hot / (double) n;
            std::printf("[2] vocal_raw_hard_9s.wav: %.1f%% of the sung samples are flagged as consonants\n", pct);
            CHECK(pct > 1.0 && pct < 20.0, "%.1f%% flagged on the excerpt that has real consonants (expected 1 to 20)", pct);
        }
        for (const char* name : { "vocal_raw_9s.wav", "vocal_autotuned_9s.wav" })
        {
            Audio a; CHECK(readWav16(root + "/tests/data/" + name, a), "cannot read the fixture %s", name);
            choir::ConsonantDetector d; d.prepare(a.sr); const auto m = voicedMask(a.x, a.sr); size_t n = 0, hot = 0;
            for (size_t i = 0; i < a.x.size(); ++i) { const float s = d.process(a.x[i]); if (m[i]) { ++n; if (s > .5f) ++hot; } }
            share[k] = 100.0 * (double) hot / (double) n;
            std::printf("[2] %s: %.1f%% of the sung samples are flagged as consonants\n", name, share[k]); ++k;
            CHECK(share[k - 1] < 12.0, "%.1f%% of the sung samples flagged as consonants: vowels are being mistaken for them (a real take measures 5 to 8%%)", share[k - 1]);
        }
        CHECK(std::fabs(share[0] - share[1]) < 3.0, "the raw and the tuned take differ by %.1f points", std::fabs(share[0] - share[1]));
    }

    // 3. in the stage: the choir stops smearing the consonants, the vowels are untouched, OFF is the old sound -------------------------
    {
        const auto ph = makePhrase(sr, 5);
        auto base = pluginDefaults(); base.mix = 1.f; base.leadGain = 0.f; base.levelMatch = false; base.master = 1.f;
        auto on = base; on.consonants = 1.f; auto off = base; off.consonants = 0.f;
        const auto yOff = render(ph.x, sr, off).l, yOn = render(ph.x, sr, on).l;
        // how well the output's high-frequency envelope follows the input's (best alignment within 150 ms, normalised correlation)
        const auto eIn = envelope(ph.x, sr, 3000.0), eOff = envelope(yOff, sr, 3000.0), eOn = envelope(yOn, sr, 3000.0);
        const auto follow = [&](const std::vector<float>& e)
        {
            double best = -1;
            for (int lag = 0; lag <= (int) (.15 * sr); lag += 8)
            {
                double ab = 0, aa = 0, bb = 0;
                for (size_t i = (size_t) sr; i + (size_t) lag < ph.x.size(); i += 4) { const double a = eIn[i], b = e[i + (size_t) lag]; ab += a * b; aa += a * a; bb += b * b; }
                best = std::max(best, ab / std::sqrt(aa * bb + 1e-30));
            }
            return best;
        };
        // smear: high-frequency energy in the 30..120 ms after each consonant (where the clean version has silence or a vowel), against inside it
        const auto smear = [&](const std::vector<float>& y, size_t latency)
        {
            double inside = 0, after = 0; const auto e = envelope(y, sr, 3000.0);
            for (const auto& c : ph.consonants)
            {
                if (c.first < (size_t) sr) continue;
                for (size_t i = c.first + latency; i < c.second + latency && i < e.size(); ++i) inside += (double) e[i] * e[i];
                for (size_t i = c.second + latency + (size_t) (.03 * sr); i < c.second + latency + (size_t) (.12 * sr) && i < e.size(); ++i) after += (double) e[i] * e[i];
            }
            return dB(std::sqrt(after / std::max(inside, 1e-30)));
        };
        const size_t latency = 128;   // the stage's own block latency; the choir's grains add a variable delay on top, which the correlation search absorbs
        const double fOff = follow(eOff), fOn = follow(eOn), sOff = smear(yOff, latency), sOn = smear(yOn, latency);
        std::printf("[3] high-frequency envelope follows the input: consonants off %.2f, on %.2f\n", fOff, fOn);
        std::printf("[3] smear (energy after a consonant against inside it): off %.1f dB, on %.1f dB\n", sOff, sOn);
        CHECK(fOn > fOff + .04 && fOn > .95, "CONSONANTS should make the high-frequency timing follow the input (%.2f against %.2f)", fOn, fOff);
        CHECK(sOn < sOff - 6.0, "CONSONANTS should cut the smear by at least 6 dB (%.1f dB against %.1f dB)", sOn, sOff);
        // the vowels are unharmed: the level of the choir in the middle of the vowels changes by under 1 dB
        double a = 0, b = 0; size_t n = 0;
        for (size_t i = (size_t) (2.0 * sr); i + (size_t) (.5 * sr) < ph.x.size(); ++i) if (ph.x[i] != 0.f) { bool near = false; for (auto& c : ph.consonants) if (i + (size_t) (.1 * sr) >= c.first && i < c.second + (size_t) (.15 * sr)) { near = true; break; } if (!near) { a += (double) yOff[i] * yOff[i]; b += (double) yOn[i] * yOn[i]; ++n; } }
        const double vowelChange = dB(std::sqrt(b / (double) n)) - dB(std::sqrt(a / (double) n));
        std::printf("[3] vowel level with CONSONANTS on against off: %+.2f dB\n", vowelChange);
        CHECK(std::fabs(vowelChange) < 1.0, "CONSONANTS changed the vowels by %+.2f dB", vowelChange);
        // with the choir off nothing is added (the dry voice already has its own consonants), and a vowel-only signal is bit-identical
        auto choirOff = on; choirOff.choirOn = false; auto choirOffRef = off; choirOffRef.choirOn = false;
        const auto c1 = render(ph.x, sr, choirOff).l, c2 = render(ph.x, sr, choirOffRef).l; double worst = 0;
        for (size_t i = 0; i < c1.size(); ++i) worst = std::max(worst, (double) std::fabs(c1[i] - c2[i]));
        CHECK(worst == 0.0, "CONSONANTS added %g to the output with the choir off", worst);
        const auto vowels = sungPhrase(sr, 6, std::pow(10.0, -18.0 / 20), 196.0);
        const auto v1 = render(vowels, sr, on).l, v2 = render(vowels, sr, off).l; worst = 0;
        for (size_t i = 0; i < v1.size(); ++i) worst = std::max(worst, (double) std::fabs(v1[i] - v2[i]));
        std::printf("[3] a vowel-only phrase: CONSONANTS on differs from off by at most %.5f\n", worst);
        CHECK(worst < .01, "CONSONANTS changed a phrase that has no consonants by %.5f", worst);
        auto p0 = off; auto classicOn = on; classicOn.classicStage = true; auto classicOff = off; classicOff.classicStage = true;
        const auto k1 = render(ph.x, sr, classicOn).l, k2 = render(ph.x, sr, classicOff).l; worst = 0;
        for (size_t i = 0; i < k1.size(); ++i) worst = std::max(worst, (double) std::fabs(k1[i] - k2[i]));
        CHECK(worst == 0.0, "the classic stage changed with CONSONANTS by %g", worst);
        (void) p0;
    }
    // 4. on real singing: inside the consonants the output follows the voice at the stage's own latency, and the smear after them shrinks
    {
        Audio h; CHECK(readWav16(root + "/tests/data/vocal_raw_hard_9s.wav", h), "cannot read the hard fixture");
        auto base = pluginDefaults(); base.mix = 1.f; base.leadGain = 0.f; base.levelMatch = false; base.master = 1.f;
        auto on = base; on.consonants = 1.f; auto off = base; off.consonants = 0.f;
        const auto yOff = render(h.x, h.sr, off).l, yOn = render(h.x, h.sr, on).l;
        const auto eIn = envelope(h.x, h.sr, 3000.0), eOff = envelope(yOff, h.sr, 3000.0), eOn = envelope(yOn, h.sr, 3000.0);
        // the consonant windows, found from the input alone by a separate rule: much of the energy above 3 kHz and a sound that is not silent
        std::vector<float> total(h.x.size()); { double e = 0; const double k = 1.0 - std::exp(-1.0 / (.005 * h.sr)); for (size_t i = 0; i < h.x.size(); ++i) { e += k * (std::fabs(h.x[i]) - e); total[i] = (float) e; } }
        std::vector<char> window(h.x.size(), 0); size_t inWindow = 0;
        for (size_t i = (size_t) h.sr; i < h.x.size(); ++i) if (total[i] > .004f && eIn[i] > .30f * total[i]) { window[i] = 1; ++inWindow; }
        CHECK(inWindow > (size_t) (.2 * h.sr), "the hard excerpt should contain at least 0.2 s of consonant-like sound (found %.2f s)", (double) inWindow / h.sr);
        const size_t latency = 128;   // what the clean path promises: one stage block
        const auto fidelity = [&](const std::vector<float>& e)   // normalised correlation of the output envelope with the input's, inside the windows, at the stage latency
        {
            double ab = 0, aa = 0, bb = 0;
            for (size_t i = (size_t) h.sr; i + latency < h.x.size(); ++i) if (window[i]) { const double a = eIn[i], b = e[i + latency]; ab += a * b; aa += a * a; bb += b * b; }
            return ab / std::sqrt(aa * bb + 1e-30);
        };
        // the smear: high-frequency energy in the 40 to 140 ms after each window ends, against the energy inside it
        const auto smear = [&](const std::vector<float>& e)
        {
            double inside = 0, after = 0;
            for (size_t i = (size_t) h.sr; i + (size_t) (.15 * h.sr) < h.x.size(); ++i)
            {
                if (window[i]) inside += (double) e[i + latency] * e[i + latency];
                else if (i > 0 && window[i - 1]) {}                                         // (the end of a window)
            }
            size_t windows = 0;
            for (size_t i = (size_t) h.sr + 1; i + (size_t) (.15 * h.sr) < h.x.size(); ++i)
                if (window[i - 1] && !window[i])
                {
                    ++windows;
                    for (size_t j = i + (size_t) (.04 * h.sr); j < i + (size_t) (.14 * h.sr); ++j) if (!window[j]) after += (double) e[j + latency] * e[j + latency];
                }
            (void) windows;
            return dB(std::sqrt(after / std::max(inside, 1e-30)));
        };
        const double fOff = fidelity(eOff), fOn = fidelity(eOn), sOff = smear(eOff), sOn = smear(eOn);
        std::printf("[4] real singing, inside the consonants: the output follows the voice at the stage latency %.2f without, %.2f with CONSONANTS\n", fOff, fOn);
        std::printf("[4] real singing: energy after the consonants against inside them %.1f dB without, %.1f dB with CONSONANTS\n", sOff, sOn);
        CHECK(fOn > fOff + .15 && fOn > .6, "inside the consonants the output should follow the voice (%.2f against %.2f)", fOn, fOff);
        CHECK(sOn < sOff - 3.0, "CONSONANTS should cut the smear after real consonants by at least 3 dB (%.1f against %.1f dB)", sOn, sOff);
        auto lvl = [&](const std::vector<float>& y) { double q = 0; size_t n = 0; for (size_t i = (size_t) h.sr; i < y.size(); ++i) { q += (double) y[i] * y[i]; ++n; } return dB(std::sqrt(q / (double) n)); };
        std::printf("[4] overall level with CONSONANTS on against off: %+.2f dB\n", lvl(yOn) - lvl(yOff));
        CHECK(std::fabs(lvl(yOn) - lvl(yOff)) < 1.5, "CONSONANTS changes the overall level by %+.2f dB", lvl(yOn) - lvl(yOff));
    }
    std::printf("ALL CONSONANT TESTS PASSED\n");
}
