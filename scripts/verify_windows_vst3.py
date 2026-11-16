#!/usr/bin/env python3
"""Fail-closed Windows x64 SkellyChoir VST3 release artifact verifier.

Usage: python scripts/verify_windows_vst3.py path/to/SkellyChoir.vst3 [--zip path.zip]
Requires Python 3.9+; does not modify the build or ZIP.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import sys
import zipfile

NAME = "SkellyChoir.vst3"
MODULE = "Contents/x86_64-win/SkellyChoir.vst3"

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def check_pe(data):
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("module lacks DOS MZ signature")
    offset = struct.unpack_from("<I", data, 0x3c)[0]
    if offset > len(data) - 26 or data[offset:offset + 4] != b"PE\\0\\0":
        raise ValueError("invalid PE header offset/signature")
    machine, sections = struct.unpack_from("<HH", data, offset + 4)
    magic = struct.unpack_from("<H", data, offset + 24)[0]
    if machine != 0x8664 or magic != 0x20b or sections == 0:
        raise ValueError("expected AMD64 PE32+ binary with sections")

def inspect(bundle, archive=None):
    bundle = pathlib.Path(bundle)
    if bundle.name != NAME or not bundle.is_dir():
        raise ValueError("expected directory named " + NAME)
    modules = [p for p in bundle.rglob("*") if p.is_file() and p.suffix.lower() == ".vst3"]
    expected = bundle / MODULE
    if len(modules) != 1 or modules[0] != expected:
        raise ValueError("exactly one module required at " + MODULE)
    data = expected.read_bytes()
    check_pe(data)
    files = {}
    for path in bundle.rglob("*"):
        if path.is_symlink():
            raise ValueError("symlink not permitted in release: " + str(path))
        if path.is_file():
            relative = path.relative_to(bundle).as_posix()
            files[relative] = sha256(path.read_bytes())
    if archive is not None:
        archive = pathlib.Path(archive)
        if not zipfile.is_zipfile(archive):
            raise ValueError("invalid ZIP")
        with zipfile.ZipFile(archive) as z:
            if z.testzip() is not None:
                raise ValueError("ZIP CRC failure")
            seen = set()
            archived = {}
            for entry in z.infolist():
                name = entry.filename
                parts = pathlib.PurePosixPath(name).parts
                if (name.startswith("/") or "\\" in name or ".." in parts
                        or not parts or parts[0] != NAME):
                    raise ValueError("unsafe/unexpected ZIP path: " + name)
                folded = name.casefold()
                if folded in seen:
                    raise ValueError("duplicate ZIP path: " + name)
                seen.add(folded)
                if not entry.is_dir():
                    archived[pathlib.PurePosixPath(name).relative_to(NAME).as_posix()] = sha256(z.read(entry))
            if archived != files:
                raise ValueError("ZIP content differs from verified bundle")
    return {"product": "SkellyChoir", "module": MODULE,
            "module_sha256": sha256(data), "files": files,
            "zip_sha256": sha256(archive.read_bytes()) if archive else None}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle")
    parser.add_argument("--zip", dest="archive")
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.bundle, args.archive), indent=2, sort_keys=True))
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print("RELEASE GATE FAILED: " + str(error), file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
