import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class Quick4RendererContractTests(unittest.TestCase):
    def test_opt_in_real_juce_target_is_separate_from_ci_tour(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn('option(OMNICHOIR_BUILD_QUICK4', cmake)
        self.assertIn('juce_add_console_app(SkellyChoirQuick4', cmake)
        self.assertIn('tools/quick4.cpp', cmake)
        self.assertIn('target_link_libraries(SkellyChoirQuick4 PRIVATE', cmake)
        self.assertIn('SkellCore::DSP', cmake)
        self.assertIn('SkellCore::UI', cmake)

    def test_headless_juce_uses_a_single_timer_driven_message_loop(self):
        source = (ROOT / "tools" / "quick4.cpp").read_text()
        self.assertIn("class Quick4Runner final : private juce::Timer", source)
        self.assertIn("runDispatchLoop()", source)
        self.assertIn("stopDispatchLoop()", source)
        self.assertNotIn("runDispatchLoopUntil", source)
        self.assertIn("juce_cryptography", (ROOT / "CMakeLists.txt").read_text())
        self.assertIn("juce_cryptography/juce_cryptography.h", source)

    def test_renderer_instantiates_real_processor_and_editor(self):
        source = (ROOT / "tools" / "quick4.cpp").read_text()
        self.assertIn("OmniChoirAudioProcessor processor", source)
        self.assertIn("processor.createEditor()", source)
        self.assertIn("createComponentSnapshot", source)
        self.assertIn("showRoomMenuForScreenshot()", source)

    def test_menu_capture_is_native_desktop_capture_not_a_mock(self):
        source = (ROOT / "tools" / "quick4.cpp").read_text()
        self.assertIn('arguments.add("scrot")', source)
        self.assertIn("ImageFileFormat::loadFrom(rawFile)", source)
        self.assertIn("PopupMenu::dismissAllActiveMenus()", source)
        self.assertIn("openbox --sm-disable", (ROOT / ".github" / "workflows" / "ui-tour.yml").read_text())
        self.assertIn("_NET_SUPPORTING_WM_CHECK", (ROOT / ".github" / "workflows" / "ui-tour.yml").read_text())
        self.assertNotIn("drawText(\"ROOMS", source)

    def test_png_verifier_fails_closed_on_exact_manifest_and_hashes(self):
        verifier = (ROOT / "scripts" / "verify_quick4.py").read_text()
        self.assertIn("EXPECTED =", verifier)
        self.assertIn("hashlib.sha256", verifier)
        self.assertIn("binascii.crc32", verifier)
        self.assertIn("zlib.decompress", verifier)
        self.assertIn("native popup capture should include", verifier)

    def test_manifest_declares_exact_four_scenes_with_hashes(self):
        source = (ROOT / "tools" / "quick4.cpp").read_text()
        self.assertIn('root->setProperty("count", static_cast<int>(scenes.size()))', source)
        self.assertIn('item->setProperty("sha256", sha256(scene.path))', source)
        self.assertIn('"00-main.png"', source)
        self.assertIn('"01-room-position-popup.png"', source)
        self.assertIn('"02-active-solo-mute.png"', source)
        self.assertIn('"03-neon-green.png"', source)

    def test_local_guide_pins_real_dependencies(self):
        doc = (ROOT / "docs" / "LOCAL_UI_SCREENSHOTS.md").read_text()
        self.assertIn("8.0.16-security", doc)
        self.assertIn("4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d", doc)
        self.assertIn("xvfb-run", doc)
        self.assertIn("scrot", doc)


if __name__ == "__main__":
    unittest.main()
