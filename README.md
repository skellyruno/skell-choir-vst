# OmniChoir FX 0.5 — live vocal choirification + full-take SATB scan + external singer tools

**VST3 effect source project — not a compiled plugin.** This is an upgrade of the OmniChoir FX starter using your original JS stage, worklet, renderer, latest `omnichoir-demo.js`, the five photographic references, and the screenshot of the larger GUI. The original JS files are kept verbatim in `Reference/` for continued native porting. The browser `AudioWorklet` itself cannot execute as a native VST3 audio callback; the relevant stage/mic/group interaction patterns and acoustic concepts are translated into C++.

## v0.5 — direct GUI stem/MIDI workflow and a real DSP fix

This iteration makes the external-singer route available in the native plugin source (not only the Python CLI): `SCAN / EDIT` has `EXPORT MIDI` for five tempo-aligned SATB+lead tracks, and the fifth `STEMS` tab loads an external singer JSON manifest and renders a spatial stereo WAV in a background worker. The lead, group and mic stage positions also affect this external stem mix. The mixer is a separate JUCE-free C++17 component with executable tests, intentionally not an unlicensed copy of the supplied JavaScript renderer. **Important:** `leadX/leadY/leadGain/humanize/formant` were not being passed from APVTS to DSP in v0.4; this is fixed.

For implementation and limits, see `docs/V0_5_CHANGELOG.md` and `docs/BUILD_TEST_V05.md`. For immediate tests **without JUCE or internet**, use:

```bash
cmake -S . -B build-native -DOMNICHOIR_NATIVE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

The additional UI features, MIDI exporter and JUCE glue are not host-tested until the VST3 is compiled. Full AI singer generation still requires licensed models or recorded multisinger stems. The original demo JS is retained in `Reference/`.

## v0.4 additional improvements

Four code repositories were reviewed as potential upgrades. Rather than silently incorporating GPL/AGPL or ambiguously licensed source, this release adds an **independent seeded natural-variation engine for 16 singer layers**, an **editable SATB-to-MIDI exporter**, and an **offline spatial mixer for separately recorded or externally AI-rendered vocal stems**. It does not include trained neural models or bundle those external repositories.

```bash
python3 tools/satb_to_midi.py Preview/Example-Editable-SATB.csv score.mid --include-lead
python3 tools/mix_external_voices.py tools/external-voices.example.json choir-from-stems.wav
python3 -m unittest discover -s tests -p 'test_interop_tools.py' -v
```

The second command requires the WAV stems listed in the manifest and will fail appropriately if they are absent. For compatible stem preparation, licensing caveats, and how `ai-choir` might fit the offline stage, see **`docs/EXTERNAL_SINGERS.md`** and **`docs/V0_4_CHANGELOG.md`**. The plugin GUI remains the compact 810×577 resizable stage from v0.3, including the independently draggable lead, mic and choir groups.

## Key changes in 0.3

- **Smaller GUI:** default **810 × 577 px**, exactly ~71% of the former 1140 × 812 dimensions (**29% smaller in each dimension**); can resize from **720 × 510** up to **1400 × 1000**. The host moves the whole plugin window by its title bar; four choir groups, the gold mic and the **new white lead singer** drag independently on the stage.
- **Fixed layout:** clearer top bar, no malformed `Â` labels or missing glyph icons, cathedral fills the stage, compact `VOICE / SPACE / ROOM / SCAN/EDIT` parameter pages instead of a huge knob wall.
- **A separate lead:** independently automatable X/Y and level; if you drag the microphone, lead offsets automatically compensate so the white singer stays where she was on stage. Double-click lead or mic to reset. Dry lead stays separate from synthesized choir.
- **Live input effect:** SATB made from incoming solo vocal, not MIDI notes. Four independent granular voices in each section, **16 layers total**, with independent timing delay, stereo spread, gradual pitch drift, detune and brightness.
- **Experimental formant compensation:** added 10th-order regularized **LPC source–filter** analysis: a granularly pitch-shifted excitation is filtered through an estimate of the source's formant envelope and blended with the dry granular shifted voice. Exposed via `CHARACTER`. This is a conventional DSP approximation; it does not produce 16 genuine human identities or preserve formants as accurately as state-of-the-art vocal resynthesis. Tune on clean monophonic voices.
- **More accurate pitch tracking:** the analyzer now works at about 8 kHz internal sampling (previously 4 kHz), with improved pitch resolution, including bass-range notes. Live 4-second scan remains as a quick tonal anchor.
- **FULL-TAKE SCAN:** On the `SCAN / EDIT` page, **IMPORT FULL TAKE** reads an entire **WAV, AIFF or FLAC** recording on a background worker. It segments voiced notes, estimates a dominant pitch class and builds SATB note assignments. Select a row and click `S-/S+`, `A-/A+`, `T-/T+`, `B-/B+` to edit its harmony in one-semitone steps.
- **OFFLINE RENDER:** `RENDER WAV` processes the full imported take to a new stereo 24-bit WAV, using the edited per-note SATB pitches, with no DAW audio-thread disk I/O. The render also writes `filename.satb.csv` next to the WAV, preserving the editable arrangement text. The original is not overwritten intentionally.
- **Headless batch workflow** (no JUCE necessary): `tools/omnichoir-batch.cpp` builds a CLI that scans a full PCM16/float32 WAV to an editable CSV and re-renders a choir after manual CSV edits; useful for test and for arrangements if JUCE cannot compile locally.

## Quick VST3 build

Requires a C++17 compiler, CMake 3.22+, and JUCE 8.0.16-security (automatically fetched when internet is available). To use a local JUCE checkout, set `-DJUCE_PATH=/path/to/JUCE`.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
```

Or publish the **contents** of this folder at the root of your GitHub repository and use **Actions → Build OmniChoir FX VST3 → Run workflow** for Windows/macOS artifacts.

The plugin is an **audio effect**, not an instrument. It can be placed on a vocal track in REAPER, Ableton, FL Studio, Cubase, etc. **VST3 does not run in Logic Pro; AU support would need to be added.** The JUCE plugin **has not been compiled or host-tested in this sandbox** because this environment cannot reach the JUCE source repository. Source-level validation and native DSP tests are separate from actual VST3 host validation.

## Everyday use

**Live:** Insert OmniChoir FX onto a *clean monophonic vocal track*. Set `PROCESS` = Live, `CHOIR ON` and adjust `MIX`. Start with 50–75% choir blend for intelligibility. Adjust `HUMANIZE` and `CHARACTER` to hear different voice-layer variation; position the 4 sections in the cathedral and move the mic.

**Quick input Scan:** Play the DAW vocal and press `SCAN LIVE INPUT`. Four seconds of incoming pitch estimates are gathered. Switch to scan/harmony processing if desired. This tracks a **pitch class**, not the full melody/chords.

**Full-take Scan and render:** Open `SCAN / EDIT` tab → `IMPORT FULL TAKE` → select a clean solo WAV, AIFF, or FLAC → wait until detected note rows appear → select a row and adjust its four section pitches with the `±` buttons → `RENDER WAV` → load the generated file back into your DAW, alongside the original lead. The processor reads and writes **external media files**; the DAW does not grant plugins automatic access to its entire timeline, so full scan requires importing a file. The imported take/edits are held in the editor's working session, and the generated `.satb.csv` is the durable arrangement export; reopening the editor does not automatically reload a take.

**Limits of the note scanner:** It is intended for one clear sung melody. It does not reliably separate a polyphonic mix, detect chord progressions, infer lyrics, or compose idiomatic counterpoint. It estimates notes and derives editable harmony from fixed templates, a dominant pitch class, section ranges, and simple scale snapping. Review and correct all SATB lines before production export. It does **not** learn genuine voice identities from a single source.

## Build, run and edit a full take without JUCE

```
g++ -O2 -std=c++17 tools/omnichoir-batch.cpp \
  Source/Offline/FullTakeScan.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp \
  -o omnichoir-batch

./omnichoir-batch scan vocal.wav score.csv
# edit the soprano/alto/tenor/bass MIDI note columns in score.csv
./omnichoir-batch render vocal.wav score.csv choir.wav
```

The portable CLI is intentionally limited to **little-endian mono/stereo PCM16 or float32 WAV** input. The JUCE UI worker can read additional types. Output of the CLI is stereo 16-bit PCM WAV; plugin UI renders 24-bit stereo WAV.

## Test results and audio comparison

**Successfully tested natively (no JUCE):**

```
g++ -O2 -std=c++17 tests/fx_tests.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp -o fx_tests && ./fx_tests
g++ -O2 -std=c++17 tests/fulltake_tests.cpp Source/Offline/FullTakeScan.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp -o fulltake_tests && ./fulltake_tests
```

These verify silence numerical safety, pitch detection, stage stereo movement, 44.1/48/96-kHz processing, 4-second scan behavior, full-note score segmentation, SATB editing and finite export-DSP samples. The included CLI was also used to scan a 10-second example into 12 detected notes and render the resulting full-length stereo WAV in `Preview/`.

`Preview/index.html` is an interactive *visual UI mock*, not a VST or audio processor. It is included for easy layout inspection and stage drag testing. `Preview/GUI-design-reference.png` is an illustrative rendering of the intended compact UI, **not a screenshot from a DAW**.

## Files

- `Source/PluginProcessor.*` — effect plugin bus, JUCE parameters, state serialization, real-time audio.
- `Source/PluginEditor.*` — 71%-size resizable GUI, draggable SATB/lead/mic, control tabs, full-take editor.
- `Source/ChoirFX.*` — 16-singer time-domain harmonizer, LPC formant compensation, live scan and editable offline pitch control.
- `Source/ChoirDSP.*` — earlier 8-line cathedral reverb/DSP modules.
- `Source/Offline/FullTakeScan.*` — JUCE-free entire-file pitch event analysis and SATB score generation.
- `Source/Offline/TakeWorker.*` — background file import/export and edited CSV score output.
- `tools/omnichoir-batch.cpp` — native WAV-to-SATB and offline choir render CLI.
- `Reference/` — user's original and updated demo JS files and image references.
- `Preview/` — visual mock, GUI concept, sample arrangement CSV and rendered example WAV.

## Remaining production work

The sound is more varied than the previous two-layer granular harmonizer, but it **will not automatically sound like a high-end 64-person real choir**. A production instrument would need licensed multisinger voice recordings or a high-quality neural/PSOLA-style voice conversion engine, better transient handling, robust chord/harmony transcription, singer breath and phoneme variation, spectral high-quality formant models, CPU profiling, fixed host latency reporting, audio click/dropout QA, and real Windows/macOS DAW integration testing. The supplied JS worklet plays *pre-rendered choir samples*. These samples and their manifest are not in the uploads, so they cannot simply be reused here for the exact original demo sound.

This package provides a substantial inspectable **source prototype**, verified native DSP and end-to-end CLI scan/edit/render, rather than an untested VST3 falsely advertised as production-ready.
