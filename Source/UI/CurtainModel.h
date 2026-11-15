#pragma once
// The opening curtain: the timeline and the geometry, JUCE-free so they can be tested natively.
// 0 .. kHoldMs        closed, the title fades in over the first kFadeInMs
// kHoldMs .. kTotalMs the two halves part (smooth start and stop); the title fades out as they open
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace curtain {

constexpr float kFadeInMs = 450.f;
constexpr float kHoldMs = 1600.f;      // when the curtain starts to open
constexpr float kOpenMs = 1800.f;      // how long it takes
constexpr float kTotalMs = kHoldMs + kOpenMs;

struct State { float open; float title; bool done; };   // open 0 = closed, 1 = fully open; title 0..1 = the lettering's opacity

inline float smooth(float x) { x = std::min(1.f, std::max(0.f, x)); return x * x * (3.f - 2.f * x); }

inline State at(float ms)
{
    const float open = smooth((ms - kHoldMs) / kOpenMs);
    const float in = smooth(ms / kFadeInMs);
    const float out = 1.f - smooth(open * 2.5f);          // gone by the time the curtain is 40% open
    return { open, in * out, ms >= kTotalMs };
}

// A click while the curtain is still closed starts the opening at once; during the opening it changes nothing.
inline float skip(float ms) { return std::max(ms, kHoldMs); }

// Each half is this wide (in pixels) when the whole curtain is totalWidth wide. The folds compress as it parts, like a gathered curtain.
inline float halfWidth(float totalWidth, float open) { return totalWidth * .5f * (1.f - open); }

// ---- the user's choice: when the curtain plays (saved per user in curtain.txt)
enum class Mode { Always = 0, OncePerSession = 1, Never = 2 };
constexpr int kNumModes = 3;
inline const char* modeName(Mode m) { return m == Mode::Always ? "Every time" : m == Mode::OncePerSession ? "Once per session" : "Never"; }
inline const char* toFileText(Mode m) { return m == Mode::Always ? "always" : m == Mode::OncePerSession ? "once" : "never"; }
// anything unknown, empty or damaged gives the default (Always)
inline Mode fromFileText(const std::string& text)
{
    std::string t; for (char c : text) if (c > ' ' && c < 127) t += (char) std::tolower((unsigned char) c);
    return t == "once" ? Mode::OncePerSession : t == "never" ? Mode::Never : Mode::Always;
}
// "session" = the host process: alreadyPlayedThisSession is a flag that lives as long as the plugin stays loaded
inline bool shouldPlay(Mode m, bool alreadyPlayedThisSession)
{
    return m == Mode::Always || (m == Mode::OncePerSession && !alreadyPlayedThisSession);
}

} // namespace curtain
