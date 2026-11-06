#!/usr/bin/env python3
"""Generates the three built-in room impulse responses in Assets/Rooms/ (44.1 kHz, 16-bit, stereo).

They are SYNTHETIC, not recordings of real rooms: sparse early reflections followed by a diffuse tail whose decay time differs
between the low, mid and high bands (long lows, shorter highs, as in stone rooms), decorrelated between the two channels.
Deterministic (fixed seeds), so re-running reproduces the same files.  Needs numpy and scipy:   python3 tools/make_room_irs.py
"""
import os
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "Rooms")

ROOMS = {
    # name: (length s, RT60 low/mid/high in s, pre-delay ms, early reflections (count, first ms, last ms), seed)
    "room_cathedral":  (6.4, (8.0, 6.2, 3.4), 38.0, (16, 45.0, 230.0), 11),
    "room_chapel":     (2.8, (2.7, 2.1, 1.0), 12.0, (12, 14.0, 95.0), 22),
    "room_small_hall": (2.0, (1.7, 1.35, .8), 8.0, (10, 8.0, 62.0), 33),
}

def band_noise(rng, n, lo=None, hi=None):
    x = rng.standard_normal(n)
    if lo is None:  return sosfilt(butter(4, hi, "low", fs=SR, output="sos"), x)
    if hi is None:  return sosfilt(butter(4, lo, "high", fs=SR, output="sos"), x)
    return sosfilt(butter(4, [lo, hi], "band", fs=SR, output="sos"), x)

def channel(rng, length, rts, pre_ms, er):
    n = int(length * SR); t = np.arange(n) / SR
    # the diffuse tail: three bands, each with its own decay, building up over ~25 ms after the pre-delay
    bands = [band_noise(rng, n, hi=250.0), band_noise(rng, n, lo=250.0, hi=2500.0), band_noise(rng, n, lo=2500.0)]
    gains = [1.0, 1.0, .8]
    tail = np.zeros(n)
    for b, rt, g in zip(bands, rts, gains):
        tail += g * b * np.exp(-6.9078 * t / rt)
    onset = np.clip((t - pre_ms / 1000.0) / .025, 0.0, 1.0) ** 2
    tail *= onset * .35
    # the early reflections: a few distinct, softly lowpassed taps that thin out and fall in level
    count, first, last = er
    early = np.zeros(n)
    times = np.sort(rng.uniform(first, last, count)) / 1000.0
    for k, tm in enumerate(times):
        i = int(tm * SR)
        if i < n: early[i] += rng.choice([-1.0, 1.0]) * 1.0 / (1.0 + .55 * k)
    early = sosfilt(butter(2, 5200.0, "low", fs=SR, output="sos"), early)
    ir = tail + early * .9
    # a short fade at the end so the file never ends on a click
    fade = int(.06 * SR); ir[-fade:] *= np.linspace(1.0, 0.0, fade)
    return ir

def main():
    os.makedirs(OUT, exist_ok=True)
    for name, (length, rts, pre, er, seed) in ROOMS.items():
        rng = np.random.default_rng(seed)
        left, right = channel(rng, length, rts, pre, er), channel(rng, length, rts, pre, er)
        data = np.stack([left, right], axis=1)
        data *= .8 / np.abs(data).max()
        path = os.path.join(OUT, name + ".wav")
        wavfile.write(path, SR, np.round(data * 32767).astype(np.int16))
        print(f"{name}: {length:.1f} s, RT60 {rts} s, {os.path.getsize(path)/1024:.0f} KB")

if __name__ == "__main__":
    main()
