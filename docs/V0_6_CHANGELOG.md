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

Native (no JUCE), all passing: DSP, FX (render() unchanged), full-take scan, stem mixer, Python interop, tracker/scanner accuracy, and
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
