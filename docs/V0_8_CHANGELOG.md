# OmniChoir FX v0.8.0

Eight of the ten ideas from the last list, plus the Melodyne-like **Listen** mode you asked for. Measured with the native tests on the recorded low male vocal in
`tests/data/`. **The JUCE layer has still never been compiled against real JUCE or loaded in a host, and nothing here has been listened to.** See "Known limits".

## Listen mode (new): analyse your take, then choir *that*

Press LISTEN, play your vocal through the plugin from the start, press it again. The plugin analyses what you sang into notes (the scanner that already existed), voices the choir for each
note in the key and scale you chose, switches PROCESS to **Listen**, and from then on the choir sings that score in time with the host's play position instead of guessing the harmony live.
The LISTEN tab shows a piano roll: your notes in white, the four choir parts in colour, the scale shaded, a gold line following playback. Click a note and move any part a semitone with the
buttons; change the key or scale and the take is re-voiced (unless you edited notes by hand: your edits are never overwritten).

| measured | result |
|---|---|
| a sung melody with vibrato, 8 notes | 8 of 8 found, right pitches, starts within 90 ms |
| your two real 9 s takes | 17 and 14 notes, covering 70% and 65% of the sung time, every choir part inside the chosen key |
| playback in C major while E is sung | the alto sings G over G# by 17 dB; the choir stays in key |
| capture thread vs message thread | 300 start/stop/analyse cycles with the audio thread feeding throughout: clean under ThreadSanitizer |

It is *not* pitch correction: the harmony notes follow your sung pitch plus a fixed interval, so they inherit your tuning (the lead itself is never retuned). It needs the host to report its
play position (most do; the footer says so if yours does not). Capturing is real time (you play the take through once), up to 120 s (less at very high sample rates: the buffer is capped at about 32 MB).
Mode 1 of PROCESS used to be "Scan / Harmony" (a 4-second key detector); that is gone and the slot is now Listen. A session saved with it opens as Listen with no take captured, which behaves like Live.

## Key and scale (instead of detection)

Two selectors in the header, KEY (C to B) and SCALE (no scale, major, minor, harmonic minor, dorian, mixolydian, major and minor pentatonic). Nothing is detected: you choose.
With a scale chosen the live harmony is diatonic (each section moves a fixed number of scale steps: +4, +2, -3, -7 in seven-note scales), so it cannot leave the key. NO SCALE keeps the fixed intervals of earlier versions.
Measured: 3,108 combinations of scale, key and sung note: every section lands in the scale. Singing E over C major, the alto sings G and not G# (53 dB apart); with vibrato in the voice the harmony still stays in key.
**SCAN LIVE INPUT is gone** from the header.

## Consonants

Sixteen voices smear s, t and k. A fast detector (broadband noise bursts: much of the energy above 3 kHz and little below 1.2 kHz) finds them; while one is sounding the choir is fed the voice **without** it,
and your original consonant is passed through cleanly. Calibrated on your recording, not on synthetic sounds: your sung vowels measure 0.05 on the detector's ratio (99.9% of frames under 0.21), your consonants 0.47 (median).

| measured | result |
|---|---|
| your full 30 s take, against an independent spectral definition of "consonant" | 87% of consonant frames found, 83% of what it flags is right, 5.6% of sung time flagged |
| detection time on bursts | about 2 ms |
| synthetic vowel-s-vowel-t-vowel-k phrase | the energy smeared after a consonant falls from +1.1 dB to -39 dB (relative to inside it) |
| your real singing, inside the consonants, at the stage latency | the output follows your voice 0.82 -> 1.00 (correlation); energy after the consonants 22.5 -> 4.0 dB |
| vowels and overall level | -0.10 dB and -0.09 dB: untouched |

Default 50%; a session from before v0.8 loads with it off. It is on the FOLLOW tab. Limits: calibrated on one singer (a low male close-mic recording): a very different voice or microphone may need a different threshold
(there is no control for that yet); a voiced fricative (z, v) can be treated as a consonant; the classic stage ignores it.

## Swell and Pulse (FOLLOW tab)

**SWELL**: the choir blooms when you sing louder than your own average (learned over about 3 seconds) and recedes when softer. A 12 dB difference between loud and soft phrases becomes 12.4 / 16.4 / 20.4 dB at SWELL 0 / 50 / 100%.
A steady singer is unchanged (+0.01 dB); at 0 it is bit-identical to before. **PULSE**: the choir breathes in time with the song, once per 2 bars, 1 bar, half a bar, a beat or half a beat, locked to the host's tempo and position
(re-synced whenever it drifts more than a sixteenth of a beat; free-runs at 120 bpm if the host reports no tempo). Measured at one beat and 120 bpm: a 0.50 s period, a 26 dB swing at full depth, and starting a quarter beat later moves it 125 ms.

## Your own styles and the host's preset list

STYLE / ROOM menu: your sounds appear under **MY STYLES**; "Save this sound as a style..." writes a small plain-text `.omnistyle` file (default folder: your application-data folder, "OmniChoir FX/Styles"; you can save anywhere to share);
"Load a style file..." reads one and keeps a copy; "Open my styles folder". A style file holds only the sound (where the singers stand, the room, how they sing, consonants/swell/pulse). Only 38 allow-listed parameters are ever read from a file, never your lead level,
output, AUTO LEVEL, CHOIR LEVEL, mode, key, scale or solo/mute: a test reads the plugin's parameter list and fails if that ever changes, and a hostile or malformed file (nan, repeated ids, huge, control characters) is tested not to get past the list.
The six factory styles are also the host's preset list (program list); picking the program that is already current does nothing, so a host that "sets program 0" on load cannot overwrite a restored session.

## First-run tour and a remembered window size

A short tour (stage, style menu, key, the size knob, Listen, Easy/Pro) shows the first time a user opens the plugin and never again (a flag file in the application-data folder; "Show the tour" in the STYLE / ROOM menu brings it back).
The window size is remembered between sessions (per user, written once it has stopped changing). The minimum window is now 800 x 540 (it was 740 x 540) to fit seven tabs.

## Tools

- **`scripts/build_listening_pack.sh`** renders one vocal through every feature (styles, rooms, AUTO LEVEL, key, Listen, MIDI chords, consonants, swell, pulse, solos) as WAV files with a README listing the measured level of each and what to listen for. It works on any WAV. A rendered pack from your takes is in `OmniChoir-listening-pack.zip`.
- **Screenshots from CI**: `tools/screenshots.cpp` (CMake option `OMNICHOIR_BUILD_SCREENSHOTS`) paints every tab, Easy and Pro, the tour, a muted/soloed section, a captured take in the piano roll, and the smallest and a large window to PNG with no DAW. The CI `screenshots` job runs it under xvfb and uploads the images. Not a gate: it cannot fail the build.
- `SANITIZE=thread scripts/run_tests.sh listen_race` runs the capture test under ThreadSanitizer (also in CI).

## The pitch tracker: tried, not adopted

A pYIN-style probabilistic tracker (candidate probabilities plus a hidden-Markov filter) was built and measured against the v0.7 tracker on both full takes and the hard excerpt. **No setting beat the existing tracker on all three measures**:
a strict prior removed glitches but lost lock on rough loud singing (92% on the hard passage); a permissive one kept lock but let the glitches back in. The v0.7 tracker stays. The code and the full table are in `docs/experiments/`.
A real fix needs better candidates (a second estimator), not a better filter, and several voices to tune on.

## Not done (as chosen)

The CPU / real-time-safety audit and the MIDI latch / scale lock were not selected and not built.

## Saved sessions

State version 4: a session from before v0.8 loads with CONSONANTS off; SWELL, PULSE, KEY and SCALE start at their neutral defaults. PROCESS gained its Listen meaning in place of Scan / Harmony (same index), so automation of PROCESS keeps its values but not its meaning for mode 1.

## Known limits (not fixed)

- **The JUCE layer has never been compiled against real JUCE or loaded in a host.** The new UI (key/scale boxes, seven tabs, piano roll, tour overlay, style menu with file choosers, the host-position code that reads the playhead and tempo, MIDI) was type-checked only against `tests/juce_stub/`, my stand-in. It caught several real mistakes during this work
  (a processor member with no constructor argument, a helper lambda with the wrong parameter type) but cannot catch a wrong assumption about the real JUCE API. The CI `plugin` job is the first real build; expect to fix something.
- The screenshots job and the tool are unverified: they have never been built.
- **Nobody has listened to v0.8.** The styles, the built-in rooms, Listen mode and the consonant path in particular are judged by measurement only. Use the listening pack.
- The consonant detector is calibrated on one singer (see above). Listen mode needs a host that reports its play position and inherits your tuning. The tracker is unchanged and still occasionally reports a wrong high note.
