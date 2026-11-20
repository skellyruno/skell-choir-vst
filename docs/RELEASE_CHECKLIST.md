# SkellyChoir 0.9.0 — Windows VST3 private-preview release checklist

This release candidate is **Windows x64 VST3 only**. Do not add a Standalone, AU, macOS or machine-wide installer target as part of this release work. The package remains private-preview only until every owner and host-acceptance gate below is closed.

## Automated exact-head gates

1. **Windows build and bundle**
   - Configure and compile with the real pinned JUCE dependency using MSVC Release x64.
   - Stage exactly one `SkellyChoir.vst3` bundle and exactly one module at `Contents/x86_64-win/SkellyChoir.vst3`.
   - Reject nested EXE/ZIP files, symlinks, extra VST3 modules, invalid PE32+ AMD64 binaries, malformed metadata, duplicate class IDs, or a product/vendor/version mismatch.
   - Preserve manufacturer code `SkAu`, plugin code `OmF2`, bundle ID `audio.skell.omnichoirfx`, state type `OMNICHOIR_FX_PARAMETERS`, and all existing host automation IDs.

2. **pluginval**
   - Run pluginval strictness 5 against the exact staged Windows VST3 bundle.
   - Do not describe the Linux native-test job as Linux VST3/pluginval acceptance.

3. **Native DSP and package tests**
   - Run `bash scripts/run_tests.sh` on Ubuntu.
   - Run `python -m unittest discover -s tests -p test_release_package.py -v`.
   - Preserve the existing sanitizer, real-vocal, listen-session, room, state, preset, allocation and UI-stub checks; a UI stub is not a real editor build.

4. **Private-preview installer**
   - Run the PowerShell self-test for fresh install, idempotency, update backup, reversible rollback, pre-swap failure preservation, tamper refusal, invalid-target preservation and checksum/path validation.
   - Run the real package with `-WhatIf` and prove that neither the destination nor backup directory is created.
   - The installer defaults to the per-user `%LOCALAPPDATA%\\Programs\\Common\\VST3` directory. It must not require administrator elevation or modify registry entries, other plugins, presets, DAW projects or host state.

5. **Installable artifact confidentiality**
   - Gate the `SkellyChoir-Windows-VST3` upload on `github.repository_visibility == 'private'`.
   - Public-repository runs must still build, run native tests, verify the AMD64 PE32+ bundle, run pluginval and installer self-tests, but must not upload the VST3 bundle, installer or provenance as an Actions artifact.
   - UI screenshot artifacts are non-installable QA evidence and remain separate.

6. **Real editor screenshot workflow**
   - The `.github/workflows/ui-tour.yml` job must run for changes to source, CMake, screenshot tooling, scripts, release documentation, or the Windows release workflow. Release-only changes must not silently skip current-source UI capture.
   - Build `OmniChoirScreenshots` against real JUCE and run it under Xvfb.
   - Require all 35 PNG captures: curtain/tour, Easy and Pro modes, tabs, solo state, room view, listen take, smallest and large sizes, three themes, 15 room scenes, selected preset, and key/scale controls.
   - The renderer must fail immediately if any editor snapshot is invalid, PNG encoding fails, the output stream cannot flush, or the file is not persisted.
   - Run `scripts/verify_ui_screenshots.py` against every capture. It validates PNG chunk CRCs, the complete zlib scanline stream, exact dimensions (765x534 standard, 730x500 minimum, 1300x900 large), the exact expected filename set, non-empty files, and duplicate-image hashes.
   - Include `ui-screenshot-manifest.json` with source/CI commit, run/event/ref, byte counts, dimensions and SHA-256 for every screenshot.
   - Upload the screenshot artifact from the same PR head. Inspect the actual images; file count and successful decoding are not visual acceptance.

## Human acceptance gates

- Inspect the actual 35-image artifact for clipping, overlaps, menu legibility, theme contrast, meter/knob states, curtain transitions, and responsive layout.
- Current UI sizes: default 765 x 534, minimum 730 x 500, large capture 1300 x 900. Do not use the old 800 x 540 minimum in release sign-off.
- In FL Studio, scan and load the VST3; verify one plugin entry, correct audio routing, bypass, MIDI chord behavior where supported, automation, parameter ranges, PDC/latency, save/reopen, and old OmniChoir FX project recall.
- Listen to the same vocal takes for rooms/preset switching, mic distance, Graveyard/Crypt, steady-note behavior with and without vibrato, consonant preservation, dry/choir balance, and the pre-RC default sound. Compare level-matched renders; no metric substitutes for owner listening approval.
- Measure CPU and latency on the owner's target Windows machine. CI runner metrics are not target-PC measurements.

## Publisher, licensing and commercial blockers

- `docs/THIRD_PARTY_NOTICES.md` is an inventory, not legal approval. Confirm JUCE's applicable AGPLv3 or commercial terms, the pinned SkellCore license, VST3 SDK notices, and ownership/redistribution rights for built-in room IRs, artwork, sample audio and MIDI examples.
- The repository currently has no owner-approved project LICENSE/COPYING file. Do not invent one.
- Factory Info vendor URL and support email must be owner-approved and valid before a commercial package is prepared.
- Code signing is not configured. Do not imply that the unsigned preview is a signed installer.
- The provenance manifest must remain `preview-only` while any legal, identity, contact, signing, FL Studio, audio, or target-hardware gate remains open.
- No public GitHub Release, website deployment, DNS change, payment, pricing/trial offer, or customer distribution is authorized by a green CI run.

## Preserved Windows preview baseline — 2026-11-18

- Baseline source commit: `3e6634fc5c8655bc620a0907c936679a138bd4bc`.
- Baseline workflow run: [#44173099588](https://github.com/skellyruno/skell-choir-vst/actions/runs/44173099588).
- Baseline artifact: [#14369236040](https://github.com/skellyruno/skell-choir-vst/actions/runs/44173099588/artifacts/14369236040), archive digest `9eea66b10fdf06a053d0dca2b9638c7834ca108001b2407c83d0b1ca5fc62495`.
- Private-preview installer branch: `release/choir-private-preview-installer-rollback-20261118`.
- Exact source head verified by run #44420964148: `b80aaf0ab6bf0f32746fe6999720273504592239`.
- Installer/update/rollback and package checksum coverage passed on that exact head; the preview remains unsigned and is not FL Studio accepted.

Keep the release PR in draft until the exact current head passes Windows build, native tests, pluginval, installer tests and the current-source screenshot workflow, followed by the separate owner-side FL Studio/audio/legal acceptance.
