# SkellyChoir 0.9.0 (formerly OmniChoir FX) - changelog

Status: **release candidate 1** (wip9). Done and tested below; what is still open is in docs/RELEASE_CHECKLIST.md and "Not done yet". As always,
the JUCE layer has never been compiled against real JUCE and nothing has been listened to: the CI `plugin` job is the first real build, so
expect fixes there. User-facing summary: docs/RELEASE_NOTES_0.9.0.md.

## Mic and lead are independent
* `Source/StageGeometry.h` (JUCE-free) holds the stage maths. The lead is drawn at `mic + (0, 2.4) + (leadX, leadY)`; the engine places her at
  `mic + (leadX, leadY)`, so her position in the room is unchanged. Home is `leadX 0, leadY 0.4`, exactly dry, as before.
* `leadX` is now -24..24 and `leadY` -16..22 (was +-9 and -5..12). The old range stopped the lead short of the right of the stage, and
  dragging the mic "kept her still" by writing offsets that hit those limits and dragged her along. Old saved values mean the same to the
  engine; only the icon moves. Host automation lanes of these two parameters are rescaled by the wider range.
* Stage drags: the thing you pick up keeps its offset from the pointer (no snapping), a click moves nothing, the nearest thing by its own
  reach wins (no lead priority), and a faint ring marks the lead's home spot. Double-click the mic or the lead to put it back.
* `tests/stage_geometry_tests.cpp`: 1.4 million mic/lead pairs reachable, exact round trip, the mic never moves the lead (on stage or in the room),
  parameter ranges and engine facts read from the sources. Mutation-tested (all 9 mutants caught after strengthening two weak checks).

## Renamed: SkellyChoir
* Changed: the editor title and subtitle (now "LISTEN & RENDER"), CMake `PRODUCT_NAME`, the plugin name the host shows, CI artifact names,
  README and docs, the listening-pack README, the settings folder (`SkellyChoir`).
* **Not changed on purpose** so old sessions load: class names, the APVTS state type `OMNICHOIR_FX_PARAMETERS`, `PLUGIN_CODE`,
  `PLUGIN_MANUFACTURER_CODE`, `BUNDLE_ID`. **Unverified:** how each host treats a VST3 whose file and display name changed but whose identity
  did not; check that an old project finds the plugin.
* The first run copies the old `OmniChoir FX` settings folder (user styles, window size, "tour seen") into `SkellyChoir`. The old folder is kept.
* `tests/branding_tests.cpp` guards all of this (7 mutants, all caught).

## Themes and the skull
* `Source/Theme.h` (JUCE-free): GOLD (the default; every value is a frozen copy of the shipped look and is compared bit-for-bit), 
  NEON GREEN, ICE BLUE. Contrast is tested for every theme: text on panels >= 4.5:1, ink on the accent gradient (including the gloss) >= 4.5:1,
  accents >= 3:1, the four sections and the lead distinct. All colours that were literals in the look-and-feel and the editor
  (header glow, stage corners, window light, dust, mute badge, recording dot, knob and fader parts, list selection, piano roll) now come from the theme;
  a test fails if a literal comes back.
* `omni::applyTheme(int)`; `refreshThemedColours()` re-reads the palette into labels, dial and fader fills and the look-and-feel. The choice is
  saved by name in `theme.txt`; another open instance of the plugin is noticed by the editor's timer.
* Each non-gold theme washes the stage picture with a gentle tint (gold has none, so its picture is untouched).
* The skull button sits right of the title, drawn from `Source/UI/SkullShape.h`; `python3 tools/render_skull_preview.py` draws it for every theme.
  Click it for the theme menu (current one ticked). A new tour step explains it. `Source/UI/TitleLayout.h` shrinks the title if needed so that
  title + skull never reach the header controls, from the 800 px minimum window upwards (tested over 5,900 width combinations).
* **Unverified:** how the tints and the skull look in the real plugin (only the Python preview of the skull has been looked at).

## Room-specific stage backgrounds
* The picture behind the singers follows the last stage preset applied (picking a ROOM, a STYLE, SURPRISE ME, or a style from the host's program list).
  Default and Cathedral keep the original photo; Concert Hall, Studio Booth, Chapel, Gospel Stage, Stadium Anthem and Headphone Choir each have a
  new 1200x530 picture in the same dark, warm, volumetric, vignetted style. They are generated, not photographs: `tools/make_scene_images.py`
  (deterministic; numpy, scipy, Pillow). Embedded through `juce_add_binary_data`, about 290 KB in total.
* Changing scene crossfades over 400 ms (Default <-> Cathedral share a picture and do not fade). `Source/Scenes.h` holds the mapping and the fade.
* The scene is saved in the session (`omniScene`; older sessions open on the original photo) and in `.omnistyle` files as `scene=N`
  (optional; a style without it leaves the picture alone; junk or out-of-range values are ignored). It affects only the picture, never the sound.
* `tests/scene_tests.cpp`: scene i == stage preset i by name, every picture is a valid 1200x530 JPEG under 400 KB and in the CMake list (and nothing
  else is), fade end points/monotonic/easing, style-file round trip and bad values, the processor and editor wiring. 15 mutants, all caught after tightening three checks.
* **Unverified:** how the pictures look behind the real stage with the singers drawn over them, and the crossfade at runtime.
  (I did look at the pictures themselves. The studio booth and chapel got one round of adjustment; expect to want more iteration.)

## Screenshots tool
* After the existing 13 pictures it now also saves `14-theme-*.png` (each theme) and `15-scene-N-*.png` (each scene, after the crossfade). The CI
  `screenshots` job uploads them with the rest. Theme screenshots do not write `theme.txt`.

## Not done yet
* Nothing from the DSP investigation is open any more: see "STEADY NOTES" below (opt-in, helps held notes only).

## v1 fixes (wip5)
- Lead singer: S (solo) and M (mute) for all five voices, lead included, in a strip along the bottom of the stage. The name tags are labels only, so nothing on the stage catches a drag any more. A muted lead is drawn faded.
- Left / right arrows at the edges of the stage switch to the previous / next stage. The stage chip shows the current stage's name.
- The faint home ring next to the mic is gone (double-click the lead still sends her home).
- Key and scale start blank; there is no "no scale" entry. Pick a scale and the key shows (C until changed).
- New stages: Graveyard (open night air, thin and far, short tail) and Crypt (low stone vault, dense heavy ring), with pictures. Gospel Stage is now a tighter room so every room differs by at least 3 dB RMS in decay (tests/room_switch_tests.cpp).
- Preset crackle: changing the room while a tail rang made the reverb's delay read points jump (Crypt -> Cathedral clicked 4x above the steady level) and the late field's level stepped when the mic array changed. Both now glide. An unchanged room is bit-identical; every golden test still passes.
- Mic distance: the tail now rises with distance from the choir (against where the preset puts the mic), so moving away adds room against the voice (about 11 dB over 8 m in the default room) and coming closer is drier. At the default mic spot the output is unchanged.
- Not verified: the JUCE UI has never been compiled against real JUCE or looked at, and nothing has been listened to.
- Arrows now clear the figures at the stage edges (the stage's width scale went from 21 to 23 m). A 90-change random fuzz of room, mic, lead and mutes (odd block sizes) stays finite, peak 0.82, no denormals; CPU is flat (~1.3 ms per 512-sample block at 48 kHz) from singing through 40 s of silence for every room.
- Last preset-switch click found and fixed: with the choir's air shelf on (Stadium Anthem, Gospel Stage...) the room level was applied as a constant per engine block, so a new reverb level stepped at the block boundary (d2 0.0142). It now ramps like the main mix, and the placed lead's room level ramps too. Worst click across 27 tail switches is now 0.00145 (the test allows 0.003).
- Editor: each stage picture is scaled once to the stage size and copied on repaint, and the previous and next stages' pictures are decoded ahead one per tick, so changing stage no longer decodes or rescales on the paint path.

## wip8 - opening curtain
- Every time the plugin window opens it starts behind a velvet stage curtain. SKELLYCHOIR fades in on it, the curtain holds for 1.6 s, then parts over 1.8 s to reveal the choir. Click it to skip the hold. The first-run tour now starts after the curtain has parted.
- UI only: no engine file changed, the default sound is untouched.
- `Source/UI/CurtainModel.h` (JUCE-free timeline and geometry) with `tests/curtain_tests.cpp` (5 mutants, all caught); `CurtainOverlay` in `PluginEditor.*`; `velvet` colour per theme in `Theme.h` (contrast checks in `theme_tests`).
- The overlay never deletes itself: the editor removes it from its own timer. `tools/screenshots.cpp` now also saves `00a-curtain-closed` and `00b-curtain-parting`.
- Sanitizers: ASan/UBSan clean over all engine and UI-logic tests, TSan clean on `listen_race_tests` (run on wip7 code; wip8 adds no engine or threaded code).
- Not verified: the curtain has never been compiled against real JUCE or looked at. Judge the fold look and timing in the CI screenshots.

## wip9 - mic limit, curtain setting, calmer room switches, STEADY NOTES, release pass
- **Mic can no longer be dragged onto the solo/mute strip.** The engine allows the mic down to y = -8 m but the stage view only draws down to
  about -1.1 m, so the mic could be dragged below the strip and out of sight. `StageGeometry::micFloorInView(plotHeight)` is the lowest y the
  view lets you drag to (the mic ring stays above the strip); it never rises above 0, so the default spot is reachable at every window size.
  Automation and presets can still put the mic anywhere the engine allows. `tests/stage_geometry_tests.cpp` section [6] (all plot heights 258-900,
  the ring size pinned to what the editor draws); 4 mutants, all caught after one weak check was fixed.
- **Curtain setting.** The skull menu has a CURTAIN section: Every time (default), Once per session (first open since the host loaded the plugin),
  Never. Saved per user in `curtain.txt`; anything unreadable means Every time. Logic in `Source/UI/CurtainModel.h`, tested (4 more mutants caught).
- **Calmer preset switches.** The feedback gains, damping and allpass gain of the late reverb now glide (`Room.h`, `kCoefSlewPerSecond`) instead of
  stepping, so a long-to-short room change no longer steps what is recirculating. Measured over all 90 room pairs: worst click excess 0.00145 ->
  0.00077, mean 0.00010 -> 0.00004. The limit in `room_switch_tests` is now 0.0010 for every room as the starting room (was 0.003 for three), and a
  new check proves the new room really arrives (a switched instance rings within 0.6 dB per band of one that started in that room; measured 0.42 dB).
  An unchanged room is bit-identical (every golden test passes). Tried and **rejected**: ramping the mic and early levels across the block
  (no measurable change, so it was removed). What is left is mostly a new tail appearing after a very short room, not a step.
- **STEADY NOTES (opt-in, default OFF).** The granular shifter reads two Hann grains half a window apart; on a pitched voice those two reads are
  out of phase, which makes sidebands at the grain rate (about 23 Hz for a shift of roughly 2.7x). With the option on, the window becomes the
  nearest EVEN number of the voice's pitch periods to 73 ms, so the two reads line up (`ChoirEffect::steerWindow`, `lockedWindowFor`). The
  window glides at most 0.002 samples per sample (a 0.2% pitch error at worst) and only follows the tracker while it is confident.
  Measured through the real engine and tracker on a steady 10-harmonic note: the energy between the harmonics falls by 23 to 47 dB (3 pitches x 3 voices).
  **Limits, measured:** with vibrato of +-25 cents or more the benefit is gone and one voice measured about 6 dB worse; a 1% pitch error already
  loses most of the gain. The window cannot follow vibrato (that would detune the voices). Click level, peak level and window range are unaffected.
  Off, the window is exactly 73 ms and nothing moves, bit for bit. A new APVTS bool `grainLock` ("Steady Notes", default false; a session without it loads
  with it off), a STEADY NOTES button on the VOICE page, never stored in style files. `tests/grain_lock_tests.cpp`: 9 mutants caught.
  **Judge by ear.** Nobody has listened to it.
- **Release pass.** `docs/RELEASE_NOTES_0.9.0.md`, `docs/RELEASE_CHECKLIST.md`. The CI `plugin` job can now code-sign and notarize: macOS Developer ID
  signing + notarization and Windows signtool, each only when its secrets exist (without them the build is unsigned, as before). Never run: needs real certificates.
- Sanitizers on the final code of this release candidate: all 34 native tests clean under ASan+UBSan, and `listen_race_tests` clean under TSan.

## Round 3 (rc2): rooms, S/M buttons, menus
- **Rooms never move anyone.** `applyStagePreset` (placement and room) is no longer used by the plugin; `applyRoomOnly` (StagePresets.h) sets only roomSize,
  roomSeconds, absorption, diffusion, wet, hall and hallSeconds. The processor's `applyRoom`, `applyStyleValues` (styles) and `loadStyleFile` (which skips
  `isPlacementId` ids) never write a singer, lead or mic parameter. Cost: Cathedral and Stadium lose their "far-away choir" placement, and
  the rooms' own ensemble/humanize values are no longer applied. `tests/room_apply_tests.cpp` (value checks, source scans, 3 mutants caught).
- **15 rooms** (was 11): Headphone Choir replaced by Opera House; Cave, Bathroom, Parking Garage, Marble Dome, Stairwell added with generated pictures
  (`tools/make_scene_images.py`). With placement fixed, rooms differ only acoustically, so `room_switch_tests` section [4] now demands every pair be >= 3.0 dB RMS apart
  in a six-band decay fingerprint (closest pair now 3.1 dB). Click limit unchanged at 0.0010; worst measured 0.00097 over 210 switches (little margin).
- **UI:** S/M buttons beside each name tag (strip removed); room name on the stage opens a room menu; the top box lists presets and your styles only;
  skull before the title (`TitleLayout.h`, test updated, 1 mutant caught); Pro hides what Easy shows (`Source/UI/PageRules.h`, `tests/page_rules_tests.cpp`,
  mutant caught); Blood Red removed (3 themes). None of this UI has been compiled against real JUCE or looked at.
- Sanitizers: the changed tests are clean under ASan/UBSan; full native suite 36/36.
