#undef NDEBUG   // these tests use assert(); a Release build must still check
#include "../Source/StagedChoir.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

static const double kPiD = 3.14159265358979323846;
using choir::StagedChoir; using choir::FXParams;

static std::vector<float> sung(double sr, double seconds, double hz = 220.0, double level = 0.22)
{
    std::vector<float> x((size_t) (sr * seconds)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        double t = i / sr, gate = std::fmod(t, 1.5) < 1.2 ? 1.0 : 0.0;
        ph += 2 * kPiD * hz * (1 + 0.005 * std::sin(2 * kPiD * 5.3 * t)) / sr; double s = 0; for (int h = 1; h <= 8; ++h) s += std::sin(h * ph) / h;
        x[i] = (float) (level * gate * s);
    }
    return x;
}
struct Out { std::vector<float> l, r; };
static Out run(StagedChoir& sc, const std::vector<float>& x, int chunk = 4096)
{
    Out o; o.l.resize(x.size()); o.r.resize(x.size());
    for (size_t i = 0; i < x.size(); i += (size_t) chunk)
    { int n = (int) std::min<size_t>((size_t) chunk, x.size() - i); sc.process(&x[i], &x[i], &o.l[i], &o.r[i], n); }
    return o;
}
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double) (b - a)); }
static bool finite(const Out& o) { for (size_t i = 0; i < o.l.size(); ++i) if (!std::isfinite(o.l[i]) || !std::isfinite(o.r[i])) return false; return true; }
static FXParams base() { FXParams p; p.mix = 1.f; p.master = 1.f; p.wet = .37f; p.gain = { .83f, .83f, .83f, .83f }; return p; }

int main()
{
    // 1. silence in -> silence out, finite, at three sample rates
    for (double sr : { 44100., 48000., 96000. })
    {
        StagedChoir sc; sc.prepare(sr); auto p = base(); p.hall = .2f; sc.setParameters(p);
        std::vector<float> z((size_t) (sr * 1.5), 0.f); auto o = run(sc, z);
        assert(finite(o)); assert(rms(o.l, 0, z.size()) < 1e-7);
        std::printf("[1] silence @%.0f Hz ok\n", sr);
    }
    // 2. latency + dry path: with 0% choir the lead at home comes out exactly, delayed by the reported latency
    {
        StagedChoir sc; sc.prepare(48000); auto p = base(); p.mix = 0.f; p.master = 1.f; sc.setParameters(p);
        std::vector<float> x(48000, 0.f); x[1000] = .5f; x[1001] = -.25f;
        auto o = run(sc, x, 300);
        const int L = StagedChoir::latencySamples;
        assert(std::fabs(o.l[1000 + L] - .5f) < 1e-4 && std::fabs(o.r[1001 + L] + .25f) < 1e-4);
        for (size_t i = 0; i < o.l.size(); ++i) if ((int) i != 1000 + L && (int) i != 1001 + L) assert(std::fabs(o.l[i]) < 1e-4);
        std::printf("[2] dry lead exact, latency = %d samples\n", L);
    }
    // 3. level: the choir matches v0.5's loudness within 1.5 dB
    {
        const double sr = 48000; auto x = sung(sr, 9); auto p = base(); const size_t from = (size_t) (3 * sr);
        choir::ChoirEffect fx; fx.prepare(sr); fx.setParameters(p); double a = 0; for (size_t i = 0; i < x.size(); ++i) { float l, r; fx.render(x[i], x[i], l, r); if (i >= from) a += .5 * ((double) l * l + (double) r * r); }
        StagedChoir sc; sc.prepare(sr); sc.setParameters(p); auto o = run(sc, x); double b = 0; for (size_t i = from; i < x.size(); ++i) b += .5 * ((double) o.l[i] * o.l[i] + (double) o.r[i] * o.r[i]);
        double db = 20 * std::log10(std::sqrt(b / a));
        std::printf("[3] choir level vs v0.5: %+.2f dB\n", db); assert(std::fabs(db) < 1.5);
    }
    // 4. bounded even when loud and over-driven; all six mic arrays work
    for (int mic = 0; mic < 6; ++mic)
    {
        StagedChoir sc; sc.prepare(48000); auto p = base(); p.mix = .9f; p.master = 1.5f; p.micMode = mic; p.hall = .3f; sc.setParameters(p);
        auto x = sung(48000, 4, 220, 0.9); auto o = run(sc, x, 512);
        float pk = 0; for (size_t i = 0; i < o.l.size(); ++i) pk = std::max(pk, std::max(std::fabs(o.l[i]), std::fabs(o.r[i])));
        assert(finite(o)); assert(pk <= 1.0001f); assert(rms(o.l, 48000, 192000) > 1e-3);
        std::printf("[4] mic array %d: peak %.3f ok\n", mic, pk);
    }
    // 5. section placement moves the image: sopranos far left vs far right
    {
        auto x = sung(48000, 4, 440); double e[2][2];
        for (int side = 0; side < 2; ++side)
        {
            StagedChoir sc; sc.prepare(48000); auto p = base(); p.harmony = 2; p.gain = { 1.f, 0.f, 0.f, 0.f }; p.wet = 0.f; p.x[0] = side ? 8.f : -8.f; p.y[0] = 5.f; sc.setParameters(p);
            auto o = run(sc, x); e[side][0] = rms(o.l, 96000, 192000); e[side][1] = rms(o.r, 96000, 192000);
        }
        std::printf("[5] left-placed L/R %.3f / %.3f | right-placed L/R %.3f / %.3f\n", e[0][0], e[0][1], e[1][0], e[1][1]);
        assert(e[0][0] > e[0][1] * 1.2 && e[1][1] > e[1][0] * 1.2);
    }
    // 6. the lead moves onto the stage: still finite, still audible, and different from the dry vocal
    {
        auto x = sung(48000, 5); StagedChoir sc; sc.prepare(48000); auto p = base(); p.mix = 0.f; p.leadX = 4.f; p.leadY = 6.f; sc.setParameters(p);
        auto o = run(sc, x, 256); assert(finite(o));
        double diff = 0, e = 0; const int L = StagedChoir::latencySamples;
        for (size_t i = 96000; i < 192000; ++i) { double d = o.l[i] - x[i - (size_t) L]; diff += d * d; e += (double) o.l[i] * o.l[i]; }
        std::printf("[6] staged lead: rms %.4f, rms difference from dry %.4f\n", std::sqrt(e / 96000), std::sqrt(diff / 96000));
        assert(e > 1e-4 && diff > 1e-4);
    }
    // 7. host buffer size never changes the sound: one sample, odd sizes and big blocks are bit-identical
    {
        auto x = sung(48000, 3); Out ref; bool first = true;
        for (int chunk : { 1, 37, 128, 513, 4096 })
        {
            StagedChoir sc; sc.prepare(48000); auto p = base(); p.hall = .15f; p.leadX = 1.5f; sc.setParameters(p); auto o = run(sc, x, chunk);
            if (first) { ref = o; first = false; } else assert(o.l == ref.l && o.r == ref.r);
        }
        std::printf("[7] identical output for host buffers of 1, 37, 128, 513, 4096\n");
    }
    // 8. automation: sweep the mic, a section, the room and the array mid-stream - no non-finite, no runaway
    {
        auto x = sung(48000, 6); StagedChoir sc; sc.prepare(48000); auto p = base(); p.hall = .2f; Out o; o.l.resize(x.size()); o.r.resize(x.size());
        for (size_t i = 0; i < x.size(); i += 256)
        {
            double t = (double) i / 48000; p.micX = (float) (3 * std::sin(t * 2)); p.micY = (float) (2 * std::sin(t * 1.3)); p.x[1] = (float) (-1.7 + 2 * std::sin(t));
            p.roomSize = (float) (.5 + .4 * std::sin(t * .9)); p.roomSeconds = (float) (2.4 + 1.5 * std::sin(t * .7)); p.micMode = (int) (t) % 6; p.leadX = (float) (2 * std::sin(t));
            sc.setParameters(p); int n = (int) std::min<size_t>(256, x.size() - i); sc.process(&x[i], &x[i], &o.l[i], &o.r[i], n);
        }
        float pk = 0; for (size_t i = 0; i < o.l.size(); ++i) pk = std::max(pk, std::fabs(o.l[i]));
        assert(finite(o)); assert(pk <= 1.0001f); std::printf("[8] automation sweep ok, peak %.3f\n", pk);
    }
    // 9. CPU
    {
        const double sr = 48000; auto x = sung(sr, 10); StagedChoir sc; sc.prepare(sr); auto p = base(); p.hall = .2f; sc.setParameters(p);
        auto t0 = std::chrono::steady_clock::now(); auto o = run(sc, x, 512); double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("[9] 10 s of audio in %.2f s = %.1fx realtime (one core)\n", sec, 10 / sec); assert(finite(o));
    }
    std::printf("ALL STAGED TESTS PASSED\n");
}
