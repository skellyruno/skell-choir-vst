# OmniChoir FX v0.6 — the faithful stage replaces the simplified one

v0.5's own notes listed "image-source reflection delays and full mic array response curves" as future work. v0.6 does
that: the stage from your JS demo (`Reference/demo-choir-stage.js`, `demo-choir-worklet.js`) is ported to C++ in
`Source/Stage/` and the 16 granular/LPC singer layers of `ChoirEffect` now stand on it. The source was ported from
your own readable source-map text, with the same constants and structure; no third-party code was added.

## Signal flow (live effect)

    mono input -> ChoirEffect::processVoices -> 16 MONO singer streams (S0-3, A0-3, T0-3, B0-3)
               -> oc::ChoirStage  each singer is a point source on the stage:
                    - 3 main-mic capsules (AB / ORTF / XY / MS / Decca / Binaural: real capsule patterns and spacing)
                    - 4 section spot mics, time-aligned to the main pair, power-compensated
                    - room pair, 6 first-order image sources per singer (walls, floor, ceiling) with wall absorption
                    - 8-line FDN late field fed by the early reflections
                    - per-singer propagation delay and air absorption, constant-power "lean"
                    - singers that are moved >0.4 m (or when YOU move >0.4 m) become featured voices (follow spot)
                    - measured top-end shelf toward a real choir's balance
               -> oc::Hall        extra longer hall under the choir (new HALL / HALL TIME controls, off by default)
    lead input -> oc::LeadStage   dry at home; moved, heard where it stands, in the choir's room
    out = ceiling( OUTPUT * ( (1-mix) * lead * LEAD LEVEL + mix * choir ) )

## What changed

- `Source/Stage/*.h` - the stage engine (JUCE-free, header-only).
- `Source/StagedChoir.*` - glue + fixed 128-sample block handling; one place to test.
- `ChoirEffect::processVoices()` / `setVoiceParameters()` - the 16 voices as separate mono streams. `render()` is
  unchanged (verified against a golden render from the v0.5 code: max difference 2e-8 on a 0.17 peak).
- Processor reports **128 samples of latency**; the dry lead is delayed with the choir so they stay aligned.
- Offline `RENDER WAV` (JUCE worker) and `omnichoir-batch render` use the same engine, latency-compensated.
- New automatable parameters: `hall` (0-0.5, default 0) and `hallSeconds` (0.5-8 s). Two dials on the ROOM page.

## Behaviour changes to know about

| Control | v0.5 | v0.6 |
|---|---|---|
| Section level | linear, default .83 | .83 is now 0 dB (level = value / .83) |
| ROOM RETURN (`wet`) | scales the simple FDN | scales the whole room return (room mics + reflections + late); .37 is now unity |
| Section X/Y, MIC X/Y, MIC SPACING, MIC ARRAY | simplified pan + attenuation | drive real mic geometry and per-singer delay/level/tone |
| Lead X/Y | pan + fade | lead stands on the stage; at home it is exactly the dry input |
| Choir loudness | - | calibrated to v0.5's (trim -26.6 dB, `StagedChoir::kVoiceTrim`; `tests/calibrate_trim.cpp`) |
| Choir timing | - | arrives ~15-20 ms later than v0.5: propagation from 6-9 m (physically correct, as in the demo) |

Saved v0.5 sessions load (same parameter IDs); their sound changes as described.

## Verification

Native (no JUCE), all passing: DSP, FX (render() unchanged), full-take scan, stem mixer, Python interop, tracker/scanner accuracy, formant choir, convolver, presets, feature chain, and
`staged_tests` (silence at 44.1/48/96 kHz; dry lead sample-exact with 128-sample latency; level within 1.5 dB of v0.5;
all six mic arrays bounded; section placement moves the stereo image; **bit-identical output for host buffers of 1, 37,
128, 513, 4096**; automation sweep; CPU). `staged_tests` is clean under AddressSanitizer + UBSan. End-to-end CLI:
scan 8 s synthetic take -> 6 correct notes -> render, output length equal to input.
CPU: ~7x realtime on one core at 48 kHz (-O2, sandbox) for voices + stage + hall.

## Round 2 — measured improvements to pitch tracking and the full-take scanner

Every change below was benchmarked against synthetic ground truth before and after (`tests/bench/`), and is now
guarded by `tests/tracker_tests.cpp` (which fails on the v0.5 code, so it is not vacuous).

**Pitch tracker rewritten (YIN, 16 kHz, 8 ms hop, sub-sample interpolation, 3-frame median).** It drives the
harmonies, the scan-mode snapping and the note scanner.

| Synthetic sung phrases | v0.5 tracker | v0.6 tracker |
|---|---|---|
| Octave/fifth errors, steady notes | 7.9 % | 0.0 % |
| Mean pitch error, steady notes | 141 cents | 0.8 cents (p99 6 cents) |
| Octave/fifth errors, weak fundamental | 5.6 % | 0.0 % |
| Time to lock onto a new note (median) | 90-140 ms | 40 ms |
| Update rate | every 48 ms | every 8 ms |

The test suite caught one bug in my first version of this: a 711 Hz note with a weak fundamental was tracked an
octave low, because its 22.5-sample period fell between two lags and looked shallower than its integer double. The
dip depth is now judged on the interpolated parabola. With vibrato of +-35 cents, the reported pitch still trails
the instantaneous pitch by the analysis window's centre (~24 ms): inherent to any windowed estimate.

**Full-take scanner updated to match.** Latency compensation now comes from the tracker (it was a hard-coded 90 ms
tuned to the old one); note onsets are taken from the signal's energy rise rather than from when pitch locked; a young
note that changes pitch (a scoop) is corrected rather than split; short thresholds are in seconds, not frames; a
pitch hysteresis stops vibrato near a semitone boundary chopping one note in two; notes end 45 ms after voicing stops.

| 14-note synthetic takes, 4 random seeds | v0.5 scanner | v0.6 scanner |
|---|---|---|
| Notes found with correct pitch and timing | 92 % (one seed) | 98-100 % |
| Median onset error | ~63 ms | 4 ms |
| Median offset error | ~121 ms | 2 ms |

Two notes in one seed (11) are still missed; the others reach 100 %. These are synthetic takes (harmonic tone,
vibrato, scoops, a noise floor): a real recorded vocal will be messier, so treat the numbers as a regression guard
rather than a promise. Try it on your own takes with `omnichoir-batch scan`.

**Tested and rejected: a sibilant/consonant bypass.** I suspected granular shifting dulled fricatives. Measured on
steady noise through the 16 voices, 93 % of the >4 kHz energy share is retained and amplitude flutter is only 4 %
(the sum of 16 incoherent voices smooths it). With no measured problem, I added nothing. Worth revisiting only if
your ears disagree on real "s" and "sh" sounds, since synthetic noise is not a real consonant.

## Round 3 - eight new features (voice engine, vowels, performance, stems, presets, impulse responses, meters, A/B)

Everything below is covered by native tests (`formant_choir_voice_engine`, `impulse_response_convolver`,
`stems_meters_ab_ir_formant_chain`, `stage_presets`) except the JUCE glue, which is **not compiled here** (see the end).

### A second voice engine: the formant choir (`Source/FormantChoir.*`)
Synthesised singers instead of re-played copies of the lead: a band-limited glottal-style source through five vowel
formants, with separate tables for soprano, alto, tenor and bass. It follows the lead's pitch, loudness and phrasing, and
uses the harmonizer's own voicing, so harmony, scan mode and score voicing all still apply. **SINGERS** tab: VOICE BLEND
crossfades from the granular harmonizer (0) to the synthesised choir (1); default 0 so existing sounds are unchanged.
Measured: every voice sings the right pitch (220.0 / 330.0 / 110.0 Hz for unison / fifth up / octave down), tracks a
12 dB diminuendo to 0.0 dB, sits at the lead's own level (ratio 1.00), runs at ~9x realtime.
It is *synthesis*: it can sound like a choir pad with real vowels, not like 16 recorded humans. That would need
recorded samples or a trained model behind `ChoirEffect::processVoices()`.

### Vowels that follow the lead
VOWEL sets a vowel by hand (A E I O U); FOLLOW LEAD lets the lead's own mouth shape steer the choir (an LPC formant
estimator, F1/F2 mapped onto the five vowels). Measured on synthesised vowels: 7 of 8 recognised; the miss is a high-pitched
"i" read as "e", an expected limit when the harmonics are sparse. It follows the vowel *colour*; it does not read lyrics.

### Performance layer
Per-singer vibrato that blooms after the note starts, a scoop up into each new note, pitch jitter, staggered entries
(26-77 ms spread measured), a breath at the start of phrases (BREATH) that settles, and swells that follow the lead's dynamics.

### Impulse-response hall
`LOAD IR` on the toolbar loads a WAV/AIFF/FLAC room recording (up to 8 s, mono or stereo, any sample rate - resampled
with a cubic interpolator that places a 10 ms click at exactly 10.000 ms). HALL sets how much you hear; `X` returns to the
algorithmic hall. The partitioned-FFT convolver matches brute-force convolution to ~1e-5, costs about 3% of a core for a 6 s
stereo IR, is loudness-matched to the algorithmic hall (+0.01 dB on noise) and swaps click-free (0.056 largest step) while
audio runs. The IR path is stored in the session and reloaded. **No IRs are bundled** (no network here, and recordings carry licences).

### Stems (multi-output)
Five optional stereo output buses, off until the host enables them: Sopranos, Altos, Tenors, Basses, Lead. Each section
stem is that section heard alone through the same stage (sopranos-only test: other stems are exactly silent; the main output
is bit-identical with or without stems requested). The four stems do not sum exactly to the main mix, whose spot microphones
and top-end shelf act on all sections together. Stems cost extra CPU only while their buses are enabled. Host support for
optional output buses varies; test in your DAW.

### Stage presets
Cathedral, Concert Hall, Studio Booth, Chapel, Gospel Stage, Stadium Anthem, Headphone Choir (and Default). They set
placement and room only (sections, microphone, room, hall, ensemble, human variation) and never touch harmony, mix, lead
level or output. Measured: levels within 5 dB of each other, and the tails really differ (Cathedral and Stadium ring ~200x
longer than Studio Booth).

### Meters and A/B
A strip on the toolbar shows the four sections, the lead, output peaks, stereo WIDTH and CORRelation; the stage view's
sections glow with their live level. **STAGE: FAITHFUL / CLASSIC v0.5** is a true A/B: the classic engine is bit-identical to
v0.5's `ChoirEffect::render` (max difference 0.00e+00 over 4 s) and delayed to the same latency, so switching never shifts
timing and leaves no stale audio behind.

### Bugs found and fixed while building this
- Section meters first read the singers *before* the section level control, so a muted section still showed full level.
- Breath noise was inaudible by design error (it went through five narrow resonators, which pass almost none of a
  broadband signal); it now has its own wide-band, vowel-shaped path.
- New `FXParams` fields were hiding legacy `Params::breath` / `Params::vowel`; renamed (`choirBreath`, `choirVowel`).
- Every test file (including the original ones) used `assert`, which a Release build compiles to nothing; each now begins
  with `#undef NDEBUG`.

### The editor
The existing stage view (robed singers, cathedral art, drag sections / lead / microphone) is kept, and gains live glow.
New toolbar: STAGE PRESET menu, A/B button, LOAD IR / clear, meter strip. New tab: SINGERS (voice blend, vowel, follow lead,
breath, vibrato); the ROOM tab gains HALL and HALL TIME.

### Not verified
- **The JUCE layer has not been compiled here.** Round 3 touches `PluginProcessor.*` (new parameters, optional output buses,
  IR load/clear, presets) and `PluginEditor.*` (toolbar, meter strip, SINGERS tab). I read the diff line by line and used
  every anchor with an assertion, but only a real build proves it. The native emulation of CI (every `add_executable` in
  `CMakeLists.txt` built with `-O3 -DNDEBUG -Wall -Wextra -Wshadow` and all registered tests run) is clean.
- Nothing here has been listened to. Levels were calibrated numerically; whether the formant choir *sounds* good, and
  how the presets feel, is for your ears.
- Optional stem buses and the IR loader need testing in your host (REAPER, Ableton, Cubase, Logic, Pro Tools differ).

## Not verified / still open

- **The JUCE layer was not compiled** (no JUCE in the build environment): `PluginProcessor.*`, `PluginEditor.cpp`,
  `TakeWorker.cpp` edits are small and reviewed but untested. Build, then test in a real host (pluginval, REAPER,
  Ableton, Cubase), including mono-in/stereo-out tracks and automation.
- Voice realism is still the granular/LPC harmonizer: it is not 16 human identities. The stage makes it *sit* like a
  choir in a hall; the singers themselves are your next lever (`ChoirEffect::processVoices` is the seam: anything that
  produces 16 mono streams can replace it).
- Voice grains still have a variable 10-90 ms delay inside `ChoirEffect`; the reported 128-sample latency covers only
  the stage's block. The choir is, by design, later than the dry lead.
- STEMS tab (external stem mixer) still uses its own lightweight offline spatializer, not this stage.
- Windows/macOS CI (`.github/workflows/build.yml`) will build the plugin on push; AU is still not enabled.
