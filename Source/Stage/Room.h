#pragma once
#include "Common.h"

namespace oc {

struct RoomSettings
{
    double size = 0.5, absorption = 0.4, diffusion = 0.7, seconds = 2.0, earlyLevel = 0.7, lateLevel = 0.6;
    bool operator==(const RoomSettings& o) const
    {
        return size == o.size && absorption == o.absorption && diffusion == o.diffusion
            && seconds == o.seconds && earlyLevel == o.earlyLevel && lateLevel == o.lateLevel;
    }
};

struct RoomGeometry
{
    double width, length, height, xMin, xMax, yMin, yMax, zMax, reflection, rt60, meanFreePathMs;
};

inline RoomGeometry roomGeometry(const RoomSettings& s, double choirMaxY, double choirMaxAbsX)
{
    RoomGeometry g {};
    double size = clampv(s.size, 0.0, 1.0);
    g.width = lerp(8, 40, size); g.length = lerp(10, 60, size); g.height = lerp(3.5, 22, size);
    // the choir always stands inside the room: walls move out if a formation is larger than the nominal room
    g.xMax = std::max(g.width * 0.5, choirMaxAbsX + 1.5); g.xMin = -g.xMax;
    g.yMax = std::max(0.4 * g.length, choirMaxY + 1.5);
    g.yMin = -0.6 * g.length;
    g.zMax = g.height;
    g.width = g.xMax - g.xMin; g.length = g.yMax - g.yMin;
    double alpha = clampv(0.08 + 0.8 * s.absorption, 0.0, 0.95);
    g.reflection = std::sqrt(1 - alpha);
    g.rt60 = clampv(s.seconds, 0.2, 10.0);
    double volume = g.width * g.length * g.height;
    double surface = 2 * (g.width * g.length + g.width * g.height + g.length * g.height);
    g.meanFreePathMs = 1000 * (4 * volume / surface) / kSpeedOfSound;
    return g;
}

// the late field: an 8-line feedback delay network excited by the early reflections
class LateReverb
{
public:
    explicit LateReverb(double sampleRate) : sr(sampleRate)
    {
        maxLen = (int) (0.25 * sr) + 16;
        for (auto& l : line) l.assign((size_t) maxLen, 0.f);
        const double apMs[4] = { 5.3, 7.9, 6.1, 8.9 };
        for (int i = 0; i < 4; ++i)
        {
            int n = std::max(8, (int) (apMs[i] * 0.001 * sr));
            ap[i].buf.assign((size_t) n, 0.f); ap[i].len = n; ap[i].pos = 0; ap[i].g = 0.5;
        }
        lfoInc = 0.37 / sr;
        RoomSettings d; d.size = 0.5; d.absorption = 0.4; d.diffusion = 0.7; d.seconds = 2.0;
        configure(d, roomGeometry(d, 8, 6));
        reset();
    }
    void reset()
    {
        for (auto& l : line) std::fill(l.begin(), l.end(), 0.f);
        for (auto& a : ap) { std::fill(a.buf.begin(), a.buf.end(), 0.f); a.pos = 0; }
        std::fill(std::begin(dampState), std::end(dampState), 0.0);
        std::fill(std::begin(writePos), std::end(writePos), 0);
        lfoPhase = 0;
    }
    void configure(const RoomSettings& s, const RoomGeometry& g)
    {
        static const double factors[8] = { 0.61, 0.73, 0.89, 1.03, 1.19, 1.37, 1.57, 1.81 };
        double rt60 = std::max(0.15, g.rt60);
        for (int i = 0; i < 8; ++i)
        {
            int n = (int) (g.meanFreePathMs * factors[i] * 0.001 * sr);
            n = clampv(n | 1, 31, maxLen - 8);
            lineLen[i] = n;
            feedback[i] = std::pow(10.0, -3.0 * n / sr / rt60);
        }
        double cutoff = lerp(12000, 1800, clampv(s.absorption, 0.0, 1.0));
        dampCoef = 1 - std::exp(-kTwoPi * std::min(cutoff, sr * 0.45) / sr);
        double gAp = 0.25 + 0.5 * clampv(s.diffusion, 0.0, 1.0);
        for (auto& a : ap) a.g = gAp;
    }

    void process(const float* inL, const float* inR, float* mainL, float* mainR, float* roomL, float* roomR, int n)
    {
        double o[8], m[8];
        for (int s = 0; s < n; ++s)
        {
            double xl = allpass(ap[1], allpass(ap[0], inL[s]));
            double xr = allpass(ap[3], allpass(ap[2], inR[s]));
            lfoPhase += lfoInc; if (lfoPhase >= 1) lfoPhase -= 1;
            double mod = 2 * std::sin(kTwoPi * lfoPhase);
            for (int i = 0; i < 8; ++i)
            {
                double delay = lineLen[i];
                if (i == 0) delay = delay + mod + 3; else if (i == 4) delay = delay - (mod - 3);
                int whole = (int) delay; double frac = delay - whole;
                int i0 = writePos[i] - whole; if (i0 < 0) i0 += maxLen;
                int i1 = i0 - 1; if (i1 < 0) i1 += maxLen;       // one sample older
                const auto& li = line[i];
                double v = li[(size_t) i0] + frac * (li[(size_t) i1] - li[(size_t) i0]);
                dampState[i] += dampCoef * (v - dampState[i]);
                o[i] = dampState[i];
            }
            for (int i = 0; i < 8; ++i) m[i] = o[i];
            for (int h = 1; h < 8; h <<= 1)
                for (int i = 0; i < 8; i += h << 1)
                    for (int j = i; j < i + h; ++j) { double a = m[j], b = m[j + h]; m[j] = a + b; m[j + h] = a - b; }
            const double norm = 0.35355339;
            for (int i = 0; i < 8; ++i)
            {
                double inp = ((i & 1) ? xr : xl) * ((i & 2) ? -0.5 : 0.5);
                line[i][(size_t) writePos[i]] = (float) (inp + m[i] * norm * feedback[i]);
                if (++writePos[i] >= maxLen) writePos[i] = 0;
            }
            mainL[s] = (float) (0.5 * (o[0] - o[2] + o[4] - o[6]));
            mainR[s] = (float) (0.5 * (o[1] - o[3] + o[5] - o[7]));
            roomL[s] = (float) (0.5 * (o[0] + o[3] - o[5] + o[6]));
            roomR[s] = (float) (0.5 * (o[1] + o[2] - o[4] + o[7]));
        }
        for (auto& d : dampState) d = flushd(d);
    }

private:
    struct AP { std::vector<float> buf; int pos = 0, len = 8; double g = 0.5; };
    static double allpass(AP& a, double x)
    {
        double d = a.buf[(size_t) a.pos];
        double y = d - a.g * x;
        a.buf[(size_t) a.pos] = (float) (x + a.g * y);
        if (++a.pos >= a.len) a.pos = 0;
        return y;
    }
    double sr; int maxLen = 0;
    std::vector<float> line[8];
    int lineLen[8] {}, writePos[8] {};
    double feedback[8] {}, dampState[8] {}, dampCoef = 0.5;
    AP ap[4];
    double lfoPhase = 0, lfoInc = 0;
};

} // namespace oc
