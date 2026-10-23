/* OMNICHOIR DEMO: the plugin dev's page script with theme-build patches marked 'theme build' (scratchpad omnichoir_demo_build.py, 2026-10-21). Do not hand-edit: re-run the build. */
(() => {
'use strict';
// OMNICHOIR website demo: the page, the whole window a stage. It opens close on the lead singer, a play button on her.
// PLAY: she sings her first line alone, then the choir comes in with her second and the camera pulls back to it; a
// hand then shows that the singers can be dragged. From then on everything on the stage can be moved: the choir's
// parts, the lead and the microphone. The sound is made on the audio thread (engine/choir-worklet.js) by the plugin's own stage code
// (engine/choir-stage.js).

// ---------------------------------------------------------------- what the page plays
// ALONE (the female vocal) with EPIC 64. labels: what each section (part of the choir) is called, by section number.
// hall: the website's extra reverb under the choir (not in the plugin): its length (RT60, seconds) and level, set so
// the hall sits 12 dB under the choir; fed only with the choir above 250 Hz, so it adds little to the low end.
const VOCAL = 'female';
const CHOIR = { slug: 'epic-64', labels: ['Sopranos', 'Altos', 'Tenors', 'Basses'], hall: { seconds: 2.4, gain: 0.195 } };
const KEY = `${VOCAL}/${CHOIR.slug}`;
// where the choir can come in: the starts of her lines (song seconds into the loop). It comes in on the first: "I gave
// you my trust" (0.07 - 1.53 s) is hers alone, and the choir comes in on "but you treated me like I couldn't see" (from
// 1.54 s, straight after the "st" of "trust"). The others (her later lines) are only for a choir that arrives late: it
// waits for the next of them.
const LINES = [1.54, 6.0, 11.75, 14.87, 18.05, 21.58, 24.93];
// the choir fades in over CHOIR_FADE_IN seconds in the silence just before her line (the choir is silent there too),
// so it is all there on the line's first word; the camera starts pulling back half its move before it
const CHOIR_FADE_IN = 0.08;
const CUE_DELAY = 0.6;       // seconds after the camera has pulled back to the choir before the hand shows
const SECTION_COLOURS = ['rose', 'amber', 'jade', 'sapphire'];   // as on the plugin's stage
const DOWNLOAD_URL = '';     // where DOWNLOAD PLUGIN goes (the plugin's page) - not given yet
const SR = 48000;
// the acapella's level against the choir, dB (0 = the plugin's own balance, where the choir sits 0 - 6 dB under the
// vocal): lower, so the choir - and where its singers stand - is what the visitor hears
const LEAD_DB = -6;

// ---------------------------------------------------------------- state
// phase: loading (PLAY not ready) > ready > solo (the lead alone; nothing can be moved yet) > choir (the choir in,
//   everything can be moved, the hand showing) > free (something has been dragged: no more help)
// lead: where the visitor has moved the lead singer ([dx, dy] metres from its place behind the microphone)
// mic: where the visitor has moved the microphone (YOU), [dx, dy] metres from where the choir has it
// sung: seconds of the song played so far (from the audio thread)
// choirAt: the song time of the line the choir comes in on (once it is here); loop: the song's length (seconds)
const state = { phase: 'loading', playing: false, choirOn: false, choirReady: false, choirAt: Infinity, loop: Infinity, dragged: false, sung: 0, cueAt: Infinity, offsets: null, lead: [0, 0], mic: [0, 0] };
let ctx = null, node = null, leadLevel = 0;
const $ = (s) => document.querySelector(s.replace(/#([\w-]+)/g, '#oc-$1'));   // theme build: ids are prefixed in the snippet

// ---------------------------------------------------------------- loading, what is needed first first
// Two stages, the second started once the first's acapella is in:
//   first - what PLAY needs: the engine, the stage's plan, the acapella, the nave, the lead and the microphone
//   choir - what the choir needs: its four parts and its singers (heard LINES[0] seconds after PLAY at the earliest)
// Each file is read as it streams in, so the page can show how far along it is. A file that fails is tried again
// (after 0.8, 1.6 and 3.2 s) before the page gives up.
const EST = { mp3: 1.24e6, png: 8.7e4, jpg: 8.8e4, js: 3.5e4, json: 2e4 };   // a file's size until the server says
const progress = { first: new Map(), choir: new Map() };
function setProgress(group, url, got, total) { progress[group].set(url, [got, total]); showProgress(); }
function fraction(group) {
  let got = 0, total = 0;
  for (const [g, t] of progress[group].values()) { got += Math.min(g, t); total += t; }
  return total ? got / total : 0;
}
const wait = (s) => new Promise((ok) => setTimeout(ok, s * 1000));
async function load(url, group, priority = 'auto') {
  const est = EST[url.split('.').pop()] || 5e4;
  setProgress(group, url, 0, est);
  for (let attempt = 0; ; ++attempt) {
    try {
      const r = await fetch((window.OCDEMO && window.OCDEMO.assets && window.OCDEMO.assets[url]) || url, { priority });   // theme build: Shopify asset_url map
      if (!r.ok) throw new Error(`${r.status} ${url}`);
      const total = Number(r.headers.get('content-length')) || est;
      if (!r.body || !r.body.getReader) { const b = await r.arrayBuffer(); setProgress(group, url, b.byteLength, b.byteLength); return b; }
      const reader = r.body.getReader(), chunks = [];
      let got = 0;
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value); got += value.length;
        setProgress(group, url, got, Math.max(total, got));
      }
      const out = new Uint8Array(got);
      let o = 0; for (const c of chunks) { out.set(c, o); o += c.length; }
      setProgress(group, url, got, got);
      return out.buffer;
    } catch (e) {
      if (attempt >= 3) throw e;
      console.warn('retrying', url, e.message);
      setProgress(group, url, 0, est);
      await wait(0.8 * 2 ** attempt);
    }
  }
}
const loadText = async (url, group, priority) => new TextDecoder().decode(await load(url, group, priority));
const loadJson = async (url, group, priority) => JSON.parse(await loadText(url, group, priority));
const sprites = {};
async function loadSprite(name, url, group) {
  const img = new Image();
  img.src = URL.createObjectURL(new Blob([await load(url, group)]));
  await img.decode().catch(() => {});
  sprites[name] = img;
}

// ---------------------------------------------------------------- audio
async function startEngine(code) {
  // the engine's two files, joined and handed to the audio thread as one module
  await ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([code.join('\n')], { type: 'text/javascript' })));
  node = new AudioWorkletNode(ctx, 'omnichoir', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
  node.connect(ctx.destination);
  node.port.postMessage({ type: 'leadLevel', gain: Math.pow(10, LEAD_DB / 20) });
  node.port.postMessage({ type: 'choirOn', on: false });   // the lead alone, until the choir comes in
  node.port.onmessage = (e) => {
    const m = e.data;
    if (m.type === 'levels') { leadLevel = m.lead; state.sung = m.played; }
  };
}

// decodes a compressed file into Int16 channels of exactly 'length' samples (the encoder's own padding dropped)
async function decode(bytes, length) {
  const buf = await ctx.decodeAudioData(bytes);
  const out = [];
  // (decoders that keep the encoder's leading priming samples give a longer file; the audio is then at its end)
  const skip = Math.max(0, buf.length - length);
  for (let c = 0; c < buf.numberOfChannels; ++c) {
    const x = buf.getChannelData(c), y = new Int16Array(length);
    for (let i = 0; i < length; ++i) { const v = x[i + skip] || 0; y[i] = v >= 1 ? 32767 : v <= -1 ? -32768 : Math.round(v * 32767); }
    out.push(y);
  }
  return out;
}

// the choir: its parts downloaded together and decoded as each comes in, then handed to the audio thread (silent until
// it comes in, but already keeping time with the lead); its singers' pictures alongside
async function loadChoir(plan, leadSent) {
  const m = await plan;
  const looks = Promise.all(SECTION_COLOURS.flatMap((c) => ['rest', 'lit'].map((k) => loadSprite(`${c}_${k}`, `assets/singer_${c}_${k}.png`, 'choir'))));
  const parts = await Promise.all(m.sources.map(async (s) => decode(await load(`audio/${VOCAL}/${CHOIR.slug}/${s.file}`, 'choir'), m.loop + m.tail)));
  const channels = parts.flat();
  await leadSent;
  node.port.postMessage({ type: 'choir', key: KEY, manifest: m, channels }, channels.map((c) => c.buffer));
  node.port.postMessage({ type: 'select', vocal: VOCAL, key: KEY, offsets: state.offsets, hall: CHOIR.hall });
  await looks;
  // in at the next of her lines: the second, unless the song is already past it; the audio thread brings it in on time
  const early = CHOIR_FADE_IN + 0.01, into = state.sung % state.loop, pass = state.sung - into;
  const next = LINES.find((t) => t - early > into + 0.05);
  state.choirAt = next != null ? pass + next : pass + state.loop + LINES[0];
  node.port.postMessage({ type: 'choirOn', on: true, at: state.choirAt - early, fade: CHOIR_FADE_IN });
  state.choirReady = true;
  waitEl.classList.remove('on');
  showMode();
}

// ---------------------------------------------------------------- the stage (as the plugin draws it: the choir seen
// from behind the listener, little people on the floor of the nave)
//
// Two looks, and a camera move between them:
//   choir off - close on the lead singer, in the middle of the window, glowing as it sings; the nave dark (its light
//               from the ceiling out), the choir not there
//   choir on  - the whole choir, all lit up, the nave lit
// Everything is drawn in "stage" coordinates (the full view) and the camera is a zoom on top: screen = Z * stage + t.
const canvas = $('#stage'), g2 = canvas.getContext('2d');
let stage = null;   // { m, groupOf, offsets, cam, boost, W, H, dpr }
const FIGURE_M = 0.70;           // a singer's height on the stage (metres), as the plugin draws them
const BACK_EDGE = 0.575, DEPTH_RATIO = 1.6;
const LOOK_SECONDS = { out: 0.55, in: 0.65 };   // the camera move: out to the whole choir, back in on the lead
// the lead's glow follows its phrases, not its syllables: on as it sings, kept on through the short gaps between
// syllables and words, off only after a real pause (HOLD seconds without a sound); faded in and out, never flicked
const GLOW = { HOLD: 0.7, ATTACK: 0.08, RELEASE: 0.45 };
const glow = { amount: 0, lastSound: -1e9, lastFrame: 0 };
function leadGlow() {
  const now = performance.now() / 1000, dt = Math.min(0.1, now - (glow.lastFrame || now));
  glow.lastFrame = now;
  if (state.playing && (leadLevel || 0) > 0.02) glow.lastSound = now;
  const want = state.playing && now - glow.lastSound < GLOW.HOLD ? 1 : 0;
  const tau = want > glow.amount ? GLOW.ATTACK : GLOW.RELEASE;
  glow.amount += (want - glow.amount) * (1 - Math.exp(-dt / tau));
  return glow.amount;
}
const look = { e: 0, from: 0, to: 0, start: 0, lead: null };   // e: 0 = on the lead (choir off) .. 1 = the whole choir

function camera(m, W, H) {
  // everything in view (the choir, with room to move it around) - fixed while the choir is on stage
  let xMin = -3, xMax = 3, yMin = -1.6, yMax = 4;      // (YOU at the front, with room in front of it)
  for (const p of m.stage.performers) { xMin = Math.min(xMin, p.x - 1); xMax = Math.max(xMax, p.x + 1); yMin = Math.min(yMin, p.y - 1); yMax = Math.max(yMax, p.y + 1.2); }
  const room = W < 640 ? 0.5 : 0.8;      // (room to move the choir about, beside it)
  xMin -= room; xMax += room; yMax += 2.5;
  const front = H - 30, back = H * BACK_EDGE;
  const c = { cx: W * 0.5, xc: 0.5 * (xMin + xMax), bounds: { xMin, xMax, yMin: Math.max(0.5, yMin + 0.6), yMax }, view: { xMin, xMax, yMin, yMax } };
  c.horizon = (DEPTH_RATIO * back - front) / (DEPTH_RATIO - 1);
  c.yCam = ((back - c.horizon) * yMax - (front - c.horizon) * yMin) / (back - front);
  const zNear = yMin - c.yCam;
  c.k = (front - c.horizon) * zNear;
  let f = 120 * (W / 1000) * zNear;
  for (const p of m.stage.performers) f = Math.min(f, (W * 0.5 - 24) * Math.max(0.5, p.y - c.yCam) / (Math.abs(p.x - c.xc) + 0.6));
  c.f = f;
  return c;
}
const toScreen = (c, x, y) => { const z = Math.max(0.25, y - c.yCam); return [c.cx + c.f * (x - c.xc) / z, c.horizon + c.k / z]; };
const toWorld = (c, sx, sy) => { const z = c.k / (Math.max(sy, c.horizon + 3) - c.horizon); return [c.xc + (sx - c.cx) * z / c.f, c.yCam + z]; };
const ppm = (c, y) => c.f / Math.max(0.25, y - c.yCam);

function showStage(m) {
  hoverGroup = -1; drag = null;
  if (!state.offsets) state.offsets = m.groups.map(() => [0, 0]);
  const groupOf = new Array(m.stage.performers.length);
  m.groups.forEach((members, gi) => members.forEach((i) => { groupOf[i] = gi; }));
  stage = { m, groupOf, offsets: state.offsets };
  resize();
}

function resize() {
  const r = canvas.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(r.width * dpr); canvas.height = Math.round(r.height * dpr);
  if (stage) {
    stage.W = r.width; stage.H = r.height; stage.dpr = dpr;
    stage.cam = camera(stage.m, r.width, r.height);
    // big enough to see and grab: everyone (the choir, the lead, the microphone) drawn larger than the plugin draws them,
    // alike, so the middle singer of the choir is at least 7.5 % of the stage's width tall on a computer (9 % on a
    // phone) however wide the choir is (a wide choir's people then stand shoulder to shoulder; never more than 3.5
    // times the plugin's size)
    const hs = stage.m.stage.performers.map((p) => FIGURE_M * ppm(stage.cam, p.y)).sort((a, b) => a - b);
    stage.boost = Math.max(1, Math.min(3.5, (r.width < 640 ? 0.09 : 0.075) * r.width / hs[hs.length >> 1]));
    look.lead = leadView();
  }
}

// the camera close on the lead: its figure about a third of the window's height, in the middle of the window (where
// PLAY sits on it; past the edges of the full view there is only the dark of the nave)
function leadView() {
  const { W, H } = stage, [x, y, w, h] = figureRect(LEAD);
  const Z = Math.max(1.4, Math.min(3.5, (W < 640 ? 0.3 : 0.36) * H / h));
  return { Z, tx: 0.5 * W - Z * (x + w / 2), ty: 0.5 * H - Z * (y + h / 2) };
}
const ease = (t) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2);
function lookTo(on) {
  look.from = look.e; look.to = on ? 1 : 0; look.start = performance.now();
  if (!on && stage) look.lead = leadView();      // close on the lead where it stands now
}
// the camera now: { Z, tx, ty } between the lead (e = 0) and the whole choir (e = 1)
function view() {
  const t = Math.min(1, (performance.now() - look.start) / ((look.to > look.from ? LOOK_SECONDS.out : LOOK_SECONDS.in) * 1000));
  look.e = look.from + (look.to - look.from) * ease(t);
  const L = look.lead || { Z: 1, tx: 0, ty: 0 }, e = look.e;
  return { Z: L.Z + (1 - L.Z) * e, tx: L.tx * (1 - e), ty: L.ty * (1 - e), e };
}

// where performer i stands now: their place in the formation, moved with their group (as the engine places them)
function placeOf(i) {
  if (i === LEAD) return leadPlace();
  if (i === MIC) return micPlace();
  const p = stage.m.stage.performers[i], [dx, dy] = stage.offsets[stage.groupOf[i]], k = stage.m.stage.stage.sizeScale;
  return [Math.max(-22, Math.min(22, p.x + dx * k)), Math.max(0.5, Math.min(45, p.y + dy * Math.sqrt(k)))];
}
// YOU: the microphone the choir is heard from (and the lead sings into), where the preset puts it, moved by the visitor
const LEAD = -2, MIC = -3, LEAD_SCALE = 1.06, MIC_SCALE = 1.12, LEAD_HOME_Y = 0.4;
const MIC_RANGE = { xMin: -10, xMax: 10, yMin: -10, yMax: 12 };   // the plugin's MAIN MIC X / Y
function micPlace() {
  const m = stage.m.stage.mics.mainPos;
  return [Math.max(MIC_RANGE.xMin, Math.min(MIC_RANGE.xMax, m[0] + state.mic[0])), Math.max(MIC_RANGE.yMin, Math.min(MIC_RANGE.yMax, m[1] + state.mic[1]))];
}
// the lead singer: its own place just behind the microphone, moved by the visitor (its offset is from the microphone)
function leadPlace() {
  const [mx, my] = micPlace();
  return [mx + state.lead[0], my + LEAD_HOME_Y + state.lead[1]];
}

// a figure's rectangle on the stage (stage coordinates)
function figureRect(i) {
  const c = stage.cam, [x, y] = placeOf(i), [fx, fy] = toScreen(c, x, y);
  const scale = stage.boost * (i === LEAD ? LEAD_SCALE : i === MIC ? MIC_SCALE : 1);
  const h = Math.max(12, FIGURE_M * scale * ppm(c, y));
  const H = (i === MIC ? 0.98 : 0.96) * h, Wd = H * (i === MIC ? 133 / 400 : 248 / 448);
  return [fx - 0.5 * Wd, fy + 0.035 * h - H, Wd, H];
}
// the ring of light round the microphone's base, a circle: [centre x, centre y, radius, radius]
function micRing() {
  const c = stage.cam, [x, y] = micPlace(), [fx, fy] = toScreen(c, x, y);
  const h = Math.max(16, FIGURE_M * MIC_SCALE * stage.boost * ppm(c, y));
  const r = 0.36 * h; return [fx, fy - 0.02 * h, r, r];
}
const groupOfFigure = (i) => (i === LEAD || i === MIC ? i : stage.groupOf[i]);

// the nave picture, filling the stage as a background would ('cover', its window a little above the middle)
function drawNave(W, H) {
  const img = sprites.nave; if (!img) return;
  const s = Math.max(W / img.width, H / img.height), w = img.width * s, h = img.height * s;
  g2.drawImage(img, (W - w) / 2, (H - h) * 0.38, w, h);
}

// the part of the choir the hand shows: the one nearest the middle of the window
function cueGroup(onScreen) {
  let best = 0, bestD = Infinity;
  stage.m.groups.forEach((members, g) => {
    const x = members.reduce((s, i) => { const [rx, , rw] = onScreen(figureRect(i)); return s + rx + rw / 2; }, 0) / members.length;
    const d = Math.abs(x - stage.W / 2);
    if (d < bestD) { bestD = d; best = g; }
  });
  return best;
}

let hoverGroup = -1, drag = null;
const cueEl = $('#cue');
function draw() {
  requestAnimationFrame(draw);
  flow();
  if (!stage || !stage.cam) return;
  const { W, H, dpr } = stage, c = stage.cam, P = stage.m.stage.performers;
  const v = view(), e = v.e;
  const onScreen = ([x, y, w, h]) => [v.Z * x + v.tx, v.Z * y + v.ty, v.Z * w, v.Z * h];
  // the hand, once the camera has pulled back to the choir, until something has been dragged
  const cueOn = state.phase === 'choir' && state.choirOn && look.e >= 1 && !drag && performance.now() >= state.cueAt;
  const cued = cueOn ? cueGroup(onScreen) : -1;
  g2.setTransform(dpr, 0, 0, dpr, 0, 0);
  g2.clearRect(0, 0, W, H);
  // ---- the stage, through the camera
  g2.setTransform(dpr * v.Z, 0, 0, dpr * v.Z, dpr * v.tx, dpr * v.ty);
  g2.fillStyle = '#07060a'; g2.fillRect(-W, -H, 3 * W, 3 * H);
  drawNave(W, H);
  const [lx, ly, lw, lh] = figureRect(LEAD), lcx = lx + lw / 2, lcy = ly + lh * 0.55;
  // choir off: the nave dark (the light from the ceiling gone), only a pool of light where the lead stands
  if (e < 1) {
    const r0 = lh * 0.9, r1 = Math.max(W, H) * 0.9;
    const dark = g2.createRadialGradient(lcx, lcy, r0, lcx, lcy, r1);
    dark.addColorStop(0, `rgba(6, 5, 9, ${0.35 * (1 - e)})`); dark.addColorStop(0.25, `rgba(6, 5, 9, ${0.86 * (1 - e)})`); dark.addColorStop(1, `rgba(6, 5, 9, ${0.94 * (1 - e)})`);
    g2.fillStyle = dark; g2.fillRect(-W, -H, 3 * W, 3 * H);
  }
  // choir on: a soft pool of light where the choir stands
  {
    const [, fy] = toScreen(c, 0, c.bounds.yMin);
    const grad = g2.createRadialGradient(W / 2, fy, 10, W / 2, fy, W * 0.6);
    grad.addColorStop(0, `rgba(255, 196, 110, ${0.10 * e})`); grad.addColorStop(1, 'rgba(255, 196, 110, 0)');
    g2.fillStyle = grad; g2.fillRect(0, c.horizon, W, H - c.horizon);
  }
  const touched = drag ? drag.group : hoverGroup !== -1 ? hoverGroup : cued;
  // the microphone's ring of light on the floor
  {
    const [cx, cy, rx, ry] = micRing(), on = touched === MIC;
    const glow = g2.createRadialGradient(cx, cy, 0, cx, cy, rx * 1.7);
    glow.addColorStop(0, `rgba(232, 194, 122, ${on ? 0.45 : 0.3})`); glow.addColorStop(1, 'rgba(232, 194, 122, 0)');
    g2.save(); g2.translate(cx, cy); g2.scale(1, ry / rx); g2.fillStyle = glow; g2.beginPath(); g2.arc(0, 0, rx * 1.7, 0, 2 * Math.PI); g2.fill(); g2.restore();
    g2.strokeStyle = `rgba(232, 194, 122, ${on ? 1 : 0.8})`; g2.lineWidth = (on ? 1.8 : 1.2) / v.Z;
    g2.beginPath(); g2.ellipse(cx, cy, rx, ry, 0, 0, 2 * Math.PI); g2.stroke();
  }
  // the lead's glow, as it sings a phrase (strongest when it sings alone)
  const sing = leadGlow();
  {
    const a = (0.18 + 0.5 * sing) * (1 - 0.6 * e);
    const glow = g2.createRadialGradient(lcx, lcy, 0, lcx, lcy, lh * 1.3);
    glow.addColorStop(0, `rgba(255, 236, 200, ${a})`); glow.addColorStop(0.45, `rgba(240, 200, 140, ${a * 0.45})`); glow.addColorStop(1, 'rgba(240, 200, 140, 0)');
    g2.save(); g2.globalCompositeOperation = 'lighter'; g2.fillStyle = glow; g2.fillRect(lcx - lh * 1.3, lcy - lh * 1.3, lh * 2.6, lh * 2.6); g2.restore();
  }
  // the singers, the lead and the microphone, the furthest first
  const order = [...P.map((_, i) => i), LEAD, MIC].sort((a, b) => placeOf(b)[1] - placeOf(a)[1] || (a === MIC) - (b === MIC));
  // the lead lights up as it sings; the choir all lights up while it is on (and playing), and is not there while it is off
  const choirLit = state.choirOn && state.playing && e > 0.4;
  for (const i of order) {
    const [x, y, w, h] = figureRect(i), active = groupOfFigure(i) === touched;
    const alpha = i >= 0 ? e : 1;        // (the choir appears as the camera pulls back to it)
    if (alpha <= 0.001) continue;
    g2.save();
    if (active) { g2.shadowColor = 'rgba(255, 210, 140, 0.9)'; g2.shadowBlur = 14 * dpr * v.Z; }
    g2.globalAlpha = alpha;
    if (i === MIC) { if (sprites.mic) g2.drawImage(sprites.mic, x, y, w, h); }
    else if (i === LEAD) {
      // lit as much as it glows: the lit figure faded over the one at rest
      const lit = active ? 1 : sing;
      if (sprites.lead_rest && lit < 1) g2.drawImage(sprites.lead_rest, x, y, w, h);
      if (sprites.lead_lit && lit > 0) { g2.globalAlpha = lit; g2.drawImage(sprites.lead_lit, x, y, w, h); }
    } else {
      const img = sprites[`${SECTION_COLOURS[P[i].section]}_${choirLit || active ? 'lit' : 'rest'}`];
      if (img) g2.drawImage(img, x, y, w, h);
    }
    g2.restore();
  }
  // ---- on top, unzoomed: the name of what is being touched (or shown by the hand)
  g2.setTransform(dpr, 0, 0, dpr, 0, 0);
  if (touched === LEAD) label('LEAD VOCAL', [LEAD], onScreen);
  else if (touched === MIC) label('MICROPHONE', [MIC], onScreen);
  else if (touched >= 0) label(CHOIR.labels[P[stage.m.groups[touched][0]].section], stage.m.groups[touched], onScreen);
  // the hand, its fingertip on the middle of that part of the choir; DOWNLOAD PLUGIN with it, from then on
  cueEl.classList.toggle('on', cueOn);
  if ((state.phase === 'choir' || state.phase === 'free') && performance.now() >= state.cueAt) downloadEl.classList.add('on');
  if (cueOn) {
    const rs = stage.m.groups[cued].map((i) => onScreen(figureRect(i)));
    const x = rs.reduce((s, [rx, , rw]) => s + rx + rw / 2, 0) / rs.length, y = rs.reduce((s, [, ry, , rh]) => s + ry + 0.45 * rh, 0) / rs.length;
    cueEl.style.transform = `translate(${x.toFixed(1)}px, ${y.toFixed(1)}px)`;
  }
}
function label(text, figures, onScreen) {
  const rs = figures.map((i) => onScreen(figureRect(i)));
  const top = Math.min(...rs.map((r) => r[1])), xs = rs.map(([x, , w]) => x + w / 2);
  g2.font = '600 12px Cinzel, Georgia, serif'; g2.textAlign = 'center'; g2.fillStyle = 'rgba(255, 226, 170, 0.95)';
  g2.fillText(text.toUpperCase(), (Math.min(...xs) + Math.max(...xs)) / 2, top - 8);
}

// what is under the pointer (stage coordinates; min: the smallest a target is, in stage px): a group of the choir (its
// index), the lead (LEAD), the microphone (MIC) or nothing (-1). The nearest wins; as on the plugin's stage, wherever
// the lead is under the pointer it is the lead you take (it stands just behind the microphone): the microphone is
// then taken by its ring of light on the floor.
function hit(px, py, Z = 1) {
  if (!stage || !stage.cam) return -1;
  const inside = ([x, y, w, h], minW, minH) => {
    const ex = Math.max(0, (minW / Z - w) / 2), ey = Math.max(0, (minH / Z - h) / 2);
    return px >= x - ex && px <= x + w + ex && py >= y - ey && py <= y + h + ey;
  };
  const found = [];
  // (a figure can be grabbed a little around it: at least 30 x 36 px on the screen, for fingers)
  for (const i of [...stage.m.stage.performers.map((_, k) => k), LEAD]) if (inside(figureRect(i), 30, 36)) found.push(i);
  const [cx, cy, rx, ry] = micRing(), u = (px - cx) / (rx + 3 / Z), v = (py - cy) / Math.max(6 / Z, ry + 3 / Z);
  const onRing = u * u + v * v <= 1, leadUnder = inside(figureRect(LEAD), 0, 0);
  if (onRing && !leadUnder) found.push(MIC);
  else if (!leadUnder && inside(figureRect(MIC), 24, 36)) found.push(MIC);
  if (!found.length) return -1;
  found.sort((a, b) => placeOf(a)[1] - placeOf(b)[1] || (b === MIC) - (a === MIC));
  return groupOfFigure(found[0]);
}

// nothing can be moved until the choir is in and the camera has pulled back to it
const movable = () => (state.phase === 'choir' || state.phase === 'free') && look.e > 0.5;

// the pointer in stage coordinates (through the camera), and the camera's zoom
function pointer(e) {
  const r = canvas.getBoundingClientRect(), v = view();
  return [(e.clientX - r.left - v.tx) / v.Z, (e.clientY - r.top - v.ty) / v.Z, v.Z];
}
canvas.addEventListener('pointerdown', (e) => {
  if (!movable()) return;
  const [px, py, Z] = pointer(e), gi = hit(px, py, Z);
  if (gi === -1) return;
  try { canvas.setPointerCapture(e.pointerId); } catch (_) { /* (synthetic pointers) */ }
  const [wx, wy] = toWorld(stage.cam, px, py);
  drag = { group: gi, from: [wx, wy], start: (gi === LEAD ? state.lead : gi === MIC ? state.mic : stage.offsets[gi]).slice(), leadStart: state.lead.slice(), micStart: micPlace() };
  canvas.style.cursor = 'grabbing';
});
canvas.addEventListener('pointermove', (e) => {
  const [px, py, Z] = pointer(e);
  if (!drag) { hoverGroup = movable() ? hit(px, py, Z) : -1; canvas.style.cursor = hoverGroup !== -1 ? 'grab' : 'default'; return; }
  // moved something: the visitor has found it, no more help
  if (!state.dragged) { state.dragged = true; state.phase = 'free'; }
  const [wx, wy] = toWorld(stage.cam, px, py), b = stage.cam.bounds, v = stage.cam.view, P = stage.m.stage.performers;
  let dx = drag.start[0] + wx - drag.from[0], dy = drag.start[1] + wy - drag.from[1];
  if (drag.group === MIC) {
    // the microphone stays on the stage that is shown (and in the plugin's range); the lead stays where it stands
    const m = stage.m.stage.mics.mainPos;
    dx = Math.max(Math.max(v.xMin, MIC_RANGE.xMin) - m[0], Math.min(Math.min(v.xMax, MIC_RANGE.xMax) - m[0], dx));
    dy = Math.max(Math.max(v.yMin + 0.3, MIC_RANGE.yMin) - m[1], Math.min(Math.min(v.yMax, MIC_RANGE.yMax) - m[1], dy));
    state.mic = [dx, dy];
    const [mx, my] = micPlace();
    state.lead = [drag.leadStart[0] - (mx - drag.micStart[0]), drag.leadStart[1] - (my - drag.micStart[1])];
    if (node) { node.port.postMessage({ type: 'listener', dx, dy }); node.port.postMessage({ type: 'leadMove', dx: state.lead[0], dy: state.lead[1] }); }
    return;
  }
  if (drag.group === LEAD) {
    // the lead stays on the stage that is shown, and goes no further forward than the microphone
    const [mx, my] = micPlace();
    dx = Math.max(b.xMin - mx, Math.min(b.xMax - mx, dx));
    dy = Math.max(-LEAD_HOME_Y, Math.min(b.yMax - my - LEAD_HOME_Y, dy));
    state.lead = [dx, dy];
    if (node) node.port.postMessage({ type: 'leadMove', dx, dy });
    return;
  }
  // a group stays on the stage that is shown
  for (const i of stage.m.groups[drag.group]) {
    dx = Math.max(b.xMin - P[i].x, Math.min(b.xMax - P[i].x, dx));
    dy = Math.max(b.yMin - P[i].y, Math.min(b.yMax - P[i].y, dy));
  }
  stage.offsets[drag.group] = [dx, dy];
  if (node) node.port.postMessage({ type: 'move', group: drag.group, dx, dy });
});
const endDrag = () => { drag = null; canvas.style.cursor = 'default'; };
canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', endDrag);
canvas.addEventListener('pointerleave', () => { if (!drag) hoverGroup = -1; });
window.addEventListener('resize', resize);

// ---------------------------------------------------------------- the show: PLAY, the lead alone, the choir, the hand
const startEl = $('#start'), playBtn = $('#play'), pauseBtn = $('#pause'), waitEl = $('#wait');
const controlsEl = $('#controls'), powerBtn = $('#power'), downloadEl = $('#download');
  // theme build: the download slot is the RSVP modal trigger (markup), no href
function showProgress() {
  if (state.phase === 'loading') $('#ring').style.strokeDashoffset = String(1 - 0.97 * fraction('first'));
  $('#waitBar').style.transform = `scaleX(${fraction('choir').toFixed(3)})`;
}
function setPlaying(on) {
  state.playing = on;
  if (on) {
    // (iPhone: play even with the ring / silent switch on silent, where Safari lets a page say so)
    try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch (_) {}
    ctx.resume(); node.port.postMessage({ type: 'play' });
  } else node.port.postMessage({ type: 'pause' });
  pauseBtn.classList.toggle('paused', !on);
  pauseBtn.setAttribute('aria-label', on ? 'Pause' : 'Play');
}
// every frame: the camera pulls back to the choir as the audio thread brings it in (if it is still on its way when it
// is due, a bar shows how far along it is, and it comes in at the next of her lines once it is here)
function flow() {
  if (state.phase !== 'solo') return;
  if (!state.choirReady) { if (state.sung >= LINES[0]) waitEl.classList.add('on'); return; }
  if (state.sung < state.choirAt - LOOK_SECONDS.out / 2) return;
  choirIn();
}
// the choir in (the camera pulls back to it) and, the first time, the hand after it
function choirIn() {
  if (state.phase === 'solo') { state.phase = 'choir'; state.cueAt = performance.now() + (LOOK_SECONDS.out + CUE_DELAY) * 1000; }
  state.choirOn = true;
  lookTo(true);
  showMode();
}
// CHOIR (the power switch): the choir out (the camera back in on the lead) or in, now (and no longer at her line, if it was still
// waiting for it)
function setChoir(on) {
  if (on === state.choirOn || !state.choirReady) return;
  node.port.postMessage({ type: 'choirOn', on });
  if (on) choirIn();
  else { state.choirOn = false; lookTo(false); showMode(); }
}
function showMode() {
  powerBtn.setAttribute('aria-pressed', String(state.choirOn));
  powerBtn.disabled = !state.choirReady;
}
function ready() {
  state.phase = 'ready';
  $('#ring').style.strokeDashoffset = '0';
  startEl.classList.add('ready');
  playBtn.disabled = false;
}
function failed(e) {
  console.warn('could not load:', e && e.message);
  const msg = 'COULD NOT LOAD · TAP TO RETRY';
  if (state.phase === 'loading') { startEl.classList.add('failed'); $('#status').textContent = msg; playBtn.disabled = false; }
  else { $('#waitText').textContent = msg; waitEl.classList.add('failed'); }
}
playBtn.addEventListener('click', () => {
  if (startEl.classList.contains('failed')) { location.reload(); return; }
  if (state.phase !== 'ready') return;
  state.phase = 'solo';
  startEl.classList.add('gone');
  controlsEl.classList.add('on');
  setPlaying(true);
});
pauseBtn.addEventListener('click', () => setPlaying(!state.playing));
powerBtn.addEventListener('click', () => { setChoir(!state.choirOn); if (!state.playing && state.phase !== 'loading' && state.phase !== 'ready') setPlaying(true); });   // theme build (user 2026-10-21): CHOIR on/off while paused starts playback again
waitEl.addEventListener('click', () => { if (waitEl.classList.contains('failed')) location.reload(); });
document.addEventListener('keydown', (e) => {
  if (e.code !== 'Space' || e.repeat || e.target.closest('button, a, input, textarea, select, [contenteditable]') || document.querySelector('#dlModal.show')) return;   // theme build: never steal Space from the page's form / modal
  e.preventDefault();
  if (state.phase === 'ready') playBtn.click();
  else if (state.phase !== 'loading') setPlaying(!state.playing);
});

// ---------------------------------------------------------------- start: everything asked for at once, in order of need
(function main() {
  $('#fonts').media = 'all';
  // (the context starts suspended; PLAY resumes it. It runs at 48 kHz, as the choirs were rendered: the browser
  // converts for the device)
  ctx = new AudioContext({ sampleRate: SR, latencyHint: 'interactive' });
  const engine = Promise.all(['engine/choir-stage.js', 'engine/choir-worklet.js'].map((u) => loadText(u, 'first', 'high'))).then(startEngine);
  const plan = loadJson(`audio/${VOCAL}/${CHOIR.slug}/manifest.json`, 'first', 'high');
  const leadMeta = loadJson(`audio/${VOCAL}/lead.json`, 'first', 'high');
  const leadBytes = load(`audio/${VOCAL}/lead.mp3`, 'first', 'high');
  const looks = Promise.all([['nave', 'assets/nave.jpg'], ['lead_rest', 'assets/singer_lead_rest.png'], ['lead_lit', 'assets/singer_lead_lit.png'], ['mic', 'assets/mic.png']].map(([n, u]) => loadSprite(n, u, 'first')));
  // the stage, as soon as its plan and its pictures are in (the acapella is usually still on its way)
  Promise.all([plan, looks]).then(([m]) => { showStage(m); canvas.classList.add('on'); }).catch(() => {});
  // the acapella, decoded and handed to the audio thread: then PLAY
  const leadSent = (async () => {
    const [meta, bytes] = await Promise.all([leadMeta, leadBytes]);
    state.loop = meta.loop / SR;
    const channels = await decode(bytes, meta.loop + meta.tail);
    await engine;
    node.port.postMessage({ type: 'lead', vocal: VOCAL, loop: meta.loop, tail: meta.tail, channels }, channels.map((c) => c.buffer));
    node.port.postMessage({ type: 'vocal', vocal: VOCAL });
    node.port.postMessage({ type: 'leadMove', dx: 0, dy: 0 });
    node.port.postMessage({ type: 'listener', dx: 0, dy: 0 });
  })();
  Promise.all([leadSent, plan, looks]).then(ready, failed);
  // the choir, once the acapella is in (so the two never share the line while PLAY is still waiting)
  leadBytes.then(() => loadChoir(plan, leadSent)).catch(failed);
  requestAnimationFrame(draw);
})();
})();
