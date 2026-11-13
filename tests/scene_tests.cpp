// Stage scenes: the mapping, the fade, the picture files, the CMake list and the saved scene. JUCE-free. Usage: scene_tests <project root>
#include "../Source/Scenes.h"
#include "../Source/StagePresets.h"
#include "../Source/StyleFile.h"
#include "test_support.h"
#include <set>

// width and height of a baseline/progressive JPEG, read from its SOFn marker; false if it is not a JPEG
static bool jpegSize(const std::string& d, int& w, int& h)
{
    if (d.size() < 4 || (unsigned char) d[0] != 0xff || (unsigned char) d[1] != 0xd8) return false;
    size_t i = 2;
    while (i + 9 < d.size())
    {
        if ((unsigned char) d[i] != 0xff) return false;
        const unsigned char m = (unsigned char) d[i + 1];
        const size_t len = ((unsigned char) d[i + 2] << 8) | (unsigned char) d[i + 3];
        if (m >= 0xc0 && m <= 0xcf && m != 0xc4 && m != 0xc8 && m != 0xcc) { h = ((unsigned char) d[i + 5] << 8) | (unsigned char) d[i + 6]; w = ((unsigned char) d[i + 7] << 8) | (unsigned char) d[i + 8]; return true; }
        i += 2 + len;
    }
    return false;
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    // 1. one scene per stage preset, in the same order and under the same names
    CHECK(scenes::kNumScenes == choir::kNumStagePresets, "%d scenes for %d stage presets", scenes::kNumScenes, choir::kNumStagePresets);
    for (int i = 0; i < scenes::kNumScenes; ++i) CHECK(std::string(scenes::kScenes[i].presetName) == choir::kStagePresets[i].name, "scene %d is '%s' but stage preset %d is '%s'", i, scenes::kScenes[i].presetName, i, choir::kStagePresets[i].name);
    CHECK(scenes::samePicture(0, 1) && !scenes::samePicture(1, 2) && !scenes::samePicture(2, 3), "Default and Cathedral keep the existing photo; every other scene has its own");
    std::set<std::string> files; for (const auto& s : scenes::kScenes) files.insert(s.file);
    CHECK(files.size() == 9, "expected 9 distinct pictures, found %zu", files.size());
    CHECK(scenes::clampScene(-1) == 0 && scenes::clampScene(10) == 0 && scenes::clampScene(9) == 9, "clampScene");

    // 2. the crossfade: 0 -> 1 over 400 ms, monotonic, flat at both ends
    CHECK(scenes::kFadeMs == 400 && scenes::fadeProgress(-5) == 0.f && scenes::fadeProgress(0) == 0.f && scenes::fadeProgress(400) == 1.f && scenes::fadeProgress(1e6) == 1.f, "fade end points");
    CHECK(std::fabs(scenes::fadeProgress(200) - .5f) < 1e-6f, "the fade is not half done at half time");
    float last = 0; for (int ms = 0; ms <= 400; ms += 5) { const float p = scenes::fadeProgress(ms); CHECK(p >= last - 1e-6f && p >= 0 && p <= 1, "fade not monotonic at %d ms", ms); last = p; }
    CHECK(scenes::fadeProgress(20) < .02f && scenes::fadeProgress(380) > .98f, "the fade should ease in and out");

    // 3. every picture exists, is a JPEG of the right size, and is in CMake's binary-data list
    const std::string cmake = ts::readFile(root + "/CMakeLists.txt");
    CHECK(!cmake.empty(), "cannot read CMakeLists.txt under %s", root.c_str());
    const auto at = cmake.find("juce_add_binary_data(OmniChoirFXArt"); CHECK(at != std::string::npos, "binary data target not found");
    const std::string list = cmake.substr(at, cmake.find(')', at) - at);
    for (const auto& f : files)
    {
        const std::string data = ts::readFile(root + "/" + f); int w = 0, h = 0;
        CHECK(!data.empty(), "%s is missing", f.c_str());
        CHECK(jpegSize(data, w, h), "%s is not a valid JPEG", f.c_str());
        CHECK(w == scenes::kImageWidth && h == scenes::kImageHeight, "%s is %dx%d, expected %dx%d", f.c_str(), w, h, scenes::kImageWidth, scenes::kImageHeight);
        CHECK(data.size() < 400 * 1024, "%s is %zu bytes: keep pictures small, they are embedded in the plugin", f.c_str(), data.size());
        CHECK(list.find(f) != std::string::npos, "%s is not in the juce_add_binary_data list in CMakeLists.txt", f.c_str());
    }
    // and nothing in the list that no scene uses (the three room impulse responses are the only other assets)
    size_t pos = 0; int jpgsInList = 0;
    while ((pos = list.find(".jpg", pos)) != std::string::npos) { ++jpgsInList; pos += 4; }
    CHECK(jpgsInList == (int) files.size(), "CMake lists %d pictures but the scenes use %zu", jpgsInList, files.size());

    // 4. the editor and the processor use the scene machinery
    const std::string editor = ts::readFile(root + "/Source/PluginEditor.cpp"), proc = ts::readFile(root + "/Source/PluginProcessor.cpp");
    for (const auto& s : scenes::kScenes) { std::string n = s.file; n = n.substr(n.rfind('/') + 1); for (auto& c : n) if (c == '.') c = '_'; CHECK(editor.find("BinaryData::" + n + ";") != std::string::npos && editor.find("BinaryData::" + n + "Size;") != std::string::npos, "the editor does not load %s", n.c_str()); }
    CHECK(proc.find("setAttribute(\"omniScene\",scene.load())") != std::string::npos && proc.find("getIntAttribute(\"omniScene\",0)") != std::string::npos, "the scene is not both saved to and restored from the processor state");
    CHECK(proc.find("scene.store(scenes::clampScene(index))") != std::string::npos && proc.find("values,scene.load()") != std::string::npos && proc.find("data.scene>=0") != std::string::npos, "applyStagePreset / saveStyleFile / loadStyleFile do not handle the scene");

    // 5. the scene in a style file
    {
        const auto text = choir::writeStyleFile("Hall", { { "mix", .5f } }, 4); choir::StyleFileData d;
        CHECK(choir::parseStyleFile(text, d) && d.scene == 4 && d.values.size() == 1, "scene= did not survive a style file round trip");
        CHECK(choir::parseStyleFile(choir::writeStyleFile("Old", { { "mix", .5f } }), d) && d.scene == -1 && text.find("scene=4") != std::string::npos, "a style with no scene must say none");
        for (const char* bad : { "scene=10", "scene=99", "scene=-3", "scene=abc", "scene=", "scene=2.5x" })
            CHECK(choir::parseStyleFile(std::string("OmniChoirStyle 1\nname=x\n") + bad + "\nmix=.4\n", d) && d.scene == -1 && d.values.size() == 1, "'%s' must be ignored", bad);
        CHECK(choir::parseStyleFile("OmniChoirStyle 1\nscene=3\nscene=5\nmix=.4\n", d) && d.scene == 3, "a repeated scene= must keep the first");
        CHECK(choir::isUserStyleId("scene") == false, "scene is not a parameter id");
    }
    std::printf("scenes: 10 scenes map to the 10 stage presets, 9 pictures valid at %dx%d and listed in CMake, fade and style-file scene tested\n", scenes::kImageWidth, scenes::kImageHeight);
    return 0;
}
