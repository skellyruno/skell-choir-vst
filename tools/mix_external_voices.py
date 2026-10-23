#!/usr/bin/env python3
"""Spatially mix *pre-rendered* individual voice stems for OmniChoir.

This is a bridge for externally generated AI or human vocal stems. It does NOT
run model inference, change the identity of a singer, or create SATB harmony.
It spatializes separate recordings, e.g. renders from ai-choir (after WAV
conversion), vocal takes, or independently recorded SATB voices.

No third-party dependencies: standard-library Python and PCM16 WAV files.
The offline output is stereo PCM16 WAV; it is not a real-time plugin module.
"""
import argparse
from array import array
import json
import math
from pathlib import Path
import sys
import wave

PARTS = {"soprano": (-5.1, 6.0), "alto": (-1.7, 5.2),
         "tenor": (1.7, 5.2), "bass": (5.1, 6.0), "lead": (0.0, 0.4)}


def read_wav(path: Path):
    with wave.open(str(path), "rb") as w:
        if w.getsampwidth() != 2 or w.getnchannels() not in (1, 2):
            raise ValueError(f"{path}: must be mono/stereo 16-bit PCM WAV")
        sample_rate = w.getframerate()
        nchan = w.getnchannels()
        pcm = array("h")
        pcm.frombytes(w.readframes(w.getnframes()))
    if sys.byteorder != "little":
        pcm.byteswap()
    if nchan == 1:
        mono = array("f", (v / 32768.0 for v in pcm))
    else:
        mono = array("f", ((pcm[i] + pcm[i+1]) / 65536.0
                           for i in range(0, len(pcm), 2)))
    return sample_rate, mono


def write_wav(path: Path, sample_rate: int, left: array, right: array):
    path.parent.mkdir(parents=True, exist_ok=True)
    pcm = array("h")
    peak = max(max((abs(v) for v in left), default=0.0),
               max((abs(v) for v in right), default=0.0))
    # Attenuate when summed voices exceed full scale; never silently clip.
    gain = 0.975 / peak if peak > 0.975 else 1.0
    for l, r in zip(left, right):
        pcm.extend((round(max(-1.0, min(1.0, l * gain)) * 32767),
                    round(max(-1.0, min(1.0, r * gain)) * 32767)))
    if sys.byteorder != "little":
        pcm.byteswap()
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(pcm.tobytes())
    return peak, gain


def render(manifest_path: Path, destination: Path):
    spec = json.loads(manifest_path.read_text(encoding="utf-8"))
    voices = spec.get("voices")
    if not isinstance(voices, list) or not voices:
        raise ValueError("Manifest requires a nonempty 'voices' list")
    mic = spec.get("microphone", [0.0, 0.0])
    if not isinstance(mic, list) or len(mic) != 2:
        raise ValueError("microphone must be [x, y] in metres")
    mic_x, mic_y = float(mic[0]), float(mic[1])
    room = max(0.0, min(0.8, float(spec.get("room", 0.20))))
    output_gain = 10.0 ** (float(spec.get("output_db", -3.0)) / 20.0)
    loaded = []
    rate = None
    for index, cfg in enumerate(voices):
        part = str(cfg.get("section", "")).lower()
        if part not in PARTS:
            raise ValueError(f"Voice {index}: section must be soprano/alto/tenor/bass/lead")
        source = manifest_path.parent / str(cfg["file"])
        if not source.is_file():
            raise FileNotFoundError(f"Missing stem: {source}")
        sr, mono = read_wav(source)
        if rate is None:
            rate = sr
        if sr != rate:
            raise ValueError(f"Sample-rate mismatch: {source} is {sr}, expected {rate}; resample first")
        if not 16000 <= sr <= 192000:
            raise ValueError("Expected sample rate between 16 and 192 kHz")
        x, y = PARTS[part]
        x, y = float(cfg.get("x", x)), float(cfg.get("y", y))
        delay_ms = max(0.0, float(cfg.get("offset_ms", 0)))
        voice_gain = 10.0 ** (float(cfg.get("gain_db", -6.0)) / 20.0)
        loaded.append((mono, part, x, y, delay_ms, voice_gain))
    # Preserve each voice's start time, add propagation from stage to mic,
    # and allow a small room tail. The farthest voice determines output length.
    delays = []
    for mono, part, x, y, delay_ms, _ in loaded:
        distance = math.hypot(x-mic_x, y-mic_y)
        delay = round(rate * (delay_ms / 1000 + distance / 343.0))
        delays.append(delay)
    tail = int(rate * (0.24 if room else 0))
    frames = max(len(row[0]) + delay for row, delay in zip(loaded, delays)) + tail
    left = array("f", [0.0]) * frames
    right = array("f", [0.0]) * frames
    for (mono, part, x, y, _, source_gain), delay in zip(loaded, delays):
        dx = x - mic_x
        distance = math.hypot(dx, y-mic_y)
        angle = math.atan2(dx, max(2.0, abs(y-mic_y)))
        position = max(-0.97, min(0.97, 1.4 * angle / (math.pi/2)))
        phi = (position + 1) * math.pi / 4
        attenuation = 3.0 / (3.0 + distance)
        gain = source_gain * output_gain * attenuation
        pan_l, pan_r = math.cos(phi) * gain, math.sin(phi) * gain
        if part == "lead":
            pan_l, pan_r = (gain / math.sqrt(2), gain / math.sqrt(2))
        # Lightweight stereo floor/wall image-source echoes. These don't
        # replace OmniChoir's full stage renderer or a convolution reverb.
        echo_1 = round(rate * 0.075)
        echo_2 = round(rate * 0.163)
        for i, value in enumerate(mono):
            pos = delay + i
            left[pos] += pan_l * value
            right[pos] += pan_r * value
            if room:
                left[pos + echo_1] += room * 0.20 * pan_r * value
                right[pos + echo_1] += room * 0.20 * pan_l * value
                left[pos + echo_2] += room * 0.12 * pan_l * value
                right[pos + echo_2] += room * 0.14 * pan_r * value
    peak, gain = write_wav(destination, rate, left, right)
    return len(loaded), frames / rate, peak, gain


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    n, seconds, peak, adjusted = render(args.manifest, args.output)
    print(f"Spatialized {n} externally rendered stems to {args.output}"
          f" ({seconds:.2f}s); premaster peak={peak:.3f}, safety gain={adjusted:.3f}")


if __name__ == "__main__":
    main()
