#pragma once
// Port of ChoirStage, LeadStage and Hall (choir-stage.js): sources on a stage, mixed as the plugin mixes them.
#include "Spatial.h"
#include "Convolver.h"

namespace oc {

struct PerformerSpec { int section = 0; double x = 0, y = 0, z = 1.6; };

struct StageConfig
{
    double sampleRate = 48000;
    int engineBlock = 128;
    std::vector<PerformerSpec> performers;
    MicSettings mics;
    RoomSettings room;
    double sectionGain[kNumSections] = { 1, 1, 1, 1 };
    double choirGain = 1, reverb = 1, power = 0.5;
    double sizeScale = 1;
    bool freeMovement = false, circle = false;
};

// A default formation: four sections left to right (S A T B), rows of four, standing on risers behind YOU.
inline StageConfig makeDefaultStage(double sampleRate, int singersPerSection)
{
    StageConfig c; c.sampleRate = sampleRate;
    for (int s = 0; s < kNumSections; ++s)
        for (int i = 0; i < singersPerSection; ++i)
        {
            int col = i % 4, row = i / 4;
            PerformerSpec p;
            p.section = s;
            p.x = (s - 1.5) * 3.2 + (col - 1.5) * 0.7 + (row % 2 ? 0.35 : 0.0);
            p.y = 6.0 + row * 0.85;
            p.z = 1.6 + row * 0.25;
            c.performers.push_back(p);
        }
    c.room.size = 0.6; c.room.seconds = 2.0;
    return c;
}

struct Source { int section = 0; std::vector<int> members; };

class ChoirStage
{
public:
    struct Performer { int section; V3 home; double dx = 0, dy = 0; };

    ChoirStage(const StageConfig& cfg, std::vector<Source> srcs)
        : sr(cfg.sampleRate), block(cfg.engineBlock), sources(std::move(srcs)), cfgPower(cfg.power), cfgChoirGain(cfg.choirGain), cfgReverb(cfg.reverb)
    {
        for (auto& p : cfg.performers) performers.push_back({ p.section, v3(p.x, p.y, p.z) });
        spatial = std::make_unique<SpatialRenderer>(sr, block, (int) sources.size());
        mics = cfg.mics; home = cfg.mics.mainPos; room = cfg.room;
        sizeK = cfg.sizeScale; free = cfg.freeMovement; circle = cfg.circle;
        for (int s = 0; s < kNumSections; ++s) sectionGain[s] = cfg.sectionGain[s];
        spatial->setFreeMovement(free);
        choirGainTarget = cfg.choirGain; reverbTarget = cfg.reverb;
        mono.assign((size_t) block, 0.f); dry.assign((size_t) block, 0.0);
        dirL = dirR = roomL = roomR = mono;
        reset();
    }

    void reset()
    {
        spatial->reset();
        stageValid = false; clock = 0;
        for (auto& g : sectionGainSmoothed) g = 0;
        choirGainSmoothed = 0; reverbSmoothed = cfgReverb;
        air = Air();
    }

    V3 placeOf(int i) const
    {
        const auto& p = performers[(size_t) i];
        double x = p.home.x + p.dx * sizeK, y = p.home.y + p.dy * std::sqrt(sizeK);
        if (!free) { x = clampv(x, -22.0, 22.0); y = circle ? clampv(y, -22.0, 45.0) : clampv(y, 0.5, 45.0); }
        return v3(x, y, p.home.z);
    }
    void setOffset(int i, double dx, double dy)
    {
        auto& p = performers[(size_t) i];
        if (p.dx != dx || p.dy != dy) { p.dx = dx; p.dy = dy; stageValid = false; }
    }
    void setSectionOffset(int section, double dx, double dy)
    {
        for (int i = 0; i < (int) performers.size(); ++i) if (performers[(size_t) i].section == section) setOffset(i, dx, dy);
    }
    // YOU - the main microphones - moved by dx, dy metres from where the preset puts them. Moved, everybody is placed
    // as heard from where YOU now stand (from 0.4 m moved, fully from 2 m: each singer heard as a featured voice).
    void setListener(double dx, double dy)
    {
        double x = clampv(home.x + dx, -10.0, 10.0), y = clampv(home.y + dy, -10.0, 12.0);
        if (x == mics.mainPos.x && y == mics.mainPos.y) return;
        mics.mainPos = v3(x, y, home.z);
        double d = std::sqrt((x - home.x) * (x - home.x) + (y - home.y) * (y - home.y));
        listenerMoved = smoothstep((d - 0.4) / 1.6);
        stageValid = false;
    }
    void setRoom(const RoomSettings& r) { room = r; }
    void setMainArray(int type) { mics.mainType = type; }
    void setMainSpacing(double metres) { mics.mainSpacing = metres; }
    void setSectionGain(int s, double g) { sectionGain[s] = g; }
    void setChoirGain(double g) { choirGainTarget = g; }
    void setReverb(double r) { reverbTarget = r; }

    // One engine block (n = engineBlock): input[j] = source j's audio. Writes the choir alone as it goes into the
    // output stage (limited on its own, before the lead is added).
    void process(const float* const* input, float* outL, float* outR)
    {
        const int n = block;
        if (!stageValid) layout();
        spatial->setRoom(room);
        spatial->setMics(mics);
        spatial->setStage(centroids, maxY, maxAbsX);
        std::fill(dry.begin(), dry.end(), 0.0);
        double start[kNumSections];
        for (int s = 0; s < kNumSections; ++s) { start[s] = sectionGainSmoothed[s]; sectionGainSmoothed[s] = sectionGain[s]; }
        for (int j = 0; j < (int) sources.size(); ++j)
        {
            int s = sources[(size_t) j].section;
            const float* x = input[j];
            double g = start[s]; const double step = (sectionGain[s] - start[s]) / n;
            for (int i = 0; i < n; ++i) { mono[(size_t) i] = (float) (x[i] * g); g += step; dry[(size_t) i] += mono[(size_t) i]; }
            spatial->renderSinger(j, mono.data(), n);
        }
        spatial->finishBlock(dirL.data(), dirR.data(), roomL.data(), roomR.data(), n);
        applyChoirAir(n);
        const double cgT = choirGainTarget, rgT = clampv(reverbTarget, 0.0, 1.5);
        double cg = choirGainSmoothed, rg = reverbSmoothed;
        const double cgStep = (cgT - cg) / n, rgStep = (rgT - rg) / n;
        for (int i = 0; i < n; ++i)
        {
            double l = roomL[(size_t) i] * rg + dirL[(size_t) i], r = roomR[(size_t) i] * rg + dirR[(size_t) i];
            outL[i] = (float) softLimit(l * cg); outR[i] = (float) softLimit(r * cg);
            cg += cgStep; rg += rgStep;
        }
        choirGainSmoothed = cgT; reverbSmoothed = rgT;
        clock += n;
    }

    // state the lead stage needs
    MicSettings mics; RoomSettings room; bool free = false;
    V3 centroids[kNumSections]; double maxY = 8, maxAbsX = 6;
    double reverbSmoothed = 1;
    bool hasLayout() const { return stageValid || clock > 0; }
    const std::vector<Performer>& getPerformers() const { return performers; }

private:
    void layout()
    {
        std::vector<V3> places; for (int i = 0; i < (int) performers.size(); ++i) places.push_back(placeOf(i));
        V3 sum[kNumSections]; int count[kNumSections] = { 0, 0, 0, 0 };
        double my = 1, mx = 1;
        for (int i = 0; i < (int) places.size(); ++i)
        {
            int s = performers[(size_t) i].section;
            sum[s] = sum[s] + places[(size_t) i]; count[s]++;
            my = std::max(my, places[(size_t) i].y); mx = std::max(mx, std::fabs(places[(size_t) i].x));
        }
        for (int s = 0; s < kNumSections; ++s) centroids[s] = count[s] ? sum[s] * (1.0 / count[s]) : v3(0, 3, 1.6);
        maxY = my; maxAbsX = mx;
        const bool first = clock == 0;
        for (int j = 0; j < (int) sources.size(); ++j)
        {
            V3 c; double dx = 0, dy = 0;
            const auto& mem = sources[(size_t) j].members;
            for (int i : mem) { c = c + places[(size_t) i]; dx += performers[(size_t) i].dx; dy += performers[(size_t) i].dy; }
            c = c * (1.0 / mem.size()); dx /= mem.size(); dy /= mem.size();
            spatial->setSingerPosition(j, sources[(size_t) j].section, c, normalised(v3(-c.x, -c.y, 0)), first);   // everyone faces the conductor
            double k = sizeK, moved = std::sqrt(dx * dx * k * k + dy * dy * k);
            spatial->setSingerFocus(j, std::max(smoothstep((moved - 0.4) / 1.6), listenerMoved));
        }
        stageValid = true;
    }

    // The choir's top end measured on the singers' dry voices (over seconds) and a gentle shelf above 5 kHz cutting it
    // towards a real choir's balance.
    struct Air
    {
        Biquad voiceHp, voiceLp, highHp, highLp, shelfL, shelfR;
        double voiceE = 0, highE = 0, heardS = 0, cutDb = 0, shelfDb = 1; bool on = false;
    };
    void applyChoirAir(int n)
    {
        auto& a = air;
        if (a.shelfDb > 0.5)
        {
            a.voiceHp.setHighpass(150, 0.707, sr); a.voiceLp.setLowpass(3000, 0.707, sr);
            a.highHp.setHighpass(5000, 0.707, sr); a.highLp.setLowpass(std::min(12000.0, 0.45 * sr), 0.707, sr);
        }
        double ev = 0, eh = 0;
        for (int i = 0; i < n; ++i)
        {
            double m = dry[(size_t) i];
            double v = a.voiceLp.process(a.voiceHp.process(m)), h = a.highLp.process(a.highHp.process(m));
            ev += v * v; eh += h * h;
        }
        a.voiceHp.flush(); a.voiceLp.flush(); a.highHp.flush(); a.highLp.flush();
        if (ev > 1e-6 * n)        // the choir sings (its voice band above -60 dB)
        {
            a.heardS += n / sr;
            double tau = std::min(3.0, 0.3 + a.heardS);
            double k = 1 - std::exp(-n / (tau * sr));
            a.voiceE += k * (ev - a.voiceE); a.highE += k * (eh - a.highE);
        }
        if (a.heardS > 0)
        {
            double balanceDb = 10 * std::log10(std::max(1e-20, a.highE) / std::max(1e-20, a.voiceE));
            double targetDb = -27 + 6 * (clampv(cfgPower, 0.0, 1.0) - 0.5);
            double want = clampv((targetDb - balanceDb) / 0.8, -12.0, 0.0);
            a.cutDb += (1 - std::exp(-n / (0.4 * sr))) * (want - a.cutDb);
        }
        if (std::fabs(a.cutDb - a.shelfDb) > 0.05)
        {
            a.shelfL.setHighShelf(5000, a.cutDb, sr); a.shelfR.setHighShelf(5000, a.cutDb, sr);
            a.shelfDb = a.cutDb;
        }
        a.on = a.cutDb < -0.01;
        if (a.on)   // the plugin shelves each section and the room before the mix adds them: the same filter on that sum
        {
            const double rg = reverbSmoothed;
            for (int i = 0; i < n; ++i)
            {
                double l = a.shelfL.process(dirL[(size_t) i] + rg * roomL[(size_t) i]), r = a.shelfR.process(dirR[(size_t) i] + rg * roomR[(size_t) i]);
                dirL[(size_t) i] = (float) l; dirR[(size_t) i] = (float) r; roomL[(size_t) i] = 0; roomR[(size_t) i] = 0;
            }
            a.shelfL.flush(); a.shelfR.flush();
        }
    }

    double sr; int block;
    std::vector<Source> sources; std::vector<Performer> performers;
    std::unique_ptr<SpatialRenderer> spatial;
    V3 home; double listenerMoved = 0, sizeK = 1; bool circle = false;
    double sectionGain[kNumSections] {}, sectionGainSmoothed[kNumSections] {};
    double cfgPower, cfgChoirGain, cfgReverb;
    double choirGainTarget = 1, reverbTarget = 1, choirGainSmoothed = 0;
    bool stageValid = false; long long clock = 0;
    std::vector<float> mono, dirL, dirR, roomL, roomR; std::vector<double> dry;
    Air air;
};

// ---------------------------------------------------------------- the lead vocal on the stage
// The lead's own place is just behind the main microphones: there it is the dry vocal exactly as it is. Moved, it is
// heard where it stands - through the choir's room and microphones, its first sound still arriving with the dry vocal,
// its reverberation coming from its side of the room - cross-faded from the dry vocal over the first 1.2 m.
constexpr double kLeadHomeY = 0.4, kLeadStageRamp = 1.2, kLeadMinDistance = 1.5;
constexpr double kLeadPlacedLevel = kLeadMinDistance / 3;

class LeadStage
{
public:
    LeadStage(double sampleRate, int blockSize) : block(blockSize), spatial(sampleRate, blockSize, 1)
    {
        spatial.setMinimumDistance(kLeadMinDistance);
        spatial.setRoomKeepsItsLevel(true);
        spatial.setSoloist(true);
        mono.assign((size_t) block, 0.f); dirL = dirR = roomL = roomR = mono;
    }
    void setOffset(double x, double y) { dx = x; dy = y; }
    double amount() const
    {
        double d = std::sqrt(dx * dx + dy * dy);
        if (d < 0.005) return 0;
        double x = d / kLeadStageRamp;
        return x >= 1 ? 1 : x * x * (3 - 2 * x);
    }
    void process(const float* inL, const float* inR, ChoirStage& choir, float* outL, float* outR)
    {
        const int n = block; const double amt = amount();
        if ((amt > 0 || staged) && choir.hasLayout())
        {
            spatial.setRoom(choir.room); spatial.setMics(choir.mics); spatial.setFreeMovement(choir.free);
            spatial.setStage(choir.centroids, choir.maxY, choir.maxAbsX);
            const V3 m = choir.mics.mainPos;
            V3 pos = v3(m.x + dx, m.y + kLeadHomeY + dy, 1.6);
            V3 face = v3(m.x - pos.x, m.y - pos.y, 0);
            face = len(face) > 0.01 ? normalised(face) : v3(0, -1, 0);    // facing the microphones
            spatial.setSingerPosition(0, 0, pos, face, !staged);
            spatial.setSingerFocus(0, 1);              // always a featured voice: heard exactly where it stands
            spatial.setLateBalance(spatial.lean(0));   // and its reverberation comes from its side of the room too
            staged = true;
        }
        const double from = mix, to = amt;
        if (staged)
        {
            for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (inL[i] + inR[i]);
            spatial.renderSinger(0, mono.data(), n);
            spatial.finishBlock(dirL.data(), dirR.data(), roomL.data(), roomR.data(), n);
        }
        const double rg = choir.reverbSmoothed;
        for (int i = 0; i < n; ++i)
        {
            float dl = inL[i], dr = inR[i];
            if (!staged) { outL[i] = dl; outR[i] = dr; continue; }
            double m = from + (to - from) * (i + 1) / n;
            double pl = kLeadPlacedLevel * (dirL[(size_t) i] + roomL[(size_t) i] * rg), pr = kLeadPlacedLevel * (dirR[(size_t) i] + roomR[(size_t) i] * rg);
            outL[i] = (float) (dl + (pl - dl) * m); outR[i] = (float) (dr + (pr - dr) * m);
        }
        mix = to;
        if (staged && to <= 0) { staged = false; spatial.reset(); }   // back at the microphones: dry again
    }
private:
    int block; SpatialRenderer spatial;
    double dx = 0, dy = 0, mix = 0; bool staged = false;
    std::vector<float> mono, dirL, dirR, roomL, roomR;
};

// ---------------------------------------------------------------- an extra hall under the choir
// The plugin's late-field design (8-line FDN), longer and smoother - or, when an impulse response is loaded, a real
// room: a stereo partitioned convolution. Fed only with the choir above 250 Hz (no mud), through a 25 ms pre-delay
// (the convolver's own latency counts towards it), its top softened above 7 kHz.
struct ConvPair
{
    Convolver left, right;
    ConvPair(const float* l, const float* r, int n, int partition) : left(l, n, partition), right(r, n, partition) {}
};

class Hall
{
public:
    Hall(double sampleRate, int blockSize) : sr(sampleRate), block(blockSize), late(sampleRate)
    {
        preLen = (int) std::lround(0.025 * sampleRate);
        preSize = preLen + 2;
        preL.assign((size_t) preSize, 0.f); preR = preL;
        for (int c = 0; c < 2; ++c) { f[c][0].setHighpass(250, 0.707, sr); f[c][1].setHighpass(250, 0.707, sr); f[c][2].setLowpass(7000, 0.707, sr); }
        inL.assign((size_t) block, 0.f); inR = mL = mR = rL = rR = cinL = cinR = coutL = coutR = inL;
    }
    void set(double seconds, double g)
    {
        if (seconds != secs)
        {
            RoomSettings r; r.size = 0.85; r.absorption = 0.35; r.diffusion = 0.8; r.seconds = seconds; r.earlyLevel = 0; r.lateLevel = 1;
            late.configure(r, roomGeometry(r, 8, 6));
            secs = seconds;
        }
        gain = g;
    }
    // audio thread: swap in a convolution (or nullptr to go back to the algorithmic hall); returns what it replaces,
    // which the caller must not destroy on the audio thread
    std::unique_ptr<ConvPair> adoptConvolver(std::unique_ptr<ConvPair> c)
    {
        std::swap(conv, c);
        if (conv) { conv->left.reset(); conv->right.reset(); }
        return c;
    }
    bool usesConvolver() const { return conv != nullptr; }
    void reset()
    {
        late.reset(); std::fill(preL.begin(), preL.end(), 0.f); std::fill(preR.begin(), preR.end(), 0.f); prePos = 0;
        for (auto& row : f) for (auto& q : row) q.z1 = q.z2 = 0;
        if (conv) { conv->left.reset(); conv->right.reset(); }
        gainNow = gain;
    }
    // adds the hall of l / r (the choir) to outL / outR; in and out may be the same arrays
    void process(const float* l, const float* r, float* outL, float* outR)
    {
        const int n = block;
        const int convTap = conv ? std::max(0, preLen - conv->left.latency()) : preLen;
        for (int i = 0; i < n; ++i)
        {
            double a = f[0][2].process(f[0][1].process(f[0][0].process(l[i]))), b = f[1][2].process(f[1][1].process(f[1][0].process(r[i])));
            preL[(size_t) prePos] = (float) a; preR[(size_t) prePos] = (float) b;
            int t = prePos - preLen; if (t < 0) t += preSize;
            inL[(size_t) i] = preL[(size_t) t]; inR[(size_t) i] = preR[(size_t) t];
            int u = prePos - convTap; if (u < 0) u += preSize;
            cinL[(size_t) i] = preL[(size_t) u]; cinR[(size_t) i] = preR[(size_t) u];
            if (++prePos >= preSize) prePos = 0;
        }
        for (auto& row : f) for (auto& q : row) q.flush();
        const double m0 = convMix, target = conv ? 1.0 : 0.0, m1 = m0 + clampv(target - m0, -(double) n / (.1 * sr), (double) n / (.1 * sr));
        const bool needFdn = m0 < 1.0 || m1 < 1.0, needConv = conv && (m0 > 0.0 || m1 > 0.0);
        if (needFdn) late.process(inL.data(), inR.data(), mL.data(), mR.data(), rL.data(), rR.data(), n);
        if (needConv) { conv->left.process(cinL.data(), coutL.data(), n); conv->right.process(cinR.data(), coutR.data(), n); }
        double g0 = gainNow, g1 = gain, step = (g1 - g0) / n;
        for (int i = 0; i < n; ++i)
        {
            const double m = m0 + (m1 - m0) * (i + 1) / n, g = g0 + step * (i + 1);
            double wl = 0, wr = 0;
            if (needFdn) { wl += (1 - m) * (mL[(size_t) i] + rL[(size_t) i]); wr += (1 - m) * (mR[(size_t) i] + rR[(size_t) i]); }
            if (needConv) { wl += m * coutL[(size_t) i]; wr += m * coutR[(size_t) i]; }
            outL[i] += (float) (g * wl); outR[i] += (float) (g * wr);
        }
        convMix = m1; gainNow = g1;
    }
private:
    double sr; int block; LateReverb late; int preLen = 1, preSize = 3, prePos = 0;
    std::vector<float> preL, preR, inL, inR, mL, mR, rL, rR, cinL, cinR, coutL, coutR;
    Biquad f[2][3]; double secs = 0, gain = 0, gainNow = 0, convMix = 0;
    std::unique_ptr<ConvPair> conv;
};

} // namespace oc
