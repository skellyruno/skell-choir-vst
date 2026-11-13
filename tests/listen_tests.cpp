// LISTEN: capture a take, analyse it into notes, voice them in the chosen key, and have the choir sing that score in time with the host.
#include "test_support.h"
#include "../Source/Offline/ListenSession.h"
#include <chrono>
#include <thread>

using namespace ts;
using namespace choir;
static const double PI = 3.14159265358979323846;
static double hz(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }
static bool waitFor(const ListenSession& s, ListenSession::State want, double seconds = 20.0)
{
    for (int i = 0; i < (int) (seconds * 100); ++i) { if (s.state() == want) return true; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    return false;
}

struct Melody { std::vector<float> x; std::vector<int> notes; std::vector<double> starts, ends; };
// a sung melody: each note a steady vowel with 5.5 Hz vibrato of +-35 cents, a gap of 90 ms between notes
static Melody sing(int sr, const std::vector<int>& notes, double noteSeconds = .55, double gapSeconds = .09, double lead = .3)
{
    Melody m; m.notes = notes; double ph = 0;
    auto silence = [&](double s) { for (size_t i = 0; i < (size_t) (s * sr); ++i) m.x.push_back(0.f); };
    silence(lead);
    for (int n : notes)
    {
        const size_t count = (size_t) (noteSeconds * sr); m.starts.push_back((double) m.x.size() / sr);
        for (size_t i = 0; i < count; ++i)
        {
            const double t = (double) i / sr, f = hz(n) * std::pow(2.0, .35 / 12.0 * std::sin(2 * PI * 5.5 * t));
            ph += f / sr; double v = 0; for (int h = 1; h <= 8; ++h) v += std::sin(2 * PI * h * ph) / (h * h);
            const double env = std::min(1.0, std::min((double) i / (.015 * sr), (double) (count - i) / (.02 * sr)));
            m.x.push_back((float) (.16 * env * v));
        }
        m.ends.push_back((double) m.x.size() / sr);
        silence(gapSeconds);
    }
    silence(.4);
    return m;
}
static double bin(const std::vector<float>& x, int sr, size_t from, size_t to, double f)
{
    double best = 0;
    for (double d = -3.0; d <= 3.0; d += .25)
    {
        double re = 0, im = 0; const size_t n = to - from;
        for (size_t i = from; i < to; ++i) { const double w = .5 - .5 * std::cos(2 * PI * (double) (i - from) / (double) n), v = x[i] * w, ph = 2 * PI * (f + d) * (double) i / sr; re += v * std::cos(ph); im += v * std::sin(ph); }
        best = std::max(best, std::sqrt(re * re + im * im) / (double) n);
    }
    return best;
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const int sr = 44100;
    const std::vector<int> tune { 60, 64, 67, 64, 69, 65, 62, 60 };           // C E G E A F D C

    // 1. finding the note at a time -------------------------------------------------------------------------------------------
    {
        FullTakeScore s; NoteEvent a; a.start = 1.0; a.end = 1.5; a.melody = 60; NoteEvent b; b.start = 1.55; b.end = 2.0; b.melody = 64; NoteEvent c; c.start = 3.0; c.end = 3.4; c.melody = 67;
        s.notes = { a, b, c };
        CHECK(findNoteIndex(s, .5) == -1 && findNoteIndex(s, 1.2) == 0 && findNoteIndex(s, 1.52) == 0 && findNoteIndex(s, 1.6) == 1 && findNoteIndex(s, 2.05) == 1 && findNoteIndex(s, 2.5) == -1 && findNoteIndex(s, 3.2) == 2 && findNoteIndex(s, 9.0) == -1, "note lookup");
        std::printf("[1] note lookup, including the 80 ms bridge over short gaps\n");
    }

    // 2. voicing in the key you chose: every section lands in the scale, wherever the melody goes ---------------------------------
    {
        FullTakeScore s; for (int n = 48; n <= 84; ++n) { NoteEvent e; e.start = n; e.end = n + .5; e.melody = n; s.notes.push_back(e); }
        for (int scale = 1; scale < kNumScales; ++scale) for (int root = 0; root < 12; ++root)
        {
            auto v = s; voiceScoreInKey(v, root, scale, 0);
            for (const auto& e : v.notes) for (int p = 0; p < 4; ++p)
            {
                CHECK(inScale(e.satb[(size_t) p], root, scale), "%s in %s: part %d of a %d melody is %d, not in the scale", kScales[scale].name, kKeyNames[root], p, e.melody, e.satb[(size_t) p]);
                CHECK(std::abs(e.satb[(size_t) p] - e.melody) <= 12 + 12, "interval too large");
            }
        }
        auto u = s; voiceScoreInKey(u, 0, 1, 2);
        for (const auto& e : u.notes) for (int p = 0; p < 4; ++p) CHECK(e.satb[(size_t) p] == e.melody, "unison harmony must keep every part on the melody");
        auto f = s; voiceScoreInKey(f, 0, 0, 0); CHECK(f.notes[16].satb[0] == f.notes[16].melody + 7 && f.notes[16].satb[1] == f.notes[16].melody + 4, "without a scale: the fixed intervals");
        auto m = s; voiceScoreInKey(m, 0, 0, 1); CHECK(m.notes[16].satb[1] == m.notes[16].melody + 3, "without a scale, minor harmony: a minor third");
        std::printf("[2] voicing: 7 scales x 12 keys x 37 notes all land in key; unison and chromatic behave\n");
    }

    // 3. analysing a sung melody (with vibrato) into notes ------------------------------------------------------------------------
    FullTakeScore analysed;
    {
        const auto m = sing(sr, tune);
        analysed = analyseTake(m.x.data(), m.x.size(), sr, 0, 1, 0);
        std::printf("[3] %zu notes found for %zu sung; first notes:", analysed.notes.size(), tune.size());
        for (size_t i = 0; i < analysed.notes.size() && i < 4; ++i) std::printf("  %d@%.2fs", analysed.notes[i].melody, analysed.notes[i].start);
        std::printf("\n");
        CHECK(analysed.notes.size() == tune.size(), "found %zu notes, %zu were sung", analysed.notes.size(), tune.size());
        for (size_t i = 0; i < tune.size(); ++i)
        {
            const auto& e = analysed.notes[i];
            CHECK(e.melody == tune[i], "note %zu: found %d, sung %d", i, e.melody, tune[i]);
            CHECK(std::fabs(e.start - m.starts[i]) < .09 && std::fabs(e.end - m.ends[i]) < .12, "note %zu timing: %.2f-%.2f found, %.2f-%.2f sung", i, e.start, e.end, m.starts[i], m.ends[i]);
            for (int p = 0; p < 4; ++p) CHECK(inScale(e.satb[(size_t) p], 0, 1), "note %zu part %d is not in C major", i, p);
        }
        CHECK(analysed.root == 0, "the key is the one you chose (C), not a detected one");
    }

    // 4. the session: capture on the "audio thread", analyse on the worker, edit, re-voice, clear ---------------------------------------
    {
        const auto m = sing(sr, tune); ListenSession session;
        CHECK(session.state() == ListenSession::Idle && session.score() == nullptr, "a new session is idle");
        CHECK(session.start(sr, 60.0), "start");
        const double hostStart = 12.0; const int block = 512; size_t fed = 0;
        for (size_t b = 0; b + block <= m.x.size(); b += block) { session.feed(m.x.data() + b, block, hostStart + (double) b / sr, true); fed += block; }
        CHECK(session.state() == ListenSession::Listening && std::fabs(session.capturedSeconds() - (double) fed / sr) < 1e-6, "capturing");
        CHECK(session.hostPositionKnown() && std::fabs(session.originSeconds() - hostStart) < 1e-9, "the take's origin on the host timeline is remembered");
        session.stop(0, 1, 0);
        CHECK(waitFor(session, ListenSession::Ready), "analysis did not finish");
        const auto* s = session.score(); CHECK(s != nullptr && s->notes.size() == tune.size(), "the score has %zu notes", s ? s->notes.size() : 0);
        // after Analysing the audio thread no longer captures
        const double before = session.capturedSeconds(); session.feed(m.x.data(), block, 99.0, true); CHECK(session.capturedSeconds() == before, "capture must stop after stop()");
        // edits publish a new immutable copy and leave the old one intact
        const auto* old = session.score(); const int oldValue = old->notes[2].satb[1];
        CHECK(session.transpose(2, 1, 1) && session.score() != old && session.score()->notes[2].satb[1] == oldValue + 1 && old->notes[2].satb[1] == oldValue, "an edit changes a copy, not the score the audio thread may be reading");
        CHECK(!session.revoice(0, 2, 0), "re-voicing must not overwrite notes you edited by hand");
        session.clear(); CHECK(session.state() == ListenSession::Idle && session.score() == nullptr, "clear");
        // a second pass replaces the first; re-voicing works while nothing is edited
        CHECK(session.start(sr, 60.0), "start again");
        for (size_t b = 0; b + block <= m.x.size(); b += block) session.feed(m.x.data() + b, block, (double) b / sr, true);
        session.stop(0, 1, 0); CHECK(waitFor(session, ListenSession::Ready), "second analysis");
        CHECK(session.revoice(2, 6, 0), "re-voicing after a key change");
        for (const auto& e : session.score()->notes) for (int p = 0; p < 4; ++p) CHECK(inScale(e.satb[(size_t) p], 2, 6), "re-voiced part is not in D major pentatonic");
        // a host that never reports its position is noticed
        ListenSession blind; blind.start(sr, 5.0); blind.feed(m.x.data(), block, 0.0, false); CHECK(!blind.hostPositionKnown() && blind.originSeconds() < 0, "no host position: say so");
        // a full buffer stops by itself and says so
        ListenSession tiny; tiny.start(sr, .5); for (size_t b = 0; b + block <= m.x.size(); b += block) tiny.feed(m.x.data() + b, block, 0.0, true);
        CHECK(tiny.isFull() && tiny.capturedSeconds() <= .5 + 1e-6, "a full buffer is reported");
        std::printf("[4] session: capture, analyse on a worker, immutable edits, re-voice, clear, no-host-position and full-buffer cases\n");
    }

    // 5. the choir sings the score in time with the host clock, in the key ----------------------------------------------------------
    {
        const auto m = sing(sr, tune);
        auto p = pluginDefaults(); p.mode = 1; p.mix = 1.f; p.leadGain = 0.f; p.levelMatch = false; p.humanize = .2f; p.ensemble = .2f; p.consonants = 0.f; p.keyRoot = 0; p.scale = 1;
        auto runScore = [&](const FullTakeScore* score)
        {
            choir::StagedChoir sc; sc.prepare(sr); sc.setParameters(p); ListenPlayback playback; std::vector<float> l(m.x.size()), r(m.x.size());
            for (size_t b = 0; b < m.x.size(); b += 512)
            {
                const int n = (int) std::min<size_t>(512, m.x.size() - b);
                playback.update(sc.voices(), score, (double) b / sr);        // the host clock: seconds into the take
                sc.process(m.x.data() + b, m.x.data() + b, l.data() + b, r.data() + b, n);
            }
            return l;
        };
        const auto run = [&](const FullTakeScore& score) { return runScore(&score); };
        const auto runNone = [&](bool) { return runScore(nullptr); };
        const auto y = run(analysed);
        // inside the second note (E4, 0.94 to 1.49 s), once the choir has had time to respond and the previous note has died away: its sections should be B, G, B, E
        const size_t from = (size_t) (m.starts[1] * sr + .32 * sr), to = (size_t) (m.ends[1] * sr);   // late enough that the previous note (its soprano also sings G) has left the choir
        double inKey = -1e9, outKey = -1e9;
        for (int n = 40; n <= 80; ++n) { const double e = dB(bin(y, sr, from, to, hz(n))); if (inScale(n, 0, 1)) inKey = std::max(inKey, e); else outKey = std::max(outKey, e); }
        const double g = dB(bin(y, sr, from, to, hz(67))), gSharp = dB(bin(y, sr, from, to, hz(68)));
        std::printf("[5] while the E is sung: G %.1f dB, G# %.1f dB; strongest note in C major %.1f dB, outside it %.1f dB\n", g, gSharp, inKey, outKey);
        CHECK(g > gSharp + 12.0, "the alto should sing G (the key), not G# (%.1f vs %.1f dB)", g, gSharp);
        // (overtones of in-key notes are themselves outside the key: the third harmonic of B is F#, so a spectral probe never sees a perfectly clean key)
        CHECK(inKey - outKey > 8.0, "the choir leaves the key while singing the score (%.1f dB over the notes outside it)", inKey - outKey);
        // The choir must be reading the SCORE, not just following the live tracker in the same key: edit the score so the alto sings G# during the E
        // (a note the live harmony in C major would never choose), and that is what must be heard. Without a score the same audio gives G.
        {
            FullTakeScore edited = analysed; edited.notes[1].satb[1] += 1;     // the alto's G (67) becomes G# (68)
            const auto withEdit = run(edited);
            const double eG = dB(bin(withEdit, sr, from, to, hz(67))), eGSharp = dB(bin(withEdit, sr, from, to, hz(68)));
            const auto live = runNone(false);
            const double lG = dB(bin(live, sr, from, to, hz(67))), lGSharp = dB(bin(live, sr, from, to, hz(68)));
            std::printf("[5] edited score (alto on G#): G %.1f dB, G# %.1f dB; the same audio with no score: G %.1f dB, G# %.1f dB\n", eG, eGSharp, lG, lGSharp);
            // (the granular shifter puts sidebands about 23 Hz either side of every partial, which lands G beside G#, so one bin never reads clean:
            // the evidence is the swing the edit causes, and the sign on each side)
            const double swing = (eGSharp - eG) - (lGSharp - lG);
            std::printf("[5] the edit moves the G# against G balance by %.1f dB\n", swing);
            CHECK(eGSharp > eG + 5.0, "the choir does not sing the edited note: G# %.1f dB against G %.1f dB", eGSharp, eG);
            CHECK(lG > lGSharp + 8.0, "without a score the alto should follow the live key (G), got G %.1f dB against G# %.1f dB", lG, lGSharp);
            CHECK(swing > 15.0, "editing the score should move the harmony (balance moved only %.1f dB)", swing);
        }
        // and when the clock does not match the take (the host says we are 3 s past the end) the choir does not invent notes: it simply follows the live tracker
        choir::StagedChoir sc; sc.prepare(sr); sc.setParameters(p); ListenPlayback playback; playback.update(sc.voices(), &analysed, 99.0);
        CHECK(playback.currentNote() == -1, "far outside the take there must be no note");
        playback.update(sc.voices(), &analysed, -1.0); CHECK(playback.currentNote() == -2, "an unknown position hands control back to the live tracker");
        std::printf("[5] playback lines up with the host clock; with no position it falls back to live\n");
    }

    // 6. real singing: a sensible score from the recorded vocal (raw and tuned agree) -----------------------------------------------------
    {
        FullTakeScore scores[2]; int k = 0;
        for (const char* name : { "vocal_raw_9s.wav", "vocal_autotuned_9s.wav" })
        {
            Audio a; CHECK(readWav16(root + "/tests/data/" + name, a), "cannot read %s", name);
            scores[k] = analyseTake(a.x.data(), a.x.size(), a.sr, 9, 2, 0);                  // A natural minor
            const auto& s = scores[k]; double voiced = 0, covered = 0;
            const auto m = voicedMask(a.x, a.sr); for (char c : m) voiced += c;
            for (const auto& e : s.notes) { CHECK(e.end > e.start + .04 && e.melody >= 40 && e.melody <= 80, "an implausible note %d at %.2f-%.2f", e.melody, e.start, e.end); covered += (e.end - e.start) * a.sr; }
            for (size_t i = 1; i < s.notes.size(); ++i) CHECK(s.notes[i].start >= s.notes[i - 1].start, "notes out of order");
            for (const auto& e : s.notes) for (int p = 0; p < 4; ++p) CHECK(inScale(e.satb[(size_t) p], 9, 2), "real take: part %d of a %d melody is not in A minor", p, e.melody);
            std::printf("[6] %s: %zu notes, covering %.0f%% of the sung time\n", name, s.notes.size(), 100.0 * covered / voiced);
            CHECK(s.notes.size() >= 6, "only %zu notes found in 9 s of singing", s.notes.size());
            CHECK(covered / voiced > .5 && covered / voiced < 1.4, "the notes cover %.0f%% of the sung time", 100.0 * covered / voiced);
            ++k;
        }
        const double ratio = (double) scores[0].notes.size() / (double) std::max<size_t>(1, scores[1].notes.size());
        CHECK(ratio > .7 && ratio < 1.4, "the raw and the tuned take give %zu and %zu notes", scores[0].notes.size(), scores[1].notes.size());
    }
    std::printf("ALL LISTEN TESTS PASSED\n");
}
