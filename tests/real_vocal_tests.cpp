// Real-voice tests. Every other test uses synthetic signals; this one runs a recorded low male lead vocal (raw, and the same take
// after Auto-Tune Pro) through the pitch tracker and the whole choir engine at the plugin's default settings.
// See tests/data/README.md for the fixtures.
#include "test_support.h"

// usage: real_vocal_tests [fixtures directory]   (default: tests/data)

using namespace ts;

struct Track { double lockedShare = 0, medianMidi = 0, plausibleShare = 0; std::vector<float> midi; };

// Feed the tracker sample by sample; read it once per 10 ms
static Track trackPitch(const Audio& a)
{
    choir::PitchTracker t; t.prepare((float) a.sr);
    Track r; const int hop = a.sr / 100, blk = a.sr / 10; size_t active = 0, locked = 0, plausible = 0; std::vector<float> notes;
    for (size_t i = 0; i < a.x.size(); ++i)
    {
        t.push(a.x[i]);
        if ((int) (i % hop) == 0) r.midi.push_back(t.midi());
    }
    for (size_t b = (size_t) a.sr; b + blk <= a.x.size(); b += blk)
    {
        double e = 0; for (int i = 0; i < blk; ++i) e += (double) a.x[b + i] * a.x[b + i];
        if (std::sqrt(e / blk) <= .01) continue;
        ++active;
        const float m = r.midi[(b + blk - 1) / (size_t) hop];
        if (m >= 0) { ++locked; notes.push_back(m); if (m >= 40 && m <= 72) ++plausible; }
    }
    std::sort(notes.begin(), notes.end());
    r.lockedShare = (double) locked / std::max<size_t>(active, 1);
    r.plausibleShare = (double) plausible / std::max<size_t>(locked, 1);
    r.medianMidi = notes.empty() ? 0 : notes[notes.size() / 2];
    return r;
}

int main(int argc, char** argv)
{
    Audio raw, tuned;
    const std::string dir = argc > 1 ? argv[1] : "tests/data";
    CHECK(readWav16(dir + "/vocal_raw_9s.wav", raw), "cannot read %s/vocal_raw_9s.wav", dir.c_str());
    CHECK(readWav16(dir + "/vocal_autotuned_9s.wav", tuned), "cannot read %s/vocal_autotuned_9s.wav", dir.c_str());
    CHECK(raw.sr == 44100 && tuned.sr == 44100 && raw.x.size() == tuned.x.size(), "fixtures changed shape");

    // 1. the pitch tracker on a real voice ----------------------------------------------------------------------
    const Track tr = trackPitch(raw), tt = trackPitch(tuned);
    std::printf("[1] tracker, raw take:  locked on %.0f%% of sung frames, median note MIDI %.1f, %.0f%% of locked frames in MIDI 40..72\n", 100 * tr.lockedShare, tr.medianMidi, 100 * tr.plausibleShare);
    std::printf("[1] tracker, tuned take: locked on %.0f%% of sung frames, median note MIDI %.1f, %.0f%% of locked frames in MIDI 40..72\n", 100 * tt.lockedShare, tt.medianMidi, 100 * tt.plausibleShare);
    for (const Track* t : { &tr, &tt })
    {
        CHECK(t->lockedShare > .95, "the tracker lost the voice on %.0f%% of sung frames", 100 * (1 - t->lockedShare));
        CHECK(t->medianMidi > 52 && t->medianMidi < 62, "median note MIDI %.1f is not a low male voice (about A3 expected)", t->medianMidi);
        // KNOWN WEAKNESS (docs/V0_6_CHANGELOG.md, v0.6.1): low-confidence frames (breaths, consonants, note changes) can report a note above
        // 520 Hz: 2% of this excerpt, 7% of the full 30 s take. It does not affect the default Live sound. This guard stops it getting
        // worse; raise the limit when the tracker is improved.
        CHECK(t->plausibleShare > .93, "only %.0f%% of tracked notes are in a plausible range for this voice", 100 * t->plausibleShare);
    }
    {   // the two takes are the same performance: where both are locked they must agree to well under a semitone
        std::vector<float> d; for (size_t i = (size_t) tr.midi.size() / 10; i < tr.midi.size(); ++i) if (tr.midi[i] > 0 && tt.midi[i] > 0) d.push_back(std::fabs(tr.midi[i] - tt.midi[i]));
        std::sort(d.begin(), d.end()); const double med = d.empty() ? 99 : d[d.size() / 2];
        std::printf("[1] raw vs tuned take: median disagreement %.2f semitones\n", med);
        CHECK(med < .6, "tracker disagrees with itself between takes by %.2f semitones", med);
    }

    // 2. the whole engine at the plugin defaults ----------------------------------------------------------------
    const auto p0 = pluginDefaults();
    double vsLead[2], vsInput[2]; int k = 0;
    for (const Audio* a : { &raw, &tuned })
    {
        const auto b = balance(a->x, a->sr, p0);
        vsLead[k] = dB(b.choir) - dB(b.lead); vsInput[k] = dB(b.total) - dB(b.input);
        const auto o = render(a->x, a->sr, p0);
        std::printf("[2] %s: voiced input %.1f dBFS; choir %+.1f dB against the lead; output %+.1f dB against the input; peak %.3f\n", k ? "tuned" : "raw  ", dB(b.input), vsLead[k], vsInput[k], peak(o));
        CHECK(vsLead[k] > 0.0 && vsLead[k] < 9.0, "choir against lead %+.1f dB (want 0..+9)", vsLead[k]);
        CHECK(vsInput[k] > -9.0, "output is %.1f dB below the input (limit -9 dB)", vsInput[k]);
        CHECK(peak(o) <= 1.0, "peak %.3f over full scale", peak(o));
        for (size_t i = 0; i < o.l.size(); ++i) CHECK(std::isfinite(o.l[i]) && std::isfinite(o.r[i]), "non-finite output sample %zu", i);
        ++k;
    }
    CHECK(std::fabs(vsLead[0] - vsLead[1]) < 1.0 && std::fabs(vsInput[0] - vsInput[1]) < 1.0, "the choir treats the raw and the tuned take differently (%.2f / %.2f dB)", vsLead[0] - vsLead[1], vsInput[0] - vsInput[1]);

    // 3. every section is audible on a real voice ---------------------------------------------------------------
    {
        const auto m = voicedMask(raw.x, raw.sr); double lv[4], mean = 0;
        for (int s = 0; s < 4; ++s)
        {
            auto p = p0; p.mix = 1.f; p.leadGain = 0.f; for (int o = 0; o < 4; ++o) if (o != s) p.gain[(size_t) o] = 0.f;
            lv[s] = dB(rms(render(raw.x, raw.sr, p), m)); mean += lv[s] / 4;
        }
        std::printf("[3] sections soprano/alto/tenor/bass %+.1f / %+.1f / %+.1f / %+.1f dB around their mean\n", lv[0] - mean, lv[1] - mean, lv[2] - mean, lv[3] - mean);
        for (int s = 0; s < 4; ++s) CHECK(std::fabs(lv[s] - mean) < 3.0, "section %d is %+.1f dB from the others", s, lv[s] - mean);
    }

    std::printf("ALL REAL-VOCAL TESTS PASSED\n");
}
