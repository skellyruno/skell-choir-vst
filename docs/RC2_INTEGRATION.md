# SkellyChoir 0.9.0 RC2 integration report

## Source and compatibility
- Input: user-supplied `SkellyChoir-v0.9.0-rc2.zip` (`restore/` root).
- Base: GitHub `skellyruno/skell-choir-vst` main at `2193cc8a853196963f47712ed77f37c0fd02de97` (November 15, 2026).
- Three-way merge of RC2 editor, processor, CMake and README against the earlier WIP7 ancestor. No text conflicts.
- Preserve SkellCore hub-free `WorldLink` auto-follow and its Key/Scale/A4 transport. The RC2 archive lacks this dependency and must not be copied wholesale.
- Preserve VST3 plugin identity: manufacturer `SkAu`, plugin code `OmF2`, bundle `audio.skell.omnichoirfx`, state type `OMNICHOIR_FX_PARAMETERS`, existing automation IDs. Keep legacy application-data folder migration.
- Retain existing `AGENTS.md`, original release workflow, Quick4 integration guidance, and the older headphone scene asset in the tree. Do not delete old scene resources solely because RC2 no longer shows them.

## RC2 contents integrated
- 15 scene choices with room imagery, room-only changes that leave singers/microphone in place, smoother stage transitions and distance response.
- New opening curtain; themed skull UI (Gold, Neon Green, Ice Blue); simplified Easy/Pro page rules and separately movable singer/lead/microphone.
- Optional STEADY NOTES DSP, voice/pitch stability tests, scene/curtain/room-apply and UI layout regressions.
- Source-only tests and utility improvements, not generated `__pycache__` files.

## Verification policy
- `cmake -DOMNICHOIR_NATIVE_ONLY=ON -DOMNICHOIR_BUILD_TESTS=ON`: local configure succeeds.
- `bash scripts/check_ui_syntax.sh`: local type/syntax check against the provided JUCE stub succeeds. **This is not a real JUCE/VST3 build.**
- `bash scripts/run_tests.sh`: see the GitHub PR checks and the result reported in chat; every test must complete before promoting to a release.
- GitHub Windows VST3 Actions, actual FL Studio loading, UI screenshots from the current compiled editor, DSP listening A/B and CPU/latency measurements remain distinct acceptance gates. Do not mark RC2 released without them.

## Safety
- Do not change the plugin's internal four-character identity, state root, registered automation IDs, or legacy presets without tested migration.
- Prioritize defects and real DSP evidence over additional UI features. Keep the PR in draft until build and regressions are verified.
