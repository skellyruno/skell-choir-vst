# OmniChoir — spatial choir VST3 (source project)

**OmniChoir is a playable, MIDI-controlled VST3 instrument** based on the architecture and stage concept of the supplied browser choir demo, built on the supplied Skell Audio JUCE template. This repository contains complete project source, a working standalone native DSP engine, graphical editor, DAW automation/state serialization, and Windows/macOS build workflow. It is **not a precompiled `.vst3` binary**, and its generated voices are **not the original sampled/original-engine voices** of the browser demo.

## What works

- Play immediately with MIDI; formant-filtered band-limited oscillators, 32 internal voices, four SATB sections with separate gains and positions
- **MIDI channel 1:** auto-distribute pitches among Soprano / Alto / Tenor / Bass using tessitura overlaps; **MIDI channels 2, 3, 4, 5:** dedicated Soprano, Alto, Tenor, Bass respectively; channels 6–16 use the auto assignment
- Keyboard note on/off, sustain pedal CC64, all notes off, 2-semitone pitch bend
- Interactive stage GUI: drag four sections or the **YOU / MIC** marker; positions are saved and automatable
- Six microphone arrays: AB, ORTF, XY, Mid/Side, Decca, Binaural-inspired
- Stereo arrival-time / mic-pattern / distance simulation, native 8-line feedback delay network, room absorption/size/diffusion/RT60, voice attack/release, vowel (Ah/Eh/Ee/Oh/Oo), detune/ensemble, breath, master gain
- All controls, including mic and section XY positions, appear as host automatable parameters; parameter state is saved with the DAW project
- Works at 44.1, 48, 96 kHz and other typical host sample rates, without any hardcoded 48-kHz audio-thread assumption

## Build the VST3

### Easiest: GitHub Actions

1. Create an empty GitHub repository and upload **the contents** of this folder (including `.github`).
2. Go to **Actions → Build OmniChoir VST3 → Run workflow**.
3. After a successful build, download the `OmniChoir-Windows-VST3` or `OmniChoir-macOS-VST3` artifact.
4. Unzip and copy the `.vst3` bundle to your platform's VST3 location, rescan plugins in the DAW, and insert **OmniChoir** as an **instrument** (not an audio effect).

### Local

Install CMake >=3.22, C++17 toolchain, and JUCE 8.0.12 (or allow CMake internet access to fetch it). On Windows with Visual Studio 2022:

```sh
cmake -S . -B build -A x64 -DJUCE_PATH=C:/JUCE
cmake --build build --config Release --parallel 2
```

macOS/Linux (with JUCE GUI/build prerequisites):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=/path/to/JUCE
cmake --build build --config Release -j2
```

`JUCE_PATH` is optional; without it, the build downloads JUCE from GitHub. On macOS, distributing to other machines may require code signing/notarization. The project has not been built with JUCE in this environment; the portable DSP core has native tests.

## Test the audio DSP without JUCE

```sh
g++ -std=c++17 -O2 tests/dsp_tests.cpp Source/ChoirDSP.cpp -o dsp_tests
./dsp_tests
```

## Where the original demo fits in

The submitted demo's web worklet **plays prerecorded, already-rendered singers** and uses `OmniChoir.ChoirStage` to spatialize them; it loads `audio/female/epic-64/manifest.json`, `lead.json`, `lead.mp3`, section stem audio, and sprite images from remote paths. Those files and the original full sound-generation engine were not among the uploads. The `Reference/` directory retains the supplied JS and extracted source maps for later fidelity work.

This VST3 is a **new native implementation** of a choir instrument following that design. It does not embed a website, require Web Audio, or depend on those missing prerecorded stems. It cannot claim exact vocal, microphone, or DSP sample-for-sample parity with the source demo. Specifically, the browser's staged prerecorded choir playback, singer-by-singer source grouping, custom room model, lead acapella control, and original visual artwork are not ported one-for-one.

## Roadmap to reproduce the actual demo sound

1. Supply the original multi-voice audio stems, manifest JSONs, and lead WAV/MP3 (or, better, original C++ choir voice synthesis engine) with rights to use and distribute them.
2. Add an instrument sampler/voice bank or port the source choir synth. The VST already has MIDI/part routing and an audio output stage.
3. Port the `SpatialRenderer`/`ChoirStage` algorithm and its numerical reference tests from the reference JS, rather than pretending an approximation is exact.
4. Add bank browsing, editable SATB voicing presets, formation modes, sample library installation and authorization where needed.
5. Test VST3 binaries in REAPER, Cubase, FL Studio, Studio One and Ableton, then package signed installers.

## Structure

- `Source/ChoirDSP.*`: native, JUCE-independent synthesis / array microphones / spatial arrival delays / reverb
- `Source/PluginProcessor.*`: VST3 MIDI event handling, plugin parameters, state, stereo buses
- `Source/PluginEditor.*`: native UI and draggable stage
- `.github/workflows/build.yml`: Windows/macOS VST3 builds
- `tests/dsp_tests.cpp`: core sound/event/room/stereo tests
- `Reference/`: unmodified user-provided web demo plus human-readable source-map extraction

Generated from the user's template. Prototype version **0.1.0**. The included GitHub workflow is buildable configuration but its run has not been independently verified here.
