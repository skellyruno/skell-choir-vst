#!/usr/bin/env python3
"""Fail-closed validation and provenance for the SkellyChoir real-editor PNG audit."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
STANDARD_SIZE = (765, 534)
EXPECTED = {
    "00a-curtain-closed.png": STANDARD_SIZE,
    "00b-curtain-parting.png": STANDARD_SIZE,
    "01-first-run-tour.png": STANDARD_SIZE,
    "02-easy-mode.png": STANDARD_SIZE,
    "03-pro-main.png": STANDARD_SIZE,
    "04-pro-main-solo-soprano.png": STANDARD_SIZE,
    "05-room.png": STANDARD_SIZE,
    "06-voice.png": STANDARD_SIZE,
    "07-follow.png": STANDARD_SIZE,
    "08-listen-empty.png": STANDARD_SIZE,
    "09-listen-take.png": STANDARD_SIZE,
    "10-take-file.png": STANDARD_SIZE,
    "11-stems.png": STANDARD_SIZE,
    "12-smallest-window.png": (730, 500),
    "13-large-window.png": (1300, 900),
    "14-theme-gold.png": STANDARD_SIZE,
    "14-theme-ice-blue.png": STANDARD_SIZE,
    "14-theme-neon-green.png": STANDARD_SIZE,
    "15-scene-0-default.png": STANDARD_SIZE,
    "15-scene-1-cathedral.png": STANDARD_SIZE,
    "15-scene-2-concert-hall.png": STANDARD_SIZE,
    "15-scene-3-studio-booth.png": STANDARD_SIZE,
    "15-scene-4-chapel.png": STANDARD_SIZE,
    "15-scene-5-gospel-stage.png": STANDARD_SIZE,
    "15-scene-6-stadium-anthem.png": STANDARD_SIZE,
    "15-scene-7-opera-house.png": STANDARD_SIZE,
    "15-scene-8-graveyard.png": STANDARD_SIZE,
    "15-scene-9-crypt.png": STANDARD_SIZE,
    "15-scene-10-cave.png": STANDARD_SIZE,
    "15-scene-11-bathroom.png": STANDARD_SIZE,
    "15-scene-12-parking-garage.png": STANDARD_SIZE,
    "15-scene-13-marble-dome.png": STANDARD_SIZE,
    "15-scene-14-stairwell.png": STANDARD_SIZE,
    "16-selected-preset.png": STANDARD_SIZE,
    "17-key-scale-selected.png": STANDARD_SIZE,
}
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


class ScreenshotError(ValueError):
    """A real-editor screenshot failed integrity or release-layout validation."""


def inspect_png(path: Path) -> dict:
    if path.is_symlink() or not path.is_file():
        raise ScreenshotError(f"Screenshot must be a regular file: {path.name}")
    raw = path.read_bytes()
    if len(raw) < 57 or not raw.startswith(PNG_SIGNATURE):
        raise ScreenshotError(f"Invalid or truncated PNG: {path.name}")

    offset = len(PNG_SIGNATURE)
    width = height = bit_depth = color_type = interlace = None
    idat = bytearray()
    saw_ihdr = saw_idat = saw_iend = False

    while offset < len(raw):
        if offset + 12 > len(raw):
            raise ScreenshotError(f"Truncated PNG chunk header: {path.name}")
        length = struct.unpack_from(">I", raw, offset)[0]
        kind = raw[offset + 4:offset + 8]
        end = offset + 12 + length
        if end > len(raw):
            raise ScreenshotError(f"Truncated PNG chunk {kind!r}: {path.name}")
        payload = raw[offset + 8:offset + 8 + length]
        expected_crc = struct.unpack_from(">I", raw, offset + 8 + length)[0]
        actual_crc = zlib.crc32(kind + payload) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ScreenshotError(f"PNG CRC mismatch in {kind.decode('ascii', 'replace')}: {path.name}")

        if not saw_ihdr:
            if kind != b"IHDR" or length != 13:
                raise ScreenshotError(f"PNG must begin with a 13-byte IHDR: {path.name}")
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if width <= 0 or height <= 0:
                raise ScreenshotError(f"PNG dimensions must be positive: {path.name}")
            if compression != 0 or filtering != 0:
                raise ScreenshotError(f"Unsupported PNG compression/filter method: {path.name}")
            if interlace != 0:
                raise ScreenshotError(f"Interlaced screenshot PNGs are not accepted: {path.name}")
            if color_type not in CHANNELS or bit_depth not in (1, 2, 4, 8, 16):
                raise ScreenshotError(f"Unsupported PNG color type/bit depth: {path.name}")
            if color_type in (2, 4, 6) and bit_depth < 8:
                raise ScreenshotError(f"Invalid PNG bit depth for color type: {path.name}")
            saw_ihdr = True
        elif kind == b"IHDR":
            raise ScreenshotError(f"Duplicate PNG IHDR: {path.name}")

        if kind == b"IDAT":
            if saw_iend:
                raise ScreenshotError(f"IDAT appears after IEND: {path.name}")
            saw_idat = True
            idat.extend(payload)
        elif kind == b"IEND":
            if length != 0 or not saw_idat:
                raise ScreenshotError(f"Invalid or premature PNG IEND: {path.name}")
            saw_iend = True
            offset = end
            if offset != len(raw):
                raise ScreenshotError(f"Unexpected bytes after PNG IEND: {path.name}")
            break
        offset = end

    if not (saw_ihdr and saw_idat and saw_iend):
        raise ScreenshotError(f"PNG is missing IHDR, IDAT or IEND: {path.name}")

    try:
        inflater = zlib.decompressobj()
        pixels = inflater.decompress(bytes(idat)) + inflater.flush()
        if not inflater.eof or inflater.unused_data or inflater.unconsumed_tail:
            raise ScreenshotError(f"PNG image data has an incomplete or trailing zlib stream: {path.name}")
    except zlib.error as exc:
        raise ScreenshotError(f"PNG image data cannot be decompressed: {path.name}: {exc}") from exc

    row_bytes = (width * CHANNELS[color_type] * bit_depth + 7) // 8
    expected_raw_size = height * (row_bytes + 1)
    if len(pixels) != expected_raw_size:
        raise ScreenshotError(
            f"PNG scanline size mismatch: {path.name} has {len(pixels)} bytes, "
            f"expected {expected_raw_size}"
        )
    for row in range(height):
        if pixels[row * (row_bytes + 1)] > 4:
            raise ScreenshotError(f"Invalid PNG scanline filter at row {row}: {path.name}")

    return {
        "width": width,
        "height": height,
        "size_bytes": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest(),
    }


def verify_directory(
    directory: Path,
    manifest_path: Path,
    *,
    source_commit: str,
    ci_commit: str,
    run_id: str,
    event: str,
    ref: str,
    expected: dict[str, tuple[int, int]] = EXPECTED,
) -> dict:
    directory = Path(directory)
    if not directory.is_dir():
        raise ScreenshotError(f"Screenshot directory does not exist: {directory}")

    actual_pngs = {p.name for p in directory.glob("*.png")}
    expected_pngs = set(expected)
    missing = sorted(expected_pngs - actual_pngs)
    extra = sorted(actual_pngs - expected_pngs)
    if missing or extra:
        raise ScreenshotError(f"PNG set mismatch; missing={missing}, extra={extra}")

    files = []
    hashes = set()
    for name, dimensions in expected.items():
        path = directory / name
        info = inspect_png(path)
        actual_dimensions = (info["width"], info["height"])
        if actual_dimensions != dimensions:
            raise ScreenshotError(
                f"{name} dimensions are {actual_dimensions}; expected {dimensions}"
            )
        if info["size_bytes"] < 1024:
            raise ScreenshotError(f"Screenshot is implausibly small: {name}")
        if info["sha256"] in hashes:
            raise ScreenshotError(f"Duplicate screenshot content detected: {name}")
        hashes.add(info["sha256"])
        files.append({"name": name, **info})

    manifest = {
        "schema_version": 1,
        "product": "SkellyChoir",
        "audit": "real-juce-editor",
        "expected_png_count": len(expected),
        "source_commit": source_commit,
        "ci_checkout_commit": ci_commit,
        "workflow_run_id": str(run_id),
        "event": event,
        "ref": ref,
        "screenshots": files,
    }
    if not all((source_commit, ci_commit, run_id, event, ref)):
        raise ScreenshotError("Source commit, CI commit, run ID, event and ref are required for provenance")

    manifest_path = Path(manifest_path)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = manifest_path.with_suffix(manifest_path.suffix + ".tmp")
    temp_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temp_path.replace(manifest_path)
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--ci-commit", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--event", required=True)
    parser.add_argument("--ref", required=True)
    args = parser.parse_args(argv)
    try:
        result = verify_directory(
            args.directory, args.manifest,
            source_commit=args.source_commit,
            ci_commit=args.ci_commit,
            run_id=args.run_id,
            event=args.event,
            ref=args.ref,
        )
    except (OSError, ScreenshotError) as exc:
        print(f"::error::{exc}", file=sys.stderr)
        return 1
    print(
        f"Verified {len(result['screenshots'])} real SkellyChoir editor PNGs; "
        f"manifest: {args.manifest}"
    )
    for item in result["screenshots"]:
        print(f"{item['name']} {item['width']}x{item['height']} {item['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
