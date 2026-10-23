#pragma once
// Standalone C++17 PCM16 WAV stem mixer. No JUCE dependency and no inference.
// Offline only. Used by the JUCE background worker and compiled independently in tests.
#include <functional>
#include <string>
#include <vector>

namespace choir::stems {
struct Voice {
    std::string file;
    std::string section;
    double x=0, y=0, offsetMs=0, gainDb=-6;
};
struct Manifest {
    double micX=0, micY=0, room=0.2, outputDb=-3;
    std::vector<Voice> voices;
};
struct Result {
    int sampleRate=0;
    int voiceCount=0;
    double seconds=0, peak=0, safetyGain=1;
};
// Two-pass disk streaming: bounded memory even for long takes. Every stem must be
// aligned at t=0 and PCM16 mono/stereo, with a matching sample rate.
// Cancel callback can return false; incomplete output is removed on failure.
bool render(const Manifest&, const std::string& wavOutput, Result& result,
            std::string& error, const std::function<bool(float)>& progress={});
}
