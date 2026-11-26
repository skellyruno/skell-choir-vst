// The period-synchronous pitch shifter (FXParams::pitchSync, the STEADY NOTES button). Off must be the original sound: the new engine must not even run.
// On, it must (1) shift by exactly the intended interval, (2) stay clean through vibrato, where the old window lock could not, (3) put the choir
// closer to the lead, (4) keep the level, (5) survive any input and a switch in mid-note, and (6) do all of that on real vocals, not only on
// synthetic tones. Every number below was measured first (see docs/V0_9_CHANGELOG.md, "Pitch-synchronous shifter") and the limits leave margin.
#include "../Source/ChoirFX.h"
#include "test_support.h"
#include <complex>
#include <cstdlib>

using namespace choir;
static const double PI = 3.14159265358979323846;

static void fft(std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i) { int b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap(a[(size_t) i], a[(size_t) j]); }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * PI / len; const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) { std::complex<double> w(1); for (int j = 0; j < len / 2; ++j) { auto u = a[(size_t) (i + j)], v = a[(size_t) (i + j + len / 2)] * w; a[(size_t) (i + j)] = u + v; a[(size_t) (i + j + len / 2)] = u - v; w *= wl; } }
    }
}
// energy between the harmonics, in dB re the energy on them, and the pitch of the harmonic comb (searched for, so the intended pitch is NOT assumed)
static double spurious(const std::vector<float>& x, size_t start, double sr, double* f0Out = nullptr)
{
    const int N = 1 << 16; std::vector<std::complex<double>> a((size_t) N);
    for (int i = 0; i < N; ++i) a[(size_t) i] = x[start + (size_t) i] * (.5 - .5 * std::cos(2 * PI * i / N));
    fft(a); const double bin = sr / N; std::vector<double> e((size_t) N / 2); for (int i = 0; i < N / 2; ++i) e[(size_t) i] = std::norm(a[(size_t) i]);
    auto comb = [&](double f0, double& on, double& all) { on = all = 0; for (int i = (int) (100 / bin); i < (int) (5000 / bin); ++i) { all += e[(size_t) i]; const double f = i * bin, k = std::round(f / f0); if (k >= 1 && std::fabs(f - k * f0) < 4.0) on += e[(size_t) i]; } };
    double best = 0, bf = 0, on, all;
    for (double f0 = 70; f0 < 700; f0 += .25) { comb(f0, on, all); if (on > best) { best = on; bf = f0; } }
    // a comb at f0/2 catches every harmonic of f0 too: take the highest comb that holds nearly as much energy
    for (double f0 = 700; f0 >= 70; f0 -= .25) { comb(f0, on, all); if (on >= .97 * best) { bf = f0; best = std::max(best, on); break; } }
    for (double f0 = bf - .3; f0 < bf + .3; f0 += .01) { comb(f0, on, all); if (on > best) { best = on; bf = f0; } }
    comb(bf, on, all); if (f0Out) *f0Out = bf; return 10 * std::log10(std::max(1e-30, all - on) / on);
}
// the pitch of a held note, from the spectral peaks of its first six harmonics (parabolic interpolation on the log magnitude), searched within +-3% of `want`
static double peakPitch(const std::vector<float>& x, size_t start, double sr, double want)
{
    const int N = 1 << 17; std::vector<std::complex<double>> a((size_t) N);
    for (int i = 0; i < N; ++i) a[(size_t) i] = x[start + (size_t) i] * (.5 - .5 * std::cos(2 * PI * i / N));
    fft(a); const double bin = sr / N; double sum = 0; int n = 0;
    for (int h = 1; h <= 6; ++h)
    {
        const int lo = (int) (h * want * .97 / bin), hi = (int) (h * want * 1.03 / bin); int pk = lo; double m = 0;
        for (int i = lo; i <= hi; ++i) { const double v = std::abs(a[(size_t) i]); if (v > m) { m = v; pk = i; } }
        const double l = std::log(std::abs(a[(size_t) pk - 1]) + 1e-30), c = std::log(m + 1e-30), r = std::log(std::abs(a[(size_t) pk + 1]) + 1e-30), den = l - 2 * c + r;
        sum += (pk + (std::fabs(den) > 1e-12 ? .5 * (l - r) / den : 0.0)) * bin / h; ++n;
    }
    return sum / n;
}
// the 10-50 Hz part of the log level, dB rms, over the samples where mask[i - lag] is set (empty mask = everything after `from`)
static double flutter(const std::vector<float>& s, int sr, int from, const std::vector<char>* mask = nullptr, int lag = 0)
{
    const int w = sr / 200; std::vector<double> le; std::vector<char> mk;
    for (int i = from; i + 2 * w < (int) s.size(); i += w)
    {
        double a = 0; for (int k = 0; k < 2 * w; ++k) a += (double) s[(size_t) (i + k)] * s[(size_t) (i + k)];
        le.push_back(10 * std::log10(a / (2 * w) + 1e-12));
        const int c = i + w - lag; mk.push_back(!mask ? 1 : (c >= 0 && c < (int) mask->size() ? (*mask)[(size_t) c] : 0));
    }
    auto ma = [&](int half) { std::vector<double> o(le.size(), 0.0); for (int i = 0; i < (int) le.size(); ++i) { double a = 0; int c = 0; for (int k = std::max(0, i - half); k <= std::min((int) le.size() - 1, i + half); ++k) { a += le[(size_t) k]; ++c; } o[(size_t) i] = a / c; } return o; };
    const auto fast = ma(2), slow = ma(10); double ss = 0; long c = 0;
    for (size_t i = 0; i < le.size(); ++i) if (mk[i]) { const double d = fast[i] - slow[i]; ss += d * d; ++c; }
    return c ? std::sqrt(ss / (double) c) : 0.0;
}
static double rmsOf(const std::vector<float>& x, size_t from, size_t to) { double s = 0; for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i]; return std::sqrt(s / (double) (to - from)); }
static double maxD2(const std::vector<float>& x, size_t from, size_t to) { double m = 0; for (size_t i = from; i + 2 < to; ++i) m = std::max(m, (double) std::fabs(x[i + 2] - 2 * x[i + 1] + x[i])); return m; }

struct Voices { std::vector<float> v[16]; std::unique_ptr<ChoirEffect> fx; };
static FXParams engineParams(bool sync)
{
    FXParams p = ts::pluginDefaults(); p.humanize = 0.f; p.ensemble = 0.f; p.voiceBlend = 0.f; p.formant = 0.f; p.sensitivity = 1.f; p.mode = 0; p.harmony = 0; p.lockGrains = false; p.pitchSync = sync;
    return p;
}
static Voices runVoices(const std::vector<float>& x, int sr, FXParams p)
{
    Voices r; r.fx = std::make_unique<ChoirEffect>(); r.fx->prepare(sr); r.fx->setVoiceParameters(p);
    for (auto& v : r.v) v.reserve(x.size());
    float out[16];
    for (float s : x) { r.fx->processVoices(s, out); for (int k = 0; k < 16; ++k) r.v[k].push_back(out[k]); }
    return r;
}
// a sustained 12-harmonic note, f0 (Hz) with vibrato +-cents at 5.5 Hz
static std::vector<float> tone(int sr, double seconds, double f0, double cents, double onsetAt = 0.0)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double t = (double) n / sr, f = f0 * std::pow(2.0, cents * std::sin(2 * PI * 5.5 * t) / 1200.0);
        ph += 2 * PI * f / sr; double s = 0; for (int h = 1; h <= 12; ++h) s += std::sin(h * ph) / h;
        x[n] = t >= onsetAt ? (float) (.25 * s) : 0.f;
    }
    return x;
}

// YIN on one frame: f0 (Hz) and periodicity (1 - min CMND over 60..800 Hz)
struct Yin { double f0, per; };
static Yin yin(const float* x, int sr)
{
    const int W = 1536, minLag = std::max(4, sr / 800), maxLag = sr / 60;
    std::vector<double> c((size_t) maxLag + 2, 1.0); double run = 0;
    for (int tau = 1; tau <= maxLag; ++tau) { double s = 0; for (int j = 0; j < W; ++j) { const double v = (double) x[j] - x[j + tau]; s += v * v; } run += s; c[(size_t) tau] = run > 1e-12 ? s * tau / run : 1.0; }
    int best = -1; double m = 1e9;
    for (int tau = minLag; tau <= maxLag; ++tau) if (c[(size_t) tau] < m) { m = c[(size_t) tau]; best = tau; }
    for (int tau = minLag + 1; tau < maxLag; ++tau) if (c[(size_t) tau] < .15 && c[(size_t) tau] <= c[(size_t) tau - 1] && c[(size_t) tau] < c[(size_t) tau + 1]) { best = tau; break; }
    double ref = best;
    if (best > minLag && best < maxLag) { const double a = c[(size_t) best - 1], b = c[(size_t) best], cc = c[(size_t) best + 1], den = a - 2 * b + cc; if (std::fabs(den) > 1e-9) ref += std::max(-.5, std::min(.5, .5 * (a - cc) / den)); }
    return { sr / ref, 1.0 - m };
}

struct VocalResult { double lagMs[4], perMean[4], errMedian[4], errGross[4], flutterVoices, flutterInput, levelDb, seamRho; long seams; };
static double median(std::vector<double> a) { if (a.empty()) return 0; std::sort(a.begin(), a.end()); return a[a.size() / 2]; }
// One real vocal through the 16 voices: how late the choir is, how exact and periodic each section is, how steady its level, and how good the seams are
static VocalResult measureVocal(const std::vector<float>& x, int sr, bool sync)
{
    const int N = (int) x.size();
    const Voices r = runVoices(x, sr, engineParams(sync));
    const double ratio[4] = { std::pow(2., 7 / 12.), std::pow(2., 4 / 12.), std::pow(2., -5 / 12.), .5 };
    VocalResult out {};
    auto env = [&](const std::vector<float>& s) { const int w = sr / 200; std::vector<double> e; for (int i = 0; i + w < N; i += w) { double a = 0; for (int k = 0; k < w; ++k) a += (double) s[(size_t) (i + k)] * s[(size_t) (i + k)]; e.push_back(std::sqrt(a / w)); } return e; };
    const auto ei = env(x);
    for (int sec = 0; sec < 4; ++sec)
    {
        std::vector<float> sum((size_t) N, 0.f); for (int j = 0; j < 4; ++j) for (int n = 0; n < N; ++n) sum[(size_t) n] += r.v[sec * 4 + j][(size_t) n];
        const auto eo = env(sum); double bestc = -1e9; int bl = 0;
        for (int l = 0; l < 30; ++l) { double cc = 0; for (size_t i = 0; i + (size_t) l < eo.size() && i < ei.size(); ++i) cc += ei[i] * eo[i + (size_t) l]; if (cc > bestc) { bestc = cc; bl = l; } }
        out.lagMs[sec] = bl * 5.0;
    }
    const int W = 1536, span = W + sr / 60 + 4, hop = sr * 3 / 100;
    std::vector<double> perIn, perOut[4], err[4]; long gross[4] = {}, cnt[4] = {};
    std::vector<char> pm((size_t) N + 8 * (size_t) sr, 0);
    for (int s = sr; s + span + sr / 5 < N; s += hop)
    {
        double e = 0; for (int i = 0; i < W; ++i) e += (double) x[(size_t) (s + i)] * x[(size_t) (s + i)];
        if (std::sqrt(e / W) < .02) continue;
        const Yin yi = yin(&x[(size_t) s], sr); if (yi.per < .85) continue;
        perIn.push_back(yi.per);
        if (yi.per >= .9) for (int i = 0; i < hop; ++i) pm[(size_t) (s + W / 2 - hop / 2 + i)] = 1;
        for (int sec = 0; sec < 4; ++sec)
        {
            const int off = (int) std::lround(out.lagMs[sec] * sr / 1000.0);
            for (int j : { 0, 2 })
            {
                const int k = sec * 4 + j; if (s + off + span >= N) continue;
                const Yin yo = yin(&r.v[k][(size_t) (s + off)], sr);
                perOut[sec].push_back(yo.per); ++cnt[sec];
                if (yo.per > .5) { const double c = 1200 * std::log2(yo.f0 / (yi.f0 * ratio[sec])); if (std::fabs(c) > 500) ++gross[sec]; else err[sec].push_back(std::fabs(c)); }
            }
        }
    }
    for (int sec = 0; sec < 4; ++sec)
    {
        double m = 0; for (double q : perOut[sec]) m += q; out.perMean[sec] = perOut[sec].empty() ? 0 : m / (double) perOut[sec].size();
        out.errMedian[sec] = median(err[sec]); out.errGross[sec] = cnt[sec] ? 100.0 * (double) gross[sec] / (double) cnt[sec] : 0;
    }
    out.flutterInput = flutter(x, sr, 0, &pm, 0);
    for (int k : { 0, 4, 8, 12 }) out.flutterVoices += flutter(r.v[k], sr, 0, &pm, (int) std::lround(out.lagMs[k / 4] * sr / 1000.0)) / 4;
    double ein = 0, eo = 0; long cn = 0;
    for (int n = sr; n < N; ++n) if (std::fabs(x[(size_t) n]) > .01f) { ein += (double) x[(size_t) n] * x[(size_t) n]; for (int k = 0; k < 16; ++k) eo += (double) r.v[k][(size_t) n] * r.v[k][(size_t) n]; ++cn; }
    out.levelDb = 10 * std::log10((eo / 16) / std::max(ein, 1e-30));
    double rs = 0; long pj = 0; for (int k = 0; k < 16; ++k) { pj += r.fx->syncPitchedJumps(k); rs += r.fx->syncSeamCorrelation(k) * r.fx->syncPitchedJumps(k); }
    out.seams = pj; out.seamRho = pj ? rs / (double) pj : 0;
    return out;
}

int main(int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "tests/data";
    const int sr = 48000;

    // 1. OFF is the original sound: the new engine does not run at all; ON it runs, deterministically
    {
        const auto x = tone(sr, 2.0, 196, 0);
        const auto off = runVoices(x, sr, engineParams(false)), off2 = runVoices(x, sr, engineParams(false));
        int seams = 0; for (int k = 0; k < 16; ++k) seams += off.fx->syncJumps(k);
        CHECK(seams == 0, "with pitchSync off the new engine must never run (%d seams)", seams);
        bool same = true; for (int k = 0; k < 16; ++k) same = same && off.v[k] == off2.v[k];
        CHECK(same, "two renders with the flag off must be bit-identical");
        const auto on = runVoices(x, sr, engineParams(true)), on2 = runVoices(x, sr, engineParams(true));
        int onSeams = 0; for (int k = 0; k < 16; ++k) onSeams += on.fx->syncJumps(k);
        CHECK(onSeams > 100, "with pitchSync on the voices must be jumping (%d seams)", onSeams);
        same = true; for (int k = 0; k < 16; ++k) same = same && on.v[k] == on2.v[k];
        CHECK(same, "two renders with the flag on must be bit-identical");
        CHECK(!FXParams().pitchSync, "pitchSync must default to off");
        std::printf("[1] off: engine never runs, bit-identical; on: %d seams in 2 s, deterministic\n", onSeams);
    }

    // 2. a held note: the energy between the harmonics falls, and the pitch is the intended interval to within a few cents
    {
        const auto x = tone(sr, 6.0, 196, 0);
        const auto lg = runVoices(x, sr, engineParams(false)), sy = runVoices(x, sr, engineParams(true));
        const int idx[4] = { 0, 4, 8, 12 }; const double semis[4] = { 7, 4, -5, -12 };
        for (int s = 0; s < 4; ++s)
        {
            const double dl = spurious(lg.v[idx[s]], (size_t) (3 * sr), sr), ds = spurious(sy.v[idx[s]], (size_t) (3 * sr), sr);
            const double want = 196 * std::pow(2.0, semis[s] / 12.0), cents = 1200 * std::log2(peakPitch(sy.v[idx[s]], (size_t) (2 * sr), sr, want) / want);
            const double centsLg = 1200 * std::log2(peakPitch(lg.v[idx[s]], (size_t) (2 * sr), sr, want) / want);
            std::printf("    voice %2d (%+3.0f st): between-harmonic energy legacy %6.1f dB, sync %6.1f dB; pitch off by %+.2f cents (two-grain %+.2f)\n", idx[s], semis[s], dl, ds, cents, centsLg);
            CHECK(ds < dl - 10.0, "voice %d: sync %.1f dB is not clearly cleaner than the two-grain shifter %.1f dB", idx[s], ds, dl);
            CHECK(ds < -28.0, "voice %d: %.1f dB between the harmonics of a held note", idx[s], ds);
            CHECK(std::fabs(cents) < 4.0, "voice %d: pitch is %+.2f cents from the intended interval", idx[s], cents);
        }
        std::printf("[2] held note: far fewer sidebands, intervals exact\n");
    }

    // 3. vibrato: this is what the window lock could not do. Level flutter (10-50 Hz) stays near zero where the old shifter reaches 1-2 dB
    {
        for (double cents : { 35.0, 60.0 })
            for (double f0 : { 150.0, 244.0 })
            {
                const auto x = tone(sr, 6.0, f0, cents);
                const auto lg = runVoices(x, sr, engineParams(false)), sy = runVoices(x, sr, engineParams(true));
                for (int k : { 0, 8, 12 })
                {
                    const double fl = flutter(lg.v[k], sr, sr), fs = flutter(sy.v[k], sr, sr);
                    CHECK(fs < .45, "f0 %.0f vibrato +-%.0f c voice %d: level flutter %.2f dB", f0, cents, k, fs);
                    CHECK(fs < .5 * fl + .05, "f0 %.0f vibrato +-%.0f c voice %d: flutter %.2f dB is not clearly below the two-grain shifter's %.2f dB", f0, cents, k, fs, fl);
                }
            }
        std::printf("[3] vibrato +-35 and +-60 cents at 150 and 244 Hz: level flutter < .45 dB and under half the old shifter's\n");
    }

    // 4. the choir sits closer to the lead: time for a note's onset to reach half level at the voice output
    {
        const auto x = tone(sr, 3.0, 196, 0, 1.0);
        auto onset = [&](bool sync) {
            const auto r = runVoices(x, sr, engineParams(sync)); const int w = sr / 1000 * 5;
            std::vector<float> sum(x.size(), 0.f); for (int j = 0; j < 4; ++j) for (size_t n = 0; n < x.size(); ++n) sum[n] += r.v[8 + j][n];
            const double ref = rmsOf(sum, (size_t) (2.5 * sr), (size_t) (2.9 * sr));
            for (size_t i = (size_t) sr; i + w < x.size(); i += (size_t) (w / 5)) if (rmsOf(sum, i, i + (size_t) w) >= .5 * ref) return 1000.0 * ((double) i - sr) / sr;
            return -1.0; };
        const double lg = onset(false), sy = onset(true);
        std::printf("    onset to half level: two-grain %.0f ms, pitch-synchronous %.0f ms\n", lg, sy);
        CHECK(sy >= 0 && lg >= 0 && sy < lg - 20.0, "the new shifter must bring the choir at least 20 ms closer (%.0f vs %.0f ms)", sy, lg);
        CHECK(sy < 45.0, "onset to half level takes %.0f ms", sy);
        std::printf("[4] the choir arrives %.0f ms sooner\n", lg - sy);
    }

    // 5. level: the same as the two-grain shifter, so switching does not change the mix
    {
        for (double cents : { 0.0, 35.0 })
        {
            const auto x = tone(sr, 5.0, 196, cents); const auto lg = runVoices(x, sr, engineParams(false)), sy = runVoices(x, sr, engineParams(true));
            for (int k : { 0, 4, 8, 12 })
            {
                const double d = ts::dB(rmsOf(sy.v[k], (size_t) (2 * sr), (size_t) (5 * sr)) / rmsOf(lg.v[k], (size_t) (2 * sr), (size_t) (5 * sr)));
                CHECK(std::fabs(d) < 1.5, "voice %d level differs from the two-grain shifter by %.2f dB (vibrato %.0f c)", k, d, cents);
            }
        }
        std::printf("[5] level within 1.5 dB of the two-grain shifter on every voice (the real-vocal check below is tighter)\n");
    }

    // 6. switching in the middle of a note: nothing jumps, nothing blows up, in both directions
    {
        auto x = tone(sr, 8.0, 196, 25);
        FXParams p = engineParams(false); ChoirEffect fx; fx.prepare(sr); fx.setVoiceParameters(p);
        std::vector<float> v0, v8; float out[16]; double steadyMax = 0;
        for (size_t n = 0; n < x.size(); ++n)
        {
            const bool want = std::fmod((double) n / sr, 1.6) >= .8; p.pitchSync = want; fx.setVoiceParameters(p);
            fx.processVoices(x[n], out); v0.push_back(out[0]); v8.push_back(out[8]);
            for (int k = 0; k < 16; ++k) CHECK(std::isfinite(out[k]) && std::fabs(out[k]) < 4.f, "voice %d went to %f", k, out[k]);
        }
        const double dRef = std::max(maxD2(runVoices(x, sr, engineParams(false)).v[0], (size_t) (2 * sr), x.size()), maxD2(runVoices(x, sr, engineParams(true)).v[0], (size_t) (2 * sr), x.size()));
        steadyMax = std::max(maxD2(v0, (size_t) (2 * sr), v0.size()), maxD2(v8, (size_t) (2 * sr), v8.size())); (void) dRef;
        const double peakLevel = rmsOf(v0, (size_t) (2 * sr), v0.size());
        CHECK(steadyMax < 12 * peakLevel, "toggling mid-note makes a %.3f step (voice rms %.3f)", steadyMax, peakLevel);
        std::printf("[6] pressing the button every 0.8 s during a note: finite, no clicks\n");
    }

    // 7. any input, any sample rate: finite, bounded, and the read point never leaves the history
    {
        for (int rate : { 16000, 22050, 44100, 48000, 88200, 96000, 192000 })
            for (int kind = 0; kind < 5; ++kind)
            {
                std::vector<float> x((size_t) (rate * 2.5)); unsigned s = 12345u + (unsigned) kind;
                for (size_t n = 0; n < x.size(); ++n)
                {
                    s = s * 1664525u + 1013904223u; const double nz = ((s >> 8) / 16777216.0) * 2 - 1, t = (double) n / rate;
                    switch (kind) {
                        case 0: x[n] = (float) (.5 * nz); break;                                            // noise
                        case 1: x[n] = 0.f; break;                                                          // silence
                        case 2: x[n] = std::fmod(t, .5) < .25 ? 1.f : -1.f; break;                          // full-scale square, slow
                        case 3: x[n] = (float) (.4 * std::sin(2 * PI * (80 + 900 * t) * t)); break;         // a sweep through the whole voice range
                        default: x[n] = (float) (.6 * std::sin(2 * PI * 220 * t) * (std::fmod(t, .3) < .15 ? 1.0 : 0.0) + .02 * nz); break; } // notes with gaps
                }
                FXParams p = engineParams(true); p.harmony = 3; p.mode = kind % 3; p.humanize = 1.f; p.ensemble = 1.f; p.voiceBlend = kind == 4 ? .5f : 0.f; p.formant = 1.f;
                ChoirEffect fx; fx.prepare(rate); fx.setVoiceParameters(p); float out[16];
                for (float v : x) { fx.processVoices(v, out); for (int k = 0; k < 16; ++k) CHECK(std::isfinite(out[k]) && std::fabs(out[k]) < 8.f, "%d Hz input kind %d: voice %d = %f", rate, kind, k, out[k]); }
            }
        std::printf("[7] 16 kHz .. 192 kHz, noise / silence / square / sweep / gapped notes, harmony 3, every mode: finite and bounded\n");
    }

    // 8. real vocals (no Auto-Tune): the same claims, on singing
    for (const char* name : { "vocal_raw_9s.wav", "vocal_deuce_9s.wav", "vocal_alchemist_9s.wav", "vocal_lowmale_autotuned_9s.wav", "vocal_wtuwant_9s.wav" })
    {
        ts::Audio a;
        // Public CI uses the existing tracked vocal; owner-only recordings stay local.
        const bool privateFixture = std::string(name) != "vocal_raw_9s.wav";
        const char* privateMode = std::getenv("SKELLYCHOIR_PRIVATE_VOCALS");
        if (privateFixture && (!privateMode || std::string(privateMode) != "1")) {
            std::printf("    SKIP private fixture: %s (owner-only test)\n", name);
            continue;
        }
        CHECK(ts::readWav16(dir + "/" + name, a), "cannot read %s/%s", dir.c_str(), name);
        const VocalResult lg = measureVocal(a.x, a.sr, false), sy = measureVocal(a.x, a.sr, true);
        std::printf("    %s\n", name);
        const char* nm[4] = { "S+7", "A+4", "T-5", "B-12" };
        for (int s = 0; s < 4; ++s)
        {
            std::printf("      %-4s lag %3.0f -> %3.0f ms | periodicity %.3f -> %.3f | pitch error median %.1f -> %.1f cents, gross %.1f%% -> %.1f%%\n", nm[s], lg.lagMs[s], sy.lagMs[s], lg.perMean[s], sy.perMean[s], lg.errMedian[s], sy.errMedian[s], lg.errGross[s], sy.errGross[s]);
            CHECK(sy.lagMs[s] <= 25.0 && sy.lagMs[s] <= lg.lagMs[s] - 15.0, "%s %s: the choir still trails by %.0f ms (two-grain: %.0f ms)", name, nm[s], sy.lagMs[s], lg.lagMs[s]);
            CHECK(sy.errMedian[s] <= 7.0 && sy.errMedian[s] <= lg.errMedian[s] + 1.0, "%s %s: median pitch error %.1f cents (two-grain %.1f)", name, nm[s], sy.errMedian[s], lg.errMedian[s]);
            CHECK(sy.errGross[s] <= 3.5, "%s %s: %.1f%% of frames are an octave or more off", name, nm[s], sy.errGross[s]);
            CHECK(sy.perMean[s] >= lg.perMean[s] - .02, "%s %s: periodicity %.3f fell below the two-grain shifter's %.3f", name, nm[s], sy.perMean[s], lg.perMean[s]);
        }
        std::printf("      level vs lead %.2f -> %.2f dB | vowel flutter %.2f -> %.2f dB (input %.2f) | %ld pitched seams, mean correlation %.2f\n", lg.levelDb, sy.levelDb, lg.flutterVoices, sy.flutterVoices, sy.flutterInput, sy.seams, sy.seamRho);
        CHECK(std::fabs(sy.levelDb - lg.levelDb) < .5, "%s: level moved by %.2f dB", name, sy.levelDb - lg.levelDb);
        // (the two-grain shifter can read lower than the lead itself, because its 73 ms window smooths the singer's own level wobble; the right bound is the lead)
        CHECK(sy.flutterVoices <= sy.flutterInput + .45, "%s: the voices flutter %.2f dB against the input's own %.2f", name, sy.flutterVoices, sy.flutterInput);
        CHECK(sy.seams > 300 && sy.seamRho >= .75, "%s: %ld pitched seams with mean correlation %.2f (the seams must line up on the waveform)", name, sy.seams, sy.seamRho);
    }
    std::printf("[8] real vocals: closer, more exact, as periodic, same level\nALL SYNC SHIFT TESTS PASSED\n");
    return 0;
}
