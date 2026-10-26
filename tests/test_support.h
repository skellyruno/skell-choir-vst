#pragma once
// Shared helpers for the goal-based tests (balance_tests, real_vocal_tests). Header-only, JUCE-free.
#include "../Source/StagedChoir.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n      ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); std::exit(1); } } while (0)

namespace ts {

inline double dB(double amplitude) { return 20.0 * std::log10(std::max(amplitude, 1e-12)); }

// FXParams exactly as the plugin starts (the defaults of the APVTS layout in Source/PluginProcessor.cpp).
// balance_tests.cpp parses PluginProcessor.cpp and fails if these drift, so the tests always measure what a user hears.
inline choir::FXParams pluginDefaults()
{
    choir::FXParams p;
    p.gain = { .83f, .83f, .83f, .83f };
    p.ensemble = .58f; p.humanize = .75f; p.formant = .5f; p.sensitivity = .65f;
    p.roomSize = .52f; p.roomSeconds = 2.4f; p.absorption = .4f; p.diffusion = .7f; p.wet = .37f;
    p.width = .6f; p.micMode = 1;
    p.leadX = 0.f; p.leadY = .4f; p.leadGain = 1.f;
    p.mix = .78f; p.master = 1.2f; p.choirLevelDb = 0.f;
    return p;
}

struct Audio { int sr = 44100; std::vector<float> x; };

// 16-bit PCM, mono or stereo (first channel is used)
inline bool readWav16(const std::string& path, Audio& a)
{
    std::ifstream s(path, std::ios::binary); if (!s) return false;
    char id[4]; uint32_t sz; s.read(id, 4); s.read((char*) &sz, 4); s.read(id, 4);
    uint16_t ch = 1, bits = 16; uint32_t sr = 44100;
    while (s.read(id, 4))
    {
        s.read((char*) &sz, 4);
        if (!std::memcmp(id, "fmt ", 4)) { std::vector<char> b(sz); s.read(b.data(), sz); std::memcpy(&ch, &b[2], 2); std::memcpy(&sr, &b[4], 4); std::memcpy(&bits, &b[14], 2); }
        else if (!std::memcmp(id, "data", 4))
        {
            if (bits != 16) return false;
            std::vector<char> b(sz); s.read(b.data(), sz);
            const size_t n = sz / 2 / ch; a.sr = (int) sr; a.x.resize(n);
            for (size_t i = 0; i < n; ++i) { int16_t q; std::memcpy(&q, &b[i * ch * 2], 2); a.x[i] = q / 32768.f; }
            return true;
        }
        else s.seekg(sz + (sz & 1), std::ios::cur);
    }
    return false;
}

// samples where the input is "being sung": 100 ms RMS above the threshold. The first second is skipped (stage fill, smoothing)
inline std::vector<char> voicedMask(const std::vector<float>& x, int sr, double thresh = .01)
{
    std::vector<char> m(x.size(), 0); const int blk = sr / 10;
    for (size_t b = 0; b + blk <= x.size(); b += blk)
    {
        double e = 0; for (int i = 0; i < blk; ++i) e += (double) x[b + i] * x[b + i];
        if (std::sqrt(e / blk) > thresh) for (int i = 0; i < blk; ++i) m[b + i] = 1;
    }
    for (size_t i = 0; i < std::min(m.size(), (size_t) sr); ++i) m[i] = 0;
    return m;
}

struct Rendered { std::vector<float> l, r; };
inline Rendered render(const std::vector<float>& x, int sr, const choir::FXParams& p)
{
    choir::StagedChoir sc; sc.prepare(sr); sc.setParameters(p);
    Rendered o; o.l.assign(x.size(), 0.f); o.r.assign(x.size(), 0.f);
    sc.process(x.data(), x.data(), o.l.data(), o.r.data(), (int) x.size());
    return o;
}
// stereo RMS over the masked samples (the mean of the two channels' power), and the largest absolute sample
inline double rms(const Rendered& o, const std::vector<char>& m)
{
    double s = 0; size_t n = 0;
    for (size_t i = 0; i < o.l.size(); ++i) if (m[i]) { s += .5 * ((double) o.l[i] * o.l[i] + (double) o.r[i] * o.r[i]); ++n; }
    return std::sqrt(s / std::max<size_t>(n, 1));
}
inline double rms(const std::vector<float>& x, const std::vector<char>& m)
{
    double s = 0; size_t n = 0;
    for (size_t i = 0; i < x.size(); ++i) if (m[i]) { s += (double) x[i] * x[i]; ++n; }
    return std::sqrt(s / std::max<size_t>(n, 1));
}
inline double peak(const Rendered& o) { double p = 0; for (size_t i = 0; i < o.l.size(); ++i) p = std::max(p, (double) std::max(std::fabs(o.l[i]), std::fabs(o.r[i]))); return p; }

// The three numbers the user hears, at the given parameters:
//   choir  = what the choir alone contributes (lead muted)
//   lead   = what the dry lead contributes at this MIX (full-lead render x (1 - mix))
//   total  = the finished output
struct Balance { double choir, lead, total, input; };
inline Balance balance(const std::vector<float>& x, int sr, const choir::FXParams& p)
{
    const auto m = voicedMask(x, sr);
    choir::FXParams c = p; c.leadGain = 0.f;
    choir::FXParams l = p; l.mix = 0.f;
    Balance b;
    b.choir = rms(render(x, sr, c), m);
    b.lead = (1.0 - p.mix) * rms(render(x, sr, l), m);
    b.total = rms(render(x, sr, p), m);
    b.input = rms(x, m);
    return b;
}

// A synthetic sung phrase, for the tests that need a controllable signal. Vowel-like (two formants), vibrato, gated phrases.
inline std::vector<float> sungPhrase(int sr, double seconds, double rmsTarget, double f0)
{
    const double PI = 3.14159265358979323846;
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0;
    auto res = [&](double in, double f, double bw, double& y1, double& y2) { const double r = std::exp(-PI * bw / sr), th = 2 * PI * f / sr; const double y = in + 2 * r * std::cos(th) * y1 - r * r * y2; y2 = y1; y1 = y; return y * (1 - r); };
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = i / (double) sr, gate = std::fmod(t, 2.0) < 1.6 ? 1.0 : 0.0;
        const double f = f0 * (std::fmod(t, 4.0) < 2 ? 1.0 : 1.122) * (1 + .012 * std::sin(2 * PI * 5.5 * t));
        ph += f / sr; ph -= std::floor(ph);
        const double src = (ph < .4 ? std::sin(PI * ph / .4) : 0.0) - .3;
        x[i] = (float) (gate * (res(src, 700, 90, a1, a2) + .5 * res(src, 1200, 110, b1, b2)));
    }
    double e = 0; size_t n = 0; for (float v : x) if (v != 0.f) { e += (double) v * v; ++n; }
    const double k = rmsTarget / std::sqrt(e / std::max<size_t>(n, 1));
    for (auto& v : x) v = (float) (v * k);
    return x;
}

} // namespace ts
