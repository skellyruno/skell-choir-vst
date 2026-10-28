# Build and verify OmniChoir FX (v0.6.1)

## 1. The engine tests, with nothing installed but a C++17 compiler

```bash
scripts/run_tests.sh                 # builds and runs every JUCE-free test (about a minute)
scripts/run_tests.sh balance         # only tests whose name contains "balance"
SANITIZE=1 scripts/run_tests.sh      # the same under AddressSanitizer + UBSan (slow)
CXX=clang++ scripts/run_tests.sh     # choose the compiler
```

No CMake, no JUCE, no internet. Logs are written to `build-native/`. The suite includes:

| Test | What it protects |
|---|---|
| `dsp`, `fx`, `tracker`, `formant`, `convolver`, `stem_mixer`, `fulltake` | The DSP building blocks, the pitch tracker and the full-take scanner |
| `staged`, `features`, `preset` | The faithful stage: latency, A/B bit-identity with v0.5, meters, stems, presets, block-size independence |
| `balance` | **Goals, not history.** At the plugin's own defaults the choir sits 0..+9 dB over the lead, the output is not far below the input, every section is audible, CHOIR LEVEL does what it says, nothing passes full scale. It also fails if `tests/test_support.h` drifts from the defaults in `PluginProcessor.cpp`. |
| `real_vocal` | A recorded low male vocal (raw and Auto-Tuned, `tests/data/`) through the tracker and the engine |
| `python_interop` | MIDI export and the external-stem tools |

The same tests run under CTest: `cmake -S . -B build-native -DOMNICHOIR_NATIVE_ONLY=ON && cmake --build build-native && ctest --test-dir build-native --output-on-failure`.

`scripts/check_ui_syntax.sh` (also run by `scripts/run_tests.sh`) type-checks the JUCE glue against `tests/juce_stub/`, a stand-in for the JUCE API, and checks that UI strings are plain ASCII. It is a fast sanity check, not a substitute for the real build below.

`tests/calibrate_trim.cpp` is a measuring tool, not a test: it prints the choir level of the v0.5 stage against the faithful stage.

## 2. The plugin (VST3)

Requires C++17, CMake 3.22+ and JUCE 8.0.16-security (fetched from GitHub, or pass `-DJUCE_PATH=/path/to/JUCE`).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOMNICHOIR_BUILD_TESTS=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

On Linux install the JUCE system packages first (see the `plugin` job in `.github/workflows/build.yml` for the list).

## 3. Continuous integration (`.github/workflows/build.yml`)

On every push to `main` and every pull request:

1. `native-tests`: `scripts/run_tests.sh`, then again with the sanitizers.
2. `plugin` on Windows, macOS and Linux: configure, build, run the tests through CTest, then validate the built VST3 with **pluginval at strictness level 5** (this loads the plugin, runs it with varied buffer sizes and sample rates, exercises parameters and state save/restore, and creates the editor).

Until this workflow has run green, the JUCE layer (processor, editor, TakeWorker) has been read but never compiled or loaded. Treat a green `plugin` job as the gate before trusting a build, and still try it in your own DAW: pluginval does not tell you how it sounds.

## 4. UI behaviour worth knowing

- Tabs: `MAIN` (choir mix, ensemble, humanize, space, output, the four section faders and the lead fader, PROCESS and HARMONY), `ROOM`, `VOICE`, `TAKE`, `STEMS`. Hover any control for a tooltip.
- Stage: drag the four sections, your lead or the mic. Double-click one to put it back to its default.
- `ROOM`: the ENGINE switch picks the faithful stage or the simpler classic v0.5 stage; LOAD ROOM IR takes a recording of a real room for the HALL (click it again for a menu to swap or remove it).
- `VOICE`: **CHOIR LEVEL** (-12..+12 dB) sets the choir against the lead. It applies to the faithful stage; the classic stage is bit-identical to v0.5 and ignores it.
- `TAKE`: after importing a take, `EXPORT MIDI` writes a five-track MIDI file (Soprano, Alto, Tenor, Bass, Lead).
- `STEMS`: load a JSON manifest, then `MIX STEMS WAV`. Closing the plugin editor cancels active file operations.
- For the imported-stem path, set `"room": 0.2` in the manifest; its spatial engine is a lightweight offline approximation.
