# SkellyChoir 0.9.0 - release notes

SkellyChoir (formerly OmniChoir FX) turns one vocal into a 16-voice choir (soprano, alto, tenor, bass). The choir sound and the tracking are
unchanged from 0.8: every new sound feature is off by default, and the default sound is bit-for-bit what it was.

## New
- **Opening curtain.** The window opens behind a velvet curtain with the name on it, then it parts to show the choir. Click it to skip the wait.
  The skull menu chooses when it plays: every time, once per session, or never.
- **Three themes** (Gold, Neon Green, Ice Blue), changed from the skull button beside the title. Gold is the look you know.
- **15 rooms, each with its own picture:** Default, Cathedral, Concert Hall, Studio Booth, Chapel, Gospel Stage, Stadium Anthem, Opera House, Graveyard,
  Crypt, Cave, Bathroom, Parking Garage, Marble Dome and Stairwell. Click the room name on the stage for a menu, or use the arrows at the edges.
  A room is only the space: **choosing a room, a preset or a style file never moves the singers, the lead or the mic.** (Headphone Choir was dropped.)
- **The top box is presets only.** Rooms live behind the room name on the stage.
- **Solo and mute for every voice, the lead included,** as two small S and M buttons beside each name tag.
- **The skull comes first** (left of the title). **Blood Red is gone:** a saved Blood Red choice opens as Gold.
- **Pro no longer repeats Easy:** CHOIR SIZE, CHOIR MIX, OUTPUT and HARMONY show in Easy only.
- **The mic and the lead move independently.**
- **Key and scale start blank.** Choose a scale and the key appears.
- **Mic distance matters:** the further the mic is from the choir, the more room tail you hear against the direct sound. At the default spot nothing changes.
- **Smoother room changes:** switching stages while a tail is ringing now produces a much smaller click (measured; not yet listened to).
- **STEADY NOTES (optional, VOICE page).** For held, steady notes it removes the faint ripple of the harmony pitch shifting. It does not help with
  strong vibrato, and it is off until you turn it on. Please tell us what you hear.

## Changed
- The plugin is now called SkellyChoir. Old sessions, styles and settings still load: the plugin's identity inside the host did not change.
  Your old settings are copied into the new settings folder the first time it runs.

## Known limitations
- Latency is not fixed: the pitch shifter has a variable 10-90 ms delay, so the host's delay compensation is approximate.
- STEADY NOTES helps only on steady notes (see above).
- Native DSP tests and a Windows VST3 build are checked by the repository's GitHub Actions. A release remains pending until the RC2 branch passes its exact-commit build and UI checks, plus real FL Studio and audio validation; macOS and Linux VST3 deliverables are not part of the current release workflow.
