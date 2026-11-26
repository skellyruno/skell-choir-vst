// PHRASE BREATH (FXParams::phraseBreath) and the reset action. When you start singing after a pause the choir takes a soft, airy breath with you.
// Off: nothing at all. On: the breath lands with the first note after a pause, is airy (highs, not a pitched tone), sits well under the choir, ends
// within a fraction of a second, and never appears while you sing on. Also: the reset action puts every v0.9 spread/tone/breath setting back to neutral.
#include "../Source/StagedChoir.h"
#include "test_support.h"
#include <complex>
#include <fstream>
#include <sstream>
using namespace choir;
static const double PI = 3.14159265358979323846;
static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }
// a note train: 196 Hz notes with vibrato between the given intervals (seconds)
static std::vector<float> notes(int sr, double seconds, std::vector<std::pair<double,double>> on)
{
    std::vector<float> x((size_t) (sr * seconds), 0.f); double ph = 0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / sr; bool sing = false; for (auto& iv : on) sing = sing || (t >= iv.first && t < iv.second);
        ph += 2 * PI * 196 * std::pow(2.0, 15 * std::sin(2 * PI * 5.5 * t) / 1200.0) / sr; if (!sing) continue;
        double s = 0; for (int h = 1; h <= 12; ++h) s += std::sin(h * ph) / h;
        const double fade = std::min(1.0, std::min(t - 0.0, 1e9) * 40.0); (void) fade;
        x[n] = (float) (.3 * s);
    }
    return x;
}
// the same, each note with its own loudness: {from, to, amplitude}
static std::vector<float> notesAmp(int sr, double seconds, std::vector<std::array<double,3>> on)
{
    std::vector<float> x((size_t) (sr * seconds), 0.f); double ph = 0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / sr; double amp = 0; for (auto& iv : on) if (t >= iv[0] && t < iv[1]) amp = iv[2];
        ph += 2 * PI * 196 * std::pow(2.0, 15 * std::sin(2 * PI * 5.5 * t) / 1200.0) / sr; if (amp == 0) continue;
        double s = 0; for (int h = 1; h <= 12; ++h) s += std::sin(h * ph) / h; x[n] = (float) (amp * s);
    }
    return x;
}
static FXParams params(float breath)
{
    FXParams p = ts::pluginDefaults(); p.humanize = 0.f; p.ensemble = 0.f; p.sensitivity = 1.f; p.leadGain = 0.f; p.wet = 0.f; p.phraseBreath = breath; return p;
}
static double rmsOf(const std::vector<float>& x, double from, double to, int sr) { double s = 0; size_t a = (size_t) (from * sr), b = std::min(x.size(), (size_t) (to * sr)); for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return std::sqrt(s / (double) (b - a)); }
static double db(double v) { return 20 * std::log10(v + 1e-12); }
// 1-6 kHz vs below 600 Hz energy of a stretch, from a Hann DFT
static double highShare(const std::vector<float>& x, double from, int sr)
{
    const int N = 4096; double hi = 0, lo = 0; const size_t a = (size_t) (from * sr);
    for (int k = 2; k < N / 2; ++k) { std::complex<double> s = 0; for (int i = 0; i < N; ++i) s += x[a + (size_t) i] * (.5 - .5 * std::cos(2 * PI * i / N)) * std::polar(1.0, -2 * PI * k * i / N); const double f = (double) k * sr / N, e = std::norm(s); if (f >= 1000 && f <= 6000) hi += e; else if (f < 600) lo += e; }
    return 10 * std::log10((hi + 1e-30) / (lo + 1e-30));
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const int sr = 48000;
    // note 1 (after .5 s of silence), a one-second pause, note 2, then singing on without a gap, a short gap (.1 s), more singing
    const auto x = notes(sr, 9.0, { { .5, 2.5 }, { 3.5, 6.0 }, { 6.1, 8.8 } });

    // 1. off = nothing; on = the difference is the breath alone
    const auto off = ts::render(x, sr, params(0.f)), on = ts::render(x, sr, params(1.f)), off2 = ts::render(x, sr, params(0.f));
    CHECK(off.l == off2.l && off.r == off2.r, "two renders with the breath off differ");
    std::vector<float> diff(x.size()); for (size_t i = 0; i < diff.size(); ++i) diff[i] = on.l[i] - off.l[i];
    const double choir = rmsOf(off.l, 1.0, 2.4, sr);
    const double b1 = rmsOf(diff, .5, .75, sr), b2 = rmsOf(diff, 3.5, 3.75, sr);
    std::printf("[1] choir level %.1f dB; breath after the first pause %.1f dB, after the second %.1f dB\n", db(choir), db(b1), db(b2));
    CHECK(b1 > .02 * choir && b2 > .02 * choir, "the breath is inaudible (%.1f / %.1f dB vs choir %.1f dB)", db(b1), db(b2), db(choir));
    CHECK(b1 < .6 * choir && b2 < .6 * choir, "the breath is as loud as the choir (%.1f / %.1f dB vs %.1f dB)", db(b1), db(b2), db(choir));
    // 2. timing: nothing before the first note, over within ~.5 s, nothing while singing on, nothing in a gap that is too short to be a breath
    {
        const double before = rmsOf(diff, 0.0, .495, sr), pause = rmsOf(diff, 2.7, 3.45, sr), late = rmsOf(diff, 4.3, 6.0, sr), shortGap = rmsOf(diff, 6.0, 8.8, sr), tail1 = rmsOf(diff, 1.2, 2.4, sr);
        std::printf("[2] breath before the note %.1f dB, 0.7 s on %.1f dB, in a pause %.1f dB, singing on %.1f dB, after a .1 s gap %.1f dB\n", db(before), db(tail1), db(pause), db(late), db(shortGap));
        CHECK(before < 1e-6, "a breath before any singing (%.1f dB)", db(before));
        CHECK(tail1 < .04 * b1, "the breath is still sounding a second in (%.1f dB vs %.1f dB)", db(tail1), db(b1));
        CHECK(pause < 1e-6 && late < 1e-6, "breath where there was no new phrase (%.1f / %.1f dB)", db(pause), db(late));
        CHECK(shortGap < 1e-6, "a .1 s gap is a consonant, not a breath, yet it breathed (%.1f dB)", db(shortGap));
    }
    // 3. airy: the breath is mostly highs, the choir's note is mostly low
    {
        const double airy = highShare(diff, .52, sr), sung = highShare(off.l, 1.2, sr);
        std::printf("[3] 1-6 kHz vs under 600 Hz: breath %+.1f dB, sung note %+.1f dB\n", airy, sung);
        CHECK(airy > sung + 6.0, "the breath is not airier than the note (%+.1f vs %+.1f dB)", airy, sung);
    }
    // 4. amount: the knob scales it, half is 6 dB-ish quieter
    {
        const auto half = ts::render(x, sr, params(.5f)); std::vector<float> d(x.size()); for (size_t i = 0; i < d.size(); ++i) d[i] = half.l[i] - off.l[i];
        const double r = rmsOf(d, .5, .75, sr) / b1; std::printf("[4] half amount is %.2fx the full breath\n", r);
        CHECK(r > .4 && r < .6, "half amount gives %.2fx", r);
    }
    // 5. any input at any rate: finite and bounded, and it never makes sound out of silence
    for (int rate : { 16000, 44100, 96000, 192000 })
    {
        std::vector<float> n((size_t) (rate * 3.0)); unsigned s = 5; for (size_t i = 0; i < n.size(); ++i) { s = s * 1664525u + 1013904223u; n[i] = (i % (size_t) rate) < (size_t) (rate / 2) ? (float) (((s >> 8) / 16777216.0) * 1.6 - .8) : 0.f; }
        FXParams p = ts::pluginDefaults(); p.phraseBreath = 1.f; StagedChoir sc; sc.prepare(rate); sc.setParameters(p); std::vector<float> l(n.size()), r(n.size()); sc.process(n.data(), n.data(), l.data(), r.data(), (int) n.size());
        for (size_t i = 0; i < l.size(); ++i) CHECK(std::isfinite(l[i]) && std::fabs(l[i]) < 4.f && std::isfinite(r[i]), "%d Hz: %f", rate, l[i]);
    }
    { std::vector<float> z((size_t) (sr * 2.0), 0.f); const auto q = ts::render(z, sr, params(1.f)); for (float v : q.l) CHECK(v == 0.f, "breath out of pure silence"); }
    std::printf("[5] noise bursts at 16 k to 192 kHz finite and bounded; silence stays silent\n");
    // 7. the sections do not breathe at the same instant: basses first, then tenors, altos, sopranos, each a few milliseconds apart
    {
        auto onset = [&](int section) {
            FXParams p = params(1.f), q = params(0.f); for (int i = 0; i < 4; ++i) p.gain[(size_t) i] = q.gain[(size_t) i] = (i == section ? 1.f : 0.f);
            const auto a = ts::render(x, sr, p), b = ts::render(x, sr, q);
            double peak = 0; for (size_t i = (size_t) (.5 * sr); i < (size_t) (.9 * sr); ++i) peak = std::max(peak, (double) std::fabs(a.l[i] - b.l[i]) + std::fabs(a.r[i] - b.r[i]));
            for (size_t i = (size_t) (.5 * sr); i < (size_t) (.9 * sr); ++i) if (std::fabs(a.l[i] - b.l[i]) + std::fabs(a.r[i] - b.r[i]) > .08 * peak) return 1000.0 * ((double) i / sr - .5);
            return -1.0; };
        const double s = onset(0), a = onset(1), t = onset(2), b = onset(3);
        std::printf("[7] the breath starts (ms after the note): soprano %.1f, alto %.1f, tenor %.1f, bass %.1f\n", s, a, t, b);
        CHECK(b >= 0 && s > a - .5 && a > t - .5 && t > b - .5, "the sections do not breathe in order: S %.1f A %.1f T %.1f B %.1f", s, a, t, b);
        CHECK(s - b > 14.0 && s - b < 40.0, "soprano starts %.1f ms after the bass (want 14-40)", s - b);
    }
    // 8. the breath follows the note it starts with: a whisper after a loud passage gets a whispered breath; a soft entry breathes longer than a loud one
    {
        const auto y = notesAmp(sr, 9.0, { { .5, 2.0, .5 }, { 3.5, 5.0, .02 }, { 6.5, 8.0, .5 } });
        const auto o = ts::render(y, sr, params(0.f)), w = ts::render(y, sr, params(1.f)); std::vector<float> d(y.size()); for (size_t i = 0; i < d.size(); ++i) d[i] = w.l[i] - o.l[i];
        const double loud1 = rmsOf(d, .5, .75, sr), soft = rmsOf(d, 3.5, 3.75, sr), loud2 = rmsOf(d, 6.5, 6.75, sr);
        const double noteLoud = rmsOf(o.l, 1.2, 1.9, sr), noteSoft = rmsOf(o.l, 4.2, 4.9, sr);
        std::printf("[8] loud entry: breath %.1f dB (note %.1f); soft entry after it: breath %.1f dB (note %.1f); loud again: %.1f dB\n", db(loud1), db(noteLoud), db(soft), db(noteSoft), db(loud2));
        CHECK(soft < .15 * loud1, "a whispered entry after a loud passage still breathes %.1f dB vs %.1f dB", db(soft), db(loud1));
        CHECK(std::fabs(db(soft) - db(noteSoft) - (db(loud1) - db(noteLoud))) < 10.0, "the soft breath is not in proportion to its note (%.1f vs %.1f dB re the note)", db(soft) - db(noteSoft), db(loud1) - db(noteLoud));
        // how long the breath lasts: the time for it to fall 20 dB under its peak
        auto length = [&](double from) { double pk = 0; const size_t a = (size_t) (from * sr); for (size_t i = a; i < a + (size_t) (.2 * sr); ++i) pk = std::max(pk, std::fabs((double) d[i]));
            size_t last = a; for (size_t i = a; i < a + (size_t) (.6 * sr); ++i) if (std::fabs(d[i]) > .1 * pk) last = i; return 1000.0 * (double) (last - a) / sr; };
        const double lLoud = length(.5), lSoft = length(3.5);
        std::printf("    breath lasts %.0f ms after a loud entry, %.0f ms after a soft one\n", lLoud, lSoft);
        CHECK(lSoft > lLoud + 25.0, "a soft entry's breath (%.0f ms) is not longer than a loud one's (%.0f ms)", lSoft, lLoud);
    }
    // 6. wiring and the reset action
    {
        const std::string proc = slurp(root + "/Source/PluginProcessor.cpp"), ed = slurp(root + "/Source/PluginEditor.cpp");
        CHECK(proc.find("addFloat(layout,\"phraseBreath\",\"Phrase Breath\",0.f,1.f,.001f,0.f)") != std::string::npos, "phraseBreath must default to 0");
        CHECK(proc.find("p.phraseBreath=read(raw.phraseBreath)") != std::string::npos, "the parameter does not reach the engine");
        CHECK(ed.find("addKnob(\"PHRASE BREATH\",\"phraseBreath\"") != std::string::npos, "no knob");
        CHECK(!FXParams().phraseBreath, "phraseBreath must default to off");
        const auto a = proc.find("void OmniChoirAudioProcessor::resetSpreads"), b = proc.find("void OmniChoirAudioProcessor::applyStack");
        CHECK(a != std::string::npos && b > a, "resetSpreads is missing");
        const std::string body = proc.substr(a, b - a);
        for (const char* need : { "\"vibeSpread\",0.f", "\"stageSpread\",1.f", "\"phraseBreath\",0.f", "\"toneAmount\",1.f", "\"tone\"+juce::String(i),0.f" }) CHECK(body.find(need) != std::string::npos, "resetSpreads does not set %s", need);
        for (const char* never : { "leadGain", "\"mix\"", "\"harmony\"", "sectionState", "\"x0\"", "\"output\"", "\"master\"" }) CHECK(body.find(never) == std::string::npos, "resetSpreads must not touch %s", never);
        CHECK(ed.find("presetBox.addItem(\"Reset spreads, tones and breath\",300)") != std::string::npos && ed.find("processor.resetSpreads()") != std::string::npos, "the reset item is not in the menu or does nothing");
        CHECK(ed.find("toned!=0") != std::string::npos, "a toned section does not show a ring on the stage");
        std::printf("[6] default off, knob present, reset puts the spreads, tones and breath back and touches nothing else, toned sections show a ring\n");
    }
    std::printf("ALL PHRASE BREATH TESTS PASSED\n");
    return 0;
}
