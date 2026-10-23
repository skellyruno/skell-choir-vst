#include "ChoirDSP.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace choir {
namespace {
float limit(float x, float a, float b) { return std::max(a, std::min(b, x)); }
float sq(float x) { return x*x; }
float pattern(float angleCos, float omniShare) { return omniShare + (1.0f - omniShare)*angleCos; }
}

void Biquad::setBandpass(float hz, float q, float sr) {
    const float w = 2*pi*limit(hz, 30.f, sr*0.44f)/sr;
    const float alpha = std::sin(w)/(2*std::max(0.2f, q));
    const float a0 = 1.f + alpha;
    b0 = alpha/a0; b1 = 0.f; b2 = -alpha/a0;
    a1 = -2*std::cos(w)/a0; a2 = (1.f-alpha)/a0;
}
float Biquad::process(float x) {
    const float y = b0*x + z1;
    z1 = b1*x - a1*y + z2;
    z2 = b2*x - a2*y;
    return y;
}
void Biquad::reset() { z1 = z2 = 0; }

float Room::Allpass::process(float x) {
    if (samples.empty()) return x;
    const float d = samples[(size_t)write];
    const float y = d - gain*x;
    samples[(size_t)write] = x + gain*y;
    if (++write >= (int)samples.size()) write = 0;
    return y;
}
void Room::Allpass::reset() { std::fill(samples.begin(), samples.end(), 0.f); write = 0; }
void Room::prepare(float sampleRate) {
    sr = sampleRate;
    for (auto& l : lines) l.samples.assign((size_t)(sr*.3f)+64, 0.f);
    const float lengths[4] = { 5.3f, 7.9f, 6.1f, 8.9f };
    for (int i = 0; i < 4; ++i) allpasses[(size_t)i].samples.assign((size_t)(lengths[i]*.001f*sr)+1, 0.f);
    lastRT = -1;
    reset();
}
void Room::reset() {
    for (auto& l : lines) { std::fill(l.samples.begin(), l.samples.end(), 0.f); l.write = 0; l.low = 0; }
    for (auto& a : allpasses) a.reset();
}
void Room::configure(const Params& p) {
    if (p.roomSeconds == lastRT && p.roomSize == lastSize && p.absorption == lastAbsorb && p.diffusion == lastDiffuse) return;
    lastRT = p.roomSeconds; lastSize = p.roomSize; lastAbsorb = p.absorption; lastDiffuse = p.diffusion;
    // The same room-size/diffusion concepts as the supplied demo's 8-line FDN.
    const float width = 8.f + 32.f*p.roomSize;
    const float length = 10.f + 50.f*p.roomSize;
    const float height = 3.5f + 18.5f*p.roomSize;
    const float volume = width*length*height;
    const float surface = 2.f*(width*length + width*height + length*height);
    const float meanPath = (4.f*volume/surface)/343.f;
    const float factors[8] = { .61f, .73f, .89f, 1.03f, 1.19f, 1.37f, 1.57f, 1.81f };
    for (int i = 0; i < 8; ++i) {
        auto& l = lines[(size_t)i];
        // Odd lengths reduce shared resonances.
        const int n = ((int)(meanPath*factors[i]*sr) | 1);
        l.length = std::max(31, std::min(n, (int)l.samples.size()-2));
        l.feedback = std::pow(10.f, -3.f*l.length/(sr*std::max(.2f, p.roomSeconds)));
    }
    const float cutoff = 12000.f - 10200.f*p.absorption;
    damping = 1.f - std::exp(-2*pi*std::min(cutoff, sr*.45f)/sr);
    for (auto& a : allpasses) a.gain = .25f + .5f*p.diffusion;
}
void Room::process(float inL, float inR, float& outL, float& outR) {
    const float l = allpasses[1].process(allpasses[0].process(inL));
    const float r = allpasses[3].process(allpasses[2].process(inR));
    float o[8], m[8];
    for (int i = 0; i < 8; ++i) {
        auto& line = lines[(size_t)i];
        int tap = line.write - line.length;
        if (tap < 0) tap += (int)line.samples.size();
        const float v = line.samples[(size_t)tap];
        line.low += damping*(v-line.low);
        o[i] = m[i] = line.low;
    }
    for (int h = 1; h < 8; h <<= 1) {
        for (int i = 0; i < 8; i += 2*h) {
            for (int j = i; j < i+h; ++j) {
                const float a = m[j], b = m[j+h];
                m[j] = a+b; m[j+h] = a-b;
            }
        }
    }
    constexpr float norm = .35355339f;
    for (int i = 0; i < 8; ++i) {
        auto& line = lines[(size_t)i];
        const float input = (i&1 ? r : l)*(i&2 ? -.5f : .5f);
        line.samples[(size_t)line.write] = input + m[i]*norm*line.feedback;
        if (++line.write >= (int)line.samples.size()) line.write = 0;
    }
    outL = .4f*(o[0]-o[2]+o[4]-o[6]);
    outR = .4f*(o[1]-o[3]+o[5]-o[7]);
}

void Instrument::prepare(double sampleRate) {
    sr = (float)std::max(8000., sampleRate);
    // Long enough for microphones/singers separated by more than 40 metres, even at 192 kHz.
    const int delayLength = (int)(sr*.2f)+32;
    for (auto& v : voices) v.ring.assign((size_t)delayLength, 0.f);
    room.prepare(sr);
    reset();
}
void Instrument::reset() {
    for (auto& v : voices) { v.active = false; v.env = 0; v.write = 0; std::fill(v.ring.begin(), v.ring.end(), 0.f); for (auto& f : v.formants) f.reset(); }
    sustainDown.fill(false); bend.fill(0.f); room.reset(); outputGain = 0.f; wetNow = 0.f;
}
void Instrument::setParameters(const Params& parameters) {
    const bool revoice = parameters.vowel != p.vowel;
    p = parameters;
    room.configure(p);
    for (auto& v : voices) if (v.active) {
        if (revoice) retune(v);
        updateSpatial(v);
    }
}
float Instrument::rangeWeight(int section, int note) const {
    // S/A/T/B tessitura. Notes near the borders softly overlap adjacent sections.
    constexpr int low[4] = { 57, 50, 43, 34 }, high[4] = { 90, 82, 74, 67 };
    const float in = limit((note-low[section]+5)/9.f, 0.f, 1.f);
    const float out = limit((high[section]-note+5)/9.f, 0.f, 1.f);
    return in*out;
}
void Instrument::retune(Voice& v) {
    const float semis = (float)v.note - 69.f + bend[(size_t)(v.channel-1)];
    v.hz = 440.f*std::pow(2.f, semis/12.f);
    const float formants[5][3] = {
        { 820.f, 1220.f, 2900.f }, // Ah
        { 600.f, 1770.f, 2550.f }, // Eh
        { 310.f, 2250.f, 3000.f }, // Ee
        { 580.f, 900.f, 2600.f },  // Oh
        { 350.f, 790.f, 2350.f }   // Oo
    };
    const float voiceScale[4] = { 1.09f, 1.00f, .91f, .83f };
    const float q[3] = { 1.8f, 3.f, 4.f };
    const int vowel = std::max(0, std::min(4, p.vowel));
    for (int i = 0; i < 3; ++i) v.formants[(size_t)i].setBandpass(formants[vowel][i]*voiceScale[v.section], q[i], sr);
}
void Instrument::spawn(int channel, int note, int section, float velocity, float weight) {
    Voice* chosen = nullptr;
    for (auto& v : voices) if (!v.active) { chosen = &v; break; }
    if (chosen == nullptr) chosen = &*std::min_element(voices.begin(), voices.end(), [](const Voice& a, const Voice& b) {
        // Steal the quietest/released voice, and oldest on ties.
        const float ga = a.env*(a.keyDown ? 1.f : .1f);
        const float gb = b.env*(b.keyDown ? 1.f : .1f);
        return ga == gb ? a.age < b.age : ga < gb;
    });
    auto& v = *chosen;
    v.active = v.keyDown = true; v.held = false;
    v.channel = channel; v.note = note; v.section = section;
    v.velocity = limit(velocity, 0.f, 1.f);
    v.gain = weight;
    v.env = 0.f;
    v.phase = { .137f*(section+1), .383f*(section+1), .673f*(section+1) };
    v.noise = 0x9e3779b9u ^ (uint32_t)(note*65537+channel*1021+section*7);
    v.age = ++voiceClock;
    v.write = 0;
    std::fill(v.ring.begin(), v.ring.end(), 0.f);
    for (auto& f : v.formants) f.reset();
    retune(v);
    updateSpatial(v);
    v.delayL = v.targetL; v.delayR = v.targetR;
    v.currentGainL = v.gainL; v.currentGainR = v.gainR;
}
void Instrument::noteOn(int channel, int note, float velocity) {
    if (channel < 1 || channel > 16 || note < 0 || note > 127) return;
    if (velocity <= 0) { noteOff(channel, note); return; }
    if (channel >= 2 && channel <= 5) {
        spawn(channel, note, channel-2, velocity, 1.f);
    } else {
        float weights[4], energy = 0;
        for (int s = 0; s < 4; ++s) { weights[s] = rangeWeight(s, note); energy += weights[s]*weights[s]; }
        if (energy < .001f) { const int s = note >= 78 ? 0 : note >= 64 ? 1 : note >= 52 ? 2 : 3; spawn(channel, note, s, velocity, 1.f); return; }
        const float scale = 1.f/std::sqrt(energy);
        for (int s = 0; s < 4; ++s) if (weights[s] > .05f) spawn(channel, note, s, velocity, weights[s]*scale);
    }
}
void Instrument::noteOff(int channel, int note) {
    for (auto& v : voices) if (v.active && v.channel == channel && v.note == note && v.keyDown) {
        v.keyDown = false;
        v.held = sustainDown[(size_t)(channel-1)];
    }
}
void Instrument::sustain(int channel, bool down) {
    if (channel < 1 || channel > 16) return;
    sustainDown[(size_t)(channel-1)] = down;
    if (!down) for (auto& v : voices) if (v.active && v.channel == channel && !v.keyDown) v.held = false;
}
void Instrument::allNotesOff() { for (auto& v : voices) { v.keyDown = false; v.held = false; } sustainDown.fill(false); }
void Instrument::pitchWheel(int channel, int value14) {
    if (channel < 1 || channel > 16) return;
    bend[(size_t)(channel-1)] = ((float)value14-8192.f)/8192.f*2.f;
    for (auto& v : voices) if (v.active && v.channel == channel) retune(v);
}
float Instrument::polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t+t-t*t-1.f; }
    if (t > 1.f-dt) { t = (t-1.f)/dt; return t*t+t+t+1.f; }
    return 0.f;
}
float Instrument::generate(Voice& v) {
    const float freq = limit(v.hz, 18.f, sr*.2f);
    float raw = 0.f;
    for (int i = 0; i < 3; ++i) {
        const float spread = p.ensemble*.0065f*(i-1)*(1.f + (v.section*.07f));
        const float dt = limit(freq*(1+spread)/sr, .00001f, .45f);
        float& phase = v.phase[(size_t)i];
        // Band-limited saw with a softer sine-like glottal component.
        float saw = 2.f*phase-1.f-polyBlep(phase, dt);
        raw += (saw*.65f + std::sin(phase*2*pi)*.35f)*(i == 1 ? .52f : .24f);
        phase += dt; if (phase >= 1.f) phase -= 1.f;
    }
    const float sung = 1.35f*v.formants[0].process(raw)
                      + 1.0f*v.formants[1].process(raw)
                      + .55f*v.formants[2].process(raw);
    v.noise = 1664525u*v.noise + 1013904223u;
    const float breath = (float)((v.noise >> 8)&0xffff)/32768.f-1.f;
    // A little unvoiced breath improves soft releases; weighted by the note envelope.
    return (sung*(1.f-.3f*p.breath) + breath*.08f*p.breath);
}
void Instrument::updateSpatial(Voice& v) {
    const float drift = .12f*(static_cast<int>(v.age%9)-4); // slight displacement between choir members
    const float x = p.x[(size_t)v.section] + drift;
    const float y = p.y[(size_t)v.section] + .14f*(static_cast<int>(v.age%5)-2);
    const float mx = p.micX, my = p.micY;
    const float forwardY = y-my;
    auto arrival = [&](float cx, float cy, float direction, float omni, bool headShadow) {
        const float dx = x-cx, dy = y-cy;
        const float distance = std::max(.35f, std::sqrt(dx*dx+dy*dy+2.1f*2.1f));
        const float cosAngle = (dx*std::sin(direction)+dy*std::cos(direction))/distance;
        const float facing = limit(.75f + .25f*forwardY/std::max(1.f,distance), .5f, 1.f);
        const float shadow = headShadow ? (1.f-.22f*limit(-cosAngle,0.f,1.f)) : 1.f;
        return std::array<float, 2>{ distance/343.f*sr, pattern(cosAngle,omni)*3.f/distance*facing*shadow };
    };
    std::array<float, 2> l, r;
    const float w = limit(p.width, .1f, 3.f);
    switch (p.micMode) {
        case 0: l = arrival(mx-w*.5f,my,0,1.f,false); r = arrival(mx+w*.5f,my,0,1.f,false); break;
        case 1: l = arrival(mx-.085f,my,-55*pi/180,.5f,false); r = arrival(mx+.085f,my,55*pi/180,.5f,false); break;
        case 2: l = arrival(mx,my,-45*pi/180,.5f,false); r = arrival(mx,my,45*pi/180,.5f,false); break;
        case 3: {
            const auto mid = arrival(mx,my,0,.5f,false);
            const auto side = arrival(mx,my,-pi*.5f,0.f,false);
            l = { mid[0], .7071f*(mid[1]+side[1]) };
            r = { mid[0], .7071f*(mid[1]-side[1]) };
            break;
        }
        case 4: {
            const auto a = arrival(mx-std::max(.5f,w),my,0,1.f,false);
            const auto b = arrival(mx+std::max(.5f,w),my,0,1.f,false);
            const auto c = arrival(mx,my+.75f,0,1.f,false);
            l = { a[0], a[1]+c[1]*.3f }; r = { b[0], b[1]+c[1]*.3f };
            break;
        }
        default: l = arrival(mx-.0875f,my,-pi*.5f,.7f,true); r = arrival(mx+.0875f,my,pi*.5f,.7f,true); break;
    }
    const float secGain = p.gain[(size_t)v.section];
    // Prevent an extreme near-microphone spike when a performer is dragged on top of the mic.
    v.targetL = limit(l[0], 0.f, (float)v.ring.size()-4.f);
    v.targetR = limit(r[0], 0.f, (float)v.ring.size()-4.f);
    v.gainL = limit(l[1], -3.f, 3.f)*secGain*v.gain;
    v.gainR = limit(r[1], -3.f, 3.f)*secGain*v.gain;
}
void Instrument::render(float& left, float& right) {
    float dryL = 0, dryR = 0;
    const float attackStep = 1.f/std::max(1.f, p.attack*sr);
    const float releaseStep = 1.f/std::max(1.f, p.release*sr);
    for (auto& v : voices) {
        if (!v.active) continue;
        if (v.keyDown || v.held) v.env = std::min(1.f, v.env+attackStep);
        else v.env = std::max(0.f, v.env-releaseStep);
        if (!v.keyDown && !v.held && v.env <= 0.f) { v.active = false; continue; }
        const float signal = generate(v)*v.env*sq(v.velocity)*.18f;
        auto& line = v.ring;
        line[(size_t)v.write] = signal;
        v.delayL += (v.targetL-v.delayL)*.0007f;
        v.delayR += (v.targetR-v.delayR)*.0007f;
        auto read = [&](float delay) {
            float cursor = (float)v.write-delay;
            if (cursor < 0) cursor += (float)line.size();
            const int i0 = (int)cursor;
            const int i1 = i0+1 == (int)line.size() ? 0 : i0+1;
            const float f = cursor-(float)i0;
            return line[(size_t)i0] + (line[(size_t)i1]-line[(size_t)i0])*f;
        };
        v.currentGainL += (v.gainL-v.currentGainL)*.001f;
        v.currentGainR += (v.gainR-v.currentGainR)*.001f;
        dryL += read(v.delayL)*v.currentGainL;
        dryR += read(v.delayR)*v.currentGainR;
        if (++v.write >= (int)line.size()) v.write = 0;
    }
    float wetL = 0, wetR = 0;
    room.process(dryL, dryR, wetL, wetR);
    wetNow += (p.wet-wetNow)*.001f;
    outputGain += (p.master-outputGain)*.001f;
    // Wet is additive to the natural room, avoiding a gain collapse when the mic is moved.
    left = std::tanh((dryL + wetNow*wetL*1.4f)*outputGain);
    right = std::tanh((dryR + wetNow*wetR*1.4f)*outputGain);
}
int Instrument::activeVoices() const {
    int count=0;
    for (const auto& v : voices) count += (v.active ? 1 : 0);
    return count;
}
} // namespace choir
