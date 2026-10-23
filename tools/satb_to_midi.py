#!/usr/bin/env python3
"""Convert an OmniChoir Full Scan editable CSV into standard MIDI SATB tracks.

No third-party Python packages required. The resulting MIDI is an arrangement
for editing/recording in a DAW, not synthesized human vocals.
"""
import argparse
import csv
from pathlib import Path
import struct

PARTS = ("Soprano", "Alto", "Tenor", "Bass")


def vlq(number: int) -> bytes:
    if number < 0 or number > 0x0FFFFFFF:
        raise ValueError("MIDI time delta out of range")
    buf = [number & 0x7F]
    number >>= 7
    while number:
        buf.insert(0, 0x80 | (number & 0x7F))
        number >>= 7
    return bytes(buf)


def chunk(tag: bytes, data: bytes) -> bytes:
    return tag + struct.pack(">I", len(data)) + data


def name_event(name: str) -> bytes:
    label = name.encode("ascii", "replace")
    return b"\xff\x03" + vlq(len(label)) + label


def track(part: str, channel: int, notes: list[tuple[int, int, int]], tempo: int,
          tempo_track: bool = False) -> bytes:
    msgs = [(0, -4, name_event(part))]
    if tempo_track:
        msgs.append((0, -3, b"\xff\x51\x03" + tempo.to_bytes(3, "big")))
    else:
        msgs.append((0, -2, bytes([0xC0 | channel, 52])))  # General MIDI Choir Aahs
        for start, end, pitch in notes:
            msgs.append((start, 1, bytes([0x90 | channel, pitch, 88])))
            msgs.append((end, 0, bytes([0x80 | channel, pitch, 0])))
    msgs.sort(key=lambda event: (event[0], event[1]))  # end notes before starts
    data = bytearray()
    previous = 0
    for tick, _, msg in msgs:
        data.extend(vlq(tick - previous))
        data.extend(msg)
        previous = tick
    data.extend(b"\x00\xff\x2f\x00")
    return chunk(b"MTrk", bytes(data))


def convert(csv_path: Path, midi_path: Path, tempo_bpm: float = 120,
            include_lead: bool = False) -> int:
    if not (20 <= tempo_bpm <= 300):
        raise ValueError("Tempo must be between 20 and 300 BPM")
    # 480 MIDI ticks per beat. Absolute seconds are mapped to ticks at the
    # selected constant tempo, preserving the source recording's chronology.
    ticks_per_second = 480 * tempo_bpm / 60.0
    notes = [[] for _ in range(5 if include_lead else 4)]
    with csv_path.open(newline="", encoding="utf-8-sig") as src:
        reader = csv.DictReader(src)
        for field in ("start", "end", "melody", "soprano", "alto", "tenor", "bass"):
            if field not in (reader.fieldnames or []):
                raise ValueError(f"Missing CSV column: {field}")
        for number, row in enumerate(reader, 2):
            start = float(row["start"])
            end = float(row["end"])
            if not (0 <= start < end):
                raise ValueError(f"Bad time range on CSV line {number}")
            begin = round(start * ticks_per_second)
            finish = max(begin + 1, round(end * ticks_per_second))
            for index, part in enumerate(("soprano", "alto", "tenor", "bass") +
                                         (("melody",) if include_lead else ())):
                pitch = int(row[part])
                if not 0 <= pitch <= 127:
                    raise ValueError(f"Invalid MIDI note {pitch} on line {number}")
                notes[index].append((begin, finish, pitch))
    micros_per_beat = round(60_000_000 / tempo_bpm)
    labels = PARTS + (("Lead",) if include_lead else ())
    tracks = [track("OmniChoir Score", 0, [], micros_per_beat, True)]
    for index, label in enumerate(labels):
        tracks.append(track(label, index, notes[index], micros_per_beat))
    head = struct.pack(">HHH", 1, len(tracks), 480)
    midi_path.parent.mkdir(parents=True, exist_ok=True)
    midi_path.write_bytes(chunk(b"MThd", head) + b"".join(tracks))
    return len(notes[0])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, help="Full Scan score CSV")
    parser.add_argument("midi", type=Path, help="destination .mid")
    parser.add_argument("--bpm", type=float, default=120)
    parser.add_argument("--include-lead", action="store_true")
    args = parser.parse_args()
    count = convert(args.csv, args.midi, args.bpm, args.include_lead)
    print(f"Wrote {args.midi}: {count} notes in each SATB part"
          + (" plus lead" if args.include_lead else ""))


if __name__ == "__main__":
    main()
