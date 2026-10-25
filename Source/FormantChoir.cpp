#include "FormantChoir.h"
#include <algorithm>
#include <cmath>

namespace choir {
namespace {

using oc::clampv;
constexpr double kPiD = 3.14159265358979323846;

struct Formant { double hz, bw, db; };
// Five formants per vowel (A E I O U), for soprano, alto, tenor, bass (classic singing-formant tables).
constexpr Formant kTable[4][kVowels][5] = {
    // soprano
    { { { 800, 80, 0 }, { 1150, 90, -6 }, { 2900, 120, -32 }, { 3900, 130, -20 }, { 4950, 140, -50 } },
      { { 350, 60, 0 }, { 2000, 100, -20 }, { 2800, 120, -15 }, { 3600, 150, -40 }, { 4950, 200, -56 } },
      { { 270, 60, 0 }, { 2140, 90, -12 }, { 2950, 100, -26 }, { 3900, 120, -26 }, { 4950, 120, -44 } },
      { { 450, 70, 0 }, { 800, 80, -11 }, { 2830, 100, -22 }, { 3800, 130, -22 }, { 4950, 135, -50 } },
      { { 325, 50, 0 }, { 700, 60, -16 }, { 2700, 170, -35 }, { 3800, 180, -40 }, { 4950, 200, -60 } } },
    // alto
    { { { 800, 80, 0 }, { 1150, 90, -4 }, { 2800, 120, -20 }, { 3500, 130, -36 }, { 4950, 140, -60 } },
      { { 400, 60, 0 }, { 1600, 80, -24 }, { 2700, 120, -30 }, { 3300, 150, -35 }, { 4950, 200, -60 } },
      { { 350, 50, 0 }, { 1700, 100, -20 }, { 2700, 120, -30 }, { 3700, 150, -36 }, { 4950, 200, -60 } },
      { { 450, 70, 0 }, { 800, 80, -9 }, { 2830, 100, -16 }, { 3500, 130, -28 }, { 4950, 135, -55 } },
      { { 325, 50, 0 }, { 700, 60, -12 }, { 2530, 170, -30 }, { 3500, 180, -40 }, { 4950, 200, -64 } } },
    // tenor
    { { { 650, 80, 0 }, { 1080, 90, -6 }, { 2650, 120, -7 }, { 2900, 130, -8 }, { 3250, 140, -22 } },
      { { 400, 70, 0 }, { 1700, 80, -14 }, { 2600, 100, -12 }, { 3200, 120, -14 }, { 3580, 120, -20 } },
      { { 290, 40, 0 }, { 1870, 90, -15 }, { 2800, 100, -18 }, { 3250, 120, -20 }, { 3540, 120, -30 } },
      { { 400, 40, 0 }, { 800, 80, -10 }, { 2600, 100, -12 }, { 2800, 120, -12 }, { 3000, 120, -26 } },
      { { 350, 40, 0 }, { 600, 60, -20 }, { 2700, 100, -17 }, { 2900, 120, -14 }, { 3300, 120, -26 } } },
    // bass
    { { { 600, 60, 0 }, { 1040, 70, -7 }, { 2250, 110, -9 }, { 2450, 120, -9 }, { 2750, 130, -20 } },
      { { 400, 40, 0 }, { 1620, 80, -12 }, { 2400, 100, -9 }, { 2800, 120, -12 }, { 3100, 120, -18 } },
      { { 250, 60, 0 }, { 1750, 90, -30 }, { 2600, 100, -16 }, { 3050, 120, -22 }, { 3340, 120, -28 } },
      { { 400, 40, 0 }, { 750, 80, -11 }, { 2400, 100, -21 }, { 2600, 120, -20 }, { 2900, 120, -40 } },
      { { 350, 40, 0 }, { 600, 80, -20 }, { 2400, 100, -32 }, { 2675, 120, -28 }, { 2950, 120, -36 } } } };

// reference first/second formants of each vowel, averaged over voice types (used to classify the lead)
constexpr double kRefF1[kVowels] = { 760, 460, 300, 470, 330 };
constexpr double kRefF2[kVowels] = { 1250, 1850, 2250, 900, 780 };

double polyBlep(double t, double dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

} // namespace

// ------------------------------------------------------------------------------------------- VowelEstimator
void VowelEstimator::prepare(double sampleRate)
{
    decimate = std::max(1, (int) std::lround(sampleRate / 12000.0));
    rate = sampleRate / decimate;
    reset();
}
void VowelEstimator::reset()
{
    ring.fill(0.f); frame.fill(0.f); ringWrite = phaseCount = filled = sinceAnalysis = 0;
    accumulator = previous = 0; formant1 = formant2 = 0; haveEstimate = false; w = { 1.f, 0.f, 0.f, 0.f, 0.f };
}
void VowelEstimator::push(float sample)
{
    accumulator += sample;
    if (++phaseCount < decimate) return;
    const double x = accumulator / decimate; accumulator = 0; phaseCount = 0;
    const double y = x - .95 * previous; previous = x;      // pre-emphasis: flattens the spectral tilt before LPC
    ring[(size_t) ringWrite] = (float) y;
    ringWrite = (ringWrite + 1) % (int) ring.size();
    filled = std::min(filled + 1, (int) ring.size());
    if (++sinceAnalysis >= 256 && filled == (int) ring.size()) { sinceAnalysis = 0; analyse(); }
}
void VowelEstimator::analyse()
{
    const int n = (int) ring.size(), order = 12;
    double energy = 0;
    for (int i = 0; i < n; ++i)
    {
        const double win = .5 - .5 * std::cos(2 * kPiD * i / (n - 1));
        frame[(size_t) i] = (float) (ring[(size_t) ((ringWrite + i) % n)] * win);
        energy += (double) frame[(size_t) i] * frame[(size_t) i];
    }
    if (energy / n < 2e-6) return;                          // too quiet to say anything: keep the last estimate
    double r[order + 1];
    for (int k = 0; k <= order; ++k) { double s = 0; for (int i = k; i < n; ++i) s += (double) frame[(size_t) i] * frame[(size_t) (i - k)]; r[k] = s; }
    r[0] *= 1.0001; r[0] += 1e-9;
    double a[order + 1] = { 1 }, tmp[order + 1]; double err = r[0];
    for (int i = 1; i <= order; ++i)                        // Levinson-Durbin
    {
        double acc = r[i]; for (int j = 1; j < i; ++j) acc += a[j] * r[i - j];
        const double k = -acc / err;
        for (int j = 1; j < i; ++j) tmp[j] = a[j] + k * a[i - j];
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
        a[i] = k; err *= (1 - k * k); if (err <= 0) return;
    }
    // spectral envelope on a log grid, then its peaks
    constexpr int points = 112; double env[points], freq[points];
    for (int p = 0; p < points; ++p)
    {
        freq[p] = 200.0 * std::pow(3800.0 / 200.0, (double) p / (points - 1));
        const double omega = 2 * kPiD * freq[p] / rate; double re = 1, im = 0;
        for (int j = 1; j <= order; ++j) { re += a[j] * std::cos(j * omega); im -= a[j] * std::sin(j * omega); }
        env[p] = 1.0 / (re * re + im * im + 1e-12);
    }
    double f1 = 0, f2 = 0;
    for (int p = 1; p < points - 1 && f2 == 0; ++p)
        if (env[p] > env[p - 1] && env[p] >= env[p + 1])
        {
            const double f = freq[p];
            if (f1 == 0) { if (f <= 1000) f1 = f; }
            else if (f > f1 * 1.25 && f <= 3300) f2 = f;
        }
    if (f1 == 0 || f2 == 0) return;
    formant1 = (float) f1; formant2 = (float) f2; haveEstimate = true;
    std::array<float, kVowels> nw {}; double sum = 0;
    for (int v = 0; v < kVowels; ++v)
    {
        const double d1 = (std::log(f1) - std::log(kRefF1[v])) / .22, d2 = (std::log(f2) - std::log(kRefF2[v])) / .26;
        nw[(size_t) v] = (float) std::exp(-.5 * (d1 * d1 + d2 * d2)); sum += nw[(size_t) v];
    }
    if (sum < 1e-6) return;
    for (int v = 0; v < kVowels; ++v) w[(size_t) v] += .4f * (nw[(size_t) v] / (float) sum - w[(size_t) v]);
}

// --------------------------------------------------------------------------------------------- FormantChoir
void FormantChoir::prepare(double sampleRate)
{
    sr = sampleRate; vowelEstimator.prepare(sampleRate);
    for (int k = 0; k < numVoices; ++k)
    {
        Voice& v = voices[(size_t) k];
        v = Voice();
        v.seed = 0x9E3779B9u * (uint32_t) (k + 1) + 12345u;
        v.phase = rnd(v.seed); v.vibPhase = rnd(v.seed);
        v.vibRate = 4.6 + rnd(v.seed) * 1.5; v.vibDepth = 14 + rnd(v.seed) * 18; v.vibDelay = .15 + rnd(v.seed) * .2;
        v.attack = .04 + rnd(v.seed) * .09; v.release = .12 + rnd(v.seed) * .16; v.onsetDelay = rnd(v.seed) * .045;
        v.scoopCents = 30 + rnd(v.seed) * 50; v.scoopTime = .05 + rnd(v.seed) * .04;
        retune(v, k / perSection);
    }
    reset();
}
void FormantChoir::reset()
{
    vowelEstimator.reset();
    weights = weightsSmoothed = { 1.f, 0.f, 0.f, 0.f, 0.f };
    levelEnv = 0; sinceVoiced = 10; heldMidi = 60; wasGated = false; tick = 0;
    for (auto& v : voices) { v.env = 0; v.lp = 0; v.breathEnv = 0; v.noiseLp = 0; v.jitter = v.jitterTarget = 0; for (auto& r : v.res) { r.z1 = r.z2 = 0; } }
}

// the formants of one singer for the current vowel mixture
void FormantChoir::retune(Voice& v, int section)
{
    for (int f = 0; f < 5; ++f)
    {
        double lf = 0, lbw = 0, db = 0;
        for (int vw = 0; vw < kVowels; ++vw)
        {
            const double wt = weightsSmoothed[(size_t) vw]; const Formant& t = kTable[section][vw][f];
            lf += wt * std::log(t.hz); lbw += wt * std::log(t.bw); db += wt * t.db;
        }
        const double hz = std::exp(lf) * shift, bw = std::exp(lbw);
        v.formantFreq[f] = hz; v.formantAmp[f] = std::pow(10.0, db / 20.0);
        v.res[f].setBandpass(hz, std::max(1.0, hz / bw), sr);
        if (f < 3) v.whisper[f].setBandpass(hz, 2.5, sr);
    }
}

void FormantChoir::process(float in, const FormantControl& c, float* out)
{
    vowelEstimator.push(in);
    const double dt = 1.0 / sr;
    const bool gated = c.gate > .5f;
    // the lead's loudness: fast attack, slower release, so swells and diminuendos carry over to the choir
    const double a = std::fabs((double) in);
    levelEnv += (a > levelEnv ? .0015 : .0003) * (a - levelEnv);
    if (c.midi >= 0.f)
    {
        if (std::fabs(c.midi - heldMidi) > 1.0 && gated)           // a new note: every singer scoops into it afresh
            for (auto& v : voices) { v.scoopClock = 0; v.vibClock = 0; v.vibDelay = .12 + rnd(v.seed) * .15; v.scoopCents = 25 + rnd(v.seed) * 45; }
        heldMidi = c.midi;
    }
    if (gated)
    {
        if (!wasGated && sinceVoiced > .3)                          // a phrase begins: a breath, a fresh scoop, vibrato held back
            for (auto& v : voices) { v.breathEnv = .5 + .5 * rnd(v.seed); v.scoopClock = 0; v.vibClock = 0; }
        sinceVoiced = 0;
    }
    else sinceVoiced += dt;
    wasGated = gated;

    // vowel: manual position, or what the lead is singing, then smoothed
    {
        const double pos = clampv((double) c.vowel, 0.0, 1.0) * (kVowels - 1);
        const int i0 = std::min(kVowels - 2, (int) pos); const float fr = (float) (pos - i0);
        std::array<float, kVowels> manual {}; manual[(size_t) i0] = 1.f - fr; manual[(size_t) i0 + 1] = fr;
        const float follow = (vowelEstimator.valid() && gated) ? clampv(c.follow, 0.f, 1.f) : 0.f;
        for (int v = 0; v < kVowels; ++v) weights[(size_t) v] = manual[(size_t) v] + follow * (vowelEstimator.weights()[(size_t) v] - manual[(size_t) v]);
        for (int v = 0; v < kVowels; ++v) weightsSmoothed[(size_t) v] += .0008f * (weights[(size_t) v] - weightsSmoothed[(size_t) v]);
    }
    shift = clampv((double) c.formantShift, .6, 1.6);
    if (++tick >= 32)
    {
        tick = 0;
        float moved = (float) std::fabs(shift - tunedShift);
        for (int v = 0; v < kVowels; ++v) moved = std::max(moved, std::fabs(weightsSmoothed[(size_t) v] - tunedWeights[(size_t) v]));
        if (moved > 2e-4f)     // coefficients only need recomputing while the vowel is actually moving
        {
            for (int k = 0; k < numVoices; ++k) retune(voices[(size_t) k], k / perSection);
            tunedWeights = weightsSmoothed; tunedShift = shift;
        }
    }

    const double pitchSmooth = 1.0 - std::exp(-1.0 / (.012 * sr));
    const double baseLog = std::log(440.0) + (heldMidi - 69.0) * std::log(2.0) / 12.0;
    for (int k = 0; k < numVoices; ++k)
    {
        Voice& v = voices[(size_t) k];
        // --- pitch: the lead's note x this singer's voicing, + vibrato (ramps in after the note starts), scoop, jitter
        v.vibClock += dt;
        const double vibRamp = clampv(v.vibClock / v.vibDelay - .3, 0.0, 1.0);
        v.vibPhase += v.vibRate * dt; if (v.vibPhase >= 1) v.vibPhase -= 1;
        v.scoopClock += dt;
        const double scoop = v.scoopClock < v.scoopTime ? -v.scoopCents * std::pow(1 - v.scoopClock / v.scoopTime, 2.0) : 0.0;
        if ((((int) (v.vibClock * sr)) & 255) == 0) v.jitterTarget = (rnd(v.seed) * 2 - 1) * .8;
        v.jitter += .004 * (v.jitterTarget - v.jitter);
        const double hum = clampv((double) c.humanize, 0.0, 1.0);
        const double cents = hum * (v.vibDepth * clampv((double) c.vibrato, 0.0, 1.0) * std::sin(2 * kPiD * v.vibPhase) * vibRamp + scoop + v.jitter)
                           + (1 - hum) * 0.0;
        const double ratio = c.ratios ? std::max(.05f, c.ratios[k]) : 1.0;
        const double targetLog = baseLog + std::log(ratio) + cents * std::log(2.0) / 1200.0;
        v.logHz += pitchSmooth * (targetLog - v.logHz);
        const double hz = clampv(std::exp(v.logHz), 50.0, std::min(1500.0, sr * .4));

        // --- source: band-limited saw, one pole of tilt (about -12 dB/oct overall, glottal-ish), plus aspiration noise
        const double dtp = hz / sr;
        v.phase += dtp; if (v.phase >= 1) v.phase -= 1;
        const double saw = 2 * v.phase - 1 - polyBlep(v.phase, dtp);
        v.lp += .13 * (saw - v.lp);
        v.breathEnv *= std::exp(-dt / .14);                         // the breath at the start of a phrase dies away
        const float white = (float) (rnd(v.seed) * 2 - 1);
        v.noiseLp += .35f * (white - v.noiseLp);
        const double aspiration = (white - v.noiseLp) * (v.breathEnv * 1.0 + .035) * clampv((double) c.breath, 0.0, 1.0) * 3.0;
        const double src = v.lp * 3.0;
        double hiss = 0;
        for (int f = 0; f < 3; ++f) hiss += v.formantAmp[f] * v.whisper[f].process(aspiration);

        // --- vowel: five formants in parallel
        double y = 0;
        for (int f = 0; f < 5; ++f) y += v.formantAmp[f] * v.res[f].process(src);
        y += hiss;

        // --- keep timbre loudness steady across vowels and registers
        v.rms += (1.0 / (.06 * sr)) * (y * y - v.rms);
        const double yn = y / std::sqrt(v.rms + 1e-9) * .2;

        // --- performance: each singer enters and leaves with their own timing
        v.gateTime = c.gate > .5f ? v.gateTime + dt : 0.0;
        const double g = v.gateTime > v.onsetDelay ? (double) c.gate : 0.0;
        v.env += (g > v.env ? 1.0 - std::exp(-1.0 / (v.attack * sr)) : 1.0 - std::exp(-1.0 / (v.release * sr))) * (g - v.env);
        out[k] = (float) (yn * v.env * levelEnv * kLevel * 5.0);
    }
}

} // namespace choir
