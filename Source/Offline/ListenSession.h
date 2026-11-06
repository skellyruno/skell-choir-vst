#pragma once
// v0.8: LISTEN, the Melodyne-like mode. You play a take through the plugin once while it listens; it analyses what you sang into
// notes, voices the choir for each note in the KEY and SCALE you chose, and from then on the choir sings that score in time with the
// host's play position, instead of guessing the harmony live. Because the whole take is known the harmony notes are stable per note:
// they do not chase vibrato or tracker glitches. The notes can be edited (a semitone at a time) before you play the take back.
// JUCE-free so it can be tested natively (tests/listen_tests.cpp).
#include "FullTakeScan.h"
#include "../Scales.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace choir {

// Voice every note of a score for a chosen key and scale. With a scale the sections land on scale notes (diatonic); without one the
// fixed intervals of the live mode are used (harmony 1 = minor third). Intervals are folded to within an octave, as in the live modes.
inline void voiceScoreInKey(FullTakeScore& score, int keyRoot, int scale, int harmony)
{
    score.root = keyRoot; score.minor = harmony == 1;
    for (auto& e : score.notes)
    {
        std::array<int,4> interval = kFixedIntervals;
        if (harmony == 2) interval = { 0, 0, 0, 0 };
        else if (clampScale(scale) > 0) interval = harmonyIntervals(e.melody, keyRoot, scale);
        else if (harmony == 1) interval[1] = 3;
        for (int part = 0; part < 4; ++part)
        {
            int iv = interval[(size_t) part];
            while (iv > 12) iv -= 12;
            while (iv < -12) iv += 12;
            e.satb[(size_t) part] = std::clamp(e.melody + iv, 24, 100);
        }
    }
}

// The index of the note sounding at time t (seconds into the take), or -1. A short gap between two notes of the same phrase (80 ms)
// still counts as the note before it, so the voicing does not flicker through the consonants.
inline int findNoteIndex(const FullTakeScore& score, double t, double hold = .08)
{
    const auto& n = score.notes;
    const auto it = std::upper_bound(n.begin(), n.end(), t, [](double v, const NoteEvent& e) { return v < e.start; });
    if (it == n.begin()) return -1;
    const auto prev = it - 1;
    return t <= prev->end + hold ? (int) (prev - n.begin()) : -1;
}

inline FullTakeScore analyseTake(const float* mono, size_t count, double sampleRate, int keyRoot, int scale, int harmony)
{
    FullTakeScanner scanner; scanner.prepare(sampleRate);
    for (size_t i = 0; i < count; ++i) scanner.push(mono[i]);
    auto score = scanner.finish(harmony == 1);
    voiceScoreInKey(score, keyRoot, scale, harmony);     // the key is the one you chose: nothing is detected
    return score;
}

// Drives the choir from a score: called once per host block on the audio thread. t is the position in the take in seconds, or < 0 if
// unknown (then the choir just follows the live tracker, as in the other modes).
class ListenPlayback
{
public:
    void reset() { lastIndex = -2; lastScore = nullptr; active = false; }
    void update(ChoirEffect& fx, const FullTakeScore* score, double t)
    {
        if (score == nullptr || t < 0.0) { release(fx); return; }
        const int index = findNoteIndex(*score, t);
        if (index == lastIndex && score == lastScore) return;
        lastIndex = index; lastScore = score;
        if (index < 0) { release(fx); lastIndex = -1; lastScore = score; return; }
        const auto& e = score->notes[(size_t) index];
        fx.setScoreVoicing(e.melody, e.satb); active = true;
    }
    int currentNote() const { return lastIndex; }
private:
    void release(ChoirEffect& fx) { if (active) { fx.clearScoreVoicing(); active = false; } lastIndex = -2; lastScore = nullptr; }
    int lastIndex = -2; const FullTakeScore* lastScore = nullptr; bool active = false;
};

// One listening pass: capture on the audio thread, analyse on a worker, publish an immutable score the audio thread reads without locks.
class ListenSession
{
public:
    enum State { Idle = 0, Listening, Analysing, Ready };
    ListenSession() = default;
    ~ListenSession() { join(); }
    ListenSession(const ListenSession&) = delete; ListenSession& operator=(const ListenSession&) = delete;

    // ---- message thread
    bool start(double sampleRate, double maxSeconds)
    {
        if (state() == Analysing) return false;
        join();
        state_.store(Idle, std::memory_order_release);
        waitForAudioThread();                                                // a block may still be mid-write: never reallocate under it
        sr = sampleRate; capacity = (size_t) (sampleRate * maxSeconds);
        buffer.assign(capacity, 0.f);                                        // allocated here, never on the audio thread
        written.store(0); full.store(false); originSet.store(false); origin = 0; originKnown = false; edited = false;
        current.store(nullptr, std::memory_order_release);                   // a new pass replaces the old score at once
        state_.store(Listening, std::memory_order_release);
        return true;
    }
    void stop(int keyRoot, int scale, int harmony)
    {
        if (state() != Listening) return;
        state_.store(Analysing, std::memory_order_release);                  // the audio thread stops feeding as soon as it sees this
        waitForAudioThread();                                                // and the block it may be in finishes before we read the buffer
        join();
        const size_t n = written.load(std::memory_order_acquire);
        worker = std::thread([this, n, keyRoot, scale, harmony]
        {
            publish(std::make_unique<FullTakeScore>(analyseTake(buffer.data(), n, sr, keyRoot, scale, harmony)));
            state_.store(Ready, std::memory_order_release);
        });
    }
    void clear() { join(); state_.store(Idle, std::memory_order_release); current.store(nullptr, std::memory_order_release); edited = false; }
    // Edits and re-voicing publish a new immutable copy; the audio thread picks it up on its next block.
    bool transpose(size_t note, int section, int semitones)
    {
        if (state() != Ready) return false;
        std::lock_guard<std::mutex> guard(mutex);
        const auto* now = current.load(std::memory_order_acquire); if (now == nullptr) return false;
        auto copy = std::make_unique<FullTakeScore>(*now);
        if (!transposeNote(*copy, note, section, semitones)) return false;
        edited = true; publishLocked(std::move(copy)); return true;
    }
    bool revoice(int keyRoot, int scale, int harmony)      // false if you have edited notes by hand: your edits are not overwritten
    {
        if (state() != Ready || edited) return false;
        std::lock_guard<std::mutex> guard(mutex);
        const auto* now = current.load(std::memory_order_acquire); if (now == nullptr) return false;
        auto copy = std::make_unique<FullTakeScore>(*now); voiceScoreInKey(*copy, keyRoot, scale, harmony);
        publishLocked(std::move(copy)); return true;
    }
    State state() const { return (State) state_.load(std::memory_order_acquire); }
    double capturedSeconds() const { return sr > 0 ? (double) written.load(std::memory_order_acquire) / sr : 0.0; }
    bool isFull() const { return full.load(std::memory_order_acquire); }
    bool edit() const { return edited; }

    // ---- audio thread
    void feed(const float* mono, int n, double hostSeconds, bool hostKnown)
    {
        if (state_.load(std::memory_order_acquire) != Listening) return;
        feeding.fetch_add(1, std::memory_order_acq_rel);
        if (state_.load(std::memory_order_acquire) != Listening) { feeding.fetch_sub(1, std::memory_order_acq_rel); return; }   // stopped while we were entering
        if (!originSet.load(std::memory_order_relaxed)) { origin = hostSeconds; originKnown = hostKnown; originSet.store(true, std::memory_order_release); }
        const size_t w = written.load(std::memory_order_relaxed), room = capacity - w, k = std::min((size_t) n, room);
        if (k > 0) std::memcpy(buffer.data() + w, mono, k * sizeof(float));
        written.store(w + k, std::memory_order_release);
        if (k < (size_t) n) full.store(true, std::memory_order_release);
        feeding.fetch_sub(1, std::memory_order_acq_rel);
    }
    const FullTakeScore* score() const { return current.load(std::memory_order_acquire); }
    // Where the take began on the host's timeline; < 0 if the host never told us the position
    double originSeconds() const { return originSet.load(std::memory_order_acquire) && originKnown ? origin : -1.0; }
    bool hostPositionKnown() const { return originSet.load(std::memory_order_acquire) && originKnown; }

private:
    void join() { if (worker.joinable()) worker.join(); }
    void waitForAudioThread() { while (feeding.load(std::memory_order_acquire) != 0) std::this_thread::yield(); }   // at most one block long
    void publish(std::unique_ptr<FullTakeScore> s) { std::lock_guard<std::mutex> guard(mutex); publishLocked(std::move(s)); }
    void publishLocked(std::unique_ptr<FullTakeScore> s)
    {
        archive.push_back(std::move(s));
        if (archive.size() > 48) archive.erase(archive.begin());           // the audio thread only ever holds the newest, so old ones are safe to free
        current.store(archive.back().get(), std::memory_order_release);
    }
    std::vector<float> buffer; size_t capacity = 0; double sr = 0;
    std::atomic<size_t> written { 0 }; std::atomic<bool> full { false }; std::atomic<int> feeding { 0 };
    std::atomic<int> state_ { Idle }; std::atomic<bool> originSet { false }; double origin = 0; bool originKnown = false;
    std::atomic<const FullTakeScore*> current { nullptr };
    std::vector<std::unique_ptr<FullTakeScore>> archive; std::mutex mutex; std::thread worker; bool edited = false;
};

} // namespace choir
