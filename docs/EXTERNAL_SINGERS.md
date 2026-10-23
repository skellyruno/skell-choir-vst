# OmniChoir FX — external singer integration (experimental offline workflow)

The plugin's live effect currently makes **16 DSP-derived layers from the same incoming vocal**. An external model that renders a *different person's voice* is the missing ingredient for convincingly independent singers. The provided `ai-choir-master.zip` is one possible **offline** renderer. This workflow does not pretend to load it directly in a VST3 audio callback.

## From Full Scan to independently rendered voices

1. In OmniChoir `SCAN / EDIT`, import a clean monophonic vocal and adjust the detected soprano, alto, tenor and bass notes. Render WAV to save the SATB CSV, or scan with `omnichoir-batch`.
2. Export the editable score to standard MIDI with four SATB tracks plus an optional guide lead:

   ```bash
   python3 tools/satb_to_midi.py Preview/Example-Editable-SATB.csv score.mid --include-lead
   ```

   The generated tracks use General MIDI Choir Aahs as a *guide instrument*, not real singers. Import the `.mid` into your DAW and record/render **separate lead-quality vocals singing those pitches**, or produce per-part guides for a voice conversion system that accepts them. Pitch bends, lyrics, phonemes and breaths are **not** inferred by the CSV-to-MIDI exporter.
3. If you use `ai-choir`, run it separately, after obtaining permitted model weights and following that repository's setup. Its bundled script converts a provided vocal to different voice identities and mixes them, but **does not derive SATB melodic lines from OmniChoir's score**. For real SATB, feed each part's separately sung/rendered phrase to your own authorized model workflow, saving each identity as an independent WAV stem. A stem must be an actual rendered voice; no models or AI inference are included here.
4. Create a manifest describing the files' section, position, gain, and optional arrival offset. Copy `tools/external-voices.example.json` beside a `stems/` directory. The `file` paths are relative to the JSON manifest; missing files fail with a clear message.
5. Mix the stems in cathedral-stage positions:

   ```bash
   python3 tools/mix_external_voices.py tools/external-voices.example.json choir-from-stems.wav
   ```

   Replace the example paths with your stems. All inputs must be mono or stereo **16-bit PCM WAV**, at the **same sample rate**, aligned to the same start point. Convert externally first if needed:

   ```bash
   ffmpeg -i voice-conversion-result.mp3 -ar 48000 -ac 1 -c:a pcm_s16le stems/soprano-1.wav
   ```

   The mixer outputs stereo 16-bit PCM. It approximates distance, left/right arrival and a few early reflections; it is **not a port** of the JavaScript `SpatialRenderer` image-source and eight-line FDN reverb. The microphone's position and singers' positions are meters, as in your demo.

## What the additional repositories contribute

| Repository | Relevant technique | Directly bundled into v0.4? | Reason |
| --- | --- | --- | --- |
| sc-virtual-choir | Pitch-class harmonization mapping, live control | No | SuperCollider and Tartini dependency; GPLv3 code. Native conceptual reference only. |
| Harmonizer | YIN/MPM pitch analysis and granular/delay shifter | No | Older JUCE plugin with copyright notices; no clear top-level redistribution license in the uploaded ZIP. |
| Seraph | Musical doubler, micro pitch, randomized expression, de-essing, compressor | No | AGPLv3; do not silently merge source into a different license. |
| ai-choir | External voice identities using so-vits-svc and offline stem stacking | No | README describes MIT for the top project, but its embedded so-vits-svc is AGPLv3. The 7 speaker folders contain config JSON **without model.pth files**; inspect licensing and individual model rights before use. |

These repositories are preserved as **external research inputs**; OmniChoir v0.4 does not redistribute their source or model weights. The added CLI scripts use Python's standard library and were written separately. This is technical integration guidance, not a legal opinion. If distributing a proprietary plugin, obtain legal advice and verify JUCE's license and every third-party model asset before shipping.

## What's genuinely new in v0.4

- Native **16-singer** processing gets independent seeded, slowly wandering pitch, level and timing changes, rather than every layer's pitch moving on a predictable periodic sine. It is deterministic across runs and performs no audio-thread allocations.
- **Editable SATB MIDI** export from existing score CSV; notes are placed at their source time positions. Five tracks when `--include-lead` is requested.
- **Offline voice-stem spatial mixer** accepts independent human or AI voice recordings and blends them as a choir on the stage with microphone geometry. The lead can be its own neutral central track.
- Validation tests include native 44.1/48/96 kHz checks, score editing, MIDI structure and external-stem roundtrip.

## Still missing from a production-grade choir FX

- No pretrained AI voice weights; no inference inside the plugin; no real 64-person vocal library.
- No guaranteed pitch/formant preservation of real voice identity during transposition (existing LPC path remains experimental).
- Full-take SATB score remains template-derived and requires musical review, not automatic contrapuntal arrangement.
- Offline spatial mixer is separate, **not a VST3 button**; no DAW-host compilation or pluginval pass has been performed here.
- Live pitch analysis still favors a clean, solo input and has nonzero capture/processing latency, even though the plugin does not yet publish a fixed delay value.
