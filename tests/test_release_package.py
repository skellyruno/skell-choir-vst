"""Offline regression tests for the SkellyChoir Windows VST3 release gate."""
import importlib.util
import json
import pathlib
import struct
import tempfile
import unittest
import zipfile

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "scripts" / "verify_windows_vst3.py"
spec = importlib.util.spec_from_file_location("verify_windows_vst3", SCRIPT)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

COMPONENT_CID = "ABCDEF019182FAEB536B41754F6D4632"
CONTROLLER_CID = "ABCDEF011234ABCD536B41754F6D4632"


def pe(machine=0x8664, magic=0x20B, signature=b"PE\0\0", offset=128, sections=1):
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, offset)
    if offset + 26 <= len(data):
        data[offset:offset + 4] = signature
        struct.pack_into("<HH", data, offset + 4, machine, sections)
        struct.pack_into("<H", data, offset + 24, magic)
    return bytes(data)


def moduleinfo():
    # Mirrors JUCE's trailing-comma output. The comma-like text must remain intact.
    return '''{
  "Name": "SkellyChoir",
  "Version": "0.9.0",
  "Factory Info": {
    "Vendor": "Skell Audio",
    "URL": "",
    "E-Mail": "",
    "Description": "comma ,} inside a string",
  },
  "Classes": [
    {
      "CID": "ABCDEF019182FAEB536B41754F6D4632",
      "Category": "Audio Module Class",
      "Name": "SkellyChoir",
      "Vendor": "Skell Audio",
      "Version": "0.9.0",
    },
    {
      "CID": "ABCDEF011234ABCD536B41754F6D4632",
      "Category": "Component Controller Class",
      "Name": "SkellyChoir",
      "Vendor": "Skell Audio",
      "Version": "0.9.0",
    },
  ],
}
'''


class ReleaseGateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name)
        self.bundle = self.root / "SkellyChoir.vst3"
        self.module = self.bundle / gate.MODULE_RELATIVE
        self.info = self.bundle / gate.MODULEINFO_RELATIVE
        self.module.parent.mkdir(parents=True)
        self.info.parent.mkdir(parents=True)
        self.module.write_bytes(pe())
        self.info.write_text(moduleinfo(), encoding="utf-8")

    def make_archive(self, entries=None):
        archive = self.root / "product.zip"
        with zipfile.ZipFile(archive, "w") as zipped:
            for path in self.bundle.rglob("*"):
                if path.is_file():
                    zipped.write(path, self.bundle.name + "/" + path.relative_to(self.bundle).as_posix())
            for name, data in entries or []:
                zipped.writestr(name, data)
        return archive

    def test_valid_bundle_normalizes_trailing_commas_and_checks_real_pe(self):
        result = gate.verify_bundle(self.bundle)
        self.assertTrue(result["moduleinfo_normalized"])
        self.assertEqual(result["version"], "0.9.0")
        self.assertEqual(result["binary_sha256"], gate.digest(pe()))
        parsed = json.loads(self.info.read_text(encoding="utf-8"))
        self.assertEqual(parsed["Factory Info"]["Description"], "comma ,} inside a string")
        self.assertEqual(len(result["class_ids"]), 2)

    def test_rejects_fake_backslash_zero_pe_signature(self):
        self.module.write_bytes(pe(signature=b"PE\\0\\0"))
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_wrong_architecture(self):
        self.module.write_bytes(pe(machine=0x14C))
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_pe32_not_pe32_plus(self):
        self.module.write_bytes(pe(magic=0x10B))
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_invalid_pe_offset(self):
        self.module.write_bytes(pe(offset=16))
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_zero_pe_sections(self):
        self.module.write_bytes(pe(sections=0))
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_duplicate_metadata_keys(self):
        self.info.write_text('{"Name":"SkellyChoir","Name":"Other"}', encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_duplicate_class_ids(self):
        text = moduleinfo().replace(CONTROLLER_CID, COMPONENT_CID)
        self.info.write_text(text, encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_extra_vst3_module(self):
        (self.bundle / "extra.vst3").write_bytes(pe())
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_rejects_standalone_or_nested_zip(self):
        (self.bundle / "unexpected.exe").write_bytes(b"MZ")
        with self.assertRaises(gate.PackageError):
            gate.verify_bundle(self.bundle)

    def test_valid_product_archive_matches_bundle(self):
        result = gate.verify_archive(self.bundle, self.make_archive())
        self.assertEqual(len(result), 64)

    def test_rejects_zip_traversal(self):
        with self.assertRaises(gate.PackageError):
            gate.verify_archive(self.bundle, self.make_archive([("../evil", b"x")]))

    def test_rejects_duplicate_zip_path(self):
        with self.assertRaises(gate.PackageError):
            gate.verify_archive(
                self.bundle,
                self.make_archive([(self.bundle.name + "/" + gate.MODULE_RELATIVE.as_posix(), pe())]),
            )

    def test_rejects_zip_content_mismatch(self):
        with self.assertRaises(gate.PackageError):
            gate.verify_archive(self.bundle, self.make_archive([(self.bundle.name + "/extra.txt", b"x")]))


if __name__ == "__main__":
    unittest.main()
