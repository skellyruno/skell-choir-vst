# Build and verify OmniChoir FX v0.5

## Native core tests (no JUCE, no internet)

```bash
cmake -S . -B build-native -DOMNICHOIR_NATIVE_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

The JUCE-free test suite compiles `ExternalStemMixer.cpp` with g++ and validates actual stereo WAV output, sample-rate mismatches, cancellation and input overwrite protections.

## Compile VST3

Requires C++17, CMake 3.22+, JUCE 8.0.16-security (downloaded from GitHub, or supply `-DJUCE_PATH=/path/to/JUCE`).

```bash
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --config Release --parallel 2
```

Then test VST3 load/automation/GUI resizing in your DAW and use pluginval if available. Source-only tests do *not* verify DAW compatibility. Keep the original source project in version control; JUCE code generation may differ across hosts.

## New UI tab behavior

- `SCAN / EDIT` → after importing a take, `EXPORT MIDI` writes a five-track MIDI file (Soprano, Alto, Tenor, Bass, Lead).
- `STEMS` → load a valid JSON manifest, drag the stage groups to adjust position, then choose `MIX STEMS WAV`. The worker keeps running if the editor remains open. **Closing the plugin editor cancels active file operations.**
- `ROOM` controls affect the realtime DSP. For the imported-stem path, configure `"room": 0.2` in the manifest; its spatial engine is a lightweight independent offline approximation.
