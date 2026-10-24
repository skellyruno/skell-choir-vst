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

void StagedChoir::prepare(double sampleRate)
{
    sr = sampleRate;
    fx.prepare(sampleRate);

    oc::StageConfig cfg;
    cfg.sampleRate = sampleRate; cfg.engineBlock = block;
    cfg.room.size = 0.52; cfg.room.seconds = 2.4;
    std::vector<oc::Source> sources;
    for (int s = 0; s < sections; ++s)
        for (int j = 0; j < ChoirEffect::voicesPerSection; ++j)
        {
            oc::PerformerSpec ps;
            ps.section = s; ps.x = kHomeX[s] + kSpreadX[j]; ps.y = kHomeY[s] + kSpreadY[j]; ps.z = 1.6 + (j >= 2 ? 0.25 : 0.0);
            sources.push_back({ s, { (int) cfg.performers.size() } });
            cfg.performers.push_back(ps);
        }
    stage = std::make_unique<oc::ChoirStage>(cfg, sources);
    lead = std::make_unique<oc::LeadStage>(sampleRate, block);
    hall = std::make_unique<oc::Hall>(sampleRate, block);

    voiceBuf.assign((size_t) N, std::vector<float>((size_t) block, 0.f));
    voicePtr.clear();
    for (auto& v : voiceBuf) voicePtr.push_back(v.data());
    reset();
    setParameters(p);
}

void StagedChoir::reset()
{
    fx.reset();
    if (stage) stage->reset();
    std::fill(std::begin(outL_), std::end(outL_), 0.f); std::fill(std::begin(outR_), std::end(outR_), 0.f);
    for (auto& v : voiceBuf) std::fill(v.begin(), v.end(), 0.f);
    pos = 0; wetNow = 0; leadGainNow = p.leadGain;
}

void StagedChoir::setParameters(const FXParams& params)
{
    p = params;
    fx.setVoiceParameters(p);
    if (!stage) return;

    for (int s = 0; s < sections; ++s)
    {
        stage->setSectionGain(s, p.gain[(size_t) s] / kSectionUnity);
        stage->setSectionOffset(s, p.x[(size_t) s] - kHomeX[s], p.y[(size_t) s] - kHomeY[s]);
    }
    stage->setMainArray(p.micMode);
    stage->setMainSpacing(p.width);
    stage->setListener(p.micX, p.micY);

    oc::RoomSettings room;
    room.size = p.roomSize; room.absorption = p.absorption; room.diffusion = p.diffusion; room.seconds = p.roomSeconds;
    room.earlyLevel = 0.7; room.lateLevel = 0.6;
    stage->setRoom(room);
    stage->setReverb(p.wet / kWetUnity);

    lead->setOffset(p.leadX, p.leadY - 0.4);   // v0.5's lead Y is absolute (home 0.4 m); the stage wants the offset from home
    hall->set(p.hallSeconds, p.hall);
}

void StagedChoir::renderBlock()
{
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
}

void StagedChoir::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    float v[N];
    for (int i = 0; i < n; ++i)
    {
        const float l = inL[i], r = inR ? inR[i] : l;      // read before writing: in and out may be the same array
        inL_[pos] = l; inR_[pos] = r;
        fx.processVoices(.5f * (l + r), v);
        for (int k = 0; k < N; ++k) voiceBuf[(size_t) k][(size_t) pos] = v[k] * kVoiceTrim;
        outL[i] = outL_[pos];
        if (outR) outR[i] = outR_[pos];
        if (++pos == block) { renderBlock(); pos = 0; }
    }
}

} // namespace choir
