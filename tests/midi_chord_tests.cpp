// MIDI chord mode: which section sings which key (ChordTracker), and that the choir really sings the chord you hold, from your voice.
#include "test_support.h"
#include "../Source/MidiChord.h"

using namespace ts;
using choir::ChordTracker;

static const double PI = 3.14159265358979323846;
static double hz(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }

// how strongly the signal has a component near this frequency (searched +-4 Hz: the singers are detuned a few cents, and drift)
static double bin(const std::vector<float>& x, int sr, size_t from, size_t to, double f)
{
    double best = 0;
    for (double d = -4.0; d <= 4.0; d += .25)
    {
        double re = 0, im = 0; const size_t n = to - from;
        for (size_t i = from; i < to; ++i) { const double w = .5 - .5 * std::cos(2 * PI * (double) (i - from) / (double) n), v = x[i] * w, ph = 2 * PI * (f + d) * (double) i / sr; re += v * std::cos(ph); im += v * std::sin(ph); }
        best = std::max(best, std::sqrt(re * re + im * im) / (double) n);
    }
    return best;
}

// a steady sung vowel at f0 (harmonics 1..6), the thing the choir is made from
static std::vector<float> held(int sr, double seconds, double f0)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i) { ph += f0 / sr; double v = 0; for (int h = 1; h <= 6; ++h) v += std::sin(2 * PI * h * ph) / h; x[i] = (float) (.1 * v); }
    return x;
}
static std::vector<float> choirOnly(const std::vector<float>& x, int sr, const std::array<int,4>& notes, int mode = 2)
{
    auto p = pluginDefaults(); p.mode = mode; p.mix = 1.f; p.leadGain = 0.f; p.midiNotes = notes; p.humanize = .2f; p.ensemble = .2f;
    return render(x, sr, p).l;
}
static double energyDb(const std::vector<float>& y, int sr, size_t from, size_t to, double midi) { return dB(bin(y, sr, from, to, hz(midi))); }

int main()
{
    const int sr = 44100;

    // 1. which section sings which key ----------------------------------------------------------------------------
    {
        ChordTracker c;
        CHECK(!c.any() && c.sections()[0] == -1 && c.sections()[3] == -1, "an empty chord must be silent");
        c.noteOn(60);
        auto s = c.sections(); CHECK(s[0] == 60 && s[1] == 60 && s[2] == 60 && s[3] == 60, "one key: everyone sings it");
        c.noteOn(67); s = c.sections();
        CHECK(s[3] == 60 && s[2] == 60 && s[1] == 67 && s[0] == 67, "two keys: bass+tenor the lower, alto+soprano the higher (%d %d %d %d)", s[0], s[1], s[2], s[3]);
        c.noteOn(64); s = c.sections();
        CHECK(s[3] == 60 && s[2] == 64 && s[1] == 64 && s[0] == 67, "three keys: bass lowest, tenor+alto the middle, soprano highest (%d %d %d %d)", s[0], s[1], s[2], s[3]);
        c.noteOn(72); s = c.sections();
        CHECK(s[3] == 60 && s[2] == 64 && s[1] == 67 && s[0] == 72, "four keys: one each, soprano highest (%d %d %d %d)", s[0], s[1], s[2], s[3]);
        c.noteOn(48); c.noteOn(76); s = c.sections();
        CHECK(s[3] == 48 && s[0] == 76 && s[1] > s[2] && s[2] > s[3] && s[1] < s[0], "six keys: lowest to the bass, highest to the soprano, in order (%d %d %d %d)", s[0], s[1], s[2], s[3]);
        c.noteOff(76); c.noteOff(48); c.noteOff(72); c.noteOff(64); c.noteOff(67); c.noteOff(60);
        CHECK(!c.any(), "all keys released: nothing held");
        // the sustain pedal: released keys keep sounding until the pedal comes up, and a re-struck key is not stuck
        c.sustain(true); c.noteOn(60); c.noteOn(64); c.noteOff(60); c.noteOff(64);
        CHECK(c.any() && c.sections()[0] == 64 && c.sections()[3] == 60, "with the pedal down released keys keep sounding");
        c.sustain(false); CHECK(!c.any(), "pedal up: they stop");
        c.noteOn(55); c.sustain(true); c.noteOff(55); c.noteOn(55); c.noteOff(55); c.sustain(false); CHECK(!c.any(), "a re-struck pedalled key does not stick");
        c.noteOn(60); c.allOff(); CHECK(!c.any(), "all notes off");
        c.noteOn(-5); c.noteOn(200); CHECK(!c.any(), "out-of-range notes are ignored");
        std::printf("[1] chord assignment (1..6 keys) and the sustain pedal behave\n");
    }

    // 2. the choir sings the chord: G3 sung (MIDI 55), C major held in a different octave ---------------------------
    {
        const double sung = 55.0; const auto x = held(sr, 7.0, hz(sung));
        const std::array<int,4> chord {{ 72, 67, 64, 60 }};   // soprano C5, alto G4, tenor E4, bass C4: as the keys sort
        const auto y = choirOnly(x, sr, chord);
        const size_t from = (size_t) (3.0 * sr), to = (size_t) (7.0 * sr);
        // the notes the choir ends up on: each interval is folded to within an octave of the singer, so C5 is heard as C4 and the C4/E4/G4 chord is whole
        const double inChord[] = { 60, 64, 67 }, outOfChord[] = { 62, 65, 69, 71, 63, 66, 68 };   // C E G | D F A B D# F# G#
        // (a single frequency bin is not a loudness measure: the formant handling moves energy between harmonics, so one voice can read
        // 10 dB under another here while the choir's total level is flat to 2 dB across all intervals. The bar is therefore: every chord note
        // clearly above every wrong note, and well above on average.)
        double minIn = 1e9, meanIn = 0, maxOut = -1e9;
        for (double n : inChord) { const double e = energyDb(y, sr, from, to, n); minIn = std::min(minIn, e); meanIn += e / 3; }
        for (double n : outOfChord) maxOut = std::max(maxOut, energyDb(y, sr, from, to, n));
        std::printf("[2] sung G3, held C major: weakest chord note %.1f dB, mean %.1f dB, strongest wrong note %.1f dB\n", minIn, meanIn, maxOut);
        CHECK(minIn - maxOut > 8.0 && meanIn - maxOut > 15.0, "the choir does not clearly sing C, E and G: weakest %.1f dB and mean %.1f dB over the wrong notes", minIn - maxOut, meanIn - maxOut);
        // and it is a different chord when different keys are held: F major (F A C) from the same voice. Folded to within an octave of the
        // singer (G3), the notes heard are A3, C4 and F4.
        const auto y2 = choirOnly(x, sr, {{ 77, 72, 69, 65 }});
        double minF = 1e9, meanF = 0, maxNotF = -1e9;
        for (double n : { 57.0, 60.0, 65.0 }) { const double e = energyDb(y2, sr, from, to, n); minF = std::min(minF, e); meanF += e / 3; }
        for (double n : { 58.0, 59.0, 62.0, 63.0, 64.0, 66.0, 67.0 }) maxNotF = std::max(maxNotF, energyDb(y2, sr, from, to, n));
        std::printf("[2] same voice, held F major: weakest chord note %.1f dB, mean %.1f dB, strongest wrong note %.1f dB\n", minF, meanF, maxNotF);
        CHECK(minF - maxNotF > 8.0 && meanF - maxNotF > 15.0, "the choir does not move to F, A and C: weakest %.1f dB, mean %.1f dB over the wrong notes", minF - maxNotF, meanF - maxNotF);
    }

    // 3. no key held: silence. The dry mode keeps singing ------------------------------------------------------------
    {
        const auto x = held(sr, 6.0, hz(55)); const size_t from = (size_t) (3.0 * sr), to = (size_t) (6.0 * sr);
        auto rmsOf = [&](const std::vector<float>& y) { double s = 0; for (size_t i = from; i < to; ++i) s += (double) y[i] * y[i]; return dB(std::sqrt(s / (double) (to - from))); };
        const double none = rmsOf(choirOnly(x, sr, {{ -1, -1, -1, -1 }})), some = rmsOf(choirOnly(x, sr, {{ 72, 67, 64, 60 }})), live = rmsOf(choirOnly(x, sr, {{ -1, -1, -1, -1 }}, 0));
        std::printf("[3] choir level with no key held %.1f dB, with a chord %.1f dB, in Live mode %.1f dB\n", none, some, live);
        CHECK(none < some - 40.0, "the choir should be silent with no key held (%.1f dB against %.1f dB)", none, some);
        CHECK(live > some - 6.0, "Live mode must be unaffected by the MIDI state (%.1f dB against %.1f dB)", live, some);
    }

    // 4. the choir follows the singer up and down: a low bass note is folded, not shifted by two octaves ---------------
    {
        const auto x = held(sr, 7.0, hz(55)); const size_t from = (size_t) (3.0 * sr), to = (size_t) (7.0 * sr);
        const auto y = choirOnly(x, sr, {{ 36, 36, 36, 36 }});   // everyone holds C2: far below the singer
        // 36 - 55 = -19 semitones: folded to -7 -> sings MIDI 48 (C3)
        const double c3 = energyDb(y, sr, from, to, 48), wrong = std::max(energyDb(y, sr, from, to, 49), energyDb(y, sr, from, to, 50));
        std::printf("[4] C2 held far below G3: choir on C3 %.1f dB, on C#3/D3 %.1f dB\n", c3, wrong);
        CHECK(c3 - wrong > 8.0, "a note far below the singer should be folded to the nearest octave (C3), got only %.1f dB over its neighbours", c3 - wrong);
    }

    // 5. vibrato in the singer must not shake the chord: the held chord stays in tune ---------------------------------
    {
        const auto x = sungPhrase(sr, 9, std::pow(10.0, -18.0 / 20), hz(55));   // gated phrases with a +-20 cent vibrato
        const auto y = choirOnly(x, sr, {{ 72, 67, 64, 60 }});
        const size_t from = (size_t) (3.0 * sr), to = (size_t) (8.0 * sr);
        double minIn = 1e9, maxOut = -1e9;
        for (double n : { 60.0, 64.0, 67.0 }) minIn = std::min(minIn, energyDb(y, sr, from, to, n));
        for (double n : { 62.0, 65.0, 69.0, 71.0 }) maxOut = std::max(maxOut, energyDb(y, sr, from, to, n));
        std::printf("[5] sung phrase with vibrato, held C major: chord notes %.1f dB, wrong notes %.1f dB\n", minIn, maxOut);
        CHECK(minIn - maxOut > 4.0, "with vibrato the chord is no longer clear: only %.1f dB over the wrong notes", minIn - maxOut);
    }
    std::printf("ALL MIDI CHORD TESTS PASSED\n");
}
