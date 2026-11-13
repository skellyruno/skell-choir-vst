"""Offline integration smoke tests; zero third-party modules needed."""
from array import array
import importlib.util
import json
import math
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


midi = load("satb_to_midi", "satb_to_midi.py")
spatial = load("mix_external_voices", "mix_external_voices.py")
skull = load("render_skull_preview", "render_skull_preview.py")


class InteropTests(unittest.TestCase):
    def test_midi_satb_and_lead(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            score = folder / "score.csv"
            score.write_text("start,end,melody,soprano,alto,tenor,bass,confidence\n"
                             "0,0.5,60,67,64,55,48,0.9\n"
                             "0.7,1.1,62,69,65,57,50,0.8\n", encoding="utf-8")
            output = folder / "score.mid"
            self.assertEqual(2, midi.convert(score, output, include_lead=True))
            payload = output.read_bytes()
            self.assertEqual(b"MThd", payload[:4])
            self.assertEqual((1, 6, 480), struct.unpack(">HHH", payload[8:14]))
            self.assertEqual(6, payload.count(b"MTrk"))
            self.assertIn(b"Soprano", payload)
            self.assertIn(b"Lead", payload)
            # Each track contains 2 note-ons, 2 note-offs; all MIDI bytes valid.
            self.assertIn(bytes([0x90, 67, 88]), payload)
            self.assertIn(bytes([0x84, 60, 0]), payload)
            self.assertIn(bytes([0xFF, 0x51, 0x03]), payload)

    def test_external_voices_render(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            n = 1600
            for label, hz in (("soprano", 440), ("bass", 220)):
                with wave.open(str(folder / f"{label}.wav"), "wb") as w:
                    w.setnchannels(1)
                    w.setsampwidth(2)
                    w.setframerate(16000)
                    sig = array("h", (round(5000 * math.sin(2 * math.pi * hz * i / 16000))
                                      for i in range(n)))
                    if sys.byteorder != "little":
                        sig.byteswap()
                    w.writeframes(sig.tobytes())
            manifest = folder / "voices.json"
            manifest.write_text(json.dumps({"room": 0.1, "microphone": [0, 0],
                "voices": [{"file": "soprano.wav", "section": "soprano", "x": -5},
                           {"file": "bass.wav", "section": "bass", "x": 5}]}))
            output = folder / "choir.wav"
            count, duration, peak, gain = spatial.render(manifest, output)
            self.assertEqual(count, 2)
            self.assertGreater(duration, 0.1)
            self.assertTrue(0 < peak < 1)
            self.assertEqual(gain, 1)
            with wave.open(str(output), "rb") as w:
                self.assertEqual(w.getnchannels(), 2)
                self.assertEqual(w.getframerate(), 16000)
                self.assertGreater(w.getnframes(), n)
                values = array("h")
                values.frombytes(w.readframes(w.getnframes()))
                if sys.byteorder != "little":
                    values.byteswap()
                self.assertGreater(max(abs(v) for v in values), 50)
                # Different spatial gains produce nonidentical stereo signal.
                self.assertTrue(any(values[i] != values[i + 1]
                                    for i in range(0, len(values) - 1, 2)))

    def test_missing_models_or_stems_errors(self):
        with tempfile.TemporaryDirectory() as temp:
            manifest = Path(temp) / "manifest.json"
            manifest.write_text(json.dumps({"voices": [
                {"file": "nonexistent.wav", "section": "soprano"}]}))
            with self.assertRaises(FileNotFoundError):
                spatial.render(manifest, Path(temp) / "out.wav")


class SkullPreviewTests(unittest.TestCase):
    def test_header_parses_to_the_same_shapes_the_plugin_draws(self):
        text = (ROOT / "Source" / "UI" / "SkullShape.h").read_text(encoding="utf-8")
        listed = sum(1 for line in text.splitlines() if line.lstrip().startswith("{ Kind::"))   # not the examples in the comment
        shapes = skull.parse_shapes()
        self.assertEqual(listed, len(shapes), "a shape line in SkullShape.h is not in the format the preview script reads")
        self.assertEqual({"Body", "Cut", "Glint"}, {s["role"] for s in shapes})

    def test_every_theme_is_found_with_its_real_colours(self):
        themes = dict(skull.parse_themes())
        self.assertEqual(["Gold", "Blood Red", "Neon Green", "Ice Blue"], list(themes))
        self.assertEqual(0xffe9bb68, themes["Gold"]["gold"])        # the shipped accent
        self.assertEqual(0xff2f1b15, themes["Gold"]["headerTop"])   # the header glow, so the index into the palette is right
        self.assertEqual(0xff08060a, themes["Gold"]["bg"])

    def test_render_draws_a_skull(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest("Pillow is not installed")
        colours = dict(skull.parse_themes())["Gold"]
        img = skull.draw_skull(skull.parse_shapes(), 64, colours, supersample=2)
        self.assertEqual((64, 64), img.size)
        pixels = list(img.convert("RGB").getdata())
        bright = sum(1 for r, g, b in pixels if r > 150 and g > 110)     # the golden body
        self.assertTrue(0.25 * 64 * 64 < bright < 0.75 * 64 * 64, bright)
        corner = img.convert("RGB").getpixel((0, 63))
        self.assertTrue(max(corner) < 60, corner)                        # the dark background, not gold


if __name__ == "__main__":
    unittest.main()
