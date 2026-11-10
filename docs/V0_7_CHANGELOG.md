# OmniChoir FX v0.7.0

Eight features, built on v0.6.1. Everything below was measured with the native tests on the recorded low male vocal in `tests/data/`;
nothing has been listened to, and the JUCE layer (the new buttons, menus, parameters, MIDI input) has never been built against real JUCE.
See "Known limits" at the end.

## 1. AUTO LEVEL: the loudness stays put as CHOIR MIX moves

Before: at the defaults the plugin was about 6 dB under your own vocal, and moving CHOIR MIX moved the level (about 7 dB between 0% and 100%).
Now (`StagedChoir::kMatchedChoirRatio`): the output is scaled by `1 / sqrt((1-mix)^2 + (mix * 0.5)^2)`, the power sum of the voice and the choir,
where 0.5 is the measured level of the choir against the voice it is made from (-6.0 and -6.4 dB on the two real takes, -3 to -6 dB on synthetic phrases).

| | across MIX 0..100% (real vocal) |
|---|---|
| AUTO LEVEL off | output 7 dB below the input at the loudest MIX setting |
| AUTO LEVEL on | output -0.7 to 0.0 dB against the input (raw, tuned and the hard excerpt) |

It is exactly neutral at MIX 0, with the choir off, and on the classic stage (tested bit-identical). With it on, CHOIR LEVEL moves the choir against the lead
at constant loudness (+-6 dB moves the balance by 12 dB while the output changes under 3 dB). It corrects the MIX law only: stage placement moves the choir by -2.2 to +2.7 dB
and VOICE BLEND dips it by up to 2.6 dB, and AUTO LEVEL does not hide that. A vocal that already peaks near 0 dBFS reaches the soft output ceiling more often now
(0.02 to 0.04% of the sung samples on the test takes, only through the soft knee; a regression test caps it at 0.1%).
Switch: AUTO LEVEL on the VOICE tab. Default OUTPUT is back to 1.0 (it was 1.2 in v0.6.1).

## 2. CHOIR SIZE and Easy mode

`choirSize` is one macro (0..1, 0.5 neutral) applied on top of the detailed knobs: bigger is looser (ensemble, humanize), wetter (space, reverb time, room size) and wider (mic spacing);
smaller is the opposite. Measured: stereo width 0.39 at size 0, 0.45 at size 1. The detailed knobs keep their own meaning.
**Easy mode** (button at the right of the tab row; new instances start in Easy) shows only CHOIR SIZE, CHOIR MIX, OUTPUT and HARMONY, plus the stage, the style menu and SURPRISE ME.
Pro shows everything as before. A session saved before v0.7 opens in Pro.

## 3. Styles

The header menu now lists STYLES (a whole sound in one click) above ROOMS (where the singers stand, as before): Natural Choir, Gregorian Chant, Gospel Choir, Cinematic Pad,
Lo-fi Church, Intimate Quartet. A style is a room + a harmony + how they sing (ensemble, humanize, character, voice blend, vowel, breath, vibrato, size, mix).
It never touches your lead level, OUTPUT, AUTO LEVEL, CHOIR LEVEL, PROCESS or solo/mute (a test reads the processor source and fails if it ever sets one of them).
The six styles are my reading of the names, **not tuned by ear**: treat them as starting points. On the real vocal each lands between -4.8 and +0.7 dB of the input, with the choir between -0.6 and +7.8 dB against the lead.

## 4. Pitch tracker

A leap of more than a fourth inside a steady note is believed only after 5 frames (40 ms); new notes after a gap, and steps of a fourth or less, still lock at once.
On the two full 30 s takes this cut glitch excursions (a leap that comes straight back) from 5 and 8 to 3 and 5, with no cost in lock rate or lock time.
**This is a modest improvement, not a fix**: octave errors against an independent estimate are about the same (13 and 10 before, 13 and 9 after), and the tracker still occasionally reports a wrong high note.
I tried a stricter acceptance threshold as well; it cost 3% of lock on the hard passage for no gain and I removed it. A real fix would be a probabilistic pitch tracker (pYIN-style); that is a separate project.
New real-vocal test material: `tests/data/vocal_raw_hard_9s.wav`, the loud high passage. `real_vocal_tests` now caps glitch excursions.

## 5. MIDI chord mode

PROCESS has a third choice, **MIDI Chord**. Hold keys and the choir sings that chord from your voice: the held keys are spread over the four sections (1 key: everyone; 4 keys: one each, highest to the soprano)
and each section shifts your sung note to its key, folded to within an octave of your voice. The sustain pedal works. With no key held the choir is silent. The stage shows the chord being held.
Measured: sung G3 with C major held gives C4, E4 and G4 at least 20 dB over the wrong notes; the choir's total level is the same as Live mode (within 2.2 dB across all intervals); vibrato in the voice does not shake the chord.
It needs a MIDI track routed to the plugin, which depends on the DAW (Reaper, Bitwig, Cubase and FL Studio can; Ableton does not send MIDI to audio effects). The plugin is still an audio effect.
Harmony notes follow your pitch as sung, so they inherit its tuning and vibrato; they depend on the tracker (4 above).

## 6. Solo and mute

Click a section's name tag on the stage to solo it, right-click (or shift-click) to mute it; the tag shows S or M and silenced sections dim. Four automatable parameters (`sectionState0..3`).

## 7. A living stage

The singers sway and open their mouths with their section's level; the lead's halo breathes and a gold ring appears at her feet while the tracker is sure of her pitch
(a new `pitchConfidence` meter). All drawing only; nothing in the audio depends on it.

## 8. Built-in rooms and SURPRISE ME

The ROOMS / LOAD IR button opens a menu: Cathedral, Stone Chapel, Small Hall (built in), your own file, or remove. They are **synthetic** impulse responses made by `tools/make_room_irs.py`
(sparse early reflections plus a diffuse tail that decays faster at high frequencies, decorrelated between the channels), not recordings of real spaces: broadband RT60 5.5, 1.9 and 1.2 s.
They are heard through the HALL knob, so choosing one brings HALL up to 25% if it was off.
**SURPRISE ME** applies a random style drawn from limited ranges (MIX 70 to 90%, never so low that a distant room plus synthetic voices buries the choir). 300 random styles are range-checked and 60 were rendered on the real vocal:
output -3.5 to +1.3 dB of the input, choir -3.9 to +12 dB against the lead, nothing over full scale (the normal test run renders 8; `OMNICHOIR_THOROUGH=1` renders 60).

## Saved sessions

State version 3. A session saved before v0.7 loads with AUTO LEVEL off and OUTPUT as saved, so it sounds as it did (older ones still load with CHOIR LEVEL at -12 dB, see v0.6.1).
The PROCESS choice gained a third item, so a host that automates PROCESS by normalised value will see its middle position move: re-check any automation of that parameter.

## Known limits (not fixed)

- **The JUCE layer has never been compiled against real JUCE or loaded in a host.** The v0.7 UI, parameters, MIDI input, built-in-room loading and state migration were type-checked only against `tests/juce_stub/`, a stand-in I wrote (`scripts/check_ui_syntax.sh`), which already caught one real error (a lambda taking `const char*` that was passed a `String`). It cannot catch a mistake about the real API. The CI `plugin` job is the first real build; expect to fix something.
- **Nobody has listened to v0.7.** The styles and the built-in rooms in particular are unjudged by ear. The stage animation and the new layout are unseen.
- The tracker is better at glitches, not good (see 4).
- AUTO LEVEL assumes the lead fader at unity and the section faders near their defaults; moving them moves the loudness, as faders should.
- MIDI chord mode inherits the tracker's errors and your voice's tuning; it is a harmonizer, not a sampler.
