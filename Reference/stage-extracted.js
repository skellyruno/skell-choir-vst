'use strict';
// Do not "simplify" the maths: it is
// checked against the plugin's renders (Tools/web/test), and moving a singer must sound as it does in the plugin.
//
// Positions are metres: x left (-) / right (+) as seen from the listener ("YOU", the main microphones), y away from
// the listener, z height. Nothing here is a pan knob: where a singer stands decides when and how loud they reach each
// microphone, how dark they sound (air, facing away), which walls reflect them and how much of the room they excite.
//
// Plain script (no imports): it runs in an AudioWorkletGlobalScope (choir-worklet.js loads it first) and in a JS
// shell for the tests. Everything is exported on globalThis.OmniChoir.

(function (root) {

const SPEED_OF_SOUND = 343, PI = Math.PI, TWO_PI = 2 * Math.PI;
const NUM_SECTIONS = 4;
const REFERENCE_DISTANCE = 3;      // a singer 3 m from an omni capsule arrives at unity gain
const FOCUS_MAIN_SHARE = 0.6;      // a fully featured singer: the main pair carries 40 % of them, the follow spot the rest
const FOCUS_SPOT_GAIN = 0.95;
const FOCUS_WIDTH_DEG = 55;        // seen this far to the side of the main microphones, a featured singer is hard left / right
const FOCUS_CLOSE_SHARE = 0.5;     // a featured singer's bleed into the section spots
const FOCUS_ROOM_LEAN = 0.45;      // how far a featured singer's early reflections and room pickup lean to their side
const LATE_GAIN = 1.6;
const IMAGES = 6;
const MAX_CAPSULES = 3;
const DIRECT_TAPS = MAX_CAPSULES + NUM_SECTIONS + 2 + 1;   // main capsules + 4 spots + room pair + the follow spot
const FOCUS_TAP = DIRECT_TAPS - 1;
const REFLECTION_TAPS = IMAGES * 4;                       // 6 images x (main L/R + room L/R)

const f32 = Math.fround;
const clamp = (x, lo, hi) => (x < lo ? lo : (x > hi ? hi : x));
const lerp = (a, b, t) => a + (b - a) * t;
const smoothstep = (x) => { x = clamp(x, 0, 1); return x * x * (3 - 2 * x); };
const flush = (x) => (Math.abs(x) < 1e-20 ? 0 : x);
const lround = (x) => (x >= 0 ? Math.floor(x + 0.5) : -Math.floor(-x + 0.5));

// ---------------------------------------------------------------- vectors ({ x, y, z })
const v3 = (x = 0, y = 0, z = 0) => ({ x, y, z });
const add = (a, b) => v3(a.x + b.x, a.y + b.y, a.z + b.z);
const sub = (a, b) => v3(a.x - b.x, a.y - b.y, a.z - b.z);
const mul = (a, s) => v3(a.x * s, a.y * s, a.z * s);
const dot = (a, b) => a.x * b.x + a.y * b.y + a.z * b.z;
const len = (a) => Math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
const normalised = (a) => { const l = len(a); return l > 1e-6 ? v3(a.x / l, a.y / l, a.z / l) : v3(0, -1, 0); };

// ---------------------------------------------------------------- microphones
const Pattern = { Omni: 0, WideCardioid: 1, Cardioid: 2, Hypercardioid: 3, Figure8: 4 };
const ArrayType = { AB: 0, ORTF: 1, XY: 2, MS: 3, Decca: 4, Binaural: 5 };
const PATTERN_A = [1.0, 0.7, 0.5, 0.25, 0.0];
const DIFFUSE = [1.0, 0.81, 0.58, 0.5, 0.58];

function capsule() { return { pos: v3(), dir: v3(0, 1, 0), pattern: Pattern.Omni, gainL: 1, gainR: 0, headShadow: false }; }
const patternGain = (cap, cosTheta) => { const a = PATTERN_A[cap.pattern]; return a + (1 - a) * cosTheta; };

// the capsules of a stereo array from its type, position, aim and spacing (MicArray::build)
function buildArray(type, position, yawDeg, spacing) {
  const yaw = yawDeg * PI / 180;
  const fwd = v3(Math.sin(yaw), Math.cos(yaw), 0), right = v3(Math.cos(yaw), -Math.sin(yaw), 0);
  const aim = (deg) => { const a = deg * PI / 180; return normalised(add(mul(fwd, Math.cos(a)), mul(right, Math.sin(a)))); };
  const caps = [capsule(), capsule(), capsule()];
  let numCapsules = 2;
  const [c0, c1, c2] = caps;
  switch (type) {
    case ArrayType.AB:
      c0.pos = sub(position, mul(right, spacing * 0.5)); c1.pos = add(position, mul(right, spacing * 0.5));
      c0.dir = fwd; c1.dir = fwd; c0.pattern = c1.pattern = Pattern.Omni;
      c0.gainL = 1; c0.gainR = 0; c1.gainL = 0; c1.gainR = 1;
      break;
    case ArrayType.ORTF:
      c0.pos = sub(position, mul(right, 0.085)); c1.pos = add(position, mul(right, 0.085));
      c0.dir = aim(-55); c1.dir = aim(55); c0.pattern = c1.pattern = Pattern.Cardioid;
      c0.gainL = 1; c0.gainR = 0; c1.gainL = 0; c1.gainR = 1;
      break;
    case ArrayType.XY:
      c0.pos = position; c1.pos = position;
      c0.dir = aim(-45); c1.dir = aim(45); c0.pattern = c1.pattern = Pattern.Cardioid;
      c0.gainL = 1; c0.gainR = 0; c1.gainL = 0; c1.gainR = 1;
      break;
    case ArrayType.MS:
      // mid cardioid forward, side figure-8 pointing left; L = M + S, R = M - S
      c0.pos = position; c1.pos = position;
      c0.dir = fwd; c0.pattern = Pattern.Cardioid; c0.gainL = 0.7071; c0.gainR = 0.7071;
      c1.dir = mul(right, -1); c1.pattern = Pattern.Figure8; c1.gainL = 0.7071; c1.gainR = -0.7071;
      break;
    case ArrayType.Decca:
      numCapsules = 3;
      c0.pos = sub(position, mul(right, Math.max(0.5, spacing))); c1.pos = add(position, mul(right, Math.max(0.5, spacing)));
      c2.pos = add(position, mul(fwd, 0.75));
      for (const c of caps) { c.dir = fwd; c.pattern = Pattern.Omni; }
      c0.gainL = 1; c0.gainR = 0; c1.gainL = 0; c1.gainR = 1;
      c2.gainL = 0.5; c2.gainR = 0.5;
      break;
    default:   // Binaural
      c0.pos = sub(position, mul(right, 0.0875)); c1.pos = add(position, mul(right, 0.0875));
      c0.dir = mul(right, -1); c1.dir = right; c0.pattern = c1.pattern = Pattern.WideCardioid;
      c0.headShadow = c1.headShadow = true;
      c0.gainL = 1; c0.gainR = 0; c1.gainL = 0; c1.gainR = 1;
      break;
  }
  return { type, position, capsules: caps, numCapsules };
}

// ---------------------------------------------------------------- the room
function roomGeometry(s, choirMaxY, choirMaxAbsX) {
  const g = {};
  const size = clamp(s.size, 0, 1);
  g.width = lerp(8, 40, size); g.length = lerp(10, 60, size); g.height = lerp(3.5, 22, size);
  // the choir always stands inside the room: walls move out if a formation is larger than the nominal room
  g.xMax = Math.max(g.width * 0.5, choirMaxAbsX + 1.5); g.xMin = -g.xMax;
  g.yMax = Math.max(0.4 * g.length, choirMaxY + 1.5);
  g.yMin = -0.6 * g.length;
  g.zMax = g.height;
  g.width = g.xMax - g.xMin; g.length = g.yMax - g.yMin;
  const alpha = clamp(0.08 + 0.8 * s.absorption, 0, 0.95);
  g.reflection = Math.sqrt(1 - alpha);
  g.rt60 = clamp(s.seconds, 0.2, 10);
  const volume = g.width * g.length * g.height;
  const surface = 2 * (g.width * g.length + g.width * g.height + g.length * g.height);
  g.meanFreePathMs = 1000 * (4 * volume / surface) / SPEED_OF_SOUND;
  return g;
}

// the late field: an 8-line feedback delay network excited by the early reflections (LateReverb)
class LateReverb {
  constructor(sampleRate) {
    this.sr = sampleRate;
    this.maxLen = Math.trunc(0.25 * sampleRate) + 16;
    this.line = []; for (let i = 0; i < 8; ++i) this.line.push(new Float32Array(this.maxLen));
    this.lineLen = new Int32Array(8); this.writePos = new Int32Array(8);
    this.feedback = new Float64Array(8); this.dampState = new Float64Array(8);
    this.dampCoef = 0.5;
    const apMs = [5.3, 7.9, 6.1, 8.9];
    this.ap = apMs.map((ms) => { const n = Math.max(8, Math.trunc(f32(ms) * 0.001 * sampleRate)); return { buf: new Float32Array(n), pos: 0, len: n, g: 0.5 }; });
    this.lfoPhase = 0; this.lfoInc = f32(0.37 / sampleRate);
    this.o = new Float64Array(8); this.m = new Float64Array(8);
    const defaults = { size: 0.5, absorption: 0.4, diffusion: 0.7, seconds: 2.0, earlyLevel: 0.7, lateLevel: 0.6 };
    this.configure(defaults, roomGeometry(defaults, 8, 6));
    this.reset();
  }
  reset() {
    for (const l of this.line) l.fill(0);
    this.dampState.fill(0);
    for (const a of this.ap) { a.buf.fill(0); a.pos = 0; }
    this.writePos.fill(0);
    this.lfoPhase = 0;
  }
  configure(s, g) {
    const factors = [0.61, 0.73, 0.89, 1.03, 1.19, 1.37, 1.57, 1.81];
    const rt60 = Math.max(0.15, g.rt60);
    for (let i = 0; i < 8; ++i) {
      let n = Math.trunc(f32(g.meanFreePathMs * f32(factors[i]) * 0.001 * this.sr));
      n = clamp(n | 1, 31, this.maxLen - 8);
      this.lineLen[i] = n;
      this.feedback[i] = Math.pow(10, -3 * n / this.sr / rt60);
    }
    const cutoff = lerp(12000, 1800, clamp(s.absorption, 0, 1));
    this.dampCoef = 1 - Math.exp(-TWO_PI * Math.min(cutoff, this.sr * 0.45) / this.sr);
    const gAp = 0.25 + 0.5 * clamp(s.diffusion, 0, 1);
    for (const a of this.ap) a.g = gAp;
  }
  static allpass(a, x) {
    const d = a.buf[a.pos];
    const y = d - a.g * x;
    a.buf[a.pos] = x + a.g * y;
    if (++a.pos >= a.len) a.pos = 0;
    return y;
  }
  process(inL, inR, mainL, mainR, roomL, roomR, n) {
    const { line, lineLen, writePos, feedback, dampState, o, m, maxLen, ap } = this;
    const dampCoef = this.dampCoef;
    for (let s = 0; s < n; ++s) {
      const xl = LateReverb.allpass(ap[1], LateReverb.allpass(ap[0], inL[s]));
      const xr = LateReverb.allpass(ap[3], LateReverb.allpass(ap[2], inR[s]));
      this.lfoPhase = f32(this.lfoPhase + this.lfoInc); if (this.lfoPhase >= 1) this.lfoPhase = f32(this.lfoPhase - 1);
      const mod = f32(2 * Math.sin(TWO_PI * this.lfoPhase));
      for (let i = 0; i < 8; ++i) {
        let delay = lineLen[i];
        if (i === 0) delay = f32(delay + mod + 3); else if (i === 4) delay = f32(delay - (mod - 3));
        const whole = Math.trunc(delay), frac = delay - whole;
        let i0 = writePos[i] - whole; if (i0 < 0) i0 += maxLen;
        let i1 = i0 - 1; if (i1 < 0) i1 += maxLen;       // one sample older
        const li = line[i];
        const v = li[i0] + frac * (li[i1] - li[i0]);
        dampState[i] += dampCoef * (v - dampState[i]);
        o[i] = dampState[i];
      }
      // Hadamard mixing (fast Walsh-Hadamard transform, orthonormal)
      for (let i = 0; i < 8; ++i) m[i] = o[i];
      for (let h = 1; h < 8; h <<= 1)
        for (let i = 0; i < 8; i += h << 1)
          for (let j = i; j < i + h; ++j) { const a = m[j], b = m[j + h]; m[j] = a + b; m[j + h] = a - b; }
      const norm = 0.35355339;
      for (let i = 0; i < 8; ++i) {
        const inp = ((i & 1) ? xr : xl) * ((i & 2) ? -0.5 : 0.5);
        line[i][writePos[i]] = inp + m[i] * norm * feedback[i];
        if (++writePos[i] >= maxLen) writePos[i] = 0;
      }
      mainL[s] = 0.5 * (o[0] - o[2] + o[4] - o[6]);
      mainR[s] = 0.5 * (o[1] - o[3] + o[5] - o[7]);
      roomL[s] = 0.5 * (o[0] + o[3] - o[5] + o[6]);
      roomR[s] = 0.5 * (o[1] + o[2] - o[4] + o[7]);
    }
    for (let i = 0; i < 8; ++i) dampState[i] = flush(dampState[i]);
  }
}

// ---------------------------------------------------------------- the microphones capturing every source
function tap() {
  return { delay: 0, prevDelay: 0, delayR: 0, prevDelayR: 0, gainL: 0, gainR: 0, prevGainL: 0, prevGainR: 0, lpCoef: 1, lpState: 0, lpStateR: 0, valid: false };
}

class SpatialRenderer {
  // numSources: how many sources (singers, or groups of singers sung as one) this renderer places
  constructor(sampleRate, maxBlockSize, numSources) {
    this.sr = sampleRate;
    this.maxBlock = Math.max(16, maxBlockSize);
    this.ringSize = sampleRate > 60000 ? 65536 : 32768;
    this.ringMask = this.ringSize - 1;
    const ring = () => ({ l: new Float32Array(this.ringSize), r: new Float32Array(this.ringSize) });
    // the plugin keeps one ring per section and microphone group; only their sums reach the main output, so the
    // sections share one ring per microphone group here (the same sums)
    this.ringMain = ring(); this.ringClose = ring(); this.ringRoom = ring();
    this.erMain = ring(); this.erRoom = ring();
    this.scratch = new Float32Array(this.maxBlock); this.erFiltered = new Float32Array(this.maxBlock);
    this.scratchR = new Float32Array(this.maxBlock); this.erFilteredR = new Float32Array(this.maxBlock);
    this.tmpL = new Float32Array(this.maxBlock); this.tmpR = new Float32Array(this.maxBlock);
    this.lateMainL = new Float32Array(this.maxBlock); this.lateMainR = new Float32Array(this.maxBlock);
    this.lateRoomL = new Float32Array(this.maxBlock); this.lateRoomR = new Float32Array(this.maxBlock);
    this.late = new LateReverb(sampleRate);
    this.room = { size: 0.5, absorption: 0.4, diffusion: 0.7, seconds: 2.0, earlyLevel: 0.7, lateLevel: 0.6 };
    this.mics = { mainType: ArrayType.AB, mainPos: v3(0, 0, 3), mainSpacing: 0.6, roomPos: v3(0, -8, 4.5), roomSpacing: 4,
                  closeDistance: 1.2, closeHeight: 2.1, mainLevel: 1, closeLevel: 0.3548134, roomLevel: 0.5011872 };
    this.centroids = []; for (let s = 0; s < NUM_SECTIONS; ++s) this.centroids.push(v3((s - 1.5) * 2, 4, 1.6));
    this.choirMaxY = 8; this.choirMaxAbsX = 6;
    this.geometryDirty = true; this.freeMovement = false; this.minDistance = 0.3;
    this.lateBalance = 0; this.roomKeepsLevel = false; this.soloist = false;
    this.writePos = 0;
    this.singers = [];
    for (let i = 0; i < numSources; ++i) this.singers.push(this.newSinger());
    this.rebuildArrays();
    this.reset();
  }
  newSinger() {
    const s = { active: false, initialised: false, dirty: true, fading: false, fade: 1, section: 0, focus: 0, lean: 0,
                pos: v3(), target: v3(), facing: v3(0, -1, 0), direct: [], reflection: [], erLpCoef: 1, erLpState: 0, erLpStateR: 0 };
    for (let i = 0; i < DIRECT_TAPS; ++i) s.direct.push(tap());
    for (let i = 0; i < REFLECTION_TAPS; ++i) s.reflection.push(tap());
    return s;
  }
  reset() {
    for (const r of [this.ringMain, this.ringClose, this.ringRoom, this.erMain, this.erRoom]) { r.l.fill(0); r.r.fill(0); }
    for (const s of this.singers) {
      s.initialised = false; s.dirty = true; s.erLpState = s.erLpStateR = 0; s.fading = false; s.fade = 1;
      for (const t of s.direct) t.lpState = t.lpStateR = 0;
    }
    this.late.reset();
    this.writePos = 0;
  }
  setRoom(r) {
    const same = r.size === this.room.size && r.absorption === this.room.absorption && r.diffusion === this.room.diffusion
              && r.seconds === this.room.seconds && r.earlyLevel === this.room.earlyLevel && r.lateLevel === this.room.lateLevel;
    if (same && !this.geometryDirty) return;
    this.room = { ...r }; this.geometryDirty = true;
  }
  setMics(m) {
    const o = this.mics;
    const sameGeometry = m.mainType === o.mainType && m.mainPos.x === o.mainPos.x && m.mainPos.y === o.mainPos.y && m.mainPos.z === o.mainPos.z
      && m.mainSpacing === o.mainSpacing && m.roomPos.x === o.roomPos.x && m.roomPos.y === o.roomPos.y && m.roomPos.z === o.roomPos.z
      && m.roomSpacing === o.roomSpacing && m.closeDistance === o.closeDistance && m.closeHeight === o.closeHeight;
    if (m.mainLevel !== o.mainLevel || m.closeLevel !== o.closeLevel) for (const s of this.singers) s.dirty = true;
    this.mics = { ...m };
    if (!sameGeometry) this.geometryDirty = true;
  }
  setStage(centroids, maxY, maxAbsX) {
    let changed = Math.abs(maxY - this.choirMaxY) > 0.05 || Math.abs(maxAbsX - this.choirMaxAbsX) > 0.05;
    for (let s = 0; s < NUM_SECTIONS; ++s) {
      if (len(sub(centroids[s], this.centroids[s])) > 0.02) changed = true;
      this.centroids[s] = centroids[s];
    }
    this.choirMaxY = maxY; this.choirMaxAbsX = maxAbsX;
    if (changed) this.geometryDirty = true;
    if (this.geometryDirty) this.rebuildArrays();
  }
  rebuildArrays() {
    const mics = this.mics;
    this.mainArray = buildArray(mics.mainType, mics.mainPos, 0, mics.mainSpacing);
    this.roomArray = buildArray(ArrayType.AB, mics.roomPos, 0, mics.roomSpacing);
    // spot microphones: a cardioid in front of each section, aimed at it, placed in the stereo image where the
    // section stands as seen from the main microphones (YOU)
    this.spots = [];
    for (let s = 0; s < NUM_SECTIONS; ++s) {
      const c = this.centroids[s];
      const toConductor = normalised(v3(-c.x, -c.y, 0));
      const spot = capsule();
      spot.pos = v3(c.x + toConductor.x * mics.closeDistance, c.y + toConductor.y * mics.closeDistance, mics.closeHeight);
      spot.dir = normalised(sub(c, spot.pos));
      spot.pattern = Pattern.Cardioid;
      const p = clamp((c.x - mics.mainPos.x) / Math.max(2, this.choirMaxAbsX), -1, 1);
      spot.gainL = Math.cos((p + 1) * PI * 0.25); spot.gainR = Math.sin((p + 1) * PI * 0.25);
      this.spots.push(spot);
    }
    const geo = roomGeometry(this.room, this.choirMaxY, this.choirMaxAbsX);
    // the microphones stand inside the room too
    geo.yMin = Math.min(geo.yMin, mics.roomPos.y - 1, mics.mainPos.y - 1);
    geo.xMax = Math.max(geo.xMax, Math.abs(mics.roomPos.x) + mics.roomSpacing * 0.5 + 0.5, Math.abs(mics.mainPos.x) + 1.5); geo.xMin = -geo.xMax;
    geo.zMax = Math.max(geo.zMax, mics.mainPos.z + 0.5, mics.roomPos.z + 0.5);
    this.geo = geo;
    this.late.configure(this.room, geo);
    for (const s of this.singers) s.dirty = true;
    this.geometryDirty = false;
  }
  // snap = true jumps without smoothing (the first placement). Otherwise the source walks there: about 2.5 m/s, a
  // 0.1 s cross-fade per 0.25 m step, the arrival gliding to its new time.
  setSingerPosition(i, section, position, facing, snap) {
    const s = this.singers[i];
    s.section = section; s.target = position; s.facing = facing;
    if (snap || !s.initialised || !s.active) { s.pos = position; s.initialised = false; }
    s.active = true;
  }
  // (for a renderer holding one placed source, the lead - SpatialRenderer.h:) the closest a source counts as being to
  // a microphone; the late field leaning to one side (-1 left .. 1 right); a source further than 3 m exciting the room
  // more, so far away it is mostly room; a soloist's direct sound reaching the main pair and its follow spot at one
  // moment, everything it sends shifted earlier by that travel time, so its first sound arrives with the dry vocal
  setMinimumDistance(m) { this.minDistance = m; }
  setLateBalance(b) { this.lateBalance = clamp(b, -1, 1); }
  setRoomKeepsItsLevel(on) { this.roomKeepsLevel = on; }
  setSoloist(on) { this.soloist = on; }
  lean(i) { return this.singers[i].lean; }
  // a source moved out of the formation steps forward like a soloist: a spot microphone follows them, placed in the
  // stereo picture where they stand. 0 = none (the formation as recorded), 1 = fully featured.
  setSingerFocus(i, focus) {
    const s = this.singers[i];
    focus = clamp(focus, 0, 1);
    if (focus !== s.focus) { s.focus = focus; s.dirty = true; }
  }
  directTap(t, s, cap, levelScale, out) {
    const d = sub(cap.pos, s.pos);
    const dist = Math.max(len(d), this.minDistance);
    const toMic = mul(d, 1 / dist);
    const frontness = 0.5 + 0.5 * dot(s.facing, toMic);          // 1 = the singer faces the microphone
    const cosTheta = dot(cap.dir, mul(toMic, -1));                // angle of arrival at the capsule
    const gain = patternGain(cap, cosTheta) * (REFERENCE_DISTANCE / dist) * (0.7 + 0.3 * frontness) * levelScale;
    // air absorption (deliberately a little stronger than physics: it is the main tonal distance cue)
    let cutoff = clamp(20000 / (1 + dist / 9), 2500, 20000) * (0.35 + 0.65 * frontness);
    if (cap.headShadow) cutoff *= 0.25 + 0.75 * (0.5 + 0.5 * cosTheta);
    cutoff = clamp(cutoff, 800, this.sr * 0.45);
    t.delay = t.delayR = Math.min(lround(f32(dist / SPEED_OF_SOUND * this.sr)), this.ringSize / 2 - this.maxBlock - 4);
    t.gainL = gain * cap.gainL; t.gainR = gain * cap.gainR;
    t.lpCoef = 1 - Math.exp(-TWO_PI * cutoff / this.sr);
    t.valid = true;
    if (out) { out.dist = dist; out.gain = Math.abs(gain); out.cutoff = cutoff; }
  }
  computeTaps(s) {
    // the taps computed last time become the "previous snapshot" of the cross-fade
    for (const t of s.direct) { t.prevDelay = t.delay; t.prevDelayR = t.delayR; t.prevGainL = t.gainL; t.prevGainR = t.gainR; }
    for (const t of s.reflection) { t.prevDelay = t.delay; t.prevDelayR = t.delayR; t.prevGainL = t.gainL; t.prevGainR = t.gainR; }
    const mics = this.mics, main = this.mainArray, roomArr = this.roomArray, geo = this.geo;
    let k = 0;
    // MAIN array
    for (let c = 0; c < MAX_CAPSULES; ++c) {
      const t = s.direct[k++];
      if (c < main.numCapsules) this.directTap(t, s, main.capsules[c], 1, null);
      else { t.gainL = t.gainR = 0; t.delay = t.prevDelay; t.delayR = t.prevDelayR; t.valid = true; }   // unused capsule
    }
    // which main capsule feeds each channel most: everything else this source sends to the stems (spots, follow spot)
    // arrives in that channel with it (time-aligned, as engineers align spot microphones to the main pair)
    let cL = 0, cR = 0, bestL = -1, bestR = -1, firstDelay = 1 << 30;
    for (let c = 0; c < main.numCapsules; ++c) {
      const t = s.direct[c];
      firstDelay = Math.min(firstDelay, t.delay);
      if (Math.abs(t.gainL) > bestL) { bestL = Math.abs(t.gainL); cL = c; }
      if (Math.abs(t.gainR) > bestR) { bestR = Math.abs(t.gainR); cR = c; }
    }
    const soloist = this.soloist;
    const alignL = soloist ? firstDelay : s.direct[cL].delay, alignR = soloist ? firstDelay : s.direct[cR].delay;
    // a featured source (moved out of the formation): a spot follows them, panned where they stand as seen from the
    // main microphones, filtered like them; the main pair carries less of them
    {
      const spot = s.direct[FOCUS_TAP];
      if (s.focus > 0) {
        let meanGain = 0;
        for (let c = 0; c < main.numCapsules; ++c) meanGain += Math.abs(s.direct[c].gainL) + Math.abs(s.direct[c].gainR);
        meanGain /= main.numCapsules;
        const keep = 1 - FOCUS_MAIN_SHARE * s.focus;
        const lp = 0.5 * (s.direct[cL].lpCoef + s.direct[cR].lpCoef);
        for (let c = 0; c < main.numCapsules; ++c) {
          const t = s.direct[c]; t.gainL *= keep; t.gainR *= keep;
          if (soloist) { t.delay = t.delayR = firstDelay; t.lpCoef = lp; }
        }
        const azimuth = Math.atan2(s.pos.x - main.position.x, Math.max(0.05, Math.abs(s.pos.y - main.position.y)));
        const pan = clamp(azimuth / (FOCUS_WIDTH_DEG * PI / 180), -1, 1);
        const g = FOCUS_SPOT_GAIN * s.focus * meanGain;
        spot.gainL = g * Math.cos((pan + 1) * PI * 0.25); spot.gainR = g * Math.sin((pan + 1) * PI * 0.25);
        spot.delay = alignL; spot.delayR = alignR; spot.lpCoef = lp; spot.valid = true;
        s.lean = pan * FOCUS_ROOM_LEAN * s.focus;
      } else { spot.gainL = spot.gainR = 0; spot.delay = spot.prevDelay; spot.delayR = spot.prevDelayR; spot.valid = true; s.lean = 0; }
    }
    // CLOSE spots (every spot hears every source; other sections arrive as bleed), time-aligned to the main pair
    for (let sec = 0; sec < NUM_SECTIONS; ++sec) {
      const t = s.direct[k++];
      this.directTap(t, s, this.spots[sec], 1, null);
      t.delay = alignL; t.delayR = alignR;
    }
    if (s.focus > 0)
      for (let sec = 0; sec < NUM_SECTIONS; ++sec) {
        const t = s.direct[MAX_CAPSULES + sec];
        const keep = soloist ? 0 : 1 - FOCUS_CLOSE_SHARE * s.focus;
        t.gainL *= keep; t.gainR *= keep;
      }
    // aligned, the spots add to the main pair in step: each channel's spots are scaled so the source's direct sound
    // keeps the power it had when they arrived at other times
    for (let ch = 0; ch < 2 && !soloist; ++ch) {
      const mt = s.direct[ch === 0 ? cL : cR];
      const m = mics.mainLevel * Math.abs(ch === 0 ? mt.gainL : mt.gainR);
      let sum = 0, sum2 = 0;
      for (let sec = 0; sec < NUM_SECTIONS; ++sec) {
        const t = s.direct[MAX_CAPSULES + sec];
        const g = mics.closeLevel * Math.abs(ch === 0 ? t.gainL : t.gainR);
        sum += g; sum2 += g * g;
      }
      if (sum <= 1e-9) continue;
      const kk = clamp((Math.sqrt(m * m + sum2) - m) / sum, 0, 1);
      for (let sec = 0; sec < NUM_SECTIONS; ++sec) {
        const t = s.direct[MAX_CAPSULES + sec];
        if (ch === 0) t.gainL *= kk; else t.gainR *= kk;
      }
    }
    // ROOM pair
    for (let c = 0; c < 2; ++c) this.directTap(s.direct[k++], s, roomArr.capsules[c], 1, null);
    if (s.lean !== 0) {
      const [bl, br] = leanGains(s.lean);
      for (let c = 0; c < 2; ++c) { const t = s.direct[k - 2 + c]; t.gainL *= bl; t.gainR *= br; }
    }
    // ---- first-order image sources of the shoebox room
    const p = s.pos;
    const images = [
      v3(2 * geo.xMin - p.x, p.y, p.z), v3(2 * geo.xMax - p.x, p.y, p.z),
      v3(p.x, 2 * geo.yMin - p.y, p.z), v3(p.x, 2 * geo.yMax - p.y, p.z),
      v3(p.x, p.y, -p.z), v3(p.x, p.y, 2 * geo.zMax - p.z),
    ];
    const caps = [main.capsules[0], main.capsules[1], roomArr.capsules[0], roomArr.capsules[1]];
    let r = 0;
    for (let im = 0; im < IMAGES; ++im) {
      // how much the source radiates towards this wall
      const toWall = normalised(sub(images[im], p));
      const radiate = 0.6 + 0.4 * (0.5 + 0.5 * dot(s.facing, toWall));
      const wall = geo.reflection * (im === 4 ? 0.6 : 1) * radiate;   // floor: risers and people absorb
      for (let c = 0; c < 4; ++c) {
        const t = s.reflection[r++];
        const d = sub(caps[c].pos, images[im]);
        const dimg = Math.max(len(d), 1);
        const cosTheta = dot(caps[c].dir, mul(d, -1 / dimg));
        const g = wall * patternGain(caps[c], cosTheta) * REFERENCE_DISTANCE / dimg;
        t.delay = t.delayR = Math.min(lround(f32(dimg / SPEED_OF_SOUND * this.sr)), this.ringSize / 2 - this.maxBlock - 4);
        t.gainL = g * caps[c].gainL; t.gainR = g * caps[c].gainR;
        t.valid = true;
      }
    }
    if (s.lean !== 0) {
      // a featured source's early reflections come more from their side of the room
      const [bl, br] = leanGains(s.lean);
      for (const t of s.reflection) { t.gainL *= bl; t.gainR *= br; }
    }
    if (this.roomKeepsLevel) {
      const kk = clamp(Math.sqrt(len(sub(main.position, s.pos)) / REFERENCE_DISTANCE), 1, 3);
      for (const t of s.reflection) { t.gainL *= kk; t.gainR *= kk; }
    }
    const erCutoff = lerp(11000, 2500, clamp(this.room.absorption, 0, 1));
    s.erLpCoef = 1 - Math.exp(-TWO_PI * Math.min(erCutoff, this.sr * 0.45) / this.sr);
    if (soloist) {
      // a soloist's first sound arrives with the dry vocal: everything it sends, earlier by the main pair's travel time
      let ref = 1 << 30;
      for (let c = 0; c < main.numCapsules; ++c) ref = Math.min(ref, s.direct[c].delay);
      const shift = (t) => { t.delay = Math.max(0, t.delay - ref); t.delayR = Math.max(0, t.delayR - ref); };
      for (let c = 0; c < main.numCapsules; ++c) shift(s.direct[c]);
      for (let q = MAX_CAPSULES; q < MAX_CAPSULES + NUM_SECTIONS + 2; ++q) shift(s.direct[q]);
      if (s.focus > 0) shift(s.direct[FOCUS_TAP]);
      for (const t of s.reflection) shift(t);
    }
    s.dirty = false;
  }
  // adds v[i] * (gain ramp) into the ring starting at 'start'
  addRamp(ring, start, v, n, g, dg) {
    if (g === 0 && dg === 0) return;
    const mask = this.ringMask;
    let pos = start & mask, done = 0;
    while (done < n) {
      const run = Math.min(n - done, mask + 1 - pos);
      if (dg === 0) for (let i = 0; i < run; ++i) ring[pos + i] += v[done + i] * g;
      else for (let i = 0; i < run; ++i) ring[pos + i] += v[done + i] * (g + dg * (done + i));
      done += run; pos = 0;
    }
  }
  // sample i arrives d0 + dd * i samples after it is sung, split between the two slots around that time
  addGlide(ring, d0, dd, v, n, g0, dg) {
    const mask = this.ringMask, wp = this.writePos;
    for (let i = 0; i < n; ++i) {
      const d = f32(d0 + dd * i);
      const whole = Math.trunc(d);
      const frac = d - whole;
      const x = v[i] * (g0 + dg * i);
      const p = wp + i + whole;
      ring[p & mask] += x * (1 - frac);
      ring[(p + 1) & mask] += x * frac;
    }
  }
  // x: what the source sends into the left channel's paths, xR into the right's (the same array for a single
  // singer, as in the plugin; a group of singers sung as one stereo stream carries each channel's own mix of them)
  writeTap(ring, t, x, xR, n, filter, fading, fadeFrom, fadeTo) {
    if (!t.valid) return;
    const silentNow = t.gainL === 0 && t.gainR === 0, silentBefore = t.prevGainL === 0 && t.prevGainR === 0;
    if (silentNow && (!fading || silentBefore)) return;
    let v = x, vR = xR;
    if (filter) {
      let z = t.lpState; const a = t.lpCoef;
      const f = this.scratch;
      for (let i = 0; i < n; ++i) { z += a * (x[i] - z); f[i] = z; }
      z = flush(f32(z));
      t.lpState = z; v = f; vR = f;
      if (xR !== x) {
        let zr = t.lpStateR; const fr = this.scratchR;
        for (let i = 0; i < n; ++i) { zr += a * (xR[i] - zr); fr[i] = zr; }
        t.lpStateR = flush(f32(zr)); vR = fr;
      }
    }
    if (!fading) {
      if (t.gainL !== 0) this.addRamp(ring.l, this.writePos + t.delay, v, n, t.gainL, 0);
      if (t.gainR !== 0) this.addRamp(ring.r, this.writePos + t.delayR, vR, n, t.gainR, 0);
      return;
    }
    const inv = (fadeTo - fadeFrom) / n;
    // the path changed a little (a step of a walk or a drag): the arrival glides to the new time, as a moving voice's
    // does. Two arrivals cross-fading instead would comb-filter for the length of the fade.
    const glideMax = Math.trunc(0.002 * this.sr);
    if (Math.abs(t.delay - t.prevDelay) <= glideMax && Math.abs(t.delayR - t.prevDelayR) <= glideMax) {
      const glide = (r, d0, d1, g0, g1, src) => {
        if (g0 === 0 && g1 === 0) return;
        const dd = d1 - d0;
        this.addGlide(r, d0 + dd * fadeFrom, dd * inv, src, n, g0 + (g1 - g0) * fadeFrom, (g1 - g0) * inv);
      };
      glide(ring.l, t.prevDelay, t.delay, t.prevGainL, t.gainL, v);
      glide(ring.r, t.prevDelayR, t.delayR, t.prevGainR, t.gainR, vR);
      return;
    }
    // a jump: cross-fade from the old arrival time / level to the new one (no pitch swoop)
    const wp = this.writePos;
    this.addRamp(ring.l, wp + t.prevDelay, v, n, t.prevGainL * (1 - fadeFrom), -t.prevGainL * inv);
    this.addRamp(ring.r, wp + t.prevDelayR, vR, n, t.prevGainR * (1 - fadeFrom), -t.prevGainR * inv);
    this.addRamp(ring.l, wp + t.delay, v, n, t.gainL * fadeFrom, t.gainL * inv);
    this.addRamp(ring.r, wp + t.delayR, vR, n, t.gainR * fadeFrom, t.gainR * inv);
  }
  // adds one source's dry block (n <= maxBlock) to the microphones: mono, or monoR given, what it sends into the
  // right channel's paths (see writeTap)
  renderSinger(i, mono, n, monoR = mono) {
    const s = this.singers[i];
    if (!s.active) return;
    // ---- movement: walk towards the target in position snapshots, cross-fading from one snapshot to the next
    let fadeFrom = 1, fadeTo = 1;
    if (!s.initialised) {
      this.computeTaps(s); this.computeTaps(s);      // twice: previous snapshot == current snapshot
      s.initialised = true; s.fading = false; s.fade = 1;
    } else {
      if (!s.fading) {
        const delta = sub(s.target, s.pos);
        const dist = len(delta);
        if (dist > 0.005 || s.dirty) {
          const maxStep = this.freeMovement ? 1e9 : 0.25;      // about 2.5 m/s
          s.pos = dist <= maxStep ? s.target : add(s.pos, mul(delta, maxStep / dist));
          this.computeTaps(s);
          s.fading = true; s.fade = 0;
        }
      }
      if (s.fading) {
        fadeFrom = s.fade;
        fadeTo = Math.min(1, f32(s.fade + n / (0.1 * this.sr)));
        s.fade = fadeTo;
      }
    }
    const fading = s.fading;
    // a silent source whose filters have settled adds nothing anywhere: skip the writes
    {
      let silent = s.erLpState === 0 && s.erLpStateR === 0;
      for (let k = 0; k < n && silent; ++k) silent = mono[k] === 0 && monoR[k] === 0;
      for (let k = 0; k < DIRECT_TAPS && silent; ++k) silent = s.direct[k].lpState === 0 && s.direct[k].lpStateR === 0;
      if (silent) { if (fading && fadeTo >= 1) s.fading = false; return; }
    }
    let k = 0;
    for (let c = 0; c < MAX_CAPSULES; ++c) this.writeTap(this.ringMain, s.direct[k++], mono, monoR, n, true, fading, fadeFrom, fadeTo);
    for (let sec = 0; sec < NUM_SECTIONS; ++sec) this.writeTap(this.ringClose, s.direct[k++], mono, monoR, n, true, fading, fadeFrom, fadeTo);
    for (let c = 0; c < 2; ++c) this.writeTap(this.ringRoom, s.direct[k++], mono, monoR, n, true, fading, fadeFrom, fadeTo);
    this.writeTap(this.ringMain, s.direct[FOCUS_TAP], mono, monoR, n, true, fading, fadeFrom, fadeTo);
    // reflections share one wall-absorption filtered copy
    let erR = this.erFiltered;
    {
      let z = s.erLpState; const a = s.erLpCoef;
      const f = this.erFiltered;
      for (let q = 0; q < n; ++q) { z += a * (mono[q] - z); f[q] = z; }
      s.erLpState = flush(f32(z));
      if (monoR !== mono) {
        let zr = s.erLpStateR; const fr = this.erFilteredR;
        for (let q = 0; q < n; ++q) { zr += a * (monoR[q] - zr); fr[q] = zr; }
        s.erLpStateR = flush(f32(zr)); erR = fr;
      }
    }
    let r = 0;
    for (let im = 0; im < IMAGES; ++im)
      for (let c = 0; c < 4; ++c)
        this.writeTap(c < 2 ? this.erMain : this.erRoom, s.reflection[r++], this.erFiltered, erR, n, false, fading, fadeFrom, fadeTo);
    if (fading && fadeTo >= 1) s.fading = false;
  }
  // finishes the block: dirL/R = the choir as the main and spot microphones hear it, roomOutL/R = room microphones,
  // early reflections and the late field
  finishBlock(dirL, dirR, roomOutL, roomOutR, n) {
    const pull = (ring, l, r, gain, addTo) => {
      let pos = this.writePos, done = 0;
      while (done < n) {
        const run = Math.min(n - done, this.ringSize - pos);
        const sl = ring.l, sr = ring.r;
        if (addTo) for (let i = 0; i < run; ++i) { l[done + i] += sl[pos + i] * gain; r[done + i] += sr[pos + i] * gain; }
        else for (let i = 0; i < run; ++i) { l[done + i] = sl[pos + i] * gain; r[done + i] = sr[pos + i] * gain; }
        sl.fill(0, pos, pos + run); sr.fill(0, pos, pos + run);
        done += run; pos = (pos + run) & this.ringMask;
      }
    };
    const mics = this.mics, room = this.room;
    roomOutL.fill(0, 0, n); roomOutR.fill(0, 0, n);
    pull(this.ringMain, dirL, dirR, mics.mainLevel, false);
    pull(this.ringClose, dirL, dirR, mics.closeLevel, true);
    pull(this.ringRoom, roomOutL, roomOutR, mics.roomLevel, true);
    // early reflections at the main and room arrays; together they excite the late field
    const { tmpL, tmpR, lateMainL, lateMainR, lateRoomL, lateRoomR } = this;
    pull(this.erMain, tmpL, tmpR, 1, false);
    pull(this.erRoom, lateMainL, lateMainR, 1, false);   // temporarily holds the room-array reflections
    for (let i = 0; i < n; ++i) { lateRoomL[i] = tmpL[i] + 0.5 * lateMainL[i]; lateRoomR[i] = tmpR[i] + 0.5 * lateMainR[i]; }
    const early = room.earlyLevel;
    for (let i = 0; i < n; ++i) {
      roomOutL[i] += early * (mics.mainLevel * tmpL[i] + mics.roomLevel * lateMainL[i]);
      roomOutR[i] += early * (mics.mainLevel * tmpR[i] + mics.roomLevel * lateMainR[i]);
    }
    // late field (excitation in lateRoomL/R; processed into the four buffers)
    tmpL.set(lateRoomL.subarray(0, n)); tmpR.set(lateRoomR.subarray(0, n));
    this.late.process(tmpL, tmpR, lateMainL, lateMainR, lateRoomL, lateRoomR, n);
    const mc = this.mainArray.capsules;
    const mainDiffuse = 0.5 * (DIFFUSE[mc[0].pattern] + DIFFUSE[mc[1].pattern]);
    const lm = room.lateLevel * LATE_GAIN * mics.mainLevel * 0.6 * mainDiffuse, lr = room.lateLevel * LATE_GAIN * mics.roomLevel;
    const [bl, br] = this.lateBalance === 0 ? [1, 1] : leanGains(this.lateBalance);
    for (let i = 0; i < n; ++i) {
      roomOutL[i] += bl * (lm * lateMainL[i] + lr * lateRoomL[i]);
      roomOutR[i] += br * (lm * lateMainR[i] + lr * lateRoomR[i]);
    }
    this.writePos = (this.writePos + n) & this.ringMask;
  }
}

function leanGains(lean) {   // constant-power lean: -1 all left .. 1 all right, 0 = both 1
  return [1.41421356 * Math.cos((lean + 1) * PI * 0.25), 1.41421356 * Math.sin((lean + 1) * PI * 0.25)];
}

// ---------------------------------------------------------------- filters (Biquad in Source/Analysis/PitchTracker.h)
class Biquad {
  constructor() { this.b0 = 1; this.b1 = 0; this.b2 = 0; this.a1 = 0; this.a2 = 0; this.z1 = 0; this.z2 = 0; }
  setLowpass(hz, q, sr) {
    const w = TWO_PI * clamp(hz, 5, sr * 0.49) / sr, c = Math.cos(w), s = Math.sin(w), alpha = s / (2 * q), a0 = 1 + alpha;
    this.b0 = (1 - c) * 0.5 / a0; this.b1 = (1 - c) / a0; this.b2 = this.b0; this.a1 = -2 * c / a0; this.a2 = (1 - alpha) / a0;
  }
  setHighpass(hz, q, sr) {
    const w = TWO_PI * clamp(hz, 5, sr * 0.49) / sr, c = Math.cos(w), s = Math.sin(w), alpha = s / (2 * q), a0 = 1 + alpha;
    this.b0 = (1 + c) * 0.5 / a0; this.b1 = -(1 + c) / a0; this.b2 = this.b0; this.a1 = -2 * c / a0; this.a2 = (1 - alpha) / a0;
  }
  setHighShelf(hz, gainDb, sr) {
    const A = Math.pow(10, gainDb / 40), w = TWO_PI * clamp(hz, 5, sr * 0.49) / sr;
    const c = Math.cos(w), s = Math.sin(w), alpha = s * 0.5 * Math.sqrt(2), sq = 2 * Math.sqrt(A) * alpha;
    const a0 = (A + 1) - (A - 1) * c + sq;
    this.b0 = A * ((A + 1) + (A - 1) * c + sq) / a0; this.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
    this.b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
    this.a1 = 2 * ((A - 1) - (A + 1) * c) / a0; this.a2 = ((A + 1) - (A - 1) * c - sq) / a0;
  }
  process(x) {
    const y = this.b0 * x + this.z1;
    this.z1 = this.b1 * x - this.a1 * y + this.z2;
    this.z2 = this.b2 * x - this.a2 * y;
    return y;
  }
}

// main output: untouched below 0.95 of full scale, then a soft knee that never exceeds full scale
function safetyCeiling(x) {
  const a = Math.abs(x);
  if (a <= 0.95) return x;
  const y = 0.95 + 0.05 * Math.tanh((a - 0.95) / 0.05);
  return x < 0 ? -y : y;
}
// the choir on its own: untouched below 0.8, then a soft knee
function softLimit(x) {
  const a = Math.abs(x);
  if (a <= 0.8) return x;
  const y = 0.8 + 0.2 * Math.tanh((a - 0.8) / 0.2);
  return x < 0 ? -y : y;
}

// ---------------------------------------------------------------- the choir: sources on the stage, mixed as the plugin mixes
//
// stage:   the preset's stage.json (room, microphones, gains, every performer's place)
// sources: what is played: [{ section, members: [performer index, ...] }]. One member per source is the plugin
//          exactly; several members sung as one stream (their voices summed) stand at their members' centre.
// air:     optional, the choir's top-end shelf as the plugin sets it while this song plays: [[engine clock, shelf dB,
//          1 = shelving on], ...] (recorded with recordAir on the singers' own voices: it is measured on their dry
//          voices, so it does not depend on where anyone stands). Without it the engine measures the voices it is given.
// Moving: setGroupOffset(members, dx, dy) moves those performers (and so their sources) by dx, dy metres from
// where the formation puts them, as dragging a selection does on the plugin's stage.
class ChoirStage {
  constructor(stage, sources, air = null) {
    this.airEvents = air;
    this.stage = stage;
    this.sr = stage.sampleRate;
    this.block = stage.engineBlock;
    this.sources = sources;
    this.performers = stage.performers.map((p) => ({ section: p.section, home: v3(p.x, p.y, p.z), dx: 0, dy: 0 }));
    this.spatial = new SpatialRenderer(this.sr, this.block, sources.length);
    const m = stage.mics;
    this.mics = { ...m, mainPos: v3(...m.mainPos), roomPos: v3(...m.roomPos) };
    this.home = v3(...m.mainPos);      // YOU (the main microphones) where the preset puts them
    this.listenerMoved = 0;
    this.room = { ...stage.room };
    this.sizeK = stage.stage.sizeScale;
    this.free = stage.stage.free;
    this.circle = stage.stage.formation === 'CIRCLE';
    this.sectionGain = stage.sectionGain.slice();
    this.mono = new Float32Array(this.block); this.monoR = new Float32Array(this.block);
    this.dry = new Float64Array(this.block);
    this.dirL = new Float32Array(this.block); this.dirR = new Float32Array(this.block);
    this.roomL = new Float32Array(this.block); this.roomR = new Float32Array(this.block);
    this.reset();
  }
  reset() {
    this.spatial.reset();
    this.stageValid = false; this.clock = 0;
    this.sectionGainSmoothed = [0, 0, 0, 0];
    this.choirGainSmoothed = 0; this.leadGainSmoothed = 0; this.reverbSmoothed = this.stage.reverb;
    this.air = { voiceHp: new Biquad(), voiceLp: new Biquad(), highHp: new Biquad(), highLp: new Biquad(), shelfL: new Biquad(), shelfR: new Biquad(),
                 voiceE: 0, highE: 0, heardS: 0, cutDb: 0, shelfDb: 1, on: false, next: 0 };
    this.airLog = null;
  }
  // records the shelf's settings as this engine measures them (for ChoirStage's air argument)
  recordAir() { this.airLog = []; return this.airLog; }
  // where performer i stands now (the formation's place, moved by their offset), as StageModel::layout puts them
  placeOf(i) {
    const p = this.performers[i], k = this.sizeK;
    let x = p.home.x + p.dx * k, y = p.home.y + p.dy * Math.sqrt(k);
    if (!this.free) { x = clamp(x, -22, 22); y = this.circle ? clamp(y, -22, 45) : clamp(y, 0.5, 45); }
    return v3(x, y, p.home.z);
  }
  setOffset(i, dx, dy) {
    const p = this.performers[i];
    if (p.dx !== dx || p.dy !== dy) { p.dx = dx; p.dy = dy; this.stageValid = false; }
  }
  setGroupOffset(members, dx, dy) { for (const i of members) this.setOffset(i, dx, dy); }
  // YOU - the main microphones, where the choir is heard from - moved by dx, dy metres from where the preset puts
  // them (within the plugin's MAIN MIC X / Y range). Moved, everybody is placed as heard from where YOU now stand
  // (from 0.4 m moved, fully from 2 m: every singer heard as a featured voice, panned where they stand from there).
  setListener(dx, dy) {
    const x = clamp(this.home.x + dx, -10, 10), y = clamp(this.home.y + dy, -10, 12);
    if (x === this.mics.mainPos.x && y === this.mics.mainPos.y) return;
    this.mics = { ...this.mics, mainPos: v3(x, y, this.home.z) };
    const d = Math.sqrt((x - this.home.x) ** 2 + (y - this.home.y) ** 2);
    this.listenerMoved = smoothstep((d - 0.4) / 1.6);
    this.stageValid = false;
  }
  layout() {
    const first = this.clock === 0;
    const places = this.performers.map((_, i) => this.placeOf(i));
    const sum = [v3(), v3(), v3(), v3()], count = [0, 0, 0, 0];
    let maxY = 1, maxAbsX = 1;
    places.forEach((q, i) => {
      const s = this.performers[i].section;
      sum[s] = add(sum[s], q); count[s]++;
      maxY = Math.max(maxY, q.y); maxAbsX = Math.max(maxAbsX, Math.abs(q.x));
    });
    this.centroids = sum.map((c, s) => (count[s] ? mul(c, 1 / count[s]) : v3(0, 3, 1.6)));
    this.maxY = maxY; this.maxAbsX = maxAbsX;
    this.sources.forEach((src, j) => {
      let c = v3(), dx = 0, dy = 0;
      for (const i of src.members) { c = add(c, places[i]); dx += this.performers[i].dx; dy += this.performers[i].dy; }
      c = mul(c, 1 / src.members.length); dx /= src.members.length; dy /= src.members.length;
      // everyone faces the conductor
      this.spatial.setSingerPosition(j, src.section, c, normalised(v3(-c.x, -c.y, 0)), first);
      // moved out of the formation: a spot follows them (from 0.4 m moved, fully from 2 m)
      const k = this.sizeK, moved = Math.sqrt(dx * dx * k * k + dy * dy * k);
      this.spatial.setSingerFocus(j, Math.max(smoothstep((moved - 0.4) / 1.6), this.listenerMoved));
    });
    this.stageValid = true;
  }
  // One engine block (n = stage.engineBlock samples): input[j] = source j's audio for this block: a Float32Array (one
  // singer, mono) or [left, right] (a group sung as a stereo stream: what reaches the left and the right microphone),
  // leadL / leadR = the lead vocal (or null), leadGain its level (0 = the choir alone). Writes the main stereo output.
  // leadGain null: writes the choir alone as it goes into the plugin's output stage (limited on its own, before the
  // lead is added and the output's ceiling: safetyCeiling(choir + lead) is the main output) - for mixing several
  // choirs (one coming in, one going out) and the lead elsewhere.
  process(input, leadL, leadR, leadGain, outL, outR) {
    const n = this.block, sp = this.spatial;
    if (!this.stageValid) this.layout();
    sp.setRoom(this.room);
    sp.setMics(this.mics);
    sp.setStage(this.centroids, this.maxY, this.maxAbsX);
    // ---- every source through the stage (its section's level ramped as the plugin ramps it)
    const dry = this.dry; dry.fill(0);
    const start = this.sectionGainSmoothed.slice();
    for (let s = 0; s < NUM_SECTIONS; ++s) this.sectionGainSmoothed[s] = this.sectionGain[s];
    const mono = this.mono, monoR = this.monoR;
    for (let j = 0; j < this.sources.length; ++j) {
      const s = this.sources[j].section, x = input[j];
      let g = start[s]; const step = f32((this.sectionGain[s] - start[s]) / n);
      if (x instanceof Float32Array) {
        for (let i = 0; i < n; ++i) { mono[i] = x[i] * g; g = f32(g + step); dry[i] += mono[i]; }
        sp.renderSinger(j, mono, n);
      } else {
        const [xl, xr] = x;
        for (let i = 0; i < n; ++i) { mono[i] = xl[i] * g; monoR[i] = xr[i] * g; g = f32(g + step); dry[i] += 0.5 * (mono[i] + monoR[i]); }
        sp.renderSinger(j, mono, n, monoR);
      }
    }
    sp.finishBlock(this.dirL, this.dirR, this.roomL, this.roomR, n);
    this.applyChoirAir(n);
    // ---- mix: the choir limited on its own, the lead passing through untouched
    const cgT = this.stage.choirGain, dgT = leadGain || 0, rgT = clamp(this.stage.reverb, 0, 1.5);
    let cg = this.choirGainSmoothed, dg = this.leadGainSmoothed, rg = this.reverbSmoothed;
    const cgStep = (cgT - cg) / n, dgStep = (dgT - dg) / n, rgStep = (rgT - rg) / n;
    for (let i = 0; i < n; ++i) {
      const l = this.roomL[i] * rg + this.dirL[i], r = this.roomR[i] * rg + this.dirR[i];
      if (leadGain === null) { outL[i] = softLimit(l * cg); outR[i] = softLimit(r * cg); }
      else {
        const ll = leadL ? leadL[i] : 0, lr = leadR ? leadR[i] : 0;
        outL[i] = safetyCeiling(softLimit(l * cg) + ll * dg);
        outR[i] = safetyCeiling(softLimit(r * cg) + lr * dg);
      }
      cg += cgStep; dg += dgStep; rg += rgStep;
    }
    this.choirGainSmoothed = cgT; this.leadGainSmoothed = dgT; this.reverbSmoothed = rgT;
    this.clock += n;
  }
  // The choir's top end measured on the singers' dry voices (over seconds) and a gentle shelf above 5 kHz cutting it
  // towards a real choir's balance (ChoirEngine::applyChoirAir; the shelf is linear, so one on the sum is the same as
  // the plugin's one per stem and one on the room)
  applyChoirAir(n) {
    const a = this.air, sr = this.sr;
    if (this.airEvents) {
      // as recorded: the shelf set and switched exactly when the plugin does it
      const ev = this.airEvents;
      while (a.next < ev.length && ev[a.next][0] <= this.clock) {
        const [, db, on] = ev[a.next++];
        if (db !== a.shelfDb) { for (const b of [a.shelfL, a.shelfR]) { const z1 = b.z1, z2 = b.z2; b.setHighShelf(5000, db, sr); b.z1 = z1; b.z2 = z2; } a.shelfDb = db; }
        a.on = on === 1;
      }
      if (a.on) this.shelve(n);
      return;
    }
    if (a.shelfDb > 0.5) {      // first use: the measuring filters
      a.voiceHp.setHighpass(150, 0.707, sr); a.voiceLp.setLowpass(3000, 0.707, sr);
      a.highHp.setHighpass(5000, 0.707, sr); a.highLp.setLowpass(Math.min(12000, 0.45 * sr), 0.707, sr);
    }
    let ev = 0, eh = 0;
    for (let i = 0; i < n; ++i) {
      const m = f32(this.dry[i]);
      const v = a.voiceLp.process(a.voiceHp.process(m)), h = a.highLp.process(a.highHp.process(m));
      ev += v * v; eh += h * h;
    }
    for (const b of [a.voiceHp, a.voiceLp, a.highHp, a.highLp]) { b.z1 = flush(b.z1); b.z2 = flush(b.z2); }
    if (ev > 1e-6 * n) {        // the choir sings (its voice band above -60 dB)
      a.heardS = f32(a.heardS + n / sr);
      const tau = Math.min(3, 0.3 + a.heardS);
      const k = 1 - Math.exp(-n / (tau * sr));
      a.voiceE += k * (ev - a.voiceE); a.highE += k * (eh - a.highE);
    }
    if (a.heardS > 0) {
      const balanceDb = 10 * Math.log10(Math.max(1e-20, a.highE) / Math.max(1e-20, a.voiceE));
      const targetDb = -27 + 6 * (clamp(this.stage.power, 0, 1) - 0.5);
      const want = clamp((targetDb - balanceDb) / 0.8, -12, 0);
      a.cutDb += (1 - Math.exp(-n / (0.4 * sr))) * (want - a.cutDb);
    }
    const before = a.shelfDb, wasOn = a.on;
    if (Math.abs(a.cutDb - a.shelfDb) > 0.05) {
      for (const b of [a.shelfL, a.shelfR]) { const z1 = b.z1, z2 = b.z2; b.setHighShelf(5000, a.cutDb, sr); b.z1 = z1; b.z2 = z2; }
      a.shelfDb = a.cutDb;
    }
    a.on = a.cutDb < -0.01;
    if (this.airLog && (a.shelfDb !== before || a.on !== wasOn)) this.airLog.push([this.clock, a.shelfDb, a.on ? 1 : 0]);
    if (a.on) this.shelve(n);
  }
  // the plugin shelves each section and the room before the mix adds them (the room times REVERB): the same filter
  // on that sum, here on the two buses the mix adds
  shelve(n) {
    const a = this.air;
    {
      const rg = this.reverbSmoothed;
      for (let i = 0; i < n; ++i) {
        const l = a.shelfL.process(this.dirL[i] + rg * this.roomL[i]), r = a.shelfR.process(this.dirR[i] + rg * this.roomR[i]);
        this.dirL[i] = l; this.dirR[i] = r; this.roomL[i] = 0; this.roomR[i] = 0;
      }
      a.shelfL.z1 = flush(a.shelfL.z1); a.shelfL.z2 = flush(a.shelfL.z2); a.shelfR.z1 = flush(a.shelfR.z1); a.shelfR.z2 = flush(a.shelfR.z2);
    }
  }
}

// ---------------------------------------------------------------- the lead vocal on the stage
//
// ChoirEngine's leadSpatial and the lead's part of its mix. The lead's own place is just behind the main microphones
// (it sings into them): there it is the acapella exactly as it is. Moved (setOffset, metres from there), it is heard
// where it stands - through the choir's room and microphones, its first sound still arriving with the vocal, its
// reverberation coming from its side of the room - cross-faded from the dry vocal over the first 1.2 m.
const LEAD_HOME_Y = 0.4, LEAD_STAGE_RAMP = 1.2, LEAD_MIN_DISTANCE = 1.5;
const LEAD_PLACED_LEVEL = LEAD_MIN_DISTANCE / 3;   // at the closest it counts as standing, as loud placed as dry
class LeadStage {
  constructor(sampleRate, block) {
    this.block = block;
    this.spatial = new SpatialRenderer(sampleRate, block, 1);
    this.spatial.setMinimumDistance(LEAD_MIN_DISTANCE);
    this.spatial.setRoomKeepsItsLevel(true);
    this.spatial.setSoloist(true);
    this.dx = 0; this.dy = 0; this.staged = false; this.mix = 0;
    this.mono = new Float32Array(block);
    this.dirL = new Float32Array(block); this.dirR = new Float32Array(block);
    this.roomL = new Float32Array(block); this.roomR = new Float32Array(block);
  }
  setOffset(dx, dy) { this.dx = dx; this.dy = dy; }
  // how much of the lead is placed: 0 at home (under 5 mm counts as not moved), all of it from 1.2 m away
  amount() {
    const d = Math.sqrt(this.dx * this.dx + this.dy * this.dy);
    if (d < 0.005) return 0;
    const x = d / LEAD_STAGE_RAMP;
    return x >= 1 ? 1 : x * x * (3 - 2 * x);
  }
  // where the lead stands (the choir's YOU: its main microphones)
  place(choir) { const m = choir.mics.mainPos; return v3(m.x + this.dx, m.y + LEAD_HOME_Y + this.dy, 1.6); }
  // one engine block, after the choir's (choir: the ChoirStage whose room, microphones and stage the lead stands in).
  // inL / inR: the acapella; mono: what the plugin hears of it (its input is one channel). Writes the lead as it
  // reaches the main output (before its level).
  process(inL, inR, choir, outL, outR) {
    const n = this.block, sp = this.spatial, amt = this.amount();
    if (amt > 0 || this.staged) {
      sp.setRoom(choir.room); sp.setMics(choir.mics); sp.freeMovement = choir.free;
      sp.setStage(choir.centroids, choir.maxY, choir.maxAbsX);
      const pos = this.place(choir), m = choir.mics.mainPos;
      let face = v3(m.x - pos.x, m.y - pos.y, 0);
      face = len(face) > 0.01 ? normalised(face) : v3(0, -1, 0);    // facing the microphones
      sp.setSingerPosition(0, 0, pos, face, !this.staged);
      sp.setSingerFocus(0, 1);              // always a featured voice: heard exactly where it stands
      sp.setLateBalance(sp.lean(0));        // and its reverberation comes from its side of the room too
      this.staged = true;
    }
    const staging = this.staged, from = this.mix, to = amt;
    if (staging) {
      const mono = this.mono;
      for (let i = 0; i < n; ++i) mono[i] = inR === inL ? inL[i] : 0.5 * (inL[i] + inR[i]);
      sp.renderSinger(0, mono, n);
      sp.finishBlock(this.dirL, this.dirR, this.roomL, this.roomR, n);
    }
    const rg = choir.reverbSmoothed;
    for (let i = 0; i < n; ++i) {
      const dl = inL[i], dr = inR[i];
      if (!staging) { outL[i] = dl; outR[i] = dr; continue; }
      const m = from + (to - from) * (i + 1) / n;
      const pl = LEAD_PLACED_LEVEL * (this.dirL[i] + this.roomL[i] * rg), pr = LEAD_PLACED_LEVEL * (this.dirR[i] + this.roomR[i] * rg);
      outL[i] = dl + (pl - dl) * m; outR[i] = dr + (pr - dr) * m;
    }
    this.mix = to;
    if (staging && to <= 0) { this.staged = false; sp.reset(); }   // back at the microphones: dry again
  }
}

// ---------------------------------------------------------------- the website's extra reverb (not in the plugin)
//
// A hall under every choir on the website, on top of the plugin's own room: the plugin's late-field design (the
// 8-line feedback delay network above), longer and smoother. Fed only with the choir above 250 Hz (so the low end stays
// clear: no mud), through a 25 ms pre-delay, its top softened above 7 kHz; the tail darkens as it decays.
// set(seconds, gain): its length (RT60) and level; changes glide over one block.
class Hall {
  constructor(sampleRate, block) {
    this.sr = sampleRate; this.block = block;
    this.late = new LateReverb(sampleRate);
    this.preLen = Math.round(0.025 * sampleRate);
    this.preL = new Float32Array(this.preLen); this.preR = new Float32Array(this.preLen); this.prePos = 0;
    this.filters = [0, 1].map(() => {
      const hp1 = new Biquad(), hp2 = new Biquad(), lp = new Biquad();
      hp1.setHighpass(250, 0.707, sampleRate); hp2.setHighpass(250, 0.707, sampleRate); lp.setLowpass(7000, 0.707, sampleRate);
      return [hp1, hp2, lp];
    });
    this.inL = new Float32Array(block); this.inR = new Float32Array(block);
    this.mL = new Float32Array(block); this.mR = new Float32Array(block); this.rL = new Float32Array(block); this.rR = new Float32Array(block);
    this.seconds = 0; this.gain = 0; this.gainNow = 0;
  }
  set(seconds, gain) {
    if (seconds !== this.seconds) {
      const room = { size: 0.85, absorption: 0.35, diffusion: 0.8, seconds, earlyLevel: 0, lateLevel: 1 };
      this.late.configure(room, roomGeometry(room, 8, 6));
      this.seconds = seconds;
    }
    this.gain = gain;
  }
  // adds the hall of inL / inR (the choir) to outL / outR
  process(inL, inR, outL, outR) {
    const n = this.block, [fl, fr] = this.filters, { preL, preR, preLen } = this;
    for (let i = 0; i < n; ++i) {
      const l = fl[2].process(fl[1].process(fl[0].process(inL[i]))), r = fr[2].process(fr[1].process(fr[0].process(inR[i])));
      this.inL[i] = preL[this.prePos]; this.inR[i] = preR[this.prePos];
      preL[this.prePos] = l; preR[this.prePos] = r;
      if (++this.prePos >= preLen) this.prePos = 0;
    }
    for (const f of [...fl, ...fr]) { f.z1 = flush(f.z1); f.z2 = flush(f.z2); }
    this.late.process(this.inL, this.inR, this.mL, this.mR, this.rL, this.rR, n);
    const g0 = this.gainNow, g1 = this.gain, step = (g1 - g0) / n;
    for (let i = 0; i < n; ++i) { const g = g0 + step * (i + 1); outL[i] += g * (this.mL[i] + this.rL[i]); outR[i] += g * (this.mR[i] + this.rR[i]); }
    this.gainNow = g1;
  }
}

// When and how loud a singer standing at p reaches the main microphone feeding each channel (fractional samples,
// linear gain; the singer faces the conductor): to build a group's stereo stream, each member put into each channel
// as much later and quieter as it arrives there than the group's centre does.
function mainArrivals(stage, p) {
  const m = stage.mics, sr = stage.sampleRate;
  const arr = buildArray(m.mainType, v3(...m.mainPos), 0, m.mainSpacing);
  const pos = v3(p.x, p.y, p.z), facing = normalised(v3(-p.x, -p.y, 0));
  const hits = arr.capsules.slice(0, arr.numCapsules).map((cap) => {
    const d = sub(cap.pos, pos), dist = Math.max(len(d), 0.3), toMic = mul(d, 1 / dist);
    const frontness = 0.5 + 0.5 * dot(facing, toMic), cosTheta = dot(cap.dir, mul(toMic, -1));
    const gain = patternGain(cap, cosTheta) * (REFERENCE_DISTANCE / dist) * (0.7 + 0.3 * frontness);
    return { delay: dist / SPEED_OF_SOUND * sr, gainL: gain * cap.gainL, gainR: gain * cap.gainR };
  });
  let cL = 0, cR = 0;
  hits.forEach((h, c) => { if (Math.abs(h.gainL) > Math.abs(hits[cL].gainL)) cL = c; if (Math.abs(h.gainR) > Math.abs(hits[cR].gainR)) cR = c; });
  return { L: { delay: hits[cL].delay, gain: Math.abs(hits[cL].gainL) }, R: { delay: hits[cR].delay, gain: Math.abs(hits[cR].gainR) } };
}

root.OmniChoir = { mainArrivals, safetyCeiling, softLimit, LeadStage, LEAD_HOME_Y, Hall, ChoirStage, SpatialRenderer, LateReverb, Biquad, roomGeometry, buildArray, NUM_SECTIONS };
})(typeof globalThis !== 'undefined' ? globalThis : this);
