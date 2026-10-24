// Measures the choir-only level of the v0.5 simplified stage and of the v0.6 faithful stage on the same sung test
// signal, and prints the trim that makes the new path match the old one. Used to set StagedChoir::kVoiceTrim.
#include "../Source/StagedChoir.h"
#include <cmath>
#include <cstdio>
#include <vector>

static const double kPiD = 3.14159265358979323846;
static std::vector<float> sung(double sr, double seconds)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        double t = i / sr, gate = std::fmod(t, 1.5) < 1.2 ? 1.0 : 0.0;
        double f = (std::fmod(t, 3.0) < 1.5 ? 220.0 : 261.63) * (1 + 0.005 * std::sin(2 * kPiD * 5.3 * t));
        ph += 2 * kPiD * f / sr; double s = 0; for (int h = 1; h <= 8; ++h) s += std::sin(h * ph) / h;
        x[i] = (float) (0.22 * gate * s);
    }
    return x;
}

int main(int argc, char** argv)
{
    const double sr = 48000; auto x = sung(sr, 9);
    choir::FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = 0.37f; p.gain = { .83f, .83f, .83f, .83f }; p.roomSize = .52f; p.leadGain = 1.f;
    const size_t from = (size_t) (sr * 3);
    double oldSq = 0, newSq = 0; size_t cnt = 0;
    {   choir::ChoirEffect fx; fx.prepare(sr); fx.setParameters(p);
        for (size_t i = 0; i < x.size(); ++i) { float l, r; fx.render(x[i], x[i], l, r); if (i >= from) { oldSq += 0.5 * ((double) l * l + (double) r * r); ++cnt; } } }
    {   choir::StagedChoir sc; sc.prepare(sr); sc.setParameters(p);
        std::vector<float> l(x.size()), r(x.size()); sc.process(x.data(), x.data(), l.data(), r.data(), (int) x.size());
        for (size_t i = from; i < x.size(); ++i) newSq += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]); }
    double oldRms = std::sqrt(oldSq / cnt), newRms = std::sqrt(newSq / cnt);
    std::printf("choir-only RMS  v0.5 %.5f | v0.6 (trim %.3f) %.5f | trim to match: %.4f (%.1f dB)\n", oldRms, (double) choir::StagedChoir::kVoiceTrim, newRms,
                oldRms / newRms * choir::StagedChoir::kVoiceTrim, 20 * std::log10(oldRms / newRms));
    (void) argc; (void) argv;
}
