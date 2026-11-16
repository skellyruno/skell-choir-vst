"""Offline release-gate regression tests; no Windows runner required."""
import importlib.util
import pathlib
import struct
import tempfile
import unittest
import zipfile

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "scripts" / "verify_windows_vst3.py"
spec = importlib.util.spec_from_file_location("verify_windows_vst3", SCRIPT)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

def pe():
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3c, 128)
    data[128:132] = b"PE\\0\\0"
    struct.pack_into("<HH", data, 132, 0x8664, 1)
    struct.pack_into("<H", data, 152, 0x20b)
    return bytes(data)

class ReleaseGateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name)
        self.bundle = self.root / gate.NAME
        self.module = self.bundle / gate.MODULE
        self.module.parent.mkdir(parents=True)
        self.module.write_bytes(pe())

    def archive(self, extra=None):
        dest = self.root / "test.zip"
        with zipfile.ZipFile(dest, "w") as z:
            z.write(self.module, gate.NAME + "/" + gate.MODULE)
            if extra:
                for name, data in extra:
                    z.writestr(name, data)
        return dest

    def test_valid_bundle_and_archive(self):
        result = gate.inspect(self.bundle, self.archive())
        self.assertEqual(result["module_sha256"], gate.sha256(pe()))
        self.assertIsNotNone(result["zip_sha256"])

    def test_rejects_wrong_architecture(self):
        data = bytearray(pe())
        struct.pack_into("<H", data, 132, 0x14c)
        self.module.write_bytes(data)
        with self.assertRaises(ValueError):
            gate.inspect(self.bundle)

    def test_rejects_extra_module(self):
        (self.bundle / "extra.vst3").write_bytes(pe())
        with self.assertRaises(ValueError):
            gate.inspect(self.bundle)

    def test_rejects_zip_traversal(self):
        with self.assertRaises(ValueError):
            gate.inspect(self.bundle, self.archive([("../evil", b"x")]))

    def test_rejects_duplicate_zip_path(self):
        with self.assertRaises(ValueError):
            gate.inspect(self.bundle, self.archive([(gate.NAME + "/" + gate.MODULE, pe())]))

    def test_rejects_zip_content_mismatch(self):
        with self.assertRaises(ValueError):
            gate.inspect(self.bundle, self.archive([(gate.NAME + "/extra.txt", b"wrong")]))

if __name__ == "__main__":
    unittest.main()
