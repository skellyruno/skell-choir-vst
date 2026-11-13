// The three built-in rooms (Assets/Rooms/*.wav, made by tools/make_room_irs.py): the files are sane, they decay in the order their
// names promise, and the engine accepts each one and stays level, finite and under full scale.
#include "test_support.h"

using namespace ts;

struct Stereo { int sr = 0; std::vector<float> l, r; };
static bool readStereo16(const std::string& path, Stereo& s)
{
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    char id[4]; uint32_t sz; f.read(id, 4); f.read((char*) &sz, 4); f.read(id, 4);
    uint16_t ch = 0, bits = 0; uint32_t sr = 0;
    while (f.read(id, 4))
    {
        f.read((char*) &sz, 4);
        if (!std::memcmp(id, "fmt ", 4)) { std::vector<char> b(sz); f.read(b.data(), sz); std::memcpy(&ch, &b[2], 2); std::memcpy(&sr, &b[4], 4); std::memcpy(&bits, &b[14], 2); }
        else if (!std::memcmp(id, "data", 4))
        {
            if (ch != 2 || bits != 16) return false;
            std::vector<char> b(sz); f.read(b.data(), sz); const size_t n = sz / 4; s.sr = (int) sr; s.l.resize(n); s.r.resize(n);
            for (size_t i = 0; i < n; ++i) { int16_t q[2]; std::memcpy(q, &b[i * 4], 4); s.l[i] = q[0] / 32768.f; s.r[i] = q[1] / 32768.f; }
            return true;
        }
        else f.seekg(sz + (sz & 1), std::ios::cur);
    }
    return false;
}
// the time (s) the Schroeder decay curve takes to fall from -5 dB to -25 dB, times three: a broadband RT60
static double rt60(const std::vector<float>& x, int sr)
{
    std::vector<double> e(x.size()); double acc = 0;
    for (size_t i = x.size(); i-- > 0;) { acc += (double) x[i] * x[i]; e[i] = acc; }
    size_t i5 = 0, i25 = 0;
    for (size_t i = 0; i < e.size(); ++i) { const double d = 10 * std::log10(e[i] / e[0] + 1e-18); if (!i5 && d < -5) i5 = i; if (!i25 && d < -25) { i25 = i; break; } }
    return (double) (i25 - i5) / sr * 3.0;
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    struct Room { const char* file; double minSeconds, maxSeconds, minRt, maxRt; };
    const Room rooms[] = { { "room_cathedral", 5.0, 8.0, 4.0, 7.5 }, { "room_chapel", 2.0, 4.0, 1.4, 2.6 }, { "room_small_hall", 1.5, 3.0, .8, 1.6 } };
    double prev = 1e9; Stereo loaded[3]; int index = 0;
    for (const auto& room : rooms)
    {
        Stereo& s = loaded[index++];
        CHECK(readStereo16(root + "/Assets/Rooms/" + room.file + ".wav", s), "cannot read Assets/Rooms/%s.wav (16-bit stereo expected)", room.file);
        const double seconds = (double) s.l.size() / s.sr;
        CHECK(s.sr == 44100 && seconds >= room.minSeconds && seconds <= room.maxSeconds, "%s: %.1f s at %d Hz is not what the room should be", room.file, seconds, s.sr);
        double peak = 0, dc = 0; for (size_t i = 0; i < s.l.size(); ++i) { peak = std::max({ peak, (double) std::fabs(s.l[i]), (double) std::fabs(s.r[i]) }); dc += s.l[i]; }
        CHECK(peak > .5 && peak < .99, "%s: peak %.2f (expected about 0.8)", room.file, peak);
        CHECK(std::fabs(dc / (double) s.l.size()) < .001, "%s has a DC offset", room.file);
        const double rt = rt60(s.l, s.sr);
        // the tail is quiet at the end and the file does not end on a click
        double tail = 0; const size_t last = s.l.size() / 20; for (size_t i = s.l.size() - last; i < s.l.size(); ++i) tail += (double) s.l[i] * s.l[i];
        const double tailDb = dB(std::sqrt(tail / (double) last)) - dB(peak);
        // the two channels are different noise: a wide, decorrelated room
        double a = 0, b = 0, c = 0; for (size_t i = 0; i < s.l.size(); ++i) { a += (double) s.l[i] * s.r[i]; b += (double) s.l[i] * s.l[i]; c += (double) s.r[i] * s.r[i]; }
        const double corr = a / std::sqrt(b * c + 1e-30);
        std::printf("[room] %-16s %.1f s, broadband RT60 %.2f s, tail %.0f dB under the peak, channel correlation %.2f\n", room.file, seconds, rt, tailDb, corr);
        CHECK(rt >= room.minRt && rt <= room.maxRt, "%s: RT60 %.2f s outside %.1f..%.1f s", room.file, rt, room.minRt, room.maxRt);
        CHECK(rt < prev, "the rooms must get shorter: cathedral, chapel, small hall");
        CHECK(tailDb < -45.0, "%s: the end of the file is only %.0f dB under the peak", room.file, tailDb);
        CHECK(std::fabs(corr) < .3, "%s: the channels are too alike (correlation %.2f)", room.file, corr);
        prev = rt;
    }

    // the engine takes each one: finite, under full scale, and about as loud as the algorithmic hall it replaces
    Audio a; CHECK(readWav16(root + "/tests/data/vocal_raw_9s.wav", a), "cannot read the fixture"); a.x.resize((size_t) (6 * a.sr));
    const auto m = voicedMask(a.x, a.sr);
    auto p = pluginDefaults(); p.hall = .4f;
    const double noIr = dB(rms(render(a.x, a.sr, p), m));
    for (int i = 0; i < 3; ++i)
    {
        choir::StagedChoir sc; sc.prepare(a.sr); sc.setParameters(p);
        sc.setImpulseResponse(loaded[i].l.data(), loaded[i].r.data(), (int) loaded[i].l.size(), (double) loaded[i].sr);
        CHECK(sc.hasImpulseResponse(), "%s was not accepted by the engine", rooms[i].file);
        std::vector<float> l(a.x.size()), r(a.x.size()); sc.process(a.x.data(), a.x.data(), l.data(), r.data(), (int) a.x.size());
        double pk = 0, s2 = 0; size_t n = 0;
        for (size_t k = 0; k < l.size(); ++k) { CHECK(std::isfinite(l[k]) && std::isfinite(r[k]), "%s: non-finite sample", rooms[i].file); pk = std::max({ pk, (double) std::fabs(l[k]), (double) std::fabs(r[k]) }); if (m[k]) { s2 += .5 * ((double) l[k] * l[k] + (double) r[k] * r[k]); ++n; } }
        const double withIr = dB(std::sqrt(s2 / (double) n));
        std::printf("[room] %-16s in the engine: output %+.1f dB against the algorithmic hall, peak %.3f\n", rooms[i].file, withIr - noIr, pk);
        CHECK(pk <= 1.0, "%s: peak %.3f over full scale", rooms[i].file, pk);
        CHECK(std::fabs(withIr - noIr) < 3.0, "%s changes the loudness by %+.1f dB against the built-in hall (limit 3 dB)", rooms[i].file, withIr - noIr);
    }
    std::printf("ALL ROOM TESTS PASSED\n");
}
