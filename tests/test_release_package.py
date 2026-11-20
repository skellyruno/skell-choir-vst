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



    def set_commercial_contact(self, url="https://skellyr.uno", email="support@skellyr.uno"):
        text = moduleinfo().replace('"URL": "",', f'"URL": "{url}",')
        text = text.replace('"E-Mail": "",', f'"E-Mail": "{email}",')
        self.info.write_text(text, encoding="utf-8")

    def test_preview_manifest_explicitly_blocks_commercial_metadata(self):
        manifest_path = self.root / "package" / "build-info.json"
        manifest = gate.write_provenance(
            self.bundle, manifest_path, "source-sha", "ci-sha", "12345"
        )
        gate_info = manifest["commercial_metadata_gate"]
        self.assertEqual(manifest["release_status"], "preview-only")
        self.assertEqual(gate_info["status"], "blocked")
        self.assertIn("Factory Info.URL must be a valid HTTPS vendor URL", gate_info["blockers"])
        self.assertIn("Factory Info.E-Mail must be a valid support email address", gate_info["blockers"])
        self.assertIn("Owner-approved LICENSE or COPYING file was not supplied", gate_info["blockers"])

    def test_commercial_gate_requires_explicit_owner_license_assertion(self):
        self.set_commercial_contact()
        license_path = self.root / "LICENSE"
        license_path.write_text("Owner-supplied license text\n", encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, self.root / "package" / "build-info.json",
                "source-sha", "ci-sha", "12345",
                require_commercial_metadata=True,
                license_file=license_path,
                owner_approved_license=False,
            )

    def test_commercial_gate_rejects_missing_license(self):
        self.set_commercial_contact()
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, self.root / "package" / "build-info.json",
                "source-sha", "ci-sha", "12345",
                require_commercial_metadata=True,
                owner_approved_license=True,
            )

    def test_commercial_gate_rejects_non_https_vendor_url(self):
        self.set_commercial_contact(url="http://skellyr.uno")
        license_path = self.root / "LICENSE"
        license_path.write_text("Owner-supplied license text\n", encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, self.root / "package" / "build-info.json",
                "source-sha", "ci-sha", "12345",
                require_commercial_metadata=True,
                license_file=license_path,
                owner_approved_license=True,
            )

    def test_commercial_gate_rejects_invalid_support_email(self):
        self.set_commercial_contact(email="support at skellyr.uno")
        license_path = self.root / "LICENSE"
        license_path.write_text("Owner-supplied license text\n", encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, self.root / "package" / "build-info.json",
                "source-sha", "ci-sha", "12345",
                require_commercial_metadata=True,
                license_file=license_path,
                owner_approved_license=True,
            )

    def test_commercial_gate_rejects_empty_license(self):
        self.set_commercial_contact()
        license_path = self.root / "LICENSE"
        license_path.write_bytes(b" \n")
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, self.root / "package" / "build-info.json",
                "source-sha", "ci-sha", "12345",
                require_commercial_metadata=True,
                license_file=license_path,
                owner_approved_license=True,
            )

    def test_commercial_gate_rejects_unsafe_license_filename(self):
        self.set_commercial_contact()
        license_path = self.root / "LICENSE-UNKNOWN"
        license_path.write_text("Owner-supplied license text\n", encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, self.root / "package" / "build-info.json",
                "source-sha", "ci-sha", "12345",
                require_commercial_metadata=True,
                license_file=license_path,
                owner_approved_license=True,
            )

    def test_commercial_gate_refuses_to_overwrite_packaged_license(self):
        self.set_commercial_contact()
        package = self.root / "package"
        package.mkdir()
        (package / "LICENSE").write_text("Existing different license\n", encoding="utf-8")
        license_path = self.root / "LICENSE"
        license_path.write_text("Owner-supplied license text\n", encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, package / "build-info.json",
                "source-sha", "ci-sha", "12345",
                require_commercial_metadata=True,
                license_file=license_path,
                owner_approved_license=True,
            )

    def test_commercial_gate_packages_and_hashes_owner_license_without_claiming_release(self):
        self.set_commercial_contact()
        license_path = self.root / "LICENSE"
        license_bytes = b"Owner-approved license text supplied by the repository owner.\n"
        license_path.write_bytes(license_bytes)
        manifest_path = self.root / "package" / "build-info.json"
        manifest = gate.write_provenance(
            self.bundle, manifest_path, "source-sha", "ci-sha", "12345",
            require_commercial_metadata=True,
            license_file=license_path,
            owner_approved_license=True,
        )
        packaged = manifest_path.parent / "LICENSE"
        self.assertEqual(packaged.read_bytes(), license_bytes)
        self.assertEqual(manifest["release_status"], "preview-only")
        self.assertEqual(manifest["commercial_metadata_gate"]["status"], "passed")
        self.assertEqual(
            manifest["commercial_metadata_gate"]["license"]["license_sha256"],
            gate.digest(license_bytes),
        )
        sums = (manifest_path.parent / "SHA256SUMS.txt").read_text(encoding="ascii")
        self.assertIn(f"{gate.digest(license_bytes)}  LICENSE", sums)
        self.assertIn("Third-party license and attribution review is pending", manifest["release_blockers"])


    def test_cli_rejects_license_file_without_commercial_gate(self):
        result = gate.main([
            "--bundle", str(self.bundle),
            "--license-file", str(self.root / "LICENSE"),
        ])
        self.assertEqual(result, 1)

    def test_cli_rejects_owner_assertion_without_commercial_gate(self):
        result = gate.main([
            "--bundle", str(self.bundle),
            "--owner-approved-license",
        ])
        self.assertEqual(result, 1)

    def test_cli_requires_manifest_for_commercial_gate(self):
        result = gate.main([
            "--bundle", str(self.bundle),
            "--require-commercial-metadata",
        ])
        self.assertEqual(result, 1)

    def test_preview_manifest_hashes_every_package_sidecar(self):
        package = self.root / "package"
        package.mkdir()
        (package / "Install-SkellyChoirPreview.ps1").write_text("installer", encoding="utf-8")
        (package / "WINDOWS-VST3-INSTALL.md").write_text("install guide", encoding="utf-8")
        (package / "THIRD_PARTY_NOTICES.md").write_text("notices", encoding="utf-8")
        manifest_path = package / "build-info.json"
        gate.write_provenance(
            self.bundle, manifest_path, "source-sha", "ci-sha", "12345"
        )
        lines = (package / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines()
        listed = {line.split("  ", 1)[1] for line in lines}
        expected = {
            path.relative_to(package).as_posix()
            for path in package.rglob("*")
            if path.is_file() and path.name != "SHA256SUMS.txt"
        }
        self.assertEqual(listed, expected)

    def test_preview_manifest_rejects_case_colliding_sidecars(self):
        package = self.root / "case-collision"
        package.mkdir()
        (package / "Readme.md").write_text("one", encoding="utf-8")
        (package / "README.md").write_text("two", encoding="utf-8")
        with self.assertRaises(gate.PackageError):
            gate.write_provenance(
                self.bundle, package / "build-info.json", "source-sha", "ci-sha", "12345"
            )

    def test_public_repository_withholds_installable_preview_artifact(self):
        workflow_path = pathlib.Path(__file__).resolve().parents[1] / ".github" / "workflows" / "build.yml"
        workflow = workflow_path.read_text(encoding="utf-8")
        marker = "      - uses: actions/upload-artifact@v6\n"
        self.assertEqual(workflow.count(marker), 1)
        upload_step = workflow.split(marker, 1)[1].split("\n      - ", 1)[0]
        expected = "        if: " + chr(36) + "{{ github.repository_visibility == 'private' }}"
        self.assertIn(expected, upload_step)
        self.assertIn("name: SkellyChoir-Windows-VST3", upload_step)
        self.assertIn("path: package/", upload_step)

if __name__ == "__main__":
    unittest.main()
