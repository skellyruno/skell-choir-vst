// KEY and SCALE: the scale maths, and that the choir really stays in the key you chose.
#include "test_support.h"
#include "../Source/Scales.h"

using namespace ts;
using namespace choir;

static const double PI = 3.14159265358979323846;
static double hz(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }
static double bin(const std::vector<float>& x, int sr, size_t from, size_t to, double f)
{
    double best = 0;
    for (double d = -3.0; d <= 3.0; d += .25)
    {
        double re = 0, im = 0; const size_t n = to - from;
        for (size_t i = from; i < to; ++i) { const double w = .5 - .5 * std::cos(2 * PI * (double) (i - from) / (double) n), v = x[i] * w, ph = 2 * PI * (f + d) * (double) i / sr; re += v * std::cos(ph); im += v * std::sin(ph); }
        best = std::max(best, std::sqrt(re * re + im * im) / (double) n);
    }
    return best;
}
static std::vector<float> held(int sr, double seconds, double f0)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i) { ph += f0 / sr; double v = 0; for (int h = 1; h <= 6; ++h) v += std::sin(2 * PI * h * ph) / h; x[i] = (float) (.1 * v); }
    return x;
}

int main()
{
    // 1. the scale maths -----------------------------------------------------------------------------------------------
    {
        CHECK(kScales[0].count == 12 && kNumScales == 8, "scale table changed shape");
        for (int s = 0; s < kNumScales; ++s)
        {
            const auto& sc = kScales[s]; CHECK(sc.notes[0] == 0, "%s must start on the root", sc.name);
            for (int i = 1; i < sc.count; ++i) CHECK(sc.notes[i] > sc.notes[i - 1] && sc.notes[i] < 12, "%s is not ascending inside one octave", sc.name);
        }
        CHECK(scaleNote(0, 1, 0) == 0 && scaleNote(0, 1, 2) == 4 && scaleNote(0, 1, 7) == 12 && scaleNote(0, 1, -1) == -1 && scaleNote(0, 1, -7) == -12, "C major steps");
        CHECK(scaleNote(9, 2, 0) == 9 && scaleNote(9, 2, 2) == 12 && scaleNote(9, 2, 4) == 16, "A natural minor steps"); // A B C D E F G: root + {0,2,3,5,7,8,10}
        CHECK(nearestIndex(64, 0, 1) == 5 * 7 + 2 && scaleNote(0, 1, nearestIndex(64, 0, 1)) == 64, "E4 is the third degree of the sixth C-major octave (step index 37)");
        CHECK(scaleNote(0, 1, nearestIndex(61, 0, 1)) == 60, "C# snaps down to C in C major (ties go to the lower note)");
        CHECK(inScale(67, 0, 1) && !inScale(66, 0, 1) && inScale(66, 6, 1), "G is in C major, F# is not, but F# is in F# major");
        CHECK(stepsFor(0, 1) == 4 && stepsFor(1, 1) == 2 && stepsFor(2, 1) == -3 && stepsFor(3, 1) == -7, "7-note scales move +4, +2, -3, -7 steps");
        CHECK(stepsFor(0, 6) == 3 && stepsFor(1, 6) == 2 && stepsFor(2, 6) == -2 && stepsFor(3, 6) == -5, "5-note scales move +3, +2, -2, -5 steps");
        CHECK(harmonyIntervals(64, 0, 0) == kFixedIntervals, "Chromatic must leave the v0.7 intervals untouched");
        std::printf("[1] scale table and scale maths\n");
    }

    // 2. diatonic harmony: every note lands in the key, from every note of the scale, in every key and scale -----------------
    {
        int checked = 0;
        for (int scale = 1; scale < kNumScales; ++scale)
            for (int root = 0; root < 12; ++root)
                for (int note = 48; note <= 84; ++note)
                {
                    const auto iv = harmonyIntervals(note, root, scale);
                    const bool sungInScale = inScale(note, root, scale);
                    for (int s = 0; s < 4; ++s)
                    {
                        CHECK(inScale(note + iv[(size_t) s], root, scale), "%s in %s: singing %d, section %d lands on %d which is not in the scale", kScales[scale].name, kKeyNames[root], note, s, note + iv[(size_t) s]);
                        CHECK(std::abs(iv[(size_t) s]) <= 14, "an interval of %d semitones is too large", iv[(size_t) s]);
                    }
                    if (sungInScale)    // from a note of the scale the sections keep their order: soprano above alto above the lead above tenor above bass
                        CHECK(iv[0] > iv[1] && iv[1] > 0 && iv[2] < 0 && iv[3] < iv[2], "voice order broken at %d in %s %s", note, kKeyNames[root], kScales[scale].name);
                    ++checked;
                }
        // the classic case: C major, singing E: soprano B, alto G, tenor B (a sixth... B3), bass E
        const auto e = harmonyIntervals(64, 0, 1);
        CHECK(64 + e[0] == 71 && 64 + e[1] == 67 && 64 + e[2] == 59 && 64 + e[3] == 52, "singing E in C major should give B, G, B, E (got %d %d %d %d)", 64 + e[0], 64 + e[1], 64 + e[2], 64 + e[3]);
        std::printf("[2] %d (scale, key, note) combinations: every section lands in the scale\n", checked);
    }

    // 3. heard: singing E4 in C major, the choir sings G, not the G# a fixed major third would give -----------------------
    {
        const int sr = 44100; const auto x = held(sr, 7.0, hz(64));
        const size_t from = (size_t) (3.0 * sr), to = (size_t) (7.0 * sr);
        const auto choirOnly = [&](int scale)
        {
            auto p = pluginDefaults(); p.mode = 0; p.mix = 1.f; p.leadGain = 0.f; p.levelMatch = false; p.humanize = .2f; p.ensemble = .2f;
            p.keyRoot = 0; p.scale = scale; return render(x, sr, p).l;
        };
        const auto fixed = choirOnly(0), keyed = choirOnly(1);
        const double gSharpFixed = dB(bin(fixed, sr, from, to, hz(68))), gFixed = dB(bin(fixed, sr, from, to, hz(67)));
        const double gSharpKeyed = dB(bin(keyed, sr, from, to, hz(68))), gKeyed = dB(bin(keyed, sr, from, to, hz(67)));
        std::printf("[3] sung E4: without a key G# %.1f dB, G %.1f dB; in C major G# %.1f dB, G %.1f dB\n", gSharpFixed, gFixed, gSharpKeyed, gKeyed);
        CHECK(gSharpFixed > gFixed + 10.0, "without a key the alto should sing the major third (G#): %.1f vs %.1f dB", gSharpFixed, gFixed);
        CHECK(gKeyed > gSharpKeyed + 10.0, "in C major the alto should sing G, not G#: %.1f vs %.1f dB", gKeyed, gSharpKeyed);
        // and every note the keyed choir sings is in the key: the energy at the notes outside C major stays well under the energy inside it
        double inKey = -1e9, outKey = -1e9;
        for (int n = 52; n <= 76; ++n) { const double e = dB(bin(keyed, sr, from, to, hz(n))); if (inScale(n, 0, 1)) inKey = std::max(inKey, e); else outKey = std::max(outKey, e); }
        std::printf("[3] keyed choir: strongest note in the key %.1f dB, strongest note outside it %.1f dB\n", inKey, outKey);
        CHECK(inKey - outKey > 12.0, "the keyed choir leaks %.1f dB into notes outside the key", inKey - outKey);
    }

    // 4. the unison harmony ignores the key; the key is untouched by MIDI chord mode ------------------------------------------
    {
        const int sr = 44100; const auto x = held(sr, 6.0, hz(64)); const size_t from = (size_t) (3.0 * sr), to = (size_t) (6.0 * sr);
        auto p = pluginDefaults(); p.mix = 1.f; p.leadGain = 0.f; p.levelMatch = false; p.humanize = .2f; p.ensemble = .2f; p.scale = 1; p.harmony = 2;
        const auto uni = render(x, sr, p).l;
        const double same = dB(bin(uni, sr, from, to, hz(64))), other = std::max(dB(bin(uni, sr, from, to, hz(67))), dB(bin(uni, sr, from, to, hz(71))));
        CHECK(same > other + 12.0, "unison harmony must keep every section on the sung note (%.1f vs %.1f dB)", same, other);
        std::printf("[4] unison harmony stays on the sung note whatever the key\n");
    }

    // 5. vibrato does not shake the key-aware harmony: the intervals do not flip on every wobble ----------------------------------
    {
        const int sr = 44100; const auto x = sungPhrase(sr, 9, std::pow(10.0, -18.0 / 20), hz(64));
        auto p = pluginDefaults(); p.mix = 1.f; p.leadGain = 0.f; p.levelMatch = false; p.scale = 1; p.keyRoot = 0;
        const auto y = render(x, sr, p).l; const size_t from = (size_t) (3.0 * sr), to = (size_t) (8.0 * sr);
        double inKey = -1e9, outKey = -1e9;
        for (int n = 52; n <= 76; ++n) { const double e = dB(bin(y, sr, from, to, hz(n))); if (inScale(n, 0, 1)) inKey = std::max(inKey, e); else outKey = std::max(outKey, e); }
        std::printf("[5] sung phrase with vibrato in C major: in key %.1f dB, outside %.1f dB\n", inKey, outKey);
        CHECK(inKey - outKey > 6.0, "with vibrato the harmony leaves the key: only %.1f dB over the wrong notes", inKey - outKey);
    }
    std::printf("ALL KEY AND SCALE TESTS PASSED\n");
}
