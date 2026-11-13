#pragma once
// v0.8: your own styles. A style file (".omnistyle") is plain text: the sound you made (where the singers stand, the room, how they sing),
// never your lead level, OUTPUT, AUTO LEVEL, CHOIR LEVEL, PROCESS, solo/mute, key or scale. Only the parameters on the allow-list below are
// ever read from a file, so a file you download cannot touch anything else. JUCE-free; tested in tests/style_file_tests.cpp.
#include "Scenes.h"
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace choir {

inline constexpr const char* kUserStyleIds[] = {
    "x0", "y0", "gain0", "x1", "y1", "gain1", "x2", "y2", "gain2", "x3", "y3", "gain3",              // where the four sections stand
    "micX", "micY", "width", "micMode",                                                                // the microphones
    "roomSize", "roomSeconds", "absorption", "diffusion", "wet", "hall", "hallSeconds",                // the room
    "harmony", "mix", "choirSize", "ensemble", "humanize", "formant",                                  // how the choir is built
    "voiceBlend", "choirVowel", "vowelFollow", "choirBreath", "vibrato",                               // how the singers sing
    "consonants", "swell", "pulseRate", "pulseDepth",                                                  // how it follows you
};
inline constexpr int kNumUserStyleIds = (int) (sizeof(kUserStyleIds) / sizeof(kUserStyleIds[0]));

inline bool isUserStyleId(const std::string& id)
{
    for (const char* known : kUserStyleIds) if (id == known) return true;
    return false;
}

struct StyleFileData
{
    std::string name;
    std::vector<std::pair<std::string,float>> values;
    int scene = -1;   // the stage picture (Scenes.h), or -1 for "leave it as it is"
};

// "OmniChoirStyle 1", then "name=...", then one "id=value" per line
inline std::string writeStyleFile(const std::string& name, const std::vector<std::pair<std::string,float>>& values, int scene = -1)
{
    std::string out = "OmniChoirStyle 1\nname=";
    for (char c : name) if (c != '\n' && c != '\r') out += c;
    out += "\n";
    if (scene >= 0 && scene < scenes::kNumScenes) out += "scene=" + std::to_string(scene) + "\n";   
    for (const auto& v : values)
    {
        if (!isUserStyleId(v.first) || !std::isfinite(v.second)) continue;
        char text[64]; std::snprintf(text, sizeof text, "%.6g", (double) v.second);
        out += v.first + "=" + text + "\n";
    }
    return out;
}

// Returns false for anything that is not a style file. Unknown or forbidden ids, repeated ids and non-finite values are dropped;
// the name is cleaned and limited to 60 characters.
inline bool parseStyleFile(const std::string& text, StyleFileData& out)
{
    out = StyleFileData {};
    size_t pos = 0; bool first = true;
    while (pos <= text.size())
    {
        size_t end = text.find('\n', pos); if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos); pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (first) { if (line != "OmniChoirStyle 1") return false; first = false; continue; }
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "name")
        {
            for (char c : value) if ((unsigned char) c >= 32 && (unsigned char) c != 127 && out.name.size() < 60) out.name.push_back(c);
            continue;
        }
        if (key == "scene")
        {
            char* end = nullptr; const long n = std::strtol(value.c_str(), &end, 10);
            if (out.scene < 0 && end != value.c_str() && *end == 0 && n >= 0 && n < scenes::kNumScenes) out.scene = (int) n;   // junk or out of range is ignored; the first one wins
            continue;
        }
        if (!isUserStyleId(key)) continue;
        bool seen = false; for (const auto& v : out.values) if (v.first == key) seen = true;
        if (seen) continue;
        char* stop = nullptr; const double number = std::strtod(value.c_str(), &stop);
        if (stop == value.c_str() || !std::isfinite(number)) continue;
        out.values.emplace_back(key, (float) number);
    }
    return !first;
}

} // namespace choir
