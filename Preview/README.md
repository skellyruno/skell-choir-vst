# Preview files

These are demonstrations and design references, **not** measurements of the current plugin.

| File | What it is |
|---|---|
| `Demo-v0.4-classic-engine.wav` | Rendered by `tests/render_fx_preview.cpp` with the **v0.4 classic engine** and hand-picked settings (MIX .93, ROOM RETURN .72, ENSEMBLE .69, a 2.3x gain boost, then peak-normalised to 0.92). It is much louder and more choir-forward than the plugin's defaults, so do not judge the plugin's level against it. |
| `Example-Choir-Render.wav` | A full-take render made with `tools/omnichoir-batch` from a synthetic 10-second example. Quiet by design (about -29 dBFS RMS). |
| `Example-Editable-SATB.csv`, `.mid` | The editable arrangement the scanner produced for that example. |
| `index.html`, `GUI-design-reference.png` | An interactive visual mock and a concept image of the GUI. Not a VST, not a DAW screenshot. |

To hear the current engine, render your own vocal: `scripts/run_tests.sh` prints the balance numbers, and `tools/omnichoir-batch.cpp` renders a take to WAV.
