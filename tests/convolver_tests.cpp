#undef NDEBUG   // these tests use assert(); a Release build must still check
#include "../Source/Stage/Convolver.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
using namespace oc;
int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::mt19937 rng(5); std::normal_distribution<float> G(0, 1);
    // 1. FFT round trip
    { Fft f(1024); std::vector<float> re(1024), im(1024, 0.f), ref; for (auto& v : re) v = G(rng); ref = re; f.transform(re.data(), im.data(), false); f.transform(re.data(), im.data(), true);
      double e = 0; for (int i = 0; i < 1024; ++i) e = std::max(e, (double) std::fabs(re[(size_t) i] / 1024 - ref[(size_t) i])); std::printf("[1] FFT round-trip max error %.2e\n", e); assert(e < 1e-5); }
    // 2. agrees with brute-force convolution, for several IR lengths and awkward host block sizes
    for (int irLen : { 1, 700, 1024, 1025, 5000, 30000 })
        for (int chunk : { 1, 97, 1024, 4096 })
        {
            std::vector<float> ir((size_t) irLen), x(40000), out(x.size());
            for (auto& v : ir) v = G(rng) * .1f;
            for (auto& v : x) v = G(rng);
            Convolver c(ir.data(), irLen, 1024); for (size_t i = 0; i < x.size(); i += (size_t) chunk) c.process(&x[i], &out[i], (int) std::min<size_t>((size_t) chunk, x.size() - i));
            double err = 0, ref = 0; const int L = c.latency();
            for (size_t n = 0; n + (size_t) L < x.size(); n += 7) { double s = 0; for (int k = 0; k < irLen && (size_t) k <= n; ++k) s += (double) ir[(size_t) k] * x[n - (size_t) k]; err = std::max(err, std::fabs(s - out[n + (size_t) L])); ref = std::max(ref, std::fabs(s)); }
            if (chunk == 97 || irLen == 30000) std::printf("[2] IR %5d samples, host block %4d: max error %.2e (signal peak %.2f)\n", irLen, chunk, err, ref);
            assert(err < 2e-3 * std::max(1.0, ref));
        }
    // 3. cost: a 6 s stereo IR at 48 kHz (the assert is only a sanity bound that also holds unoptimised; the printed figure is the real cost)
    { const int len = 6 * 48000; std::vector<float> ir((size_t) len); for (int i = 0; i < len; ++i) ir[(size_t) i] = G(rng) * std::exp(-3.0f * i / len * 5.f) * .02f;
      Convolver a(ir.data(), len, 1024), b(ir.data(), len, 1024); std::vector<float> x((size_t) 48000 * 10), y(x.size()); for (auto& v : x) v = G(rng);
      auto t0 = std::chrono::steady_clock::now(); for (size_t i = 0; i < x.size(); i += 128) { int n = (int) std::min<size_t>(128, x.size() - i); a.process(&x[i], &y[i], n); b.process(&x[i], &y[i], n); }
      double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); std::printf("[3] 6 s stereo IR (%d partitions/ch): 10 s of audio in %.2f s = %.1fx realtime\n", a.partitions(), sec, 10 / sec); assert(sec < 300); }
    std::printf("ALL CONVOLVER TESTS PASSED\n");
}
