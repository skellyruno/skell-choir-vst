# Experiment: a pYIN-style pitch tracker (v0.8). Not adopted.

**Idea.** Replace the tracker's frame-by-frame decision (YIN, then a median and a leap-confirmation rule) with the probabilistic scheme of pYIN
(Mauch and Dixon, 2014): every dip of the YIN difference function becomes a pitch candidate whose probability is the weight of the absolute thresholds
that select it; a causal hidden-Markov filter over 1/3-semitone pitch bins plus an unvoiced state prefers smooth tracks, so that one bad frame cannot win and a real leap that
stays wins in two or three frames. The pitch is the posterior mean around the strongest bin. It was implemented behind the same interface (`docs/experiments/pyin_tracker_attempt.cpp.txt`).

**Method.** The two full 30 s takes in `tests/data`'s source (raw and Auto-Tuned) and the hard 9 s excerpt (`tests/data/vocal_raw_hard_9s.wav`).
Lock = share of sung frames (level above 0.01) with a pitch. Octave errors = frames more than an octave away (+-1.5 st) from an independent autocorrelation estimate on clearly pitched
frames (that estimator is crude, so only differences matter). Glitch excursions = a leap above 5 semitones that comes back within 100 ms. `tests/tracker_tests.cpp` passed for every variant.

| tracker | lock raw / tuned / hard | octave errors raw / tuned | glitch excursions raw / tuned |
|---|---|---|---|
| v0.6 original | 99.4 / 99.4 / 99.8 % | 13 / 10 | 5 / 8 |
| **v0.7 (kept): YIN + median + leap confirmation** | **99.4 / 99.4 / 99.8 %** | **13 / 9** | **3 / 5** |
| pYIN, strict prior (V2) | 97.9 / 97.8 / 91.8 % | 12 / 10 | 2 / 7 |
| pYIN, permissive prior (W2) | 99.5 / 99.9 / 99.8 % | 13 / 11 | 6 / 15 |

**Result.** A strict prior removes glitches but declares rough, loud singing unvoiced (lock falls to 92% on the hard excerpt). A permissive prior keeps the lock but lets the glitches back
in, more of them than the original. No setting I tried beats the v0.7 tracker on all three measures, so the simpler v0.7 tracker stays.
On vibrato the pYIN filter was better in the synthetic test (100% within 50 cents against 95.5%); that is the one place it won.

**What a real fix would need.** Better candidates, not a better filter: the candidates come from a 16 kHz YIN with a 32 ms window. A longer window for low voices, or candidates from a second
estimator (a spectral comb, or a small neural model), would give the filter evidence it can use. Tuning on one singer's recordings is also a risk: a real attempt needs several voices.
