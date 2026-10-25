#pragma once
// Uniform-partitioned FFT convolution (overlap-save). One channel; the stage uses two for a stereo hall.
// JUCE-free. Latency = one partition (default 1024 samples): the hall's pre-delay absorbs it.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace oc {

class Fft
{
public:
    explicit Fft(int size) : n(size), rev((size_t) size), cosT((size_t) size / 2), sinT((size_t) size / 2)
    {
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; }
        for (int i = 0; i < n / 2; ++i) { cosT[(size_t) i] = (float) std::cos(2 * 3.14159265358979323846 * i / n); sinT[(size_t) i] = (float) std::sin(2 * 3.14159265358979323846 * i / n); }
    }
    // in place; inverse is unnormalised (divide by n yourself)
    void transform(float* re, float* im, bool inverse) const
    {
        for (int i = 0; i < n; ++i) { int j = rev[(size_t) i]; if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const float wr = cosT[(size_t) (j * step)], wi = inverse ? sinT[(size_t) (j * step)] : -sinT[(size_t) (j * step)];
                    const int a = i + j, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                }
        }
    }
    int size() const { return n; }
private:
    int n; std::vector<int> rev; std::vector<float> cosT, sinT;
};

// Catmull-Rom (cubic) resampling of an impulse response from srcRate to dstRate.
inline std::vector<float> resampleCubic(const float* x, int n, double srcRate, double dstRate)
{
    if (n <= 0) return {};
    const double step = srcRate / dstRate; const int m = std::max(1, (int) std::floor((double) n / step));
    std::vector<float> out((size_t) m);
    auto at = [&](int k) { return k < 0 || k >= n ? 0.0 : (double) x[k]; };
    for (int i = 0; i < m; ++i)
    {
        const double t = i * step; const int j = (int) std::floor(t); const double f = t - j;
        const double a = at(j - 1), b = at(j), c = at(j + 1), d = at(j + 2);
        out[(size_t) i] = (float) (b + .5 * f * (c - a + f * (2 * a - 5 * b + 4 * c - d + f * (3 * (b - c) + d - a))));
    }
    return out;
}

class Convolver
{
public:
    static constexpr int kMaxSeconds = 8;
    // ir: impulse response (already at the engine's sample rate). Built off the audio thread.
    Convolver(const float* ir, int length, int partition = 1024)
        : P(partition), fft(2 * partition)
    {
        length = std::max(1, length);
        M = (length + P - 1) / P;
        bins = P + 1;
        hRe.assign((size_t) M * bins, 0.f); hIm.assign((size_t) M * bins, 0.f);
        xRe.assign((size_t) M * bins, 0.f); xIm.assign((size_t) M * bins, 0.f);
        std::vector<float> re((size_t) 2 * P), im((size_t) 2 * P);
        for (int m = 0; m < M; ++m)
        {
            std::fill(re.begin(), re.end(), 0.f); std::fill(im.begin(), im.end(), 0.f);
            for (int i = 0; i < P; ++i) { const int idx = m * P + i; if (idx < length) re[(size_t) i] = ir[idx]; }
            fft.transform(re.data(), im.data(), false);
            for (int k = 0; k < bins; ++k) { hRe[(size_t) m * bins + (size_t) k] = re[(size_t) k]; hIm[(size_t) m * bins + (size_t) k] = im[(size_t) k]; }
        }
        inBlock.assign((size_t) P, 0.f); prevBlock.assign((size_t) P, 0.f); outBlock.assign((size_t) P, 0.f);
        accRe.assign((size_t) bins, 0.f); accIm.assign((size_t) bins, 0.f); workRe.assign((size_t) 2 * P, 0.f); workIm.assign((size_t) 2 * P, 0.f);
    }
    int latency() const { return P; }
    int partitions() const { return M; }
    void reset()
    {
        std::fill(xRe.begin(), xRe.end(), 0.f); std::fill(xIm.begin(), xIm.end(), 0.f);
        std::fill(inBlock.begin(), inBlock.end(), 0.f); std::fill(prevBlock.begin(), prevBlock.end(), 0.f); std::fill(outBlock.begin(), outBlock.end(), 0.f);
        fill = 0; head = 0;
    }
    // any n; the output is the input convolved with the IR, delayed by latency()
    void process(const float* in, float* out, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            out[i] = outBlock[(size_t) fill];
            inBlock[(size_t) fill] = in[i];
            if (++fill == P) { runBlock(); fill = 0; }
        }
    }
private:
    void runBlock()
    {
        // spectrum of [previous block | this block]
        for (int i = 0; i < P; ++i) { workRe[(size_t) i] = prevBlock[(size_t) i]; workRe[(size_t) (P + i)] = inBlock[(size_t) i]; }
        std::fill(workIm.begin(), workIm.end(), 0.f);
        fft.transform(workRe.data(), workIm.data(), false);
        head = (head + 1) % M;
        float* xr = &xRe[(size_t) head * bins]; float* xi = &xIm[(size_t) head * bins];
        for (int k = 0; k < bins; ++k) { xr[k] = workRe[(size_t) k]; xi[k] = workIm[(size_t) k]; }
        std::fill(accRe.begin(), accRe.end(), 0.f); std::fill(accIm.begin(), accIm.end(), 0.f);
        float* ar = accRe.data(); float* ai = accIm.data();
        for (int m = 0; m < M; ++m)
        {
            const int slot = (head - m + M) % M;
            const float* hr = &hRe[(size_t) m * bins]; const float* hi = &hIm[(size_t) m * bins];
            const float* sr = &xRe[(size_t) slot * bins]; const float* si = &xIm[(size_t) slot * bins];
            for (int k = 0; k < bins; ++k) { ar[k] += hr[k] * sr[k] - hi[k] * si[k]; ai[k] += hr[k] * si[k] + hi[k] * sr[k]; }
        }
        // back to time: rebuild the conjugate-symmetric half, inverse, keep the last P samples (overlap-save)
        for (int k = 0; k < bins; ++k) { workRe[(size_t) k] = ar[k]; workIm[(size_t) k] = ai[k]; }
        for (int k = 1; k < P; ++k) { workRe[(size_t) (2 * P - k)] = ar[k]; workIm[(size_t) (2 * P - k)] = -ai[k]; }
        fft.transform(workRe.data(), workIm.data(), true);
        const float scale = 1.f / (float) (2 * P);
        for (int i = 0; i < P; ++i) outBlock[(size_t) i] = workRe[(size_t) (P + i)] * scale;
        prevBlock = inBlock;
    }
    int P, M = 1, bins = 1, fill = 0, head = 0;
    Fft fft;
    std::vector<float> hRe, hIm, xRe, xIm, inBlock, prevBlock, outBlock, accRe, accIm, workRe, workIm;
};

} // namespace oc
