# OmniChoir FX — engineering notes

The original web demo scripts were included in `Reference/` unchanged, along with extracted readable source maps. They form a **spatial playback pipeline for pre-rendered individual voices**, not a reusable vocal-analysis-to-choir model. The original source stems and singer libraries were not provided.

This revised plugin is now an **insert audio effect** (`IS_SYNTH FALSE`, mono/stereo input, stereo output). It retains the former native DSP's eight-line FDN reverb but bypasses the old MIDI instrument. No voice synth is triggered by incoming MIDI.

`ChoirFX.cpp` implements a modest pitch tracker with decimated autocorrelation (roughly 64 ms hop, 180 ms buffer) and an overlapping-grain pitch-shifter. LIVE = default four fixed parallel semitone shifts, with two slightly detuned instances each. SCAN = analyze four seconds of monophonic input from the DAW and find the most common detected pitch class, then snap parallel harmonies toward a major/minor scale. This is intentionally labeled tonal-center detection rather than real chord/song key estimation. The voice sounds are resampled versions of the *input*, not independently modeled human choir voices.

The spatial stage controls and mic models are simplified compared with the supplied JavaScript spatial renderer: each section has stereo positioning, distance attenuation and tonal darkening, plus a shared FDN late room; imagesource reflection delays and full mic array response curves remain future work. The editor visually references the supplied cathedral screenshot and paints interactive synthetic singer groups over a cropped architecture background.

Buffers are allocated during `prepare`, not the audio callback. Audio callback stage calculations, note tracking and FDN are allocation-free. The scan trigger and progress share atomics between the audio and GUI threads; persistent scan root/status is saved as extra attributes alongside the JUCE parameter state. Gain/mix controls are smoothed; grain pitch ratio is also smoothed. The pitch process has a **varying algorithmic group delay**, therefore delay-compensation reporting is not exact. Test the actual performance in real hosts before production release.

**Current build identity:** `SkAu`, plugin code `OmF2`, product `OmniChoir FX`; different from the earlier MIDI synth's `OmC1`, so DAWs should not confuse the two formats. JUCE source must be fetched before compiling. Native tests at 44.1, 48 and 96 kHz are provided under `tests/fx_tests.cpp`. Neither JUCE VST3 compilation nor DAW load validation took place in this environment.


## v0.6 update

The paragraph above about the stage being simplified describes v0.5. In v0.6 the live path uses the ported stage in `Source/Stage/` (see `docs/V0_6_CHANGELOG.md`); `ChoirEffect::render()` keeps the old simplified stage only as a reference/baseline for tests.
