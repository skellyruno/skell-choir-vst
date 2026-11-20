#!/usr/bin/env python3
"""Fail-closed verification for the four real JUCE Quick4 captures."""
from __future__ import annotations

import argparse
import binascii
import hashlib
import json
from pathlib import Path
import struct
import zlib

EXPECTED = {
    "00-main.png",
    "01-room-position-popup.png",
    "02-active-solo-mute.png",
    "03-neon-green.png",
}


def inspect_png(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"{path.name}: bad PNG signature")
    pos = 8
    width = height = None
    idat = bytearray()
    saw_end = False
    while pos + 12 <= len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        end = pos + 12 + length
        if end > len(data):
            raise ValueError(f"{path.name}: truncated {kind!r} chunk")
        payload = data[pos + 8:pos + 8 + length]
        expected_crc = struct.unpack(">I", data[pos + 8 + length:end])[0]
        if (binascii.crc32(kind + payload) & 0xFFFFFFFF) != expected_crc:
            raise ValueError(f"{path.name}: CRC mismatch in {kind!r}")
        if kind == b"IHDR":
            if len(payload) != 13:
                raise ValueError(f"{path.name}: invalid IHDR")
            width, height = struct.unpack(">II", payload[:8])
        elif kind == b"IDAT":
            idat.extend(payload)
        elif kind == b"IEND":
            saw_end = True
            pos = end
            break
        pos = end
    if not saw_end or pos != len(data) or not idat or not width or not height:
        raise ValueError(f"{path.name}: incomplete PNG container")
    try:
        decoded = zlib.decompress(idat)
    except zlib.error as exc:
        raise ValueError(f"{path.name}: IDAT decompression failed: {exc}") from exc
    if len(decoded) == 0:
        raise ValueError(f"{path.name}: empty decoded image")
    return width, height


def verify(directory: Path, manifest_path: Path) -> dict:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    scenes = manifest.get("scenes")
    if manifest.get("passed") is not True or manifest.get("count") != 4:
        raise ValueError("manifest must declare passed=true and exactly four scenes")
    if not isinstance(scenes, list) or len(scenes) != 4:
        raise ValueError("manifest scene list must contain exactly four entries")
    names = [scene.get("file") for scene in scenes]
    if set(names) != EXPECTED or len(names) != len(set(names)):
        raise ValueError("manifest filenames do not match the exact four-scene set")
    actual = {path.name for path in directory.glob("*.png")}
    if actual != EXPECTED | {"contact-sheet.png"}:
        raise ValueError(f"PNG set mismatch: {sorted(actual ^ (EXPECTED | {'contact-sheet.png'}))}")
    hashes = set()
    for scene in scenes:
        path = directory / scene["file"]
        if not path.is_file() or path.stat().st_size <= 1024:
            raise ValueError(f"missing or empty capture: {path.name}")
        width, height = inspect_png(path)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if width != scene.get("width") or height != scene.get("height"):
            raise ValueError(f"{path.name}: manifest dimensions do not match")
        if path.stat().st_size != scene.get("bytes") or digest != scene.get("sha256"):
            raise ValueError(f"{path.name}: manifest size/hash mismatch")
        if digest in hashes:
            raise ValueError(f"duplicate image contents: {path.name}")
        hashes.add(digest)
    contact = directory / "contact-sheet.png"
    if not contact.is_file() or contact.stat().st_size <= 1024:
        raise ValueError("missing or empty contact sheet")
    inspect_png(contact)
    popup = next(scene for scene in scenes if scene["file"] == "01-room-position-popup.png")
    if popup["width"] <= scenes[0]["width"]:
        raise ValueError("native popup capture should include the adjacent popup beyond the editor width")
    return {"passed": True, "scenes": len(scenes), "unique_sha256": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = verify(args.directory, args.manifest)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        parser.exit(1, f"FAIL: {exc}\n")
    print(f"PASS: verified {result['scenes']} unique Quick4 JUCE PNGs and contact sheet")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
