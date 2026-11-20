# SkellyChoir — local Quick4 screenshots 🩻

This target captures the **real current JUCE AudioProcessorEditor**, including the live Rooms / Position popup used by the stage room chip. It is separate from the exhaustive 35-image CI tour and never substitutes a mockup.

## Pinned dependencies

- JUCE: `8.0.16-security` (same tag as the plugin build).
- SkellCore: `skellyruno/skell-vst-template` commit `4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d`.
- Linux GUI capture: Xvfb, Openbox, `xprop`, and `scrot` (the native popup scene needs a real window manager and desktop capture).

## Build and run

From the plugin repository root:

```bash
cmake -S . -B build-quick4 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DJUCE_PATH=/path/to/JUCE-8.0.16-security \
  -DFETCHCONTENT_SOURCE_DIR_SKELLCORESHARED=/path/to/skell-vst-template \
  -DOMNICHOIR_BUILD_QUICK4=ON \
  -DOMNICHOIR_BUILD_TESTS=OFF
cmake --build build-quick4 --target SkellyChoirQuick4 --parallel 1
xvfb-run -a -s "-screen 0 1920x1200x24" bash -euo pipefail -c '
  openbox --sm-disable >/tmp/skellychoir-openbox.log 2>&1 &
  wm_pid=$!
  trap "kill $wm_pid 2>/dev/null || true" EXIT
  for attempt in $(seq 1 30); do
    if xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id"; then
      break
    fi
    sleep 0.1
  done
  build-quick4/SkellyChoirQuick4 --output quick4-local
'
```

The output contains four current-source PNGs, a contact sheet and a JSON manifest with dimensions, byte sizes and SHA-256 for each frame:

1. `00-main.png` — Pro main page.
2. `01-room-position-popup.png` — the actual asynchronously-opened JUCE PopupMenu (not a selected dropdown or drawn imitation).
3. `02-active-solo-mute.png` — real solo and mute parameter state.
4. `03-neon-green.png` — supported Neon Green palette.

The shared workspace router registers this target as `skell-choir-vst`. Use the router's `--jobs 1` option on memory-constrained machines. Verify the output with `python scripts/verify_quick4.py --directory quick4-local --manifest quick4-local/manifest.json`.

## Verification boundary

The renderer is opt-in and does not alter the VST3 target, plugin identifiers, automation IDs, preset/state formats, or DSP. The GitHub UI workflow builds and runs this four-scene target only for the initial Quick4 migration PR; routine local screenshots should use the shared router. A local Quick4 pass validates only the editor render and the screenshot manifest. Windows x64 VST3 build/pluginval, DSP regression, FL Studio loading/state recall, audio A/B, target-PC CPU/latency, and release/legal acceptance remain independent gates.
