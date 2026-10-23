# Implementation notes

The four uploaded JavaScript sources comprise a website controller (`demo.js`), a minified audio-worklet bundle (`demo-engine.js`), and two source-map JSON documents (`demo-choir-stage.js`, `demo-choir-worklet.js`). `Reference/stage-extracted.js` and `Reference/worklet-extracted.js` restore readable code from the `sourcesContent` fields of the source maps.

The demo's `ChoirStage` and `SpatialRenderer` separate vocal source generation from acoustic placement. This VST uses the same architectural separation: native `Instrument` instances generate formant-filtered oscillator waveforms, native geometry converts each voice's stage position into microphone arrival times/level, and an 8-delay-line FDN simulates a late stereo room field. Changes of stage placement update gain and delay targets with smoothing. JUCE handles MIDI, DAW parameter automation, host state serialization, and GUI.

**Different from the original:** the reference renderer implements reflection-image geometry and section-spot/focus microphones, air shelf automation, specialized close/room/main routing, moving leads, and sample-specific stereo stems; the current plugin has simpler direct pickup plus a modeled room. It is an original playable prototype, not a direct C++ port of every algorithm.

**Performance:** voices/reverb/delay buffers are allocated once in `prepare()`. MIDI handling is sample-position accurate within DAW blocks. No file loading, heap allocation, GUI calls or audio-device initialization is performed in `render()` or `processBlock()`. Parameter changes are read per block; filters are retuned on vowel or pitchbend changes, and spatial targets are smoothed sample by sample. Complex presets with high polyphony should still be profiled in a release VST build.

**Versioning:** preserve the `SkAu` manufacturer and `OmC1` plugin identifiers for future versions of *this* instrument; use a different unique plugin code if this project is forked into a different VST product.
