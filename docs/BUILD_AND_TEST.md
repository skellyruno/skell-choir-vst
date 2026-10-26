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

- `SCAN / EDIT`: after importing a take, `EXPORT MIDI` writes a five-track MIDI file (Soprano, Alto, Tenor, Bass, Lead).
- `STEMS`: load a JSON manifest, drag the stage groups to adjust position, then `MIX STEMS WAV`. Closing the plugin editor cancels active file operations.
- `VOICE` page: **CHOIR LEVEL** (-12..+12 dB) sets the choir against the lead. It applies to the faithful stage; the classic v0.5 stage is bit-identical to v0.5 and ignores it.
- `ROOM` controls affect the realtime DSP. For the imported-stem path, set `"room": 0.2` in the manifest; its spatial engine is a lightweight offline approximation.
