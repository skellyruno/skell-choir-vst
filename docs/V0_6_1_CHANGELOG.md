# OmniChoir FX v0.6.1

## What was wrong

At the plugin's default settings the choir was too quiet to read as a choir. Measured on a real recorded low male vocal (raw take, voiced
level about -18 dBFS), with the defaults the v0.6.0 plugin shipped:

| | v0.6.0 | v0.6.1 |
|---|---|---|
| choir against the dry lead it sits behind | **-7 dB** (choir quieter than the voice) | **+4.6 dB** |
| whole output against the input | **-13 dB** | **-5.7 dB** |

The pitch tracker and the harmonies were correct (the +7, +4, -5 and -12 semitone voices are all present); only the level and balance were wrong.
v0.6 had deliberately matched v0.5's choir level, and v0.5 was already that quiet. The old `Preview/Demo-v0.4-classic-engine.wav` sounded fuller only
because it used MIX .93, ROOM RETURN .72, ENSEMBLE .69, a 2.3x gain boost and peak normalisation. See `Preview/README.md`.

## Changes

1. **Choir make-up gain, +12 dB** (`StagedChoir::kChoirMakeupDb`, `kVoiceTrim` 0.0466 -> 0.1855). Faithful stage only; the classic v0.5 stage is untouched and stays bit-identical to v0.5, so the A/B now differs by about 12 dB on purpose.
2. **CHOIR LEVEL dial** (`choirLevel`, -12..+12 dB, default 0, smoothed over about 30 ms so it cannot zipper). On the VOICE page next to MIX. The classic stage ignores it.
3. **OUTPUT default 0.9 -> 1.2.** New instances only; the range (0..1.5) is unchanged so host automation keeps its meaning. A hot vocal at OUTPUT 1.5 and CHOIR LEVEL +12 still never passes full scale (the output ceiling is tested).
4. **Saved-state versioning.** The state carries `omniStateVersion` (now 2). A session saved by v0.6.0 or earlier has no version, and loads with CHOIR LEVEL at -12 dB (`kLegacyChoirLevelDb` in `PluginProcessor.cpp`) so it sounds exactly as it did; the dial shows why. Set the constant to 0 if you would rather old sessions get the new level.
5. **Tests that check goals, not history** (`tests/balance_tests.cpp`, `tests/real_vocal_tests.cpp`, `tests/test_support.h`, fixtures in `tests/data/`):
   - at the plugin's own defaults the choir sits 0..+9 dB over the lead and the output is no more than 9 dB under the input, at three pitches and from -33 to -9 dBFS input;
   - all four sections within 3 dB of each other (measured: within about 1 dB);
   - CHOIR LEVEL moves the choir by exactly what it says, clamps, does not click, and does not affect the classic stage;
   - a real recorded vocal, raw and Auto-Tuned, through the tracker and the whole engine;
   - the test defaults are parsed out of `PluginProcessor.cpp` and compared, so the tests cannot drift from what a user hears.
   Mutation-checked: with the old quiet choir restored, `balance` and `real_vocal` both fail ("choir -4.3 dB against the lead").
6. **CI**: `native-tests` (everything, then everything under ASan+UBSan), then `plugin` on Windows, macOS and Linux: build, CTest, and **pluginval at strictness 5** on the built VST3. Previously CI only built on Windows and macOS and never ran a test.
7. **Housekeeping**: `scripts/run_tests.sh` (all tests, no CMake or JUCE), `docs/BUILD_TEST_V05.md` -> `docs/BUILD_AND_TEST.md` (rewritten), `Preview/OmniChoir-FX-v0.4-Preview.wav` -> `Preview/Demo-v0.4-classic-engine.wav` plus `Preview/README.md`, `.gitignore`, no `__pycache__`/`.pyc` in the package.

## Corrections to earlier notes

- An earlier review said the bass voice was 5 to 12 dB weaker than the others. That came from a single-frequency-bin measurement of a synthetic tone and was wrong. Measured as loudness, on synthetic and on real vocals, all four sections are within about 1 dB. Nothing was changed; the `balance` and `real_vocal` tests now guard it.
- Level figures quoted earlier in the session used the engine's struct defaults instead of the plugin's defaults and were about 2 dB low. The figures in this file use the plugin's defaults.

## Known limits (not fixed)

- **The JUCE layer has still never been compiled or loaded by anyone.** v0.6.1's edits to `PluginProcessor.*` and `PluginEditor.cpp` (the new parameter, the dial, the state migration) were written by following the existing patterns and read back, not built. The CI `plugin` job is the first thing that will compile and load them; its workflow file has been checked for YAML validity but has not run.
- **Nobody has listened to v0.6.1.** The numbers say the balance is right; they cannot say it sounds good.
- **Pitch tracker**: on real voice it holds the note and agrees with itself across the raw and tuned takes (median 0.27 semitones), but low-confidence frames (breaths, consonants, note changes) can report a note above 520 Hz: 2% of the 9 s test excerpt, about 7% of the full 30 s take. It does not touch the default Live sound (fixed intervals) but matters for Scan mode and the CHOIR VOICE blend. `real_vocal_tests` guards it from getting worse.
- The whole output is still about 5 to 6 dB under a hot input at the defaults: MIX 0.78 leaves the dry voice at 22%. CHOIR LEVEL and OUTPUT are there to bring it up; matching a hot vocal's level outright needs OUTPUT above its 1.5 maximum.
