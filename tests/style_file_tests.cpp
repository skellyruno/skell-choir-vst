// Your own styles: the file format round-trips, and a file can only ever set the parameters on the allow-list.
#include "test_support.h"
#include "../Source/StyleFile.h"
#include "../Source/StylePresets.h"

using namespace ts;
using namespace choir;

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const std::string src = readFile(root + "/Source/PluginProcessor.cpp");
    CHECK(!src.empty(), "cannot read PluginProcessor.cpp (pass the project root as the first argument)");

    // 1. the allow-list is made of real parameters, and never of anything the user owns ----------------------------------------------
    {
        const char* forbidden[] = { "leadGain", "leadX", "leadY", "master", "choirLevel", "levelMatch", "mode", "classicStage", "choirOn", "keyRoot", "scale",
                                    "sectionState0", "sectionState1", "sectionState2", "sectionState3" };
        for (int i = 0; i < kNumUserStyleIds; ++i)
        {
            // the per-section ids are declared in a loop ("x"+s), the others by name
            const std::string id = kUserStyleIds[i]; const bool perSection = id.size() > 1 && std::isdigit((unsigned char) id.back());
            const std::string stem = perSection ? id.substr(0, id.size() - 1) : id;
            CHECK(perSection ? src.find("addFloat(layout,\"" + stem + "\"+s,") != std::string::npos : declares(src, id), "style-file id '%s' is not a parameter of the plugin", kUserStyleIds[i]);
            for (const char* f : forbidden) CHECK(std::string(kUserStyleIds[i]) != f, "'%s' must never be saved in or loaded from a style file", f);
        }
        for (int i = 0; i < kNumUserStyleIds; ++i) for (int j = i + 1; j < kNumUserStyleIds; ++j) CHECK(std::string(kUserStyleIds[i]) != kUserStyleIds[j], "duplicate id %s", kUserStyleIds[i]);
        // a style file can capture every parameter a factory style or room sets, so "save what I just loaded" loses nothing
        const char* setByStyles[] = { "harmony", "mix", "choirSize", "ensemble", "humanize", "formant", "voiceBlend", "choirVowel", "vowelFollow", "choirBreath", "vibrato",
                                      "micX", "micY", "width", "micMode", "roomSize", "roomSeconds", "absorption", "diffusion", "wet", "hall", "hallSeconds" };
        for (const char* id : setByStyles) CHECK(isUserStyleId(id), "'%s' is set by the factory styles but cannot be saved in a style file", id);
        for (int s = 0; s < 4; ++s) for (const char* p : { "x", "y", "gain" }) CHECK(isUserStyleId(std::string(p) + std::to_string(s)), "section %d '%s' cannot be saved", s, p);
        std::printf("[1] the %d style-file ids are real parameters, none of them is yours (lead, output, auto level, mode, key, solo/mute)\n", kNumUserStyleIds);
    }

    // 2. round trip ------------------------------------------------------------------------------------------------------------------------
    {
        std::vector<std::pair<std::string,float>> v;
        for (int i = 0; i < kNumUserStyleIds; ++i) v.emplace_back(kUserStyleIds[i], .013f * (float) (i + 1) - .27f);
        const auto text = writeStyleFile("My Gospel 2", v); StyleFileData back;
        CHECK(parseStyleFile(text, back) && back.name == "My Gospel 2" && back.values.size() == v.size(), "round trip lost data (%zu of %zu values)", back.values.size(), v.size());
        for (size_t i = 0; i < v.size(); ++i) CHECK(back.values[i].first == v[i].first && std::fabs(back.values[i].second - v[i].second) < 1e-5f, "value %s changed", v[i].first.c_str());
        // every factory style, written and read back, is unchanged
        for (int s = 0; s < kNumStylePresets; ++s)
        {
            FXParams p = pluginDefaults(); float size = .5f; applyStyle(kStylePresets[s], p, size);
            std::vector<std::pair<std::string,float>> vals { {"mix", p.mix}, {"ensemble", p.ensemble}, {"humanize", p.humanize}, {"wet", p.wet}, {"roomSeconds", p.roomSeconds}, {"choirSize", size} };
            StyleFileData d; CHECK(parseStyleFile(writeStyleFile(kStylePresets[s].name, vals), d) && d.values.size() == vals.size() && d.name == kStylePresets[s].name, "factory style %s did not survive", kStylePresets[s].name);
        }
        std::printf("[2] all %d ids round-trip, and so do the factory styles\n", kNumUserStyleIds);
    }

    // 3. a hostile or damaged file cannot set anything else, and cannot crash it --------------------------------------------------------------
    {
        StyleFileData d;
        CHECK(!parseStyleFile("", d) && !parseStyleFile("hello", d) && !parseStyleFile("OmniChoirStyle 2\nmix=.5\n", d) && !parseStyleFile("mix=.5\nOmniChoirStyle 1\n", d), "not a style file");
        const std::string nasty = "OmniChoirStyle 1\r\nname=Evil\x01\x02 name that goes on and on and on and on and on and on and on and on and on and on and on and on\r\n"
                                  "master=9\nleadGain=9\nmode=2\nlevelMatch=0\nsectionState0=2\nkeyRoot=5\nscale=3\nchoirOn=0\n"
                                  "mix=0.7\nmix=0.1\nensemble=nan\nhumanize=inf\nwet=abc\nroomSize=\n=5\nroomSeconds=3.5 \nx0=-2\n\n\n";
        CHECK(parseStyleFile(nasty, d), "the nasty file should still be readable");
        for (const auto& v : d.values) CHECK(isUserStyleId(v.first), "a forbidden id '%s' got through", v.first.c_str());
        CHECK(d.name.size() <= 60 && d.name.find('\x01') == std::string::npos, "the name was not cleaned (%zu characters)", d.name.size());
        bool mixOk = false, secondsOk = false, xOk = false; for (const auto& v : d.values) { if (v.first == "mix") mixOk = std::fabs(v.second - .7f) < 1e-6f; if (v.first == "roomSeconds") secondsOk = std::fabs(v.second - 3.5f) < 1e-6f; if (v.first == "x0") xOk = std::fabs(v.second + 2.f) < 1e-6f; }
        CHECK(mixOk && secondsOk && xOk && d.values.size() == 3, "only mix (first value wins), roomSeconds and x0 should survive; got %zu values", d.values.size());
        std::string big = "OmniChoirStyle 1\n"; for (int i = 0; i < 20000; ++i) big += "mix=0.5\nname=" + std::string(500, 'a') + "\n";
        CHECK(parseStyleFile(big, d) && d.values.size() == 1, "a huge file must be handled");
        std::printf("[3] forbidden, repeated, non-finite and malformed lines are dropped; the name is cleaned; huge files are fine\n");
    }
    std::printf("ALL STYLE FILE TESTS PASSED\n");
}
