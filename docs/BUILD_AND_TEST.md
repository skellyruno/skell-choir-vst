# Build and verify SkellyChoir (v0.9.0; formerly OmniChoir FX)

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
| `midi_chord` | MIDI chord mode: key-to-section assignment, the sustain pedal, that the choir sings the chord you hold (spectrum check), silence with no key, octave folding, vibrato |
| `style_tests` | Styles, CHOIR SIZE and SURPRISE ME: every value inside the plugin's own parameter ranges, the processor sets what the styles say and never your lead/output, every style and random style renders a real voice at a sane level (`OMNICHOIR_THOROUGH=1` renders 60 random ones, slower) |
| `room_ir` | The built-in rooms: the files are sane, decay in the right order, and the engine takes each one at the right level |
| `key_scale` | KEY and SCALE: the scale maths, every (scale, key, note) lands in key, the choir sings G not G# over C major, unison and vibrato cases |
| `swell_pulse` | SWELL exaggerates dynamics (and is bit-identical at 0); PULSE has the right period, depth and follows the host position |
| `consonants` | The consonant detector on synthetic and on real singing, and that the stage stops smearing them (clarity measured inside the consonants) |
| `style_files` | Your own styles: the allow-list is made of real parameters and never of yours; round trip; hostile files |
| `listen` | LISTEN: note lookup, in-key voicing, analysis of a melody with vibrato, the capture session, playback lined up with a host clock, real takes |
| `listen_race` | The capture thread against the message thread; run it under ThreadSanitizer with `SANITIZE=thread scripts/run_tests.sh listen_race` |
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

- **Header**: KEY and SCALE (you choose; NO SCALE keeps fixed harmony intervals), STYLE / ROOM, CHOIR on/off. The STYLE / ROOM menu also holds MY STYLES (save, load, open the folder) and "Show the tour".
- **Easy / Pro** (button at the right of the tab row; new instances start in Easy). Easy shows CHOIR SIZE, CHOIR MIX, OUTPUT, HARMONY and a LISTEN button. Pro shows the tabs: `MAIN`, `ROOM`, `VOICE`, `FOLLOW` (CHOIR LEVEL, SENSITIVITY, CONSONANTS, SWELL, PULSE rate and depth, AUTO LEVEL), `LISTEN`, `TAKE`, `STEMS`.
- **LISTEN**: press LISTEN, play the take through the plugin from the start, press it again (it says STOP while listening). The piano roll shows the notes; click a note, then S+/S- ... B+/B- move a part by a semitone; the wheel scrolls. PROCESS switches to Listen by itself.
- **Stage**: drag the four sections, your lead or the mic; double-click one to put it back. Click a section's name to solo it, right-click or shift-click to mute it.
- **PROCESS = MIDI Chord**: route a MIDI track to the plugin and hold keys; the stage shows the chord.
- `ROOM`: ENGINE faithful/classic; ROOMS / LOAD IR (three built-in synthetic rooms, your own file, or remove). `TAKE` imports a recorded file and exports MIDI/WAV; `STEMS` mixes separately recorded singers.

## 5. The listening pack and the screenshots

`scripts/build_listening_pack.sh [vocal.wav [words.wav]]` renders the vocal through every feature into `./listening-pack` (WAV files and a README of what to listen for). It works on any WAV and needs only a C++ compiler.
`tools/screenshots.cpp` paints every tab to PNG headlessly; build it with `-DOMNICHOIR_BUILD_SCREENSHOTS=ON` (run it under `xvfb-run` on Linux). The CI `screenshots` job does exactly that and uploads the images; it never fails the build.

## 6. Regenerating the built-in rooms

`python3 tools/make_room_irs.py` (needs numpy and scipy) rewrites `Assets/Rooms/*.wav` deterministically. `tests/room_ir_tests.cpp` checks the result.
