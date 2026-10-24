#pragma once
// Port of SpatialRenderer (choir-stage.js). Positions are metres: x left(-)/right(+) as seen from the listener (YOU,
// the main microphones), y away from the listener, z height. Nothing here is a pan knob: where a singer stands decides
// when and how loud they reach each microphone, how dark they sound, which walls reflect them and how much of the room
// they excite. Mono sources only (one stream per singer).
#include "Mics.h"
#include "Room.h"

namespace oc {

constexpr int kNumSections = 4;
constexpr double kReferenceDistance = 3;     // a singer 3 m from an omni capsule arrives at unity gain
constexpr double kFocusMainShare = 0.6;      // a fully featured singer: the main pair carries 40 %, the follow spot the rest
constexpr double kFocusSpotGain = 0.95;
constexpr double kFocusWidthDeg = 55;
constexpr double kFocusCloseShare = 0.5;
constexpr double kFocusRoomLean = 0.45;
constexpr double kLateGain = 1.6;
constexpr int kImages = 6;
constexpr int kMaxCapsules = 3;
constexpr int kDirectTaps = kMaxCapsules + kNumSections + 2 + 1;   // main capsules + 4 spots + room pair + follow spot
constexpr int kFocusTap = kDirectTaps - 1;
constexpr int kReflectionTaps = kImages * 4;                        // 6 images x (main L/R + room L/R)

struct Tap
{
    int delay = 0, prevDelay = 0, delayR = 0, prevDelayR = 0;
    double gainL = 0, gainR = 0, prevGainL = 0, prevGainR = 0;
    double lpCoef = 1, lpState = 0;
    bool valid = false;
};

struct Singer
{
    bool active = false, initialised = false, dirty = true, fading = false;
    double fade = 1;
    int section = 0;
    double focus = 0, lean = 0;
    V3 pos, target, facing { 0, -1, 0 };
    Tap direct[kDirectTaps], reflection[kReflectionTaps];
    double erLpCoef = 1, erLpState = 0;
};

struct StereoRing
{
    std::vector<float> l, r;
    void init(size_t n) { l.assign(n, 0.f); r.assign(n, 0.f); }
    void clear() { std::fill(l.begin(), l.end(), 0.f); std::fill(r.begin(), r.end(), 0.f); }
};

class SpatialRenderer
{
public:
    SpatialRenderer(double sampleRate, int maxBlockSize, int numSources)
        : sr(sampleRate), maxBlock(std::max(16, maxBlockSize)), late(sampleRate)
    {
        ringSize = sampleRate > 60000 ? 65536 : 32768;
        ringMask = ringSize - 1;
        for (auto* r : { &ringMain, &ringClose, &ringRoom, &erMain, &erRoom }) r->init((size_t) ringSize);
        scratch.assign((size_t) maxBlock, 0.f); erFiltered.assign((size_t) maxBlock, 0.f);
        tmpL.assign((size_t) maxBlock, 0.f); tmpR = tmpL;
        lateMainL = lateMainR = lateRoomL = lateRoomR = tmpL;
        for (int s = 0; s < kNumSections; ++s) centroids[s] = v3((s - 1.5) * 2, 4, 1.6);
        singers.resize((size_t) numSources);
        rebuildArrays();
        reset();
    }

    void reset()
    {
        for (auto* r : { &ringMain, &ringClose, &ringRoom, &erMain, &erRoom }) r->clear();
        for (auto& s : singers)
        {
            s.initialised = false; s.dirty = true; s.erLpState = 0; s.fading = false; s.fade = 1;
            for (auto& t : s.direct) t.lpState = 0;
        }
        late.reset();
        writePos = 0;
    }

    void setRoom(const RoomSettings& r)
    {
        if (r == room && !geometryDirty) return;
        room = r; geometryDirty = true;
    }
    void setMics(const MicSettings& m)
    {
        const auto& o = mics;
        bool sameGeometry = m.mainType == o.mainType && m.mainPos.x == o.mainPos.x && m.mainPos.y == o.mainPos.y && m.mainPos.z == o.mainPos.z
            && m.mainSpacing == o.mainSpacing && m.roomPos.x == o.roomPos.x && m.roomPos.y == o.roomPos.y && m.roomPos.z == o.roomPos.z
            && m.roomSpacing == o.roomSpacing && m.closeDistance == o.closeDistance && m.closeHeight == o.closeHeight;
        if (m.mainLevel != o.mainLevel || m.closeLevel != o.closeLevel) for (auto& s : singers) s.dirty = true;
        mics = m;
        if (!sameGeometry) geometryDirty = true;
    }
    void setStage(const V3* cents, double maxY, double maxAbsX)
    {
        bool changed = std::fabs(maxY - choirMaxY) > 0.05 || std::fabs(maxAbsX - choirMaxAbsX) > 0.05;
        for (int s = 0; s < kNumSections; ++s)
        {
            if (len(cents[s] - centroids[s]) > 0.02) changed = true;
            centroids[s] = cents[s];
        }
        choirMaxY = maxY; choirMaxAbsX = maxAbsX;
        if (changed) geometryDirty = true;
        if (geometryDirty) rebuildArrays();
    }

    // snap = true jumps without smoothing (first placement). Otherwise the source walks there: about 2.5 m/s, a 0.1 s
    // cross-fade per 0.25 m step, the arrival gliding to its new time.
    void setSingerPosition(int i, int section, V3 position, V3 facing, bool snap)
    {
        auto& s = singers[(size_t) i];
        s.section = section; s.target = position; s.facing = facing;
        if (snap || !s.initialised || !s.active) { s.pos = position; s.initialised = false; }
        s.active = true;
    }
    void setMinimumDistance(double m) { minDistance = m; }
    void setLateBalance(double b) { lateBalance = clampv(b, -1.0, 1.0); }
    void setRoomKeepsItsLevel(bool on) { roomKeepsLevel = on; }
    void setSoloist(bool on) { soloist = on; }
    double lean(int i) const { return singers[(size_t) i].lean; }
    void setFreeMovement(bool on) { freeMovement = on; }
    void setSingerFocus(int i, double focus)
    {
        auto& s = singers[(size_t) i];
        focus = clampv(focus, 0.0, 1.0);
        if (focus != s.focus) { s.focus = focus; s.dirty = true; }
    }

    // adds one source's dry block (n <= maxBlock) to the microphones
    void renderSinger(int i, const float* mono, int n)
    {
        auto& s = singers[(size_t) i];
        if (!s.active) return;
        double fadeFrom = 1, fadeTo = 1;
        if (!s.initialised)
        {
            computeTaps(s); computeTaps(s);      // twice: previous snapshot == current snapshot
            s.initialised = true; s.fading = false; s.fade = 1;
        }
        else
        {
            if (!s.fading)
            {
                V3 delta = s.target - s.pos;
                double dist = len(delta);
                if (dist > 0.005 || s.dirty)
                {
                    double maxStep = freeMovement ? 1e9 : 0.25;      // about 2.5 m/s
                    s.pos = dist <= maxStep ? s.target : s.pos + delta * (maxStep / dist);
                    computeTaps(s);
                    s.fading = true; s.fade = 0;
                }
            }
            if (s.fading)
            {
                fadeFrom = s.fade;
                fadeTo = std::min(1.0, s.fade + n / (0.1 * sr));
                s.fade = fadeTo;
            }
        }
        const bool fading = s.fading;
        {   // a silent source whose filters have settled adds nothing anywhere: skip the writes
            bool silent = s.erLpState == 0;
            for (int k = 0; k < n && silent; ++k) silent = mono[k] == 0.f;
            for (int k = 0; k < kDirectTaps && silent; ++k) silent = s.direct[k].lpState == 0;
            if (silent) { if (fading && fadeTo >= 1) s.fading = false; return; }
        }
        int k = 0;
        for (int c = 0; c < kMaxCapsules; ++c) writeTap(ringMain, s.direct[k++], mono, n, true, fading, fadeFrom, fadeTo);
        for (int sec = 0; sec < kNumSections; ++sec) writeTap(ringClose, s.direct[k++], mono, n, true, fading, fadeFrom, fadeTo);
        for (int c = 0; c < 2; ++c) writeTap(ringRoom, s.direct[k++], mono, n, true, fading, fadeFrom, fadeTo);
        writeTap(ringMain, s.direct[kFocusTap], mono, n, true, fading, fadeFrom, fadeTo);
        {   // reflections share one wall-absorption filtered copy
            double z = s.erLpState; const double a = s.erLpCoef;
            for (int q = 0; q < n; ++q) { z += a * (mono[q] - z); erFiltered[(size_t) q] = (float) z; }
            s.erLpState = flushd(z);
        }
        int r = 0;
        for (int im = 0; im < kImages; ++im)
            for (int c = 0; c < 4; ++c)
                writeTap(c < 2 ? erMain : erRoom, s.reflection[r++], erFiltered.data(), n, false, fading, fadeFrom, fadeTo);
        if (fading && fadeTo >= 1) s.fading = false;
    }

    // finishes the block: dirL/R = the choir as the main and spot microphones hear it, roomOutL/R = room microphones,
    // early reflections and the late field
    void finishBlock(float* dirL, float* dirR, float* roomOutL, float* roomOutR, int n)
    {
        auto pull = [&](StereoRing& ring, float* l, float* r, double gain, bool addTo)
        {
            int pos = writePos, done = 0;
            while (done < n)
            {
                int run = std::min(n - done, ringSize - pos);
                float* sl = ring.l.data(); float* sr_ = ring.r.data();
                if (addTo) for (int i = 0; i < run; ++i) { l[done + i] += (float) (sl[pos + i] * gain); r[done + i] += (float) (sr_[pos + i] * gain); }
                else       for (int i = 0; i < run; ++i) { l[done + i] = (float) (sl[pos + i] * gain); r[done + i] = (float) (sr_[pos + i] * gain); }
                std::fill(sl + pos, sl + pos + run, 0.f); std::fill(sr_ + pos, sr_ + pos + run, 0.f);
                done += run; pos = (pos + run) & ringMask;
            }
        };
        std::fill(roomOutL, roomOutL + n, 0.f); std::fill(roomOutR, roomOutR + n, 0.f);
        pull(ringMain, dirL, dirR, mics.mainLevel, false);
        pull(ringClose, dirL, dirR, mics.closeLevel, true);
        pull(ringRoom, roomOutL, roomOutR, mics.roomLevel, true);
        // early reflections at the main and room arrays; together they excite the late field
        float *tL = tmpL.data(), *tR = tmpR.data(), *lmL = lateMainL.data(), *lmR = lateMainR.data(), *lrL = lateRoomL.data(), *lrR = lateRoomR.data();
        pull(erMain, tL, tR, 1, false);
        pull(erRoom, lmL, lmR, 1, false);   // temporarily holds the room-array reflections
        for (int i = 0; i < n; ++i) { lrL[i] = tL[i] + 0.5f * lmL[i]; lrR[i] = tR[i] + 0.5f * lmR[i]; }
        const double early = room.earlyLevel;
        for (int i = 0; i < n; ++i)
        {
            roomOutL[i] += (float) (early * (mics.mainLevel * tL[i] + mics.roomLevel * lmL[i]));
            roomOutR[i] += (float) (early * (mics.mainLevel * tR[i] + mics.roomLevel * lmR[i]));
        }
        // late field (excitation in lateRoomL/R; processed into the four buffers)
        std::copy(lrL, lrL + n, tL); std::copy(lrR, lrR + n, tR);
        late.process(tL, tR, lmL, lmR, lrL, lrR, n);
        const auto& mc = mainArray.capsules;
        double mainDiffuse = 0.5 * (kDiffuse[mc[0].pattern] + kDiffuse[mc[1].pattern]);
        double lm = room.lateLevel * kLateGain * mics.mainLevel * 0.6 * mainDiffuse, lr = room.lateLevel * kLateGain * mics.roomLevel;
        double bl = 1, br = 1;
        if (lateBalance != 0) leanGains(lateBalance, bl, br);
        for (int i = 0; i < n; ++i)
        {
            roomOutL[i] += (float) (bl * (lm * lmL[i] + lr * lrL[i]));
            roomOutR[i] += (float) (br * (lm * lmR[i] + lr * lrR[i]));
        }
        writePos = (writePos + n) & ringMask;
    }

private:
    void rebuildArrays()
    {
        mainArray = buildArray(mics.mainType, mics.mainPos, 0, mics.mainSpacing);
        roomArray = buildArray(AB, mics.roomPos, 0, mics.roomSpacing);
        // spot microphones: a cardioid in front of each section, aimed at it, placed in the stereo image where the
        // section stands as seen from the main microphones (YOU)
        for (int s = 0; s < kNumSections; ++s)
        {
            V3 c = centroids[s];
            V3 toConductor = normalised(v3(-c.x, -c.y, 0));
            Capsule spot;
            spot.pos = v3(c.x + toConductor.x * mics.closeDistance, c.y + toConductor.y * mics.closeDistance, mics.closeHeight);
            spot.dir = normalised(c - spot.pos);
            spot.pattern = Cardioid;
            double p = clampv((c.x - mics.mainPos.x) / std::max(2.0, choirMaxAbsX), -1.0, 1.0);
            spot.gainL = std::cos((p + 1) * kPi * 0.25); spot.gainR = std::sin((p + 1) * kPi * 0.25);
            spots[s] = spot;
        }
        RoomGeometry g = roomGeometry(room, choirMaxY, choirMaxAbsX);
        // the microphones stand inside the room too
        g.yMin = std::min(g.yMin, std::min(mics.roomPos.y - 1, mics.mainPos.y - 1));
        g.xMax = std::max(g.xMax, std::max(std::fabs(mics.roomPos.x) + mics.roomSpacing * 0.5 + 0.5, std::fabs(mics.mainPos.x) + 1.5)); g.xMin = -g.xMax;
        g.zMax = std::max(g.zMax, std::max(mics.mainPos.z + 0.5, mics.roomPos.z + 0.5));
        geo = g;
        late.configure(room, geo);
        for (auto& s : singers) s.dirty = true;
        geometryDirty = false;
    }

    void directTap(Tap& t, const Singer& s, const Capsule& cap, double levelScale)
    {
        V3 d = cap.pos - s.pos;
        double dist = std::max(len(d), minDistance);
        V3 toMic = d * (1 / dist);
        double frontness = 0.5 + 0.5 * dot(s.facing, toMic);         // 1 = the singer faces the microphone
        double cosTheta = dot(cap.dir, toMic * -1);                   // angle of arrival at the capsule
        double gain = patternGain(cap, cosTheta) * (kReferenceDistance / dist) * (0.7 + 0.3 * frontness) * levelScale;
        // air absorption (deliberately a little stronger than physics: it is the main tonal distance cue)
        double cutoff = clampv(20000 / (1 + dist / 9), 2500.0, 20000.0) * (0.35 + 0.65 * frontness);
        if (cap.headShadow) cutoff *= 0.25 + 0.75 * (0.5 + 0.5 * cosTheta);
        cutoff = clampv(cutoff, 800.0, sr * 0.45);
        t.delay = t.delayR = std::min(lroundJs(dist / kSpeedOfSound * sr), ringSize / 2 - maxBlock - 4);
        t.gainL = gain * cap.gainL; t.gainR = gain * cap.gainR;
        t.lpCoef = 1 - std::exp(-kTwoPi * cutoff / sr);
        t.valid = true;
    }

    void computeTaps(Singer& s)
    {
        // the taps computed last time become the "previous snapshot" of the cross-fade
        for (auto& t : s.direct) { t.prevDelay = t.delay; t.prevDelayR = t.delayR; t.prevGainL = t.gainL; t.prevGainR = t.gainR; }
        for (auto& t : s.reflection) { t.prevDelay = t.delay; t.prevDelayR = t.delayR; t.prevGainL = t.gainL; t.prevGainR = t.gainR; }
        const MicArray& main = mainArray; const MicArray& roomArr = roomArray; const RoomGeometry& g = geo;
        int k = 0;
        for (int c = 0; c < kMaxCapsules; ++c)
        {
            Tap& t = s.direct[k++];
            if (c < main.numCapsules) directTap(t, s, main.capsules[c], 1);
            else { t.gainL = t.gainR = 0; t.delay = t.prevDelay; t.delayR = t.prevDelayR; t.valid = true; }   // unused capsule
        }
        // which main capsule feeds each channel most: everything else this source sends to the stems arrives in that
        // channel with it (time-aligned, as engineers align spot microphones to the main pair)
        int cL = 0, cR = 0; double bestL = -1, bestR = -1; int firstDelay = 1 << 30;
        for (int c = 0; c < main.numCapsules; ++c)
        {
            const Tap& t = s.direct[c];
            firstDelay = std::min(firstDelay, t.delay);
            if (std::fabs(t.gainL) > bestL) { bestL = std::fabs(t.gainL); cL = c; }
            if (std::fabs(t.gainR) > bestR) { bestR = std::fabs(t.gainR); cR = c; }
        }
        const int alignL = soloist ? firstDelay : s.direct[cL].delay, alignR = soloist ? firstDelay : s.direct[cR].delay;
        {   // a featured source: a spot follows them, panned where they stand as seen from the main microphones
            Tap& spot = s.direct[kFocusTap];
            if (s.focus > 0)
            {
                double meanGain = 0;
                for (int c = 0; c < main.numCapsules; ++c) meanGain += std::fabs(s.direct[c].gainL) + std::fabs(s.direct[c].gainR);
                meanGain /= main.numCapsules;
                double keep = 1 - kFocusMainShare * s.focus;
                double lp = 0.5 * (s.direct[cL].lpCoef + s.direct[cR].lpCoef);
                for (int c = 0; c < main.numCapsules; ++c)
                {
                    Tap& t = s.direct[c]; t.gainL *= keep; t.gainR *= keep;
                    if (soloist) { t.delay = t.delayR = firstDelay; t.lpCoef = lp; }
                }
                double azimuth = std::atan2(s.pos.x - main.position.x, std::max(0.05, std::fabs(s.pos.y - main.position.y)));
                double pan = clampv(azimuth / (kFocusWidthDeg * kPi / 180), -1.0, 1.0);
                double gg = kFocusSpotGain * s.focus * meanGain;
                spot.gainL = gg * std::cos((pan + 1) * kPi * 0.25); spot.gainR = gg * std::sin((pan + 1) * kPi * 0.25);
                spot.delay = alignL; spot.delayR = alignR; spot.lpCoef = lp; spot.valid = true;
                s.lean = pan * kFocusRoomLean * s.focus;
            }
            else { spot.gainL = spot.gainR = 0; spot.delay = spot.prevDelay; spot.delayR = spot.prevDelayR; spot.valid = true; s.lean = 0; }
        }
        // CLOSE spots (every spot hears every source; other sections arrive as bleed), time-aligned to the main pair
        for (int sec = 0; sec < kNumSections; ++sec)
        {
            Tap& t = s.direct[k++];
            directTap(t, s, spots[sec], 1);
            t.delay = alignL; t.delayR = alignR;
        }
        if (s.focus > 0)
            for (int sec = 0; sec < kNumSections; ++sec)
            {
                Tap& t = s.direct[kMaxCapsules + sec];
                double keep = soloist ? 0 : 1 - kFocusCloseShare * s.focus;
                t.gainL *= keep; t.gainR *= keep;
            }
        // aligned, the spots add to the main pair in step: each channel's spots are scaled so the source's direct sound
        // keeps the power it had when they arrived at other times
        for (int ch = 0; ch < 2 && !soloist; ++ch)
        {
            const Tap& mt = s.direct[ch == 0 ? cL : cR];
            double m = mics.mainLevel * std::fabs(ch == 0 ? mt.gainL : mt.gainR);
            double sum = 0, sum2 = 0;
            for (int sec = 0; sec < kNumSections; ++sec)
            {
                const Tap& t = s.direct[kMaxCapsules + sec];
                double gg = mics.closeLevel * std::fabs(ch == 0 ? t.gainL : t.gainR);
                sum += gg; sum2 += gg * gg;
            }
            if (sum <= 1e-9) continue;
            double kk = clampv((std::sqrt(m * m + sum2) - m) / sum, 0.0, 1.0);
            for (int sec = 0; sec < kNumSections; ++sec)
            {
                Tap& t = s.direct[kMaxCapsules + sec];
                if (ch == 0) t.gainL *= kk; else t.gainR *= kk;
            }
        }
        // ROOM pair
        for (int c = 0; c < 2; ++c) directTap(s.direct[k++], s, roomArr.capsules[c], 1);
        if (s.lean != 0)
        {
            double bl, br; leanGains(s.lean, bl, br);
            for (int c = 0; c < 2; ++c) { Tap& t = s.direct[k - 2 + c]; t.gainL *= bl; t.gainR *= br; }
        }
        // ---- first-order image sources of the shoebox room
        const V3 p = s.pos;
        const V3 images[kImages] = {
            v3(2 * g.xMin - p.x, p.y, p.z), v3(2 * g.xMax - p.x, p.y, p.z),
            v3(p.x, 2 * g.yMin - p.y, p.z), v3(p.x, 2 * g.yMax - p.y, p.z),
            v3(p.x, p.y, -p.z), v3(p.x, p.y, 2 * g.zMax - p.z) };
        const Capsule* caps[4] = { &main.capsules[0], &main.capsules[1], &roomArr.capsules[0], &roomArr.capsules[1] };
        int r = 0;
        for (int im = 0; im < kImages; ++im)
        {
            V3 toWall = normalised(images[im] - p);
            double radiate = 0.6 + 0.4 * (0.5 + 0.5 * dot(s.facing, toWall));
            double wall = g.reflection * (im == 4 ? 0.6 : 1) * radiate;   // floor: risers and people absorb
            for (int c = 0; c < 4; ++c)
            {
                Tap& t = s.reflection[r++];
                V3 d = caps[c]->pos - images[im];
                double dimg = std::max(len(d), 1.0);
                double cosTheta = dot(caps[c]->dir, d * (-1 / dimg));
                double gg = wall * patternGain(*caps[c], cosTheta) * kReferenceDistance / dimg;
                t.delay = t.delayR = std::min(lroundJs(dimg / kSpeedOfSound * sr), ringSize / 2 - maxBlock - 4);
                t.gainL = gg * caps[c]->gainL; t.gainR = gg * caps[c]->gainR;
                t.valid = true;
            }
        }
        if (s.lean != 0)
        {
            double bl, br; leanGains(s.lean, bl, br);
            for (auto& t : s.reflection) { t.gainL *= bl; t.gainR *= br; }
        }
        if (roomKeepsLevel)
        {
            double kk = clampv(std::sqrt(len(main.position - s.pos) / kReferenceDistance), 1.0, 3.0);
            for (auto& t : s.reflection) { t.gainL *= kk; t.gainR *= kk; }
        }
        double erCutoff = lerp(11000, 2500, clampv(room.absorption, 0.0, 1.0));
        s.erLpCoef = 1 - std::exp(-kTwoPi * std::min(erCutoff, sr * 0.45) / sr);
        if (soloist)
        {   // a soloist's first sound arrives with the dry vocal: everything it sends, earlier by the main pair's travel time
            int ref = 1 << 30;
            for (int c = 0; c < main.numCapsules; ++c) ref = std::min(ref, s.direct[c].delay);
            auto shift = [&](Tap& t) { t.delay = std::max(0, t.delay - ref); t.delayR = std::max(0, t.delayR - ref); };
            for (int c = 0; c < main.numCapsules; ++c) shift(s.direct[c]);
            for (int q = kMaxCapsules; q < kMaxCapsules + kNumSections + 2; ++q) shift(s.direct[q]);
            if (s.focus > 0) shift(s.direct[kFocusTap]);
            for (auto& t : s.reflection) shift(t);
        }
        s.dirty = false;
    }

    // adds v[i] * (gain ramp) into the ring starting at 'start'
    void addRamp(std::vector<float>& ring, int start, const float* v, int n, double g, double dg)
    {
        if (g == 0 && dg == 0) return;
        int pos = start & ringMask, done = 0;
        while (done < n)
        {
            int run = std::min(n - done, ringMask + 1 - pos);
            if (dg == 0) for (int i = 0; i < run; ++i) ring[(size_t) (pos + i)] += (float) (v[done + i] * g);
            else         for (int i = 0; i < run; ++i) ring[(size_t) (pos + i)] += (float) (v[done + i] * (g + dg * (done + i)));
            done += run; pos = 0;
        }
    }
    // sample i arrives d0 + dd * i samples after it is sung, split between the two slots around that time
    void addGlide(std::vector<float>& ring, double d0, double dd, const float* v, int n, double g0, double dg)
    {
        for (int i = 0; i < n; ++i)
        {
            double d = d0 + dd * i;
            int whole = (int) d; double frac = d - whole;
            double x = v[i] * (g0 + dg * i);
            int p = writePos + i + whole;
            ring[(size_t) (p & ringMask)] += (float) (x * (1 - frac));
            ring[(size_t) ((p + 1) & ringMask)] += (float) (x * frac);
        }
    }

    void writeTap(StereoRing& ring, Tap& t, const float* x, int n, bool filter, bool fading, double fadeFrom, double fadeTo)
    {
        if (!t.valid) return;
        bool silentNow = t.gainL == 0 && t.gainR == 0, silentBefore = t.prevGainL == 0 && t.prevGainR == 0;
        if (silentNow && (!fading || silentBefore)) return;
        const float* v = x;
        if (filter)
        {
            double z = t.lpState; const double a = t.lpCoef;
            float* f = scratch.data();
            for (int i = 0; i < n; ++i) { z += a * (x[i] - z); f[i] = (float) z; }
            t.lpState = flushd(z);
            v = f;
        }
        if (!fading)
        {
            if (t.gainL != 0) addRamp(ring.l, writePos + t.delay, v, n, t.gainL, 0);
            if (t.gainR != 0) addRamp(ring.r, writePos + t.delayR, v, n, t.gainR, 0);
            return;
        }
        const double inv = (fadeTo - fadeFrom) / n;
        // the path changed a little (a step of a walk or a drag): the arrival glides to the new time, as a moving
        // voice's does. Two arrivals cross-fading instead would comb-filter for the length of the fade.
        const int glideMax = (int) (0.002 * sr);
        if (std::abs(t.delay - t.prevDelay) <= glideMax && std::abs(t.delayR - t.prevDelayR) <= glideMax)
        {
            auto glide = [&](std::vector<float>& r, int d0, int d1, double g0, double g1)
            {
                if (g0 == 0 && g1 == 0) return;
                double dd = d1 - d0;
                addGlide(r, d0 + dd * fadeFrom, dd * inv, v, n, g0 + (g1 - g0) * fadeFrom, (g1 - g0) * inv);
            };
            glide(ring.l, t.prevDelay, t.delay, t.prevGainL, t.gainL);
            glide(ring.r, t.prevDelayR, t.delayR, t.prevGainR, t.gainR);
            return;
        }
        // a jump: cross-fade from the old arrival time / level to the new one (no pitch swoop)
        const int wp = writePos;
        addRamp(ring.l, wp + t.prevDelay, v, n, t.prevGainL * (1 - fadeFrom), -t.prevGainL * inv);
        addRamp(ring.r, wp + t.prevDelayR, v, n, t.prevGainR * (1 - fadeFrom), -t.prevGainR * inv);
        addRamp(ring.l, wp + t.delay, v, n, t.gainL * fadeFrom, t.gainL * inv);
        addRamp(ring.r, wp + t.delayR, v, n, t.gainR * fadeFrom, t.gainR * inv);
    }

    double sr; int maxBlock; int ringSize = 32768, ringMask = 32767;
    StereoRing ringMain, ringClose, ringRoom, erMain, erRoom;
    std::vector<float> scratch, erFiltered, tmpL, tmpR, lateMainL, lateMainR, lateRoomL, lateRoomR;
    LateReverb late;
    RoomSettings room;
    MicSettings mics;
    V3 centroids[kNumSections];
    double choirMaxY = 8, choirMaxAbsX = 6;
    bool geometryDirty = true, freeMovement = false, roomKeepsLevel = false, soloist = false;
    double minDistance = 0.3, lateBalance = 0;
    int writePos = 0;
    MicArray mainArray, roomArray; Capsule spots[kNumSections];
    RoomGeometry geo {};
    std::vector<Singer> singers;
};

} // namespace oc
