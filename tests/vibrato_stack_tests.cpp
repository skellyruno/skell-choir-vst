// VIBRATO SPREAD (FXParams::vibratoSpread) and STACKS (StackPresets.h).
// Spread: every harmony voice re-plays the lead, so a section's four singers wobble in lockstep. With the knob up each bends the lead's vibrato by its own
// fixed amount. At 0 nothing may change (the engine must not touch the voices), a steady note must stay steady, and a note change must not be bent.
// Stacks: the presets must be valid (a real choir, in range) and the processor/editor must really apply and list them.
#include "../Source/ChoirFX.h"
#include "../Source/StackPresets.h"
#include "test_support.h"
#include <fstream>
#include <set>
#include <sstream>

using namespace choir;
static const double PI = 3.14159265358979323846;

static std::vector<float> note(int sr, double seconds, double f0, double cents, double stepAt = -1, double stepSemis = 0)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / sr, st = (stepAt >= 0 && t >= stepAt) ? stepSemis : 0.0;
        const double f = f0 * std::pow(2.0, (cents * std::sin(2 * PI * 5.5 * t) + 100 * st) / 1200.0);
        ph += 2 * PI * f / sr; double s = 0; for (int h = 1; h <= 12; ++h) s += std::sin(h * ph) / h;
        x[n] = (float) (.25 * s);
    }
    return x;
}
static FXParams params(float spread, bool sync)
{
    FXParams p = ts::pluginDefaults(); p.humanize = 0.f; p.ensemble = 0.f; p.voiceBlend = 0.f; p.formant = 0.f; p.sensitivity = 1.f; p.mode = 0; p.harmony = 0;
    p.lockGrains = false; p.pitchSync = sync; p.vibratoSpread = spread; return p;
}
static std::vector<std::vector<float>> run(const std::vector<float>& x, int sr, FXParams p)
{
    ChoirEffect fx; fx.prepare(sr); fx.setVoiceParameters(p); std::vector<std::vector<float>> v(16); float out[16];
    for (float s : x) { fx.processVoices(s, out); for (int k = 0; k < 16; ++k) v[(size_t) k].push_back(out[k]); }
    return v;
}
// the pitch track (cents re the first reading) of a voice, from the same tracker the plugin uses
static std::vector<double> pitchTrack(const std::vector<float>& s, int sr, double fromSec)
{
    PitchTracker t; t.prepare((float) sr); std::vector<double> m;
    for (size_t n = 0; n < s.size(); ++n) if (t.push(s[n]) && (double) n / sr >= fromSec && t.midi() >= 0) m.push_back(t.midi() * 100.0);
    return m;
}
static double peakToPeak(const std::vector<double>& m) { if (m.empty()) return 0; auto [a, b] = std::minmax_element(m.begin(), m.end()); return *b - *a; }
static double meanOf(const std::vector<double>& m) { double s = 0; for (double v : m) s += v; return m.empty() ? 0 : s / (double) m.size(); }
static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const int sr = 48000;

    for (bool sync : { false, true })
    {
        const char* eng = sync ? "pitch-synchronous" : "two-grain";
        // 1. at 0 the voices are untouched, bit for bit, through the whole life of a note that has vibrato
        {
            const auto x = note(sr, 3.0, 196, 50);
            auto a = run(x, sr, params(0.f, sync)), b = run(x, sr, params(0.f, sync));
            bool same = true; for (int k = 0; k < 16; ++k) same = same && a[(size_t) k] == b[(size_t) k];
            CHECK(same, "%s: two renders at spread 0 differ", eng);
            // switching it on and back off in the middle must return to the exact original sound afterwards
            FXParams p = params(0.f, sync); ChoirEffect fx; fx.prepare(sr); fx.setVoiceParameters(p); std::vector<float> v8; float o[16];
            for (size_t n = 0; n < x.size(); ++n) { p.vibratoSpread = (n > (size_t) sr && n < (size_t) (2 * sr)) ? 1.f : 0.f; fx.setVoiceParameters(p); fx.processVoices(x[n], o); v8.push_back(o[8]); }
            for (size_t n = 0; n < v8.size(); ++n) CHECK(std::isfinite(v8[n]) && std::fabs(v8[n]) < 4.f, "%s: voice 8 went to %f", eng, v8[n]);
        }
        // 2. vibrato: the four singers of a section now have different depths; at 0 they are identical
        {
            const auto x = note(sr, 6.0, 196, 50);
            const auto off = run(x, sr, params(0.f, sync)), on = run(x, sr, params(1.f, sync));
            double dOff[4], dOn[4];
            for (int j = 0; j < 4; ++j) { dOff[j] = peakToPeak(pitchTrack(off[(size_t) (8 + j)], sr, 2.0)); dOn[j] = peakToPeak(pitchTrack(on[(size_t) (8 + j)], sr, 2.0)); }
            const double rOff = *std::max_element(dOff, dOff + 4) / std::max(1.0, *std::min_element(dOff, dOff + 4)), rOn = *std::max_element(dOn, dOn + 4) / std::max(1.0, *std::min_element(dOn, dOn + 4));
            std::printf("[2] %s: vibrato depth (cents p-p) of the four tenors, spread 0: %.0f %.0f %.0f %.0f | spread 1: %.0f %.0f %.0f %.0f (max/min %.2f -> %.2f)\n", eng, dOff[0], dOff[1], dOff[2], dOff[3], dOn[0], dOn[1], dOn[2], dOn[3], rOff, rOn);
            CHECK(rOff < 1.1, "%s: with spread 0 the four singers' vibrato depths differ by x%.2f", eng, rOff);
            CHECK(rOn > 2.0, "%s: spread 1 only separates the singers' vibrato by x%.2f", eng, rOn);
            CHECK(*std::min_element(dOn, dOn + 4) > .15 * dOff[0] && *std::max_element(dOn, dOn + 4) < 2.2 * dOff[0], "%s: depths leave the 40-160%% range", eng);
        }
        // 3. a steady note stays steady, and the mean pitch stays on the intended interval
        {
            const auto x = note(sr, 5.0, 196, 0);
            const auto off = run(x, sr, params(0.f, sync)), on = run(x, sr, params(1.f, sync));
            for (int k : { 0, 5, 10, 15 })
            {
                const auto a = pitchTrack(off[(size_t) k], sr, 2.0), b = pitchTrack(on[(size_t) k], sr, 2.0);
                CHECK(std::fabs(meanOf(a) - meanOf(b)) < 3.0, "%s: voice %d mean pitch moved by %.1f cents on a steady note", eng, k, meanOf(b) - meanOf(a));
                CHECK(peakToPeak(b) < 12.0, "%s: voice %d wobbles %.1f cents p-p on a steady note", eng, k, peakToPeak(b));
            }
        }
        // 4. a new note is not bent: after a jump of a major third (with vibrato) every voice settles at the right pitch within 300 ms
        {
            const auto x = note(sr, 6.0, 196, 35, 3.0, 4);
            const auto off = run(x, sr, params(0.f, sync)), on = run(x, sr, params(1.f, sync));
            for (int k : { 0, 4, 9, 14 })
            {
                const auto a = pitchTrack(off[(size_t) k], sr, 3.6), b = pitchTrack(on[(size_t) k], sr, 3.6);
                CHECK(std::fabs(meanOf(a) - meanOf(b)) < 6.0, "%s: voice %d after a note change is %.1f cents from where it should be", eng, k, meanOf(b) - meanOf(a));
            }
        }
        std::printf("[1,3,4] %s: spread 0 deterministic and finite after toggling, steady notes steady, note changes not bent\n", eng);
    }

    // 5. any input at full spread: finite and bounded (unvoiced noise, silence, sweeps, gaps) at 16 k to 192 kHz
    for (int rate : { 16000, 44100, 96000, 192000 })
        for (int kind = 0; kind < 4; ++kind)
        {
            std::vector<float> x((size_t) (rate * 2.0)); unsigned s = 777u + (unsigned) kind;
            for (size_t n = 0; n < x.size(); ++n) { s = s * 1664525u + 1013904223u; const double nz = ((s >> 8) / 16777216.0) * 2 - 1, t = (double) n / rate;
                x[n] = kind == 0 ? (float) (.5 * nz) : kind == 1 ? 0.f : kind == 2 ? (float) (.4 * std::sin(2 * PI * (80 + 900 * t) * t)) : (float) (.5 * std::sin(2 * PI * 220 * t) * (std::fmod(t, .3) < .15 ? 1.0 : 0.0)); }
            FXParams p = params(1.f, kind & 1); p.harmony = kind % 3; p.humanize = 1.f; p.ensemble = 1.f; ChoirEffect fx; fx.prepare(rate); fx.setVoiceParameters(p); float o[16];
            for (float v : x) { fx.processVoices(v, o); for (int k = 0; k < 16; ++k) CHECK(std::isfinite(o[k]) && std::fabs(o[k]) < 8.f, "%d Hz kind %d: voice %d = %f", rate, kind, k, o[k]); }
        }
    std::printf("[5] 16 kHz .. 192 kHz, noise / silence / sweep / gapped notes at full spread: finite and bounded\n");

    // 6. stacks: valid presets, a real choir
    {
        std::set<std::string> names; const float eps = 1e-6f;
        CHECK(kNumStackPresets >= 4, "only %d stacks", kNumStackPresets);
        for (int i = 0; i < kNumStackPresets; ++i)
        {
            const auto& s = kStackPresets[i];
            CHECK(s.name && s.about && std::string(s.name).size() > 2 && std::string(s.about).size() > 10, "stack %d has no name or description", i);
            CHECK(names.insert(s.name).second, "duplicate stack name %s", s.name);
            int singing = 0; for (bool b : s.sings) singing += b;
            CHECK(singing >= 2, "%s: only %d sections sing", s.name, singing);
            CHECK(s.harmony >= 0 && s.harmony <= 2, "%s: harmony %d", s.name, s.harmony);
            for (float v : { s.mix, s.ensemble, s.humanize, s.voiceBlend, s.choirBreath, s.vibratoSpread }) CHECK(v >= -eps && v <= 1.f + eps, "%s: a value %.2f is out of 0..1", s.name, v);
            CHECK(s.mix >= .55f, "%s: CHOIR MIX %.2f would bury the choir under the lead", s.name, s.mix);
        }
        // the processor really applies them (all four section states, every field), the editor lists them and routes the ids
        const std::string proc = slurp(root + "/Source/PluginProcessor.cpp"), ed = slurp(root + "/Source/PluginEditor.cpp"), hdr = slurp(root + "/Source/PluginProcessor.h");
        CHECK(proc.find("void OmniChoirAudioProcessor::applyStack") != std::string::npos, "applyStack is missing from the processor");
        for (const char* needle : { "\"harmony\",(float)s.harmony", "\"mix\",s.mix", "\"ensemble\",s.ensemble", "\"humanize\",s.humanize", "\"voiceBlend\",s.voiceBlend", "\"choirBreath\",s.choirBreath", "\"vibeSpread\",s.vibratoSpread", "\"sectionState\"+juce::String(i),s.sings[(size_t)i]?0.f:2.f" })
            CHECK(proc.find(needle) != std::string::npos, "applyStack does not set %s", needle);
        CHECK(proc.find("\"micX\",s.micX") != std::string::npos && proc.find("set(\"micX\",s.micX)") != std::string::npos, "the style code lost its mic placement");
        { const auto a = proc.find("void OmniChoirAudioProcessor::applyStack"), b = proc.find("void OmniChoirAudioProcessor::surpriseMe");
          CHECK(a != std::string::npos && b > a, "applyStack not found in order");
          const std::string body = proc.substr(a, b - a);
          for (const char* never : { "leadGain", "\"output\"", "\"room", "applyRoom", "\"leadX\"", "\"micX\"", "levelMatch", "sectionState4", "choirLevel", "\"mode\"" }) CHECK(body.find(never) == std::string::npos, "a stack must never touch %s", never); }
        CHECK(hdr.find("void applyStack(int index)") != std::string::npos && hdr.find("StackPresets.h") != std::string::npos, "processor header does not declare applyStack / include the presets");
        CHECK(ed.find("presetBox.addItem(choir::kStackPresets[i].name,100+i)") != std::string::npos, "the style menu does not list the stacks");
        CHECK(ed.find("processor.applyStack(id-100)") != std::string::npos, "the style menu does not apply the stacks");
        CHECK(proc.find("addFloat(layout,\"vibeSpread\",\"Vibrato Spread\",0.f,1.f,.001f,0.f)") != std::string::npos, "the vibeSpread parameter must default to 0 (off)");
        CHECK(proc.find("p.vibratoSpread=read(raw.vibeSpread)") != std::string::npos, "the parameter does not reach the engine");
        CHECK(!FXParams().vibratoSpread, "vibratoSpread must default to off");
        std::printf("[6] %d stacks valid; applyStack sets everything it should and nothing else; menu lists and applies them\n", kNumStackPresets);
    }
    std::printf("ALL VIBRATO SPREAD AND STACK TESTS PASSED\n");
    return 0;
}
