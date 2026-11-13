// A listening pack: one vocal through every feature, as WAV files, so a person can LISTEN and say what sounds wrong.
//   render_listening_pack <vocal.wav> [vocal-with-consonants.wav] <output folder>
// The first file is used for the style, room, key, Listen and MIDI demos; the optional second one (a take with clear s/t/k sounds and some dynamics)
// for the consonant, swell and pulse demos (it defaults to the first). Both may be 16-bit or float WAV, mono or stereo. Everything is rendered
// by the same engine the plugin uses, with the plugin's own defaults (AUTO LEVEL on). Nothing is normalised, so the levels are comparable.
// Build: scripts/build_listening_pack.sh
#include "../tests/test_support.h"
#include "../Source/StylePresets.h"
#include "../Source/Offline/ListenSession.h"
#include <functional>
#include <iomanip>
#include <map>

using namespace ts;
using namespace choir;

struct Mono { int sr = 44100; std::vector<float> x; };
static bool readAny(const std::string& path, Mono& m)
{
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    char id[4]; uint32_t sz; f.read(id, 4); f.read((char*) &sz, 4); f.read(id, 4);
    uint16_t fmt = 1, ch = 1, bits = 16; uint32_t sr = 44100;
    while (f.read(id, 4))
    {
        f.read((char*) &sz, 4);
        if (!std::memcmp(id, "fmt ", 4)) { std::vector<char> b(sz); f.read(b.data(), sz); std::memcpy(&fmt, &b[0], 2); std::memcpy(&ch, &b[2], 2); std::memcpy(&sr, &b[4], 4); std::memcpy(&bits, &b[14], 2); }
        else if (!std::memcmp(id, "data", 4))
        {
            std::vector<char> b(sz); f.read(b.data(), sz); const size_t frame = (size_t) (bits / 8) * ch, n = sz / frame; m.sr = (int) sr; m.x.assign(n, 0.f);
            for (size_t i = 0; i < n; ++i)
            {
                double acc = 0;
                for (int c = 0; c < ch; ++c)
                {
                    const char* at = &b[i * frame + (size_t) c * (bits / 8)];
                    if (fmt == 3 && bits == 32) { float v; std::memcpy(&v, at, 4); acc += v; }
                    else if (bits == 16) { int16_t q; std::memcpy(&q, at, 2); acc += q / 32768.0; }
                    else if (bits == 24) { int32_t q = (int32_t) ((uint8_t) at[0] << 8 | (uint8_t) at[1] << 16 | (uint8_t) at[2] << 24) >> 8; acc += q / 8388608.0; }
                    else return false;
                }
                m.x[i] = (float) (acc / ch);
            }
            return true;
        }
        else f.seekg(sz + (sz & 1), std::ios::cur);
    }
    return false;
}
static void writeStereo16(const std::string& path, int sr, const std::vector<float>& l, const std::vector<float>& r)
{
    std::ofstream o(path, std::ios::binary); const uint32_t n = (uint32_t) l.size(), db = n * 4; uint32_t v; uint16_t u;
    o.write("RIFF", 4); v = 36 + db; o.write((char*) &v, 4); o.write("WAVEfmt ", 8); v = 16; o.write((char*) &v, 4); u = 1; o.write((char*) &u, 2); u = 2; o.write((char*) &u, 2);
    v = (uint32_t) sr; o.write((char*) &v, 4); v = (uint32_t) sr * 4; o.write((char*) &v, 4); u = 4; o.write((char*) &u, 2); u = 16; o.write((char*) &u, 2); o.write("data", 4); o.write((char*) &db, 4);
    for (size_t i = 0; i < n; ++i) { int16_t q[2] = { (int16_t) std::lround(std::max(-1.f, std::min(1.f, l[i])) * 32767), (int16_t) std::lround(std::max(-1.f, std::min(1.f, r[i])) * 32767) }; o.write((char*) q, 4); }
}
static Mono crop(const Mono& in, double from, double seconds)
{
    Mono m; m.sr = in.sr; const size_t a = std::min(in.x.size(), (size_t) (from * in.sr)), b = std::min(in.x.size(), a + (size_t) (seconds * in.sr));
    m.x.assign(in.x.begin() + (std::ptrdiff_t) a, in.x.begin() + (std::ptrdiff_t) b);
    const size_t fade = (size_t) (.01 * in.sr); for (size_t i = 0; i < fade && i < m.x.size(); ++i) { m.x[i] *= (float) i / fade; m.x[m.x.size() - 1 - i] *= (float) i / fade; }
    return m;
}

struct Stereo { int sr = 44100; std::vector<float> l, r; };   // an impulse response
struct Result { std::vector<float> l, r; };
using Hook = std::function<void(StagedChoir&, FXParams&, double secondsIntoTake)>;
static Result renderBlocks(const Mono& in, FXParams p, const Hook& hook = nullptr, const Stereo* ir = nullptr)
{
    StagedChoir sc; sc.prepare(in.sr); sc.setParameters(p);
    if (ir != nullptr) sc.setImpulseResponse(ir->l.data(), ir->r.data(), (int) ir->l.size(), (double) ir->sr);
    Result o; o.l.assign(in.x.size(), 0.f); o.r.assign(in.x.size(), 0.f);
    for (size_t b = 0; b < in.x.size(); b += 512)
    {
        const int n = (int) std::min<size_t>(512, in.x.size() - b);
        if (hook) { hook(sc, p, (double) b / in.sr); sc.setParameters(p); }
        sc.process(in.x.data() + b, in.x.data() + b, o.l.data() + b, o.r.data() + b, n);
    }
    return o;
}

int main(int argc, char** argv)
{
    if (argc < 3) { std::printf("usage: %s <vocal.wav> [vocal-with-consonants.wav] <output folder>\n", argv[0]); return 2; }
    const std::string out = argv[argc - 1];
    Mono a, c;
    if (!readAny(argv[1], a)) { std::printf("cannot read %s\n", argv[1]); return 1; }
    if (argc >= 4) { if (!readAny(argv[2], c)) { std::printf("cannot read %s\n", argv[2]); return 1; } } else c = a;
    const std::string root = argc > 0 ? std::string(argv[0]).substr(0, std::string(argv[0]).find("/tools/") == std::string::npos ? 0 : std::string(argv[0]).find("/tools/")) : ".";
    (void) root;
    a = crop(a, a.x.size() > (size_t) (9 * a.sr) ? 1.0 : 0.0, 8.0); c = crop(c, c.x.size() > (size_t) (9 * c.sr) ? 1.0 : 0.0, 8.0);

    std::string cmd = "mkdir -p \"" + out + "\""; if (std::system(cmd.c_str()) != 0) return 1;
    struct Row { std::string file, what, listen; double rmsDb, peak; };
    std::vector<Row> rows;
    const auto emit = [&](const std::string& file, const std::string& what, const std::string& listen, const Mono& src, const Result& r)
    {
        writeStereo16(out + "/" + file, src.sr, r.l, r.r);
        const auto m = voicedMask(src.x, src.sr); double s = 0, pk = 0; size_t n = 0;
        for (size_t i = 0; i < r.l.size(); ++i) { pk = std::max({ pk, (double) std::fabs(r.l[i]), (double) std::fabs(r.r[i]) }); if (m[i]) { s += .5 * ((double) r.l[i] * r.l[i] + (double) r.r[i] * r.r[i]); ++n; } }
        rows.push_back({ file, what, listen, dB(std::sqrt(s / (double) std::max<size_t>(n, 1))), pk });
        std::printf("%-36s %6.1f dBFS  peak %.3f\n", file.c_str(), rows.back().rmsDb, pk);
    };
    const auto base = pluginDefaults();

    // the reference: your voice with nothing on it
    emit("00_dry_vocal.wav", "Your vocal, untouched.", "The reference. Everything else is this voice plus a choir.", a, Result { a.x, a.x });
    emit("01_default_choir.wav", "The plugin at its defaults (AUTO LEVEL on, CONSONANTS 50%, no key).", "Is it a choir? Is the balance between your voice and the choir right? Is it as loud as the dry vocal?", a, renderBlocks(a, base));

    // styles
    for (int i = 0; i < kNumStylePresets; ++i)
    {
        auto p = base; float size = .5f; applyStyle(kStylePresets[i], p, size); applySize(p, size);
        char name[96]; std::snprintf(name, sizeof name, "02_style_%d_%s.wav", i + 1, kStylePresets[i].name); for (char* q = name; *q; ++q) if (*q == ' ') *q = '_';
        emit(name, std::string("Style: ") + kStylePresets[i].name + ". " + kStylePresets[i].about, "Does it sound like its name? Too big, too small, too wet, too dry, too fake?", a, renderBlocks(a, p));
    }

    // built-in rooms (loaded as the plugin loads them: through the HALL, brought up to 25% when it was off)
    const char* roomFiles[] = { "room_cathedral", "room_chapel", "room_small_hall" }; const char* roomNames[] = { "Cathedral", "Stone Chapel", "Small Hall" };
    for (int i = 0; i < 3; ++i)
    {
        Stereo ir; std::ifstream f(std::string("Assets/Rooms/") + roomFiles[i] + ".wav", std::ios::binary);
        if (!f) { std::printf("(room %s skipped: run this from the project folder so Assets/Rooms is found)\n", roomFiles[i]); continue; }
        // a tiny local reader for the 16-bit stereo room files
        char id[4]; uint32_t sz; f.read(id, 4); f.read((char*) &sz, 4); f.read(id, 4); uint32_t sr = 44100;
        while (f.read(id, 4)) { f.read((char*) &sz, 4); if (!std::memcmp(id, "fmt ", 4)) { std::vector<char> b(sz); f.read(b.data(), sz); std::memcpy(&sr, &b[4], 4); } else if (!std::memcmp(id, "data", 4)) { std::vector<char> b(sz); f.read(b.data(), sz); ir.sr = (int) sr; const size_t n = sz / 4; ir.l.resize(n); ir.r.resize(n); for (size_t k = 0; k < n; ++k) { int16_t q[2]; std::memcpy(q, &b[k * 4], 4); ir.l[k] = q[0] / 32768.f; ir.r[k] = q[1] / 32768.f; } break; } else f.seekg(sz + (sz & 1), std::ios::cur); }
        auto p = base; p.hall = .25f;
        char name[64]; std::snprintf(name, sizeof name, "03_room_%d_%s.wav", i + 1, roomFiles[i] + 5);
        emit(name, std::string("Built-in room: ") + roomNames[i] + " (a synthetic impulse response, HALL 25%).", "Does it sound like the space it is named after? Natural or metallic? Too long a tail?", a, renderBlocks(a, p, nullptr, &ir));
    }

    // AUTO LEVEL
    { auto p = base; p.levelMatch = false; emit("04_auto_level_off.wav", "Same as 01, with AUTO LEVEL off (the v0.6 level behaviour).", "Compare the loudness with 01 and with the dry vocal: which sounds right?", a, renderBlocks(a, p)); }

    // the key: pick the scale that fits the notes you sang best (for THIS demo only: the plugin never detects a key, you choose it)
    int bestRoot = 0, bestScale = 1; double bestFit = -1;
    {
        const auto score = analyseTake(a.x.data(), a.x.size(), a.sr, 0, 0, 0); double total = 0; for (const auto& e : score.notes) total += e.end - e.start;
        for (int scale : { 1, 2 }) for (int r = 0; r < 12; ++r) { double fit = 0; for (const auto& e : score.notes) if (inScale(e.melody, r, scale)) fit += e.end - e.start; fit /= std::max(total, 1e-9); if (fit > bestFit + 1e-9) { bestFit = fit; bestRoot = r; bestScale = scale; } }
        std::printf("(demo key: the notes sung fit %s %s best, %.0f%% of the sung time in key)\n", kKeyNames[bestRoot], kScales[bestScale].name, 100 * bestFit);
    }
    const std::string keyLabel = std::string(kKeyNames[bestRoot]) + " " + kScales[bestScale].name;
    { auto p = base; p.keyRoot = bestRoot; p.scale = bestScale;
      emit("05_key_" + std::string(kKeyNames[bestRoot]) + "_" + (bestScale == 1 ? "major" : "minor") + ".wav", "KEY " + keyLabel + " (the best fit to what you sang, chosen for this demo): the harmony stays in key.", "Compare with 01: do the harmony notes now sound right with your melody? Any note that still clashes?", a, renderBlocks(a, p)); }

    // LISTEN: analyse the whole take, voice it in the key, and have the choir sing that score
    {
        auto p = base; p.keyRoot = bestRoot; p.scale = bestScale; p.mode = 1;
        const auto score = analyseTake(a.x.data(), a.x.size(), a.sr, bestRoot, bestScale, 0); ListenPlayback playback;
        emit("06_listen_mode.wav", "LISTEN: the whole take was analysed into " + std::to_string(score.notes.size()) + " notes and the choir sings that score in " + keyLabel + ".",
             "Compare with 05: are the harmony notes steadier (no wobble, no flipping)? Do they land on the right notes at the right moments?", a,
             renderBlocks(a, p, [&](StagedChoir& sc, FXParams&, double t) { playback.update(sc.voices(), &score, t); }));
    }

    // MIDI chord mode: C, A minor, F, G over the sung line
    {
        auto p = base; p.mode = 2; const std::array<std::array<int,4>,4> chords {{ {{ 72, 67, 64, 60 }}, {{ 69, 64, 60, 57 }}, {{ 72, 69, 65, 60 }}, {{ 71, 67, 62, 55 }} }};
        emit("07_midi_chord_mode.wav", "MIDI CHORD: the choir sings C, A minor, F, G (two seconds each) from your voice, whatever you sing.", "Does the choir hold each chord clearly? Does it still sound like a choir made from your voice?", a,
             renderBlocks(a, p, [&](StagedChoir&, FXParams& q, double t) { q.midiNotes = chords[(size_t) std::min(3, (int) (t / 2.0))]; }));
    }

    // consonants, swell, pulse: on the take that has them
    { auto p = base; p.consonants = 0.f; emit("08_consonants_off.wav", "CONSONANTS 0% (the choir sings everything, including your s, t and k).", "Listen to the s, t and k sounds: smeared?", c, renderBlocks(c, p)); }
    { auto p = base; p.consonants = 1.f; emit("08_consonants_100.wav", "CONSONANTS 100% (your own consonants pass through cleanly).", "The same s, t and k sounds: clearer? Too sharp, too hissy, or just right? Do the vowels still sound like a choir?", c, renderBlocks(c, p)); }
    { auto p = base; p.swell = 1.f; emit("09_swell_100.wav", "SWELL 100%: the choir blooms when you sing loud and recedes when you sing soft.", "Compare with 08_consonants_100's dynamics: does the choir follow your phrasing in a musical way, or does it pump?", c, renderBlocks(c, p)); }
    { auto p = base; p.pulseRate = 4; p.pulseDepth = .6f; p.bpm = 120.0;
      emit("10_pulse_one_beat_120bpm.wav", "PULSE one beat at 120 bpm, depth 60%: the choir breathes twice a second.", "Is it a pleasant breathing, or a tremolo that is too obvious? (It is locked to the host tempo in the plugin.)", c,
           renderBlocks(c, p, [&](StagedChoir&, FXParams& q, double t) { q.ppq = t * 2.0; })); }
    // one section alone
    { auto p = base; for (int s = 0; s < 3; ++s) p.gain[(size_t) s] = 0.f; emit("11_bass_solo.wav", "SOLO on the bass section only.", "Does the bass sound like a bass section: warm, steady, not boomy or thin?", a, renderBlocks(a, p)); }
    { auto p = base; p.gain[3] = 0.f; p.gain[2] = 0.f; p.gain[1] = 0.f; emit("11_soprano_solo.wav", "SOLO on the soprano section only.", "Does the soprano sound like voices rather than a pitch-shifted copy of you?", a, renderBlocks(a, p)); }

    // the README
    std::ofstream readme(out + "/README.txt");
    readme << "SKELLYCHOIR LISTENING PACK\n=========================\n\nEvery file is the same vocal (8 seconds" << (argc >= 4 ? " from the first file; 08 to 10 use the second file" : "") << ") through a different setting of the plugin's engine,\n"
              "rendered with the plugin's own defaults. Nothing is normalised, so levels are comparable. Levels below are the average level while you are singing.\n"
              "Listen on headphones first, then on speakers. Start with 00 and 01.\n\nFILES\n";
    for (const auto& r : rows) readme << "\n" << r.file << "   (" << std::fixed << std::setprecision(1) << r.rmsDb << " dBFS while singing, peak " << std::setprecision(2) << r.peak << ")\n    " << r.what << "\n    LISTEN FOR: " << r.listen << "\n";
    readme << "\nWHAT TO TELL ME\n---------------\nFor each file that is not right, one line is enough, for example:\n"
              "  03_room_2: too washy / metallic on the high notes\n  01: the choir is too thin; the lead is too loud\n  06: the harmony is steadier but two notes were wrong (around 3 s)\n"
              "Words that help: too loud, too quiet, too thin, too boomy, too airy, too wet, too dry, robotic, wobbly, harsh, smeared, clicky, wrong note (and when).\n"
              "Also tell me which two or three you liked best.\n\nThe key in 05 and 06 was chosen by looking at the notes sung in this recording, for this demo only. The plugin never detects a key: you choose it.\n";
    std::printf("wrote %zu files and README.txt to %s\n", rows.size(), out.c_str());
}
