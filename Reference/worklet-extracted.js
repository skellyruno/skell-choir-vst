'use strict';
// the audio thread. Plays the chosen acapella with the chosen choir, the choir through the
// plugin's own stage, microphones and room (choir-stage.js, added to the worklet first), looping the song's 24 bars.
//
// Everything is already rendered: the singers were sung by the plugin offline, so nothing on the page waits for a
// render. What runs here is only what has to follow the visitor's hands: where each part of the choir stands.
//
// Messages in (port.postMessage from app.js):
//   { type: 'lead', vocal, loop, tail, channels: [Int16Array L, Int16Array R] }      a vocal's acapella
//   { type: 'choir', key, manifest, channels: [Int16Array, ...] }                    a choir for a vocal (channels in
//       the order of manifest.sources, a stereo source taking two)
//   { type: 'drop', key }                                                             forget a choir (memory)
//   { type: 'vocal', vocal }                 sing this vocal's acapella on its own, no choir yet (sent before a choir
//       has arrived: the lead can start while the choir is still loading). A new vocal starts its song from the top
//   { type: 'select', vocal, key, offsets, hall }   play this choir with this vocal (hall: { seconds, gain }, the
//       website's extra reverb under this choir). A new vocal starts its song from the
//       top; a new choir on the same vocal takes over where the song is. offsets: [[dx, dy] per group] (metres)
//   { type: 'move', group, dx, dy }          move a group of the playing choir (metres from its formation place)
//   { type: 'listener', dx, dy }             move YOU, the main microphones the choir is heard from (metres from
//       where the preset puts them). The lead's own place is just behind them: send leadMove too to leave it standing
//   { type: 'leadMove', dx, dy }             move the lead singer (metres from its own place, just behind the
//       microphones: there the acapella is heard exactly as it is; moved, it sings from the stage, in the choir's room)
//   { type: 'play' } / { type: 'pause' }
//   { type: 'choirOn', on, at, fade }        the choir singing (true) or silent (false): its voices fade in or out
//       (over fade seconds; a fifth of a second if not given) and its room rings out; the song, the stage and the lead
//       carry on. at (optional): the song time (seconds since the song started) to do it at, to the block; else now
//   { type: 'leadLevel', gain }              the acapella's level against the choir (linear; 1 = as the plugin mixes them)
// Messages out: { type: 'levels', key, levels: [per source], lead, seconds, played } about 30 times a second while
//   playing (key null and levels [] while the lead sings with no choir; played: seconds of the song sung so far),
//   { type: 'started', key }, { type: 'missing', key } (asked for a choir or vocal not sent yet).

const FADE = 0.04;          // seconds: voices fading in or out when choirs change over
const RING = 4.0;           // seconds a replaced choir's room keeps ringing after its voices have faded out
const CHOIR_FADE = 0.2;     // seconds: the choir coming in or going out (CHOIR ON / OFF)
const S16 = 1 / 32768;

// a looped stream's sample at song time t: this pass's, plus the end of the pass before (bar 25: the last notes'
// tails) sounding over the start of every pass after the first
function at(ch, t, L, T) {
  const p = Math.floor(t / L), tau = t - p * L;
  let v = tau < ch.length ? ch[tau] : 0;
  if (p > 0 && tau < T && L + tau < ch.length) v += ch[L + tau];
  return v * S16;
}

// one choir playing: its engine on the stage, its own place in its song, its voices' level (fading in or out)
class Voice {
  constructor(choir, lead, t, offsets, block) {
    this.choir = choir; this.L = lead.loop; this.T = lead.tail; this.t = t;
    const m = choir.manifest;
    this.engine = new OmniChoir.ChoirStage(m.stage, m.sources.map((s) => ({ section: s.section, members: s.members })), m.air.first);
    (offsets || []).forEach(([dx, dy], g) => { if (dx || dy) this.engine.setGroupOffset(m.groups[g], dx, dy); });
    this.ins = choir.sources.map((s) => (s.length === 2 ? [new Float32Array(block), new Float32Array(block)] : new Float32Array(block)));
    this.levels = new Float32Array(choir.sources.length);
    this.gain = 0; this.target = 1; this.ringLeft = Infinity; this.pass = -1;
    this.on = 1; this.onGain = 1; this.onFade = CHOIR_FADE;     // CHOIR ON / OFF
  }
  // the top-end shelf as the plugin sets it: its schedule for the first pass, then for every pass after another
  schedule() {
    const p = Math.floor(this.t / this.L);
    if (p === this.pass) return;
    this.pass = p;
    const air = this.choir.manifest.air;
    this.engine.airEvents = (p === 0 ? air.first : air.loop).map(([c, db, on]) => [c + p * this.L, db, on]);
    this.engine.air.next = 0;
  }
  render(n, fadeStep, outL, outR, vL, vR) {
    this.schedule();
    const onStep = n / (this.onFade * sampleRate), o0 = this.onGain;
    const o1 = this.on > o0 ? Math.min(this.on, o0 + onStep) : Math.max(this.on, o0 - onStep);
    this.onGain = o1;
    const f0 = this.gain, f1 = this.target > f0 ? Math.min(this.target, f0 + fadeStep) : Math.max(this.target, f0 - fadeStep);
    this.gain = f1;
    const g0 = f0 * o0, g1 = f1 * o1;
    const { L, T, t } = this;
    this.choir.sources.forEach((chans, j) => {
      const b = this.ins[j];
      if (chans.length === 2) {
        const [a, c] = chans, [bl, br] = b;
        for (let i = 0; i < n; ++i) { const g = g0 + (g1 - g0) * (i + 1) / n; bl[i] = g * at(a, t + i, L, T); br[i] = g * at(c, t + i, L, T); }
      } else {
        const a = chans[0];
        for (let i = 0; i < n; ++i) { const g = g0 + (g1 - g0) * (i + 1) / n; b[i] = g * at(a, t + i, L, T); }
      }
      // how loud this source sings (to light its figures)
      const x = chans.length === 2 ? b[0] : b;
      let e = 0; for (let i = 0; i < n; i += 4) e += x[i] * x[i];
      this.levels[j] = Math.max(this.levels[j] * 0.92, Math.sqrt(e / (n / 4)));
    });
    this.engine.clock = t;        // the song's time: the shelf's schedule follows it
    this.engine.process(this.ins, null, null, null, vL, vR);
    for (let i = 0; i < n; ++i) { outL[i] += vL[i]; outR[i] += vR[i]; }
    this.t += n;
    if (this.target === 0) this.ringLeft -= n;
  }
}

class OmniChoirProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.choirs = new Map();       // key -> { manifest, sources: [[Int16Array, ...] per source] }
    this.leads = new Map();        // vocal -> { loop, tail, chans }
    this.voices = [];              // the choir playing (last) and any still ringing out
    this.vocal = null; this.lead = null; this.leadT = 0; this.leadLevel = 1;
    this.choirOn = true;
    this.old = null;               // the acapella fading out after a change of vocal: { lead, t, gain }
    this.leadStage = new OmniChoir.LeadStage(sampleRate, 128);
    this.listener = [0, 0];
    this.hall = new OmniChoir.Hall(sampleRate, 128);   // the website's extra reverb, under every choir
    this.aL = new Float32Array(128); this.aR = new Float32Array(128);
    this.playing = false;
    this.outL = new Float32Array(128); this.outR = new Float32Array(128);
    this.vL = new Float32Array(128); this.vR = new Float32Array(128);
    this.levelEvery = Math.max(1, Math.round(sampleRate / 30 / 128)); this.levelCount = 0;
    this.port.onmessage = (e) => this.message(e.data);
  }
  message(msg) {
    switch (msg.type) {
      case 'lead': this.leads.set(msg.vocal, { loop: msg.loop, tail: msg.tail, chans: msg.channels }); break;
      case 'choir': {
        let c = 0;
        const sources = msg.manifest.sources.map((s) => { const ch = msg.channels.slice(c, c + s.channels); c += s.channels; return ch; });
        this.choirs.set(msg.key, { key: msg.key, manifest: msg.manifest, sources });
        break;
      }
      case 'drop': this.choirs.delete(msg.key); break;
      case 'vocal': if (!this.setVocal(msg.vocal)) this.port.postMessage({ type: 'missing', key: msg.vocal }); break;
      case 'select': this.select(msg); break;
      case 'move': {
        const v = this.voices[this.voices.length - 1];
        if (v && v.target > 0) v.engine.setGroupOffset(v.choir.manifest.groups[msg.group], msg.dx, msg.dy);
        break;
      }
      case 'play': this.playing = true; break;
      case 'pause': this.playing = false; break;
      case 'leadLevel': this.leadLevel = msg.gain; break;
      case 'choirOn':
        this.choirAt = msg.at == null ? null : { t: Math.round(msg.at * sampleRate), on: msg.on, fade: msg.fade };
        if (!this.choirAt) this.setChoirOn(msg.on, msg.fade);
        break;
      case 'leadMove': this.leadStage.setOffset(msg.dx, msg.dy); break;
      case 'listener':
        this.listener = [msg.dx, msg.dy];
        for (const v of this.voices) v.engine.setListener(msg.dx, msg.dy);
        break;
    }
  }
  // the choir in or out (while playing it fades in or out; switched while stopped, it is simply there, or not, when
  // the song starts)
  setChoirOn(on, fade) {
    this.choirOn = on;
    for (const v of this.voices) { v.on = on ? 1 : 0; v.onFade = fade || CHOIR_FADE; if (!this.playing) v.onGain = v.on; }
  }
  // the acapella to sing: a new vocal starts its song from its first bar, the one that was playing fading out under it
  setVocal(vocal) {
    const lead = this.leads.get(vocal);
    if (!lead) return false;
    if (vocal !== this.vocal) {
      if (this.lead) this.old = { lead: this.lead, t: this.leadT, gain: 1 };
      this.lead = lead; this.vocal = vocal; this.leadT = 0;
    }
    return true;
  }
  select(msg) {
    const choir = this.choirs.get(msg.key);
    if (!choir || !this.leads.has(msg.vocal)) { this.port.postMessage({ type: 'missing', key: msg.key }); return; }
    this.setVocal(msg.vocal);
    const lead = this.lead;
    const first = this.voices.length === 0;
    for (const v of this.voices) if (v.target > 0) { v.target = 0; v.ringLeft = Math.round(RING * sampleRate); }
    if (msg.hall) this.hall.set(msg.hall.seconds, msg.hall.gain);
    const v = new Voice(choir, lead, this.leadT, msg.offsets, 128);
    v.engine.setListener(...this.listener);
    v.on = v.onGain = this.choirOn ? 1 : 0;
    if (first) v.gain = 1;            // nothing to take over from: the choir is simply there
    this.voices.push(v);
    this.port.postMessage({ type: 'started', key: msg.key });
  }
  process(inputs, outputs) {
    const out = outputs[0], oL = out[0], oR = out[1] || out[0];
    const n = oL.length;              // 128: the engine's own block at 48 kHz
    if (!this.playing || !this.lead) { oL.fill(0); if (out[1]) out[1].fill(0); return true; }
    if (this.choirAt && this.leadT + n > this.choirAt.t) { this.setChoirOn(this.choirAt.on, this.choirAt.fade); this.choirAt = null; }
    const { outL, outR, vL, vR } = this;
    outL.fill(0); outR.fill(0);
    const fadeStep = n / (FADE * sampleRate);
    for (const v of this.voices) v.render(n, fadeStep, outL, outR, vL, vR);
    this.voices = this.voices.filter((v) => v.ringLeft > 0);
    this.hall.process(outL, outR, outL, outR);        // the choirs into the hall (the acapella is already wet)
    // the acapella (and the one before it, fading out) where the lead singer stands, in the room of the choir playing,
    // then the plugin's output ceiling
    const { aL, aR } = this;
    aL.fill(0); aR.fill(0);
    const add = (lead, t, g0, g1) => {
      const [l, r] = lead.chans;
      for (let i = 0; i < n; ++i) { const g = g0 + (g1 - g0) * (i + 1) / n; aL[i] += g * at(l, t + i, lead.loop, lead.tail); aR[i] += g * at(r, t + i, lead.loop, lead.tail); }
    };
    add(this.lead, this.leadT, 1, 1);
    this.leadT += n;
    if (this.old) {
      const o = this.old, g1 = Math.max(0, o.gain - fadeStep);
      add(o.lead, o.t, o.gain, g1); o.t += n; o.gain = g1;
      if (g1 === 0) this.old = null;
    }
    { let e = 0; for (let i = 0; i < n; i += 4) e += aL[i] * aL[i]; this.leadMeter = Math.max((this.leadMeter || 0) * 0.92, Math.sqrt(e / (n / 4))); }
    const room = this.voices[this.voices.length - 1];
    if (room && room.engine.centroids) {
      this.leadStage.process(aL, aR, room.engine, vL, vR);
      for (let i = 0; i < n; ++i) { outL[i] += this.leadLevel * vL[i]; outR[i] += this.leadLevel * vR[i]; }
    } else for (let i = 0; i < n; ++i) { outL[i] += this.leadLevel * aL[i]; outR[i] += this.leadLevel * aR[i]; }
    for (let i = 0; i < n; ++i) { oL[i] = OmniChoir.safetyCeiling(outL[i]); if (out[1]) oR[i] = OmniChoir.safetyCeiling(outR[i]); }
    if (++this.levelCount >= this.levelEvery) {
      this.levelCount = 0;
      const v = this.voices[this.voices.length - 1];
      this.port.postMessage({ type: 'levels', key: v ? v.choir.key : null, levels: v ? Array.from(v.levels) : [], lead: this.leadMeter, seconds: (this.leadT % this.lead.loop) / sampleRate, played: this.leadT / sampleRate });
    }
    return true;
  }
}

registerProcessor('omnichoir', OmniChoirProcessor);
