# OmniChoir FX v0.5 — independent singer workflow inside the GUI

**Scope:** This is a source-code iteration of the audio effect. No JUCE VST3 binary has been compiled or loaded by a DAW in this environment.

## Built in this update

1. **Critical DSP parameter-wiring fix.** The processor's `readParams()` did not forward the existing `leadX`, `leadY`, `leadGain`, `humanize`, and `formant` parameters. The stage could be dragged visually without affecting the intended processing. These parameters now reach `ChoirEffect::setParameters()`.
2. **Direct five-track MIDI export** on the `SCAN / EDIT` page. Exporting the edited score produces separate S/A/T/B/lead tracks with the captured note timing. This uses JUCE MidiFile in a dedicated worker job; no external Python script is required. Fixed tempo: 120 BPM (120 BPM maps input seconds to 960 ticks/sec). The MIDI contains guide notes, not rendered singers.
3. **Direct offline independent-stem mixing** from an external JSON manifest. New `STEMS` page has `LOAD STEM JSON` and `MIX STEMS WAV`. The mix runs on the background worker and never on the DAW audio thread. The native `ExternalStemMixer.cpp` is JUCE-independent and tested separately.
4. **Draggable stage controls now affect the stem render.** Imported manifest positions can place each singer individually, while plugin section positions move the group relative to that layout. Lead position/gain and the mic are included. The manifest's global microphone setting is used as a base offset, with plugin mic movement added.
5. **Two-pass safety gain for external stems.** The PCM16 mixer processes disk files in blocks, summing true individual stems, computing the output peak, then saving a stereo 16-bit PCM WAV with a single global safety scale. Supports aligned 16-bit WAV mono/stereo at 16–192 kHz; all sample rates must match. CPU/time requirements scale with recording length and voice count.
6. **Self-contained headless CMake test mode.** `-DOMNICHOIR_NATIVE_ONLY=ON` builds tests without fetching JUCE. Five tests now cover native DSP, full-take scan, Python/MIDI interop and C++ independent-stem mixes including a cancellation check.

## Using the complete Scan-to-Stems workflow

1. Insert OmniChoir FX on a monophonic vocal track. In `SCAN / EDIT` choose `IMPORT FULL TAKE`, then edit the four detected parts.
2. Choose `EXPORT MIDI` and import the five-track file into the DAW. Record/render actual individual singer parts to match these notes. Use licensed/offline models or human takes if desired. Save all singer files from timeline zero, with consistent sample rates.
3. Edit `tools/external-voices.example.json` so `file` points to each singer WAV and `section` names their choir part. Paths are relative to the JSON manifest. Adjust `offset_ms`/`gain_db` per singer if needed.
4. On `STEMS`, load the JSON. Drag choir sections, white lead, and the gold mic to the desired positions. Click `MIX STEMS WAV` and choose a stereo WAV destination.
5. Load that WAV in your DAW. The plugin's live processing remains available separately on the input track.

## Limitations and validation

- Neural timbre conversion/inference is **not** added: the uploaded `ai-choir` code lacks the pretrained model weights, and no third-party GPL/AGPL source has been merged.
- This offline stem mixer models relative distance, stereo position, propagation delay and two short room reflections. It **does not yet duplicate** the complete supplied JavaScript stage acoustics/FDN renderer.
- Source WAVs must be 16-bit PCM; convert FLAC/24-bit/AI-generated stems externally before using the `STEMS` page. Native worker fails cleanly for unsupported files, inconsistent sample rates and absent sources.
- MIDI export and UI integration are **source-level changes** and cannot yet be claimed as DAW-tested. CMake/native DSP and stem mixer tests pass; JUCE fetching/host builds remain to be done.
- v0.5 still needs a production vocal resynthesis engine, transient/breath handling, vocal-model licensing, plugin latency reporting and real host QA.
