// Styles, CHOIR SIZE and SURPRISE ME: the numbers are legal, the processor really sets them, nothing the user owns is touched,
// and every style (and a hundred random ones) renders a real voice at a sane level.
#include "test_support.h"
#include "../Source/StylePresets.h"

using namespace ts;
using namespace choir;

struct Range { double lo, hi; };

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const std::string src = readFile(root + "/Source/PluginProcessor.cpp");
    CHECK(!src.empty(), "cannot read %s/Source/PluginProcessor.cpp (pass the project root as the first argument)", root.c_str());

    // 1. the plugin declares every parameter a style sets, and applyStyleValues() really sets each one ------------------
    const char* styleIds[] = { "harmony", "mix", "choirSize", "ensemble", "humanize", "formant", "voiceBlend", "choirVowel", "vowelFollow", "choirBreath", "vibrato" };
    const auto begin = src.find("void OmniChoirAudioProcessor::applyStyleValues");
    CHECK(begin != std::string::npos, "applyStyleValues() is missing from PluginProcessor.cpp");
    const auto end = src.find("void OmniChoirAudioProcessor::", begin + 10);
    const std::string body = src.substr(begin, end - begin);
    for (const char* id : styleIds)
    {
        CHECK(declares(src, id), "parameter '%s' is not declared in PluginProcessor.cpp", id);
        CHECK(body.find(std::string("\"") + id + "\"") != std::string::npos, "applyStyleValues() never sets '%s'", id);
    }
    for (const char* never : { "leadGain", "master", "choirLevel", "levelMatch", "\"mode\"", "sectionState" })
        CHECK(body.find(never) == std::string::npos, "a style must not set %s", never);
    std::printf("[1] every style parameter is declared and set by the processor, and a style never sets lead level, output, AUTO LEVEL, CHOIR LEVEL, mode or solo/mute\n");

    // 2. the style values are legal (the ranges are read from the plugin's own layout) -------------------------------
    Range r[16]; int n = 0; std::vector<std::string> ids;
    const char* floatIds[] = { "mix", "choirSize", "ensemble", "humanize", "formant", "voiceBlend", "choirVowel", "vowelFollow", "choirBreath", "vibrato", "wet", "roomSeconds", "roomSize", "width" };
    for (const char* id : floatIds)
    {
        std::vector<std::string> a; CHECK(layoutArgs(src, std::string("\"") + id + "\"", a), "no layout line for '%s'", id);
        r[n++] = { num(a[3]), num(a[4]) }; ids.push_back(id);
    }
    const auto range = [&](const char* id) { for (size_t i = 0; i < ids.size(); ++i) if (ids[i] == id) return r[i]; return Range { 0, 0 }; };
    const auto legal = [&](const StylePreset& s, const char* what)
    {
        CHECK(s.stage >= 0 && s.stage < kNumStagePresets, "%s: stage %d does not exist", what, s.stage);
        CHECK(s.harmony >= 0 && s.harmony <= 2, "%s: harmony %d out of range", what, s.harmony);
        const struct { const char* id; float v; } vals[] = { {"mix", s.mix}, {"choirSize", s.size}, {"ensemble", s.ensemble}, {"humanize", s.humanize}, {"formant", s.formant},
                                                          {"voiceBlend", s.voiceBlend}, {"choirVowel", s.choirVowel}, {"vowelFollow", s.vowelFollow}, {"choirBreath", s.choirBreath}, {"vibrato", s.vibrato} };
        for (const auto& v : vals) { const auto rg = range(v.id); CHECK(v.v >= rg.lo - 1e-6 && v.v <= rg.hi + 1e-6, "%s: %s = %.3f is outside the plugin's range %.2f..%.2f", what, v.id, v.v, rg.lo, rg.hi); }
        // and after the stage preset and the size macro at either end, the engine values stay legal too
        for (float size : { 0.f, .5f, 1.f })
        {
            FXParams p = pluginDefaults(); float sz = 0; applyStyle(s, p, sz); applySize(p, size);
            const struct { const char* id; float v; } eng[] = { {"ensemble", p.ensemble}, {"humanize", p.humanize}, {"wet", p.wet}, {"roomSeconds", p.roomSeconds}, {"roomSize", p.roomSize}, {"width", p.width} };
            for (const auto& v : eng) { const auto rg = range(v.id); CHECK(v.v >= rg.lo - 1e-6 && v.v <= rg.hi + 1e-6, "%s at size %.1f: %s = %.3f outside %.2f..%.2f", what, size, v.id, v.v, rg.lo, rg.hi); }
        }
    };
    for (int i = 0; i < kNumStylePresets; ++i) legal(kStylePresets[i], kStylePresets[i].name);
    std::uint32_t seed = 12345; for (int i = 0; i < 300; ++i) legal(randomStyle(seed), "random style");
    std::printf("[2] %d styles and 300 random ones are inside every parameter's range, also after CHOIR SIZE 0 / 0.5 / 1\n", kNumStylePresets);

    // 3. CHOIR SIZE: neutral at 0.5, and bigger is wider, wetter, looser -------------------------------------------------
    {
        FXParams base = pluginDefaults(), mid = base; applySize(mid, .5f);
        CHECK(mid.ensemble == base.ensemble && mid.wet == base.wet && mid.width == base.width && mid.roomSeconds == base.roomSeconds && mid.humanize == base.humanize && mid.roomSize == base.roomSize, "CHOIR SIZE 0.5 must change nothing");
        FXParams small = base, big = base; applySize(small, 0.f); applySize(big, 1.f);
        CHECK(big.ensemble > small.ensemble && big.wet > small.wet && big.width > small.width && big.roomSeconds > small.roomSeconds && big.humanize > small.humanize && big.roomSize > small.roomSize, "CHOIR SIZE up must make the choir looser, wetter and wider on every axis");
        // and it can be heard: a bigger choir has a longer, wider sound. Measured on the stereo width the stage reports.
        Audio a; CHECK(readWav16(root + "/tests/data/vocal_raw_9s.wav", a), "cannot read the fixture");
        auto w = [&](float size) { FXParams p = pluginDefaults(); applySize(p, size); choir::StagedChoir sc; sc.prepare(a.sr); sc.setParameters(p); std::vector<float> l(a.x.size()), rr(a.x.size()); sc.process(a.x.data(), a.x.data(), l.data(), rr.data(), (int) a.x.size()); return (double) sc.meters.width.load(); };
        const double ws = w(0.f), wb = w(1.f);
        std::printf("[3] CHOIR SIZE 0 -> 1: stereo width %.2f -> %.2f\n", ws, wb);
        CHECK(wb > ws, "a bigger choir should be wider (%.2f against %.2f)", wb, ws);
    }

    // 4. SURPRISE ME is deterministic per seed, varied across seeds, and drawn from the documented limits ---------------
    {
        std::uint32_t s1 = 777, s2 = 777, s3 = 4242;
        const auto a = randomStyle(s1), b = randomStyle(s2), c = randomStyle(s3);
        CHECK(a.stage == b.stage && a.mix == b.mix && a.ensemble == b.ensemble, "the same seed must give the same style");
        CHECK(a.mix != c.mix || a.ensemble != c.ensemble || a.stage != c.stage, "different seeds must give different styles");
        std::uint32_t st = 99; int stages[16] = {}; int unison = 0, minor = 0, synth = 0; const int count = 500;
        for (int i = 0; i < count; ++i)
        {
            const auto s = randomStyle(st); stages[s.stage]++;
            CHECK(s.mix >= .7f && s.mix <= .9f && s.ensemble >= .35f && s.ensemble <= .85f && s.humanize >= .45f && s.humanize <= .9f && s.size >= .2f && s.size <= .85f, "a random style left its limits");
            unison += s.harmony == 2; minor += s.harmony == 1; synth += s.voiceBlend > .25f;
        }
        int used = 0; for (int i = 0; i < kNumStagePresets; ++i) used += stages[i] > 0;
        std::printf("[4] 500 random styles: %d of %d rooms used, %d%% unison, %d%% minor, %d%% with synthesised voices\n", used, kNumStagePresets, 100 * unison / count, 100 * minor / count, 100 * synth / count);
        CHECK(used == kNumStagePresets, "SURPRISE ME never visits some rooms");
        CHECK(unison < count / 3 && minor > count / 4, "the harmony mix of SURPRISE ME is lopsided");
    }

    // 5. every style, and random ones, sounds right on a real voice --------------------------------------------------
    {
        Audio full; CHECK(readWav16(root + "/tests/data/vocal_raw_9s.wav", full), "cannot read the fixture");
        Audio a = full; a.x.resize((size_t) (6 * a.sr));       // the first 6 s keep the run short
        const auto m = voicedMask(a.x, a.sr); const double in = dB(rms(a.x, m));
        const auto check = [&](const StylePreset& s, const char* what)
        {
            FXParams p = pluginDefaults(); float size = .5f; applyStyle(s, p, size); applySize(p, size);
            const auto o = render(a.x, a.sr, p); const double total = dB(rms(o, m)) - in;
            FXParams c = p; c.leadGain = 0.f; FXParams l = p; l.mix = 0.f;
            const double choirDb = dB(rms(render(a.x, a.sr, c), m)), leadDb = dB((1.0 - p.mix) * rms(render(a.x, a.sr, l), m));
            const double ratio = choir::StagedChoir::kMatchedChoirRatio, dry = 1.0 - p.mix, cc = p.mix * ratio, match = 1.0 / std::sqrt(dry * dry + cc * cc);
            const double vsLead = choirDb - (leadDb + dB(match));
            std::printf("[5] %-18s output %+5.1f dB against the input, choir %+5.1f dB against the lead, peak %.3f\n", what, total, vsLead, peak(o));
            CHECK(total > -6.0 && total < 4.0, "%s: output %+.1f dB from the input (limit -6..+4 dB)", what, total);
            CHECK(vsLead > -4.5, "%s: the choir is %.1f dB under the lead", what, vsLead);
            CHECK(peak(o) <= 1.0, "%s: peak %.3f over full scale", what, peak(o));
            for (size_t i = 0; i < o.l.size(); ++i) CHECK(std::isfinite(o.l[i]) && std::isfinite(o.r[i]), "%s: non-finite sample", what);
        };
        for (int i = 0; i < kNumStylePresets; ++i) check(kStylePresets[i], kStylePresets[i].name);
        const int renders = std::getenv("OMNICHOIR_THOROUGH") ? 60 : 8;   // each render takes a few seconds
        std::uint32_t st = 2024; for (int i = 0; i < renders; ++i) check(randomStyle(st), "random style");
    }
    std::printf("ALL STYLE TESTS PASSED\n");
}
