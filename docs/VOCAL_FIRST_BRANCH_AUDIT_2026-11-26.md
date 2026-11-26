# Vocal-first development and branch audit — 2026-11-26

Active branches: main (stable) and feat/choir-dsp-consolidation-20261126 (draft PR #19). No separate MIDI VST3. Preserve MIDI/Listen/stems and published state/automation IDs, but focus new development on live vocal choir sound.

## Historical refs

Compared all 19 non-active branch tips against main 1e979a81593ec20ba8810258d2ac2bd83d340785 using GitHub compare. Five are ancestors; 14 have commits not reachable from main. Some were squash-consolidated or superseded; divergence alone does not prove missing features. Do not merge old release policy or Quick4 branches wholesale. Archive exact tips before deleting any refs and verify no new commits or open PRs appeared; review unique content before selectively recovering useful changes. Branch deletion is not exposed by this session's connector; no branches were deleted.

| Branch | Tip | Ahead | Behind | Disposition |
|---|---|---:|---:|---|
| docs/choir-vst3-artifact-policy-20261120 | 36562c9e2985478c38a78324520f49188434ecf2 | 1 | 4 | Archive exact tip; review divergence |
| feat/choir-quick4-local-renderer-20261119 | da559f5b14b5f46267492b152228bbf8080e2460 | 21 | 7 | Archive exact tip; review divergence |
| feat/choir-quick4-local-renderer-20261120 | b61fb808cdf1ca985e6357e83148b61bd8c2eaec | 1 | 6 | Archive exact tip; review divergence |
| feat/choir-quick4-main-20261120 | ee630cde695c529aaf9c732a07966a387b263a05 | 2 | 6 | Archive exact tip; review divergence |
| feat/choir-theme-position-ui-20261115 | 50db7748738d132d12630c59e5c9cbadde214b34 | 0 | 8 | Ancestor; cleanup eligible |
| feat/hubless-auto-key-20261111 | cb5f1d8aa91cef5ce7ef5be737ecb15c021b50ac | 9 | 29 | Archive exact tip; review divergence |
| feat/skellychoir-rc2-20261115 | 41a514d9c0a36e27695e32571d101a73124e3a27 | 4 | 19 | Archive exact tip; review divergence |
| feature/skellcore-consumer | 5f9046e9520191cda10c63f83eed2f687abd0dd6 | 0 | 46 | Ancestor; cleanup eligible |
| feature/skellcore-guided-tour | cf47e884a228b665f78a2873d09d4390edc74e85 | 0 | 30 | Ancestor; cleanup eligible |
| fix/choir-fetch-source-commit-for-reproducible-build-20261117 | 3e6634fc5c8655bc620a0907c936679a138bd4bc | 13 | 7 | Archive exact tip; review divergence |
| fix/choir-juce-output-stream-stub-20261119 | da559f5b14b5f46267492b152228bbf8080e2460 | 21 | 7 | Archive exact tip; review divergence |
| fix/choir-juce-stub-file-output-stream-20261119 | bfd0669d443ce899b33edef070f1e818686bebcb | 22 | 7 | Archive exact tip; review divergence |
| fix/choir-midi-sustain-ghost-note-20261123 | c0b3774aae00e52e7d31fe843fbdbc8d2f5654a4 | 0 | 1 | Ancestor; cleanup eligible |
| fix/choir-restore-vst3-preview-workflow-20261117 | d8db48ba400fbbfc8a4ac828ad41aaba6f71d454 | 12 | 7 | Archive exact tip; review divergence |
| qa/choir-ui-png-integrity-20261119 | b534a7e8cb779242070d819f9b8406d1021a71d7 | 20 | 7 | Archive exact tip; review divergence |
| release/choir-private-preview-artifact-guard-20261120 | 520cfce8360befc875d69cde1814cc79a6530cc3 | 23 | 7 | Archive exact tip; review divergence |
| release/choir-private-preview-installer-rollback-20261118 | ad70fb6df54e8daf5b1f904a14deb3336d9f8922 | 17 | 7 | Archive exact tip; review divergence |
| release/choir-vst3-package-verification-20261116 | ca3ab262e75f254d1b77967fd9d6f77d14e83e7c | 11 | 7 | Archive exact tip; review divergence |
| skellychoir-wip7-integration | 05a13741804cac271be7b83649a448e64584adb3 | 0 | 21 | Ancestor; cleanup eligible |

## Current work

- Fix causal seam alignment: centred correlation windows must not sample stale ring slots beyond the write head. Move both analysis windows back equally; playback delay remains unchanged.
- Keep low-pass mark history current while Pitch Sync is disabled so enabling on a held vocal does not use obsolete marks. Clear cached pitch period while disabled.
- Share four distinct vibrato exp2 targets among sixteen singers, retaining the original expression, mapping and smoothing.
- Voice page screenshot at acac8ac clipped PHRASE BREATH; shorten visible label to INHALE, retain existing parameter ID and tooltip.
- Local source candidate: isolated causal seam test failed before (correlations 1 vs 0.807008862), passed after (both 1, identical output). Existing sync test first six groups passed, including held-note artifacts, pitch accuracy, flutter, onset and toggling; run timed out before remaining groups. Optimization alone was bit-identical in four three-second real-vocal voice renders (Pitch Sync off/on, vibrato spread 0/1, voice blend 35%). These local tests used the locally retained candidate, not the exact new GitHub commit. CI and visual recapture required. No private audio added.

## Vocal-first priorities

1. Natural vocal choir; reduce seam artifacts, phasiness, robotic consonants, preserve authentic formants. Compare actual aligned vocals per SATB section and held-out performances; use level-matched A/B and preserve legacy defaults.
2. Distinct soprano/alto/tenor/bass character, individually varied vibrato, timing and phrase inhalation; avoid unnatural pitch drift and loudness instability.
3. Profile peak and percentile callback time at 44.1/48/96/192 kHz, different buffers and concurrent instances; optimize correlation search only after checking sound equivalence. Measure fixed host latency and actual singer delay separately.
4. Refine stage and microphone acoustics, retain approved layout and Quick4 renderer. Capture, inspect, repair and recapture the exact commit. Test factory presets, old sessions, and automation.
5. MIDI and stem workflows maintenance only.

## Merge gates

Previously acac8ac Windows/native and UI CI passed. New changes require exact-head native, Windows VST3/pluginval, visual checks and review. Keep main at verified baseline until then. No commercial release authorization.
