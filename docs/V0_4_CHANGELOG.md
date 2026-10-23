# v0.4 — candidate-repository research and interop upgrade

**User-supplied code reviewed:** `sc-virtual-choir-master.zip`, `ai-choir-master.zip`, `Seraph-main.zip`, `Harmonizer-master.zip`.

## Implemented

- Native DSP: three independent, seeded per-singer low-rate wander states alter detuning, level, and delay for all 16 singers. Changes are humanize-scaled and saved parameters are unchanged (old host sessions load with the same parameter schema).
- `tools/satb_to_midi.py`: converts already-exported editable `.satb.csv` scores to SMF Type 1, four named SATB tracks, optional fifth lead track, constant-tempo setting and seconds-preserving timing.
- `tools/mix_external_voices.py`: offline microphone/geometry stereo mixer for separate recorded or AI-generated voice stems, with per-singer gains, relative delays, and simple reflections. Does not infer new voices or pitches.
- `tests/test_interop_tools.py`: standard-library unit tests for the new MIDI and stereo WAV tools.
- `docs/EXTERNAL_SINGERS.md`: explicit steps, model constraints and licensing notes.

## Not implemented

- Importing AGPL or GPL source into the VST, bundling unlicensed AI models, or using external Python inference on the real-time audio thread.
- Turning external voice-stem mixing into a clickable JUCE UI workflow (the tools are separate and run from command line).
- Claiming these additions replace trained vocal models or formant-aware neural resynthesis.
