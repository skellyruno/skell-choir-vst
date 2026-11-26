// TONE: the T key beside each section on the stage (Normal / Bright / Dark). A gentle high shelf on that section's singers only. Normal must leave the sound
// exactly as it was; Bright and Dark must move the highs and leave the body alone; pressing the key must not click; the button must be wired end to end.
#include "../Source/StagedChoir.h"
#include "../Source/StagePresets.h"
#include "../Source/UI/StageControlLayout.h"
#include "test_support.h"
#include <complex>
#include <fstream>
#include <sstream>
using namespace choir;
static const double PI = 3.14159265358979323846;
static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }
static std::vector<float> voice(int sr, double seconds)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t n = 0; n < x.size(); ++n) { ph += 2 * PI * 196 * std::pow(2.0, 20 * std::sin(2 * PI * 5.5 * n / sr) / 1200.0) / sr; double s = 0; for (int h = 1; h <= 30; ++h) s += std::sin(h * ph) / std::sqrt((double) h); x[n] = (float) (.15 * s); }
    return x;
}
// energy (dB) of a band, from a Hann-windowed DFT of one second taken after `from`
static double band(const std::vector<float>& x, size_t from, int sr, double lo, double hi)
{
    const int N = 1 << 15; double e = 0; std::vector<std::complex<double>> a((size_t) N);
    for (int i = 0; i < N; ++i) a[(size_t) i] = x[from + (size_t) i] * (.5 - .5 * std::cos(2 * PI * i / N));
    // naive DFT over just the band's bins (a few hundred): fine for a test
    const double bin = (double) sr / N;
    for (int k = (int) (lo / bin); k <= (int) (hi / bin); ++k) { std::complex<double> s = 0; for (int i = 0; i < N; ++i) s += a[(size_t) i] * std::polar(1.0, -2 * PI * k * i / N); e += std::norm(s); }
    return 10 * std::log10(e + 1e-30);
}
static FXParams soprano(std::array<int,4> tone)
{
    FXParams p = ts::pluginDefaults(); p.humanize = 0.f; p.ensemble = 0.f; p.sensitivity = 1.f; p.leadGain = 0.f; p.wet = 0.f;
    for (int i = 1; i < 4; ++i) p.gain[(size_t) i] = 0.f;
    p.sectionTone = tone; return p;
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const int sr = 48000; const auto in = voice(sr, 4.0);

    // 1. Bright lifts the highs, Dark lowers them, the body (below 1 kHz) stays, and Normal is the default
    {
        const auto n = ts::render(in, sr, soprano({ 0, 0, 0, 0 })), b = ts::render(in, sr, soprano({ 1, 0, 0, 0 })), d = ts::render(in, sr, soprano({ 2, 0, 0, 0 }));
        const size_t from = (size_t) (2.5 * sr);
        const double hiB = band(b.l, from, sr, 4000, 7000) - band(n.l, from, sr, 4000, 7000), hiD = band(d.l, from, sr, 4000, 7000) - band(n.l, from, sr, 4000, 7000);
        const double loB = band(b.l, from, sr, 200, 800) - band(n.l, from, sr, 200, 800), loD = band(d.l, from, sr, 200, 800) - band(n.l, from, sr, 200, 800);
        std::printf("[1] soprano 4-7 kHz: bright %+.1f dB, dark %+.1f dB | 200-800 Hz: bright %+.2f dB, dark %+.2f dB\n", hiB, hiD, loB, loD);
        CHECK(hiB > 2.5 && hiB < 6.0, "Bright moves the highs by %+.1f dB", hiB);
        CHECK(hiD < -3.0 && hiD > -8.0, "Dark moves the highs by %+.1f dB", hiD);
        CHECK(std::fabs(loB) < .7 && std::fabs(loD) < .7, "the body moves (%+.2f / %+.2f dB)", loB, loD);
    }
    // 1b. TONE AMOUNT: 0 is no tone (bit for bit), 1 is as designed, more is stronger and less is weaker
    {
        auto at = [&](int tone, float amount) { FXParams p = soprano({ tone, 0, 0, 0 }); p.toneAmount = amount; return ts::render(in, sr, p); };
        const auto n = ts::render(in, sr, soprano({ 0, 0, 0, 0 })); const size_t from = (size_t) (2.5 * sr);
        const double ref = band(n.l, from, sr, 4000, 7000);
        const auto z = at(1, 0.f); CHECK(z.l == n.l && z.r == n.r, "tone amount 0 must equal Normal bit for bit");
        const double h5 = band(at(1, .5f).l, from, sr, 4000, 7000) - ref, h1 = band(at(1, 1.f).l, from, sr, 4000, 7000) - ref, h15 = band(at(1, 1.5f).l, from, sr, 4000, 7000) - ref;
        const double d5 = band(at(2, .5f).l, from, sr, 4000, 7000) - ref, d15 = band(at(2, 1.5f).l, from, sr, 4000, 7000) - ref;
        std::printf("[1b] bright at amount .5 / 1 / 1.5: %+.1f / %+.1f / %+.1f dB; dark at .5 / 1.5: %+.1f / %+.1f dB\n", h5, h1, h15, d5, d15);
        CHECK(h5 > .8 && h5 < h1 - .7 && h15 > h1 + .7, "bright does not scale with the amount (%+.1f %+.1f %+.1f)", h5, h1, h15);
        CHECK(d5 < -.8 && d5 > -3.0 && d15 < -4.0, "dark does not scale with the amount (%+.1f %+.1f)", d5, d15);
    }
    // 2. it is per section: tone on the alto does not touch the soprano heard alone
    {
        const auto a = ts::render(in, sr, soprano({ 0, 0, 0, 0 })), b = ts::render(in, sr, soprano({ 0, 1, 2, 1 }));
        double d = 0, e = 0; for (size_t i = 0; i < a.l.size(); ++i) { d += (double) (a.l[i] - b.l[i]) * (a.l[i] - b.l[i]); e += (double) a.l[i] * a.l[i]; }
        std::printf("[2] soprano alone, other sections' tone changed: difference %.1f dB re the signal\n", 10 * std::log10(d / e + 1e-30));
        CHECK(d == 0.0, "another section's tone changed the soprano");
    }
    // 3. Normal after Bright returns to the original sound (the shelf amount goes back to exactly 0), and pressing it does not click
    {
        FXParams p = soprano({ 0, 0, 0, 0 }); StagedChoir ref, tog; ref.prepare(sr); tog.prepare(sr); ref.setParameters(p); tog.setParameters(p);
        std::vector<float> a(in.size()), b(in.size()); std::vector<float> src = in;
        const size_t chunk = 512; double maxStepRef = 0, maxStepTog = 0;
        for (size_t i = 0; i + chunk <= in.size(); i += chunk)
        {
            const double t = (double) i / sr; FXParams q = p; q.sectionTone[0] = (t >= .8 && t < 1.6) ? 1 : (t >= 2.0 && t < 2.8) ? 2 : 0; tog.setParameters(q);
            ref.process(&src[i], &src[i], &a[i], &a[i], (int) chunk); tog.process(&src[i], &src[i], &b[i], &b[i], (int) chunk);
        }
        for (size_t i = (size_t) (.3 * sr) + 2; i + 2 < a.size(); ++i) { maxStepRef = std::max(maxStepRef, (double) std::fabs(a[i + 1] - 2 * a[i] + a[i - 1])); maxStepTog = std::max(maxStepTog, (double) std::fabs(b[i + 1] - 2 * b[i] + b[i - 1])); }
        double d = 0, e = 0; for (size_t i = (size_t) (3.6 * sr); i < a.size(); ++i) { d += (double) (a[i] - b[i]) * (a[i] - b[i]); e += (double) a[i] * a[i]; }
        std::printf("[3] after Bright then Dark then Normal: tail differs %.1f dB re the signal; sharpest step %.4f vs %.4f without pressing\n", 10 * std::log10(d / e + 1e-30), maxStepTog, maxStepRef);
        CHECK(10 * std::log10(d / e + 1e-30) < -60.0, "Normal does not return to the original sound (%.1f dB)", 10 * std::log10(d / e + 1e-30));
        CHECK(maxStepTog < 1.5 * maxStepRef + 1e-4, "pressing the key clicks (%.4f vs %.4f)", maxStepTog, maxStepRef);
    }
    // 4. any input at every tone: finite and bounded (the shelf adds at most +.7 of the highs)
    {
        for (int rate : { 16000, 44100, 96000, 192000 }) { std::vector<float> x((size_t) (rate * 1.0)); unsigned s = 99; for (auto& v : x) { s = s * 1664525u + 1013904223u; v = (float) (((s >> 8) / 16777216.0) * 2 - 1); }
            FXParams p = ts::pluginDefaults(); p.sectionTone = { 1, 2, 1, 2 }; StagedChoir sc; sc.prepare(rate); sc.setParameters(p); std::vector<float> l(x.size()), r(x.size()); sc.process(x.data(), x.data(), l.data(), r.data(), (int) x.size());
            for (size_t i = 0; i < l.size(); ++i) CHECK(std::isfinite(l[i]) && std::isfinite(r[i]) && std::fabs(l[i]) < 4.f && std::fabs(r[i]) < 4.f, "%d Hz: output %f", rate, l[i]); }
        std::printf("[4] noise at 16 k to 192 kHz with every section toned: finite and bounded\n");
    }
    // 5. the key's layout: next to S on the same row, never over S or M, always on the stage, S and M exactly where they were
    {
        int cases = 0;
        for (float width : { 730.f, 860.f, 1300.f, 1500.f })
            for (float cx : { 40.f, 200.f, .48f * 730.f, 600.f, 720.f })
                for (float y : { 5.f, 45.f, 120.f, 380.f, 460.f }) {
                    const float left = 7.f, right = width - 35.f, size = width < 900.f ? 12.f : 19.f;
                    const auto S = stagecontrols::beside(cx - 25.f, cx + 25.f, y, size, left, 10.f, right, 480.f, false), M = stagecontrols::beside(cx - 25.f, cx + 25.f, y, size, left, 10.f, right, 480.f, true);
                    const auto T = stagecontrols::toneBeside(cx - 25.f, cx + 25.f, y, size, left, 10.f, right, 480.f);
                    auto overlap = [](const stagecontrols::Rect& a, const stagecontrols::Rect& b) { return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height; };
                    CHECK(!overlap(T, S) && !overlap(T, M), "T overlaps S or M (width %.0f, x %.0f, y %.0f)", width, cx, y);
                    CHECK(T.x >= left && T.x + T.width <= right && T.y >= 10.f && T.y + T.height <= 480.f, "T leaves the stage (width %.0f, x %.0f, y %.0f)", width, cx, y);
                    CHECK(std::fabs(T.y - S.y) < .01f && T.width == stagecontrols::kButton, "T is not on S's row");
                    ++cases;
                }
        std::printf("[5] T key layout: %d cases, never over S or M, always on the stage\n", cases);
    }
    // 6. wiring
    {
        const std::string proc = slurp(root + "/Source/PluginProcessor.cpp"), ed = slurp(root + "/Source/PluginEditor.cpp");
        CHECK(proc.find("\"tone\"+juce::String(i),1},names[i]+\" Tone\",juce::StringArray{\"Normal\",\"Bright\",\"Dark\"},0") != std::string::npos, "the tone parameters must be Normal/Bright/Dark and default to Normal");
        CHECK(proc.find("p.sectionTone[(size_t)i]=(int)read(raw.tone[(size_t)i])") != std::string::npos, "the tone does not reach the engine");
        CHECK(ed.find("cycleTone(section)") != std::string::npos && ed.find("toneHit(e.position)") != std::string::npos, "the T key does not respond to clicks");
        CHECK(!FXParams().sectionTone[0] && !FXParams().sectionTone[3], "tone must default to Normal");
        CHECK(FXParams().toneAmount == 1.f && proc.find("addFloat(layout,\"toneAmount\",\"Tone Amount\",0.f,1.6f,.01f,1.f)") != std::string::npos && proc.find("p.toneAmount=std::fabs(ta-1.f)<.005f?1.f:ta") != std::string::npos && ed.find("addKnob(\"TONE AMOUNT\",\"toneAmount\"") != std::string::npos, "tone amount must default to 1 (exactly), reach the engine and have a knob");
        std::printf("[6] parameter default Normal, routed to the engine, key reacts to clicks\n");
    }
    std::printf("ALL SECTION TONE TESTS PASSED\n");
    return 0;
}
