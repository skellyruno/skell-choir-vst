#pragma once
// Shared maths for the choir stage engine (port of choir-stage.js). No JUCE dependency.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace oc {

constexpr double kPi = 3.14159265358979323846, kTwoPi = 2.0 * kPi;
constexpr double kSpeedOfSound = 343.0;

template <class T> inline T clampv(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline double smoothstep(double x) { x = clampv(x, 0.0, 1.0); return x * x * (3 - 2 * x); }
inline double flushd(double x) { return std::fabs(x) < 1e-20 ? 0.0 : x; }
inline int lroundJs(double x) { return x >= 0 ? (int) std::floor(x + 0.5) : -(int) std::floor(-x + 0.5); }

struct V3 { double x = 0, y = 0, z = 0; };
inline V3 v3(double x = 0, double y = 0, double z = 0) { return { x, y, z }; }
inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*(V3 a, double s) { return { a.x * s, a.y * s, a.z * s }; }
inline double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double len(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 normalised(V3 a) { double l = len(a); return l > 1e-6 ? V3{ a.x / l, a.y / l, a.z / l } : V3{ 0, -1, 0 }; }

// constant-power lean: -1 all left .. 1 all right, 0 = both 1
inline void leanGains(double lean, double& l, double& r)
{
    l = 1.41421356 * std::cos((lean + 1) * kPi * 0.25);
    r = 1.41421356 * std::sin((lean + 1) * kPi * 0.25);
}

class Biquad
{
public:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass(double hz, double q, double sr)
    {
        double w = kTwoPi * clampv(hz, 5.0, sr * 0.49) / sr, c = std::cos(w), s = std::sin(w), alpha = s / (2 * q), a0 = 1 + alpha;
        b0 = (1 - c) * 0.5 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
    }
    void setHighpass(double hz, double q, double sr)
    {
        double w = kTwoPi * clampv(hz, 5.0, sr * 0.49) / sr, c = std::cos(w), s = std::sin(w), alpha = s / (2 * q), a0 = 1 + alpha;
        b0 = (1 + c) * 0.5 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
    }
    void setBandpass(double hz, double q, double sr)   // constant 0 dB peak gain
    {
        double w = kTwoPi * clampv(hz, 5.0, sr * 0.49) / sr, c = std::cos(w), s = std::sin(w), alpha = s / (2 * q), a0 = 1 + alpha;
        b0 = alpha / a0; b1 = 0; b2 = -alpha / a0; a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
    }
    void setHighShelf(double hz, double gainDb, double sr)
    {
        double A = std::pow(10.0, gainDb / 40), w = kTwoPi * clampv(hz, 5.0, sr * 0.49) / sr;
        double c = std::cos(w), s = std::sin(w), alpha = s * 0.5 * std::sqrt(2.0), sq = 2 * std::sqrt(A) * alpha;
        double a0 = (A + 1) - (A - 1) * c + sq;
        b0 = A * ((A + 1) + (A - 1) * c + sq) / a0; b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * c) / a0; a2 = ((A + 1) - (A - 1) * c - sq) / a0;
    }
    inline double process(double x)
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void flush() { z1 = flushd(z1); z2 = flushd(z2); }
};

// main output: untouched below 0.95 of full scale, then a soft knee that never exceeds full scale
inline double safetyCeiling(double x)
{
    double a = std::fabs(x);
    if (a <= 0.95) return x;
    double y = 0.95 + 0.05 * std::tanh((a - 0.95) / 0.05);
    return x < 0 ? -y : y;
}
// the choir on its own: untouched below 0.8, then a soft knee
inline double softLimit(double x)
{
    double a = std::fabs(x);
    if (a <= 0.8) return x;
    double y = 0.8 + 0.2 * std::tanh((a - 0.8) / 0.2);
    return x < 0 ? -y : y;
}

} // namespace oc
