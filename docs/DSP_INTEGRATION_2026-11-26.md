# DSP integration audit — 2026-11-26

Baseline: main 1e979a81593ec20ba8810258d2ac2bd83d340785, merged PR #18; verified Windows artifact 15039617761, 35-image real-JUCE audit 15038637838. Both are baseline evidence, not upgraded-engine evidence.

- Integrated the owner's source-only candidate, preserving current CMake, pinned JUCE/SkellCore, Quick4, release verifier, MIDI regression, IDs, and public testing artifact policy.
- New pitchSync is separate from grainLock; new IDs are appended, default neutral and old state version migration neutralizes them.
- Audit found vocal-tract variation originally changed old blended/formant presets. It is now gated by the new vibratoSpread control (tooltip describes both vibrato and tract spread); zero leaves the published response. Factory and older user styles reset new spreads/tones/breath before applying values. Engine choice is separate from style recall.
- Reset clears tone/phrase-breath state; phrase quiet counter saturates and constant decay is precomputed.
- No new private vocal WAVs included; owner-only fixtures remain optional.
- Local candidate checks before final edits: MIDI, factory preset, key/scale, style file, stage allocation (0 allocations), sync-shift, and balance passed. After final edits processor wiring 2/2, UI/processor stub syntax passed; fresh full real-JUCE CI pending.
- Local native benchmark (3 s of existing vocal_raw_9s.wav, 44100 Hz, 256 samples, g++ -O2, shared AMD EPYC 9V74 Linux VM, no host/UI/stems): legacy 9.96%, steady 10.07%, pitch sync 9.57%, sync + spread/breath/tone + .35 synthetic blend 17.38% of one real-time core budget. Peaks finite <= 1. Fixed dry impulse delay 128 samples / 2.902 ms. Full mode worst block 9.82 ms versus 5.80 ms deadline, so investigate on target machine. Measurements are not Windows DAW CPU, and are not proof of bit-identical sound versus published main.
- Release gates: new exact-head native suite, Windows pluginval/PE32+/installer, actual upgraded UI screenshots, published-vs-candidate null and preset recall comparison, Windows host CPU/latency, FL Studio project/load/automation, and owner listening acceptance. Do not merge unverified new DSP or claim commercial readiness.
