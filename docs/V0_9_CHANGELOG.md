# SkellyChoir 0.9.0 (formerly OmniChoir FX) - changelog

Status: **work in progress.** Done and tested below; what is not done is listed at the end. As always, the JUCE layer has never been
compiled against real JUCE and nothing has been listened to: the CI `plugin` job is the first real build, so expect fixes there.

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
* `Source/Theme.h` (JUCE-free): GOLD (the default; every value is a frozen copy of the shipped look and is compared bit-for-bit), BLOOD RED,
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
* The conservative DSP investigation (the ~23 Hz sidebands of the granular shifter; an optional smoother voice-quality switch).

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
