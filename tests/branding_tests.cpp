// Branding: the product is SkellyChoir; the identity that old sessions depend on did not change. JUCE-free. Usage: branding_tests <project root>
#include "../Source/Branding.h"
#include "test_support.h"
#include <cctype>

static bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const std::string cmake = ts::readFile(root + "/CMakeLists.txt"), proc = ts::readFile(root + "/Source/PluginProcessor.cpp"),
                      procH = ts::readFile(root + "/Source/PluginProcessor.h"), editor = ts::readFile(root + "/Source/PluginEditor.cpp"),
                      ci = ts::readFile(root + "/.github/workflows/build.yml");
    CHECK(!cmake.empty() && !proc.empty() && !editor.empty() && !ci.empty(), "cannot read the project under %s (pass the project root as the first argument)", root.c_str());
    const std::string name = branding::kProductName;

    // 1. the name is used everywhere the user sees it
    CHECK(has(cmake, "PRODUCT_NAME \"" + name + "\""), "CMakeLists.txt PRODUCT_NAME is not \"%s\"", name.c_str());
    CHECK(has(procH, "branding::kProductName"), "the plugin's getName() must come from Branding.h");
    CHECK(has(editor, "branding::kTitle") && has(editor, "branding::kSubtitle"), "the editor header must draw the title and subtitle from Branding.h");
    std::string upper = name; for (auto& c : upper) c = (char) std::toupper((unsigned char) c);
    CHECK(upper == branding::kTitle, "kTitle (%s) must be kProductName in capitals", branding::kTitle);
    CHECK(has(branding::kSubtitle, "LISTEN & RENDER") && !has(branding::kSubtitle, "SCAN"), "the subtitle must say LISTEN & RENDER");
    for (const char* s : { branding::kProductName, branding::kTitle, branding::kSubtitle, branding::kSettingsFolder })
        for (const char* c = s; *c; ++c) CHECK((unsigned char) *c < 128, "branding strings must be plain ASCII (juce::String asserts otherwise): %s", s);
    CHECK(has(ci, "name: SkellyChoir-${{ runner.os }}-VST3") && has(ci, "path: SkellyChoir-${{ runner.os }}-VST3.zip") && has(ci, "DestinationPath SkellyChoir-${{ runner.os }}-VST3.zip") && !has(ci, "OmniChoirFX-${{"),
          "CI artifact names must use the new name in the zip, the upload name and the upload path");

    // 2. what must NOT change, or old sessions stop loading
    CHECK(has(cmake, "PLUGIN_CODE OmF2") && has(cmake, "PLUGIN_MANUFACTURER_CODE SkAu") && has(cmake, "BUNDLE_ID \"audio.skell.omnichoirfx\""),
          "PLUGIN_CODE / PLUGIN_MANUFACTURER_CODE / BUNDLE_ID changed: saved sessions and the host's plugin identity depend on them");
    CHECK(has(proc, "\"OMNICHOIR_FX_PARAMETERS\""), "the APVTS state type changed: old sessions would not load");
    CHECK(has(cmake, "juce_add_plugin(OmniChoirFX"), "the CMake target name is internal and was meant to stay");

    // 3. the settings folder moves, and what was in the old one is copied across once
    CHECK(std::string(branding::kSettingsFolder) != branding::kLegacySettingsFolder && std::string(branding::kLegacySettingsFolder) == "OmniChoir FX", "legacy folder name");
    CHECK(has(proc, "copyDirectoryTo") && has(proc, "branding::kLegacySettingsFolder"), "settingsFolder() must copy the old folder across");
    CHECK(!has(proc, "getChildFile(\"OmniChoir FX\")") && !has(editor, "getChildFile(\"OmniChoir FX\")"), "a hard-coded old settings folder is left in the code");

    // 4. no user-facing text still says the old name
    const char* files[] = { "README.md", "docs/BUILD_AND_TEST.md", "docs/DEVELOPMENT_NOTES.md", "docs/EXTERNAL_SINGERS.md", "Source/PluginEditor.cpp", "Source/PluginProcessor.cpp",
                            "Source/PluginProcessor.h", "tools/render_listening_pack.cpp", ".github/workflows/build.yml", "scripts/build_listening_pack.sh" };
    const char* stale[] = { "OMNICHOIR FX", "OMNICHOIR\"", "SCAN & RENDER" };
    int scanned = 0;
    for (const char* f : files)
    {
        const std::string text = ts::readFile(root + "/" + f); CHECK(!text.empty(), "cannot read %s", f);
        ++scanned;
        for (const char* s : stale) CHECK(!has(text, s), "%s still contains the old name text \"%s\"", f, s);
        std::istringstream lines(text); std::string line; int n = 0;
        while (std::getline(lines, line))
        {
            ++n;
            const auto at = line.find("OmniChoir FX"); if (at == std::string::npos) continue;
            const bool history = has(line, "formerly") || has(line, "was `OmniChoir FX`") || has(line, "starter") || has(line, "kLegacy") || has(line, "pre-SkellyChoir");
            CHECK(history, "%s:%d still says \"OmniChoir FX\" outside a history note: %s", f, n, line.substr(0, 120).c_str());
        }
    }
    std::printf("branding: SkellyChoir is the name in %d user-facing files; identity codes and the state type are unchanged; the old settings folder is migrated\n", scanned);
    return 0;
}
