# Release checklist - 0.9.0

Done in the sandbox (see docs/V0_9_CHANGELOG.md): native tests (all passing), ASan/UBSan, TSan on the LISTEN capture test, mutation checks for every new test.

Needs CI or a person. None of this has happened yet:

1. **CI build against real JUCE.** Push, let `.github/workflows/build.yml` run. Fix anything the real compiler finds in the JUCE layer
   (the editor, the processor, the curtain and the new menu code have only ever been type-checked against a stub).
2. **pluginval** (strictness 5) passes on Windows, macOS and Linux (the same workflow).
3. **Look at the screenshots** the workflow uploads: the curtain (`00a`, `00b`), the S/M buttons beside the name tags, arrows and the room menu, the skull before the title, blank key/scale, all three themes, minimum
   window size (800 x 540) and a large one. Adjust the curtain's folds and timing if needed.
4. **Listen:** rooms and preset switching (any click?), mic distance (tune `kTailDistanceExponent` in `Stage.h` if it feels off), Graveyard and Crypt,
   STEADY NOTES on a held vowel with and without vibrato, and the default sound against 0.8 (it should be identical).
5. **Old projects:** open a session saved with OmniChoir FX in at least two hosts. Does the host find the plugin? Does it sound the same?
   Duplicate entries in the plugin list are a sign the identity changed.
6. **Clean install** of the zipped VST3 on a machine that has never had the plugin: Windows (`C:\Program Files\Common Files\VST3`) and macOS (`/Library/Audio/Plug-Ins/VST3`).
7. **Signing** (optional but recommended for anyone else to use it): add the repository secrets below, re-run, and check that macOS reports "accepted" and the stapled ticket validates.
   - macOS: `APPLE_CERT_P12` (base64 of the Developer ID Application .p12), `APPLE_CERT_PASSWORD`, `APPLE_SIGN_IDENTITY`, and for notarization `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD` (app-specific password).
   - Windows: `WINDOWS_CERT_PFX` (base64 of the .pfx), `WINDOWS_CERT_PASSWORD`.
8. Before tagging: re-run the full native suite, ASan/UBSan and TSan on the final commit; keep `PLUGIN_CODE`, the manufacturer code, `BUNDLE_ID` and the state type `OMNICHOIR_FX_PARAMETERS` exactly as they are.


## Windows private-preview packaging — 2026-11-18

- Baseline Windows VST3 artifact: workflow run 44173099588, artifact 14369236040, source commit 3e6634fc5c8655bc620a0907c936679a138bd4bc.
- The release workflow now stages a per-user PowerShell 7 installer, install guide, and third-party rights inventory before writing provenance.
- SHA256SUMS.txt covers every package file except itself, including the installer and documentation. The installer verifies exact inventory coverage before any write.
- The installer defaults to %LOCALAPPDATA%\\Programs\\Common\\VST3, uses a verified backup and staged swap for updates, and supports explicit reversible rollback. It refuses unknown identities, changed installed bundles, path traversal, reparse points, duplicate/case-colliding checksum paths, and non-AMD64 modules.
- This remains a private preview-only artifact. FL Studio acceptance, Quick4 current-head screenshots, real musical A/B, target-hardware CPU/latency, owner-approved licensing, attribution, vendor URL, and support email remain open.
