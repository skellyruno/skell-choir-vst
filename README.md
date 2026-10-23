# OmniChoir FX — vocal-to-choir VST3 prototype

**Audio effect, not a MIDI instrument.** Insert OmniChoir FX on a vocal track in a DAW, press play and blend the original voice with four spatial SATB harmony parts (Soprano, Alto, Tenor, Bass). Built as a JUCE 8 VST3 for Windows and macOS. The editable cathedral stage is based on the supplied demo screenshots; a cropped image supplied by the user is embedded in `Assets/cathedral_stage.jpg`.

> **Prototype limitation:** This is a conventional *granular pitch-shift harmonizer*, not an AI voice changer and **not a realistic recording of 64 separate singers**. It layers a resampled source voice (including its existing lyrics and articulation), adjusts pitch, slight detune, section pan/distance, and an eight-delay-line cathedral room. Granular processing adds character, grain modulation, and approximately 9–82 ms of **variable** delay. Because delay is time-varying, the processor makes no fixed-latency reporting/zero-latency claim. Formant-preserving vocal morphing and natural voice variation are not implemented. The demo's original vocal library is not in the uploads.

## Install/build

Requires CMake 3.22+, a C++17 compiler and JUCE 8.0.16-security. JUCE is fetched by CMake unless `-DJUCE_PATH=/absolute/path/to/JUCE` is set. Internet access is needed on the first build unless JUCE is installed locally.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
```

- `OmniChoir FX.vst3` is produced in the generated build products, typically `build/OmniChoirFX_artefacts/Release/VST3/` (exact path depends on generator and OS).
- On Windows copy the `.vst3` bundle to `C:\Program Files\Common Files\VST3\`.
- On macOS copy it to `~/Library/Audio/Plug-Ins/VST3/` (or the system library plug-in folder), then rescan plug-ins in your DAW. Unsigned local macOS builds may need Gatekeeper approval.
- Alternatively, create a GitHub repository containing **the contents of this folder** (not the outer directory) and use **Actions → Build OmniChoir FX VST3 → Run workflow**. The workflow is configured to build Windows and macOS VST3 bundles as downloadable artifacts.

**This source is not a compiled VST3.** Native DSP tests passed locally, but this environment did not provide JUCE for a full host/plugin build, so DAW compatibility has not been verified.

## Workflow

1. Insert **OmniChoir FX** as an audio effect on a recorded solo vocal or live microphone track. Don't put it on an instrument/MIDI track.
2. **LIVE mode** starts layering the input as the track plays. No prior analysis is required. Choose *Major*, *Minor*, or *Unison* for the voicing. The four pitches are intervals above/below the input: Major +7/+4/-5/-12 semitones; Minor +7/+3/-5/-12; Unison all 0, with slight ensemble detune. These are fixed-interval harmonies, not automatic chord recognition.
3. **SCAN mode:** Start DAW playback, press **SCAN INPUT · 4 SEC**, and send a clear monophonic vocal for four seconds. The scanner uses autocorrelation to estimate sung notes and finds the **dominant pitch class** among that passage (e.g. C). Once complete, it uses that pitch class to snap the four moving harmony voices to a major/minor scale. This is a simple tonal anchor, **not a full song transcription or guaranteed correct musical key**. No audio is recorded to disk, and Scan does **not** import files or reconstruct a separate performance from a complete clip.
4. Use the **CHOIR ON/OFF** button for wet-choir bypass while the dry vocal remains audible. **CHOIR MIX:** 0% is primarily direct original input, 100% is the resynthesized harmonizer and room. Adjust **SOPRANO / ALTO / TENOR / BASS** levels, **ENSEMBLE** for stereo detuning and **SENSITIVITY** to reject soft background noise.
5. Drag the four colored singer groups on the stage and the gold microphone icon. These controls are DAW-automatable and influence each section's stereo direction, distance and tone. The **MIC ARRAY** menu offers AB, ORTF, XY, Mid/Side, Decca and Binaural *approximations*. The full physical-reflection stage algorithm from the JavaScript demo is not yet ported.
6. **REVERB TIME**, **ROOM SIZE**, **ABSORPTION**, **DIFFUSION** and **ROOM RETURN** sculpt a cathedral-style FDN reverb. **OUTPUT** trims the level with gentle saturation.

### Tips

- Monophonic clean vocals produce more predictable results than full mixed music; any audio can be processed but the pitch detector may struggle with polyphonic instruments, breathy speech and noise.
- For little or no added harmony, select *Unison* and turn up *Ensemble* to create a choir-like double/chorus effect.
- For a more orchestral effect, use Major/Minor and a wetter room. Melodic harmonies may have obvious grain/pitch artifacts on fast transients.
- Keep Mix below 100% when you want intelligible lead lyrics.
- Scan and Live are both **streaming input effects**; Scan does not need a different plugin instance and does not process files in the background.
- Host bypass may be used to stop the effect. Render/freeze in the DAW when the processed take sounds right.

## Technical architecture

- `Source/PluginProcessor.*`: JUCE 8 processor with input bus, stereo/mono layout support, automation, state saving and scan request/status atomics.
- `Source/ChoirFX.*`: audio-effect DSP, input-gated stereo-to-mono pitch-layer source, autocorrelation note tracking at ~4 kHz, incremental scan pitch-class histogram, eight grain voices, stage panning and tone, real-time output blend.
- `Source/ChoirDSP.*`: original native synth/FDN core retained so its `Room` eight-delay-line FDN can be reused. The old instrument is **not routed** by this plugin.
- `Source/PluginEditor.*`: dark/gold cathedral GUI, scan button and progress, draggable SATB sections and microphone, JUCE parameter attachments.
- `Assets/cathedral_stage.jpg`: background crop from supplied screenshot; all performers are drawn dynamically in JUCE.
- `Reference/`: original demo source retained for continued porting, plus the five provided UI reference screenshots.
- `tests/fx_tests.cpp`: JUCE-free sample-rate, scan, dry mix, numerical safety and stereo spatial tests.

Run core tests without JUCE:

```bash
g++ -std=c++17 -O2 tests/fx_tests.cpp Source/ChoirFX.cpp Source/ChoirDSP.cpp -o fx_tests
./fx_tests
```

To build via CMake including both native DSP test executables: `cmake -S . -B build -DOMNICHOIR_BUILD_TESTS=ON`, `cmake --build build`, `ctest --test-dir build --output-on-failure`.

## Future production upgrades

1. Include real multisinger sample libraries or permissioned recorded SATB voices for human variation.
2. Replace simple granular shifting with higher-quality time-stretch/pitch-shift with formant preservation, transient handling and bounded/deterministic host latency.
3. Improve the detector to segment polyphonic recordings, identify chords/scale reliably and offer editable harmony lines.
4. Add a genuine import-and-analyze/offline render workflow with file capture and non-destructive take management; current scan is *four seconds of incoming DAW audio*.
5. Port the JavaScript per-singer image-source delay and mic/reflection model to the native engine, with performance profiling and host validation.
6. Test/validate builds in REAPER, FL Studio, Ableton Live, Cubase and Logic (Logic needs AU, not VST3).

This project is an inspectable working **source prototype**; no untested binary is represented as production-ready.
