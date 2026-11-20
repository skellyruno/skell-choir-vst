"""Regression tests for the SkellyChoir real-editor screenshot integrity gate."""
import importlib.util
import pathlib
import struct
import tempfile
import unittest
import zlib

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "scripts" / "verify_ui_screenshots.py"
spec = importlib.util.spec_from_file_location("verify_ui_screenshots", SCRIPT)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def chunk(kind, payload):
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def png(width=40, height=40):
    rows = []
    for y in range(height):
        pixels = bytearray()
        for x in range(width):
            pixels.extend((x % 256, y % 256, (x * 7 + y * 11) % 256))
        rows.append(b"\x00" + bytes(pixels))
    raw = b"".join(rows)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (
        gate.PNG_SIGNATURE
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


class ScreenshotGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)

    def test_valid_png_and_provenance_manifest(self):
        directory = self.root / "captures"
        directory.mkdir()
        data = png()
        (directory / "main.png").write_bytes(data)
        manifest_path = self.root / "manifest.json"
        result = gate.verify_directory(
            directory, manifest_path,
            source_commit="source-sha", ci_commit="ci-sha",
            run_id="1234", event="pull_request", ref="refs/pull/1/merge",
            expected={"main.png": (40, 40)},
        )
        self.assertEqual(result["expected_png_count"], 1)
        self.assertEqual(result["screenshots"][0]["sha256"], gate.hashlib.sha256(data).hexdigest())
        self.assertTrue(manifest_path.is_file())

    def test_rejects_bad_chunk_crc(self):
        data = bytearray(png())
        data[-5] ^= 0x01
        path = self.root / "bad-crc.png"
        path.write_bytes(data)
        with self.assertRaises(gate.ScreenshotError):
            gate.inspect_png(path)

    def test_rejects_wrong_dimensions(self):
        directory = self.root / "captures"
        directory.mkdir()
        (directory / "main.png").write_bytes(png(41, 40))
        with self.assertRaises(gate.ScreenshotError):
            gate.verify_directory(
                directory, self.root / "manifest.json",
                source_commit="source", ci_commit="ci", run_id="1",
                event="pull_request", ref="refs/pull/1/merge",
                expected={"main.png": (40, 40)},
            )

    def test_rejects_missing_expected_capture(self):
        directory = self.root / "captures"
        directory.mkdir()
        with self.assertRaises(gate.ScreenshotError):
            gate.verify_directory(
                directory, self.root / "manifest.json",
                source_commit="source", ci_commit="ci", run_id="1",
                event="pull_request", ref="refs/pull/1/merge",
                expected={"main.png": (40, 40)},
            )

    def test_rejects_duplicate_screenshot_content(self):
        directory = self.root / "captures"
        directory.mkdir()
        data = png()
        (directory / "main.png").write_bytes(data)
        (directory / "alternate.png").write_bytes(data)
        with self.assertRaises(gate.ScreenshotError):
            gate.verify_directory(
                directory, self.root / "manifest.json",
                source_commit="source", ci_commit="ci", run_id="1",
                event="pull_request", ref="refs/pull/1/merge",
                expected={"main.png": (40, 40), "alternate.png": (40, 40)},
            )

    def test_rejects_missing_provenance(self):
        directory = self.root / "captures"
        directory.mkdir()
        (directory / "main.png").write_bytes(png())
        with self.assertRaises(gate.ScreenshotError):
            gate.verify_directory(
                directory, self.root / "manifest.json",
                source_commit="", ci_commit="ci", run_id="1",
                event="pull_request", ref="refs/pull/1/merge",
                expected={"main.png": (40, 40)},
            )


if __name__ == "__main__":
    unittest.main()
