# Real-vocal test fixtures

`vocal_raw_9s.wav` and `vocal_autotuned_9s.wav` are the same 9-second excerpt of a low male lead vocal (44.1 kHz, 16-bit, mono, original level,
5 ms fades at the cut). The first is the raw take; the second is the same take after Auto-Tune Pro (low-male setting).
They were supplied by the project owner for testing and are used by `tests/real_vocal_tests.cpp`.
Do not normalise or re-level them: the loudness tests depend on the recorded level (voiced RMS about -18 dBFS, peaks about -0.2 dBFS).

`vocal_raw_hard_9s.wav` is a different 9-second excerpt of the same raw take (from 17.5 s): the loud, high, difficult part, where the pitch tracker
used to report wrong notes. It exists to keep the tracker honest on the hard material, not only on the easy phrase in the other two files.
