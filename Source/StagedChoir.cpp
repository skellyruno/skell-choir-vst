#include "StagedChoir.h"
#include <algorithm>
#include <cmath>

namespace choir {

namespace {
// where each section stands by default (the v0.5 parameter defaults), and how its four singers are spread around it
constexpr double kHomeX[sections] = { -5.1, -1.7, 1.7, 5.1 };
constexpr double kHomeY[sections] = { 6.0, 5.2, 5.2, 6.0 };
constexpr double kSpreadX[4] = { -0.45, 0.45, -0.45, 0.45 };
constexpr double kSpreadY[4] = { -0.40, -0.40, 0.40, 0.40 };

}

StagedChoir::~StagedChoir()
{
    collectGarbage();
    delete pendingIr.exchange(nullptr);
}

void StagedChoir::prepare(double sampleRate)
{
    sr = sampleRate;
    fx.prepare(sampleRate);

    oc::StageConfig cfg;
    cfg.sampleRate = sampleRate; cfg.engineBlock = block;
    cfg.room.size = 0.52; cfg.room.seconds = 2.4;
    std::vector<oc::PerformerSpec> all;
    for (int s = 0; s < sections; ++s)
        for (int j = 0; j < ChoirEffect::voicesPerSection; ++j)
        {
            oc::PerformerSpec ps;
            ps.section = s; ps.x = kHomeX[s] + kSpreadX[j]; ps.y = kHomeY[s] + kSpreadY[j]; ps.z = 1.6 + (j >= 2 ? 0.25 : 0.0);
            all.push_back(ps);
        }
    {
        oc::StageConfig c = cfg; c.performers = all; std::vector<oc::Source> sources;
        for (int i = 0; i < (int) all.size(); ++i) sources.push_back({ all[(size_t) i].section, { i } });
        stage = std::make_unique<oc::ChoirStage>(c, sources);
    }
    for (int s = 0; s < sections; ++s)    // each section alone, for the stems
    {
        oc::StageConfig c = cfg; c.performers.clear(); std::vector<oc::Source> sources;
        for (int j = 0; j < ChoirEffect::voicesPerSection; ++j) { sources.push_back({ s, { j } }); c.performers.push_back(all[(size_t) (s * ChoirEffect::voicesPerSection + j)]); }
        sectionStage[s] = std::make_unique<oc::ChoirStage>(c, sources);
    }
    lead = std::make_unique<oc::LeadStage>(sampleRate, block);
    hall = std::make_unique<oc::Hall>(sampleRate, block);

    voiceBuf.assign((size_t) N, std::vector<float>((size_t) block, 0.f));
    voicePtr.clear();
    for (auto& v : voiceBuf) voicePtr.push_back(v.data());
    for (int s = 0; s < sections; ++s)
    {
        sectionPtr[s].clear();
        for (int j = 0; j < ChoirEffect::voicesPerSection; ++j) sectionPtr[s].push_back(voiceBuf[(size_t) (s * ChoirEffect::voicesPerSection + j)].data());
    }
    reset();
    {   // keep the loaded IR across a sample-rate change
        std::lock_guard<std::mutex> g(irMutex);
        if (!irLeft.empty()) { rebuildImpulseResponse(); adoptPendingImpulseResponse(); }
    }
    setParameters(p);
}

void StagedChoir::reset()
{
    fx.reset();
    if (stage) stage->reset();
    for (auto& s : sectionStage) if (s) s->reset();
    if (hall) hall->reset();
    std::fill(std::begin(outL_), std::end(outL_), 0.f); std::fill(std::begin(outR_), std::end(outR_), 0.f);
    std::fill(std::begin(classicL_), std::end(classicL_), 0.f); std::fill(std::begin(classicR_), std::end(classicR_), 0.f);
    for (int i = 0; i < numStems; ++i) { std::fill(std::begin(stemOutL_[i]), std::end(stemOutL_[i]), 0.f); std::fill(std::begin(stemOutR_[i]), std::end(stemOutR_[i]), 0.f); }
    for (auto& v : voiceBuf) std::fill(v.begin(), v.end(), 0.f);
    pos = 0; wetNow = 0; leadGainNow = p.leadGain; lastClassic = p.classicStage; trimNow = trimGoal();
    corr = 1.f; wid = 0.f; peakL = peakR = 0.f;
}

void StagedChoir::applyTo(oc::ChoirStage& s, bool all, int onlySection)
{
    for (int sec = 0; sec < sections; ++sec)
    {
        if (!all && sec != onlySection) continue;
        s.setSectionGain(sec, p.gain[(size_t) sec] / kSectionUnity);
        s.setSectionOffset(sec, p.x[(size_t) sec] - kHomeX[sec], p.y[(size_t) sec] - kHomeY[sec]);
    }
    s.setMainArray(p.micMode);
    s.setMainSpacing(p.width);
    s.setListener(p.micX, p.micY);
    oc::RoomSettings room;
    room.size = p.roomSize; room.absorption = p.absorption; room.diffusion = p.diffusion; room.seconds = p.roomSeconds;
    room.earlyLevel = 0.7; room.lateLevel = 0.6;
    s.setRoom(room);
    s.setReverb(p.wet / kWetUnity);
}

float StagedChoir::trimGoal() const
{
    const float db = std::max(-kChoirLevelMaxDb, std::min(kChoirLevelMaxDb, p.choirLevelDb));
    return kVoiceTrim * std::pow(10.f, db / 20.f);
}

void StagedChoir::setParameters(const FXParams& params)
{
    p = params;
    if (p.classicStage) fx.setParameters(p); else fx.setVoiceParameters(p);   // the classic stage needs the v0.5 room and pan set-up too
    if (!stage) return;
    applyTo(*stage, true, 0);
    for (int s = 0; s < sections; ++s) applyTo(*sectionStage[s], false, s);
    lead->setOffset(p.leadX, p.leadY - 0.4);   // v0.5's lead Y is absolute (home 0.4 m); the stage wants the offset from home
    hall->set(p.hallSeconds, p.hall);
}

// ------------------------------------------------------------------------------------- impulse response hall
void StagedChoir::retire(oc::ConvPair* c)
{
    if (!c) return;
    for (auto& slot : graveyard) { oc::ConvPair* expected = nullptr; if (slot.compare_exchange_strong(expected, c)) return; }
    // graveyard full (IRs loaded faster than the message thread collects): keep it alive rather than free on the audio thread
}
void StagedChoir::collectGarbage()
{
    for (auto& slot : graveyard) delete slot.exchange(nullptr);
}
void StagedChoir::setImpulseResponse(const float* left, const float* right, int length, double irSampleRate)
{
    collectGarbage();
    std::lock_guard<std::mutex> g(irMutex);
    if (!left || length <= 0 || irSampleRate <= 0)
    {
        irLeft.clear(); irRight.clear();
        delete pendingIr.exchange(nullptr);
        clearIr.store(true);
        return;
    }
    irLeft.assign(left, left + length);
    irRight.assign(right ? right : left, (right ? right : left) + length);
    irRate = irSampleRate;
    rebuildImpulseResponse();
}
void StagedChoir::rebuildImpulseResponse()
{
    if (irLeft.empty()) return;
    std::vector<float> l = oc::resampleCubic(irLeft.data(), (int) irLeft.size(), irRate, sr), r = oc::resampleCubic(irRight.data(), (int) irRight.size(), irRate, sr);
    const int n = std::max(1, std::min((int) l.size(), (int) (oc::Convolver::kMaxSeconds * sr)));
    l.resize((size_t) n); r.resize((size_t) n);
    const int fade = std::min(n / 4, (int) (.005 * sr) + 1);           // 5 ms fade-out: a truncated tail must not click
    for (int i = 0; i < fade; ++i) { const float g = (float) (.5 + .5 * std::cos(3.14159265358979323846 * (i + 1) / fade)); l[(size_t) (n - fade + i)] *= g; r[(size_t) (n - fade + i)] *= g; }
    double e = 0; for (int i = 0; i < n; ++i) e += .5 * ((double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i]);
    const float scale = e > 1e-20 ? (float) std::sqrt(kIrEnergy / e) : 0.f;
    for (int i = 0; i < n; ++i) { l[(size_t) i] *= scale; r[(size_t) i] *= scale; }
    auto* cp = new oc::ConvPair(l.data(), r.data(), n, 1024);
    clearIr.store(false);
    delete pendingIr.exchange(cp);       // an older IR that never reached the audio thread
}
void StagedChoir::adoptPendingImpulseResponse()
{
    if (clearIr.exchange(false)) { auto old = hall->adoptConvolver(nullptr); retire(old.release()); }
    if (auto* c = pendingIr.exchange(nullptr)) { auto old = hall->adoptConvolver(std::unique_ptr<oc::ConvPair>(c)); retire(old.release()); }
}

// ------------------------------------------------------------------------------------------------ the block
void StagedChoir::renderBlock()
{
    adoptPendingImpulseResponse();
    float peakIn = 0.f;
    for (int i = 0; i < block; ++i) peakIn = std::max(peakIn, std::max(std::fabs(inL_[i]), std::fabs(inR_[i])));

    if (p.classicStage)
    {
        // A/B: the v0.5 stage, computed on the input side sample by sample; delivered one block later like everything else
        for (int i = 0; i < block; ++i) { outL_[i] = classicL_[i]; outR_[i] = classicR_[i]; }
        for (int i = 0; i < numStems; ++i) { std::fill(std::begin(stemL_[i]), std::end(stemL_[i]), 0.f); std::fill(std::begin(stemR_[i]), std::end(stemR_[i]), 0.f); }
    }
    else
    {
        if (lastClassic) { stage->reset(); hall->reset(); }        // whatever the idle stage still held must not leak back in
        stage->process(voicePtr.data(), choirL, choirR);
        hall->process(choirL, choirR, choirL, choirR);
        lead->process(inL_, inR_, *stage, leadL, leadR);

        const float targetWet = p.choirOn ? p.mix : 0.f;
        const float g0 = leadGainNow, g1 = p.leadGain;
        for (int i = 0; i < block; ++i)
        {
            wetNow += .002f * (targetWet - wetNow);
            const float dry = 1.f - wetNow;
            const float lg = g0 + (g1 - g0) * (float) (i + 1) / (float) block;
            outL_[i] = (float) oc::safetyCeiling(p.master * (dry * leadL[i] * lg + wetNow * choirL[i]));
            outR_[i] = (float) oc::safetyCeiling(p.master * (dry * leadR[i] * lg + wetNow * choirR[i]));
        }
        leadGainNow = g1;

        if (stemsWanted)
        {
            for (int s = 0; s < sections; ++s)
            {
                if (!lastStems) sectionStage[s]->reset();
                sectionStage[s]->process(sectionPtr[s].data(), scratchL, scratchR);
                for (int i = 0; i < block; ++i) { stemL_[s][i] = (float) oc::safetyCeiling(p.master * scratchL[i]); stemR_[s][i] = (float) oc::safetyCeiling(p.master * scratchR[i]); }
            }
            for (int i = 0; i < block; ++i) { stemL_[4][i] = (float) oc::safetyCeiling(p.master * leadL[i] * p.leadGain); stemR_[4][i] = (float) oc::safetyCeiling(p.master * leadR[i] * p.leadGain); }
        }
    }
    lastClassic = p.classicStage; lastStems = stemsWanted && !p.classicStage;

    // meters
    for (int s = 0; s < sections; ++s)
    {
        double e = 0; for (int j = 0; j < ChoirEffect::voicesPerSection; ++j) for (int i = 0; i < block; ++i) { const double v = voiceBuf[(size_t) (s * ChoirEffect::voicesPerSection + j)][(size_t) i]; e += v * v; }
        const float rmsNow = (float) std::sqrt(e / (block * ChoirEffect::voicesPerSection)) * (p.gain[(size_t) s] / kSectionUnity);   // what the section actually contributes
        meters.section[s].store(std::max(rmsNow, meters.section[s].load(std::memory_order_relaxed) * .92f), std::memory_order_relaxed);
    }
    meters.leadIn.store(std::max(peakIn, meters.leadIn.load(std::memory_order_relaxed) * .92f), std::memory_order_relaxed);
    double ll = 0, rr = 0, lr = 0, mm = 0, ss = 0; float pl = 0, pr = 0;
    for (int i = 0; i < block; ++i)
    {
        const double l = outL_[i], r = outR_[i];
        ll += l * l; rr += r * r; lr += l * r; mm += .25 * (l + r) * (l + r); ss += .25 * (l - r) * (l - r);
        pl = std::max(pl, (float) std::fabs(l)); pr = std::max(pr, (float) std::fabs(r));
    }
    peakL = std::max(pl, peakL * .92f); peakR = std::max(pr, peakR * .92f);
    if (ll + rr > 1e-9)
    {
        corr += .1f * ((float) (lr / std::sqrt(ll * rr + 1e-18)) - corr);
        wid += .1f * ((float) (std::sqrt(ss) / (std::sqrt(mm) + std::sqrt(ss) + 1e-12)) - wid);
    }
    meters.outPeakL.store(peakL, std::memory_order_relaxed); meters.outPeakR.store(peakR, std::memory_order_relaxed);
    meters.correlation.store(corr, std::memory_order_relaxed); meters.width.store(wid, std::memory_order_relaxed);
}

void StagedChoir::process(const float* inL, const float* inR, float* outL, float* outR, int n, const StemOut* stems)
{
    stemsWanted = stems != nullptr;
    float v[N];
    const float trimTarget = trimGoal();
    for (int i = 0; i < n; ++i)
    {
        const float l = inL[i], r = inR ? inR[i] : l;      // read before writing: in and out may be the same array
        inL_[pos] = l; inR_[pos] = r;
        if (p.classicStage)
        {
            float cl = 0, cr = 0; fx.render(l, r, cl, cr); classicL_[pos] = cl; classicR_[pos] = cr;
        }
        else
        {
            fx.processVoices(.5f * (l + r), v);
            trimNow += .0007f * (trimTarget - trimNow);   // ~30 ms: no zipper noise on the CHOIR LEVEL dial
            for (int k = 0; k < N; ++k) voiceBuf[(size_t) k][(size_t) pos] = v[k] * trimNow;
        }
        outL[i] = outL_[pos];
        if (outR) outR[i] = outR_[pos];
        if (stems)
            for (int s = 0; s < numStems; ++s)
            {
                if (stems[s].left) stems[s].left[i] = stemOutL_[s][pos];
                if (stems[s].right) stems[s].right[i] = stemOutR_[s][pos];
            }
        if (++pos == block)
        {
            renderBlock(); pos = 0;
            for (int s = 0; s < numStems; ++s) { std::copy(std::begin(stemL_[s]), std::end(stemL_[s]), std::begin(stemOutL_[s])); std::copy(std::begin(stemR_[s]), std::end(stemR_[s]), std::begin(stemOutR_[s])); }
        }
    }
}

} // namespace choir
