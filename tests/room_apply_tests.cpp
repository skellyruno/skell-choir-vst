// Rooms and styles never move anyone: applyRoomOnly / applyStyle change only the seven room fields, the processor's applyRoom, applyStyleValues
// and loadStyleFile never write a placement parameter, room names are unique and every room differs from the others.
#include "test_support.h"
#include "../Source/StagePresets.h"
#include "../Source/StylePresets.h"
#include "../Source/StyleFile.h"
#include <set>
using namespace choir;

static bool samePlacement(const FXParams& a, const FXParams& b)
{
    for (size_t i = 0; i < 4; ++i) if (a.x[i] != b.x[i] || a.y[i] != b.y[i] || a.gain[i] != b.gain[i]) return false;
    return a.micX == b.micX && a.micY == b.micY && a.width == b.width && a.micMode == b.micMode && a.ensemble == b.ensemble && a.humanize == b.humanize
        && a.leadX == b.leadX && a.leadY == b.leadY && a.leadGain == b.leadGain && a.mix == b.mix && a.master == b.master;
}
static bool sameRoom(const FXParams& a, const StagePreset& s)
{
    return a.roomSize == s.roomSize && a.roomSeconds == s.roomSeconds && a.absorption == s.absorption && a.diffusion == s.diffusion && a.wet == s.wet && a.hall == s.hall && a.hallSeconds == s.hallSeconds;
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    // 1. applying a room (from any starting point) sets the room and leaves everyone where they were
    for (int from = 0; from < kNumStagePresets; ++from) for (int to = 0; to < kNumStagePresets; ++to)
    {
        FXParams p = ts::pluginDefaults(); applyStagePreset(kStagePresets[from], p);
        p.leadX = .7f; p.leadY = 1.1f; p.mix = .66f; p.master = .9f;
        const FXParams before = p; applyRoomOnly(kStagePresets[to], p);
        CHECK(samePlacement(before, p), "room %s -> %s moved someone or changed how they sing", kStagePresets[from].name, kStagePresets[to].name);
        CHECK(sameRoom(p, kStagePresets[to]), "room %s did not set its room fields", kStagePresets[to].name);
    }
    // 2. a style changes the room and the musical values but never a placement
    for (int i = 0; i < kNumStylePresets; ++i)
    {
        FXParams p = ts::pluginDefaults(); p.x = { -3.f, -1.f, 1.f, 3.f }; p.micX = 1.5f; p.micY = -2.f; p.leadX = .4f; const FXParams before = p; float sz = 0;
        applyStyle(kStylePresets[i], p, sz);
        for (size_t k = 0; k < 4; ++k) CHECK(p.x[k] == before.x[k] && p.y[k] == before.y[k] && p.gain[k] == before.gain[k], "style %s moved a section", kStylePresets[i].name);
        CHECK(p.micX == before.micX && p.micY == before.micY && p.leadX == before.leadX && p.leadY == before.leadY && p.micMode == before.micMode && p.width == before.width, "style %s moved the mic or the lead", kStylePresets[i].name);
    }
    // 3. names are unique and every room is a different room
    std::set<std::string> names; for (int i = 0; i < kNumStagePresets; ++i) CHECK(names.insert(kStagePresets[i].name).second, "duplicate room name %s", kStagePresets[i].name);
    for (int a = 0; a < kNumStagePresets; ++a) for (int b = a + 1; b < kNumStagePresets; ++b)
    {
        FXParams pa, pb; applyRoomOnly(kStagePresets[a], pa); applyRoomOnly(kStagePresets[b], pb);
        CHECK(pa.roomSeconds != pb.roomSeconds || pa.wet != pb.wet || pa.hall != pb.hall || pa.roomSize != pb.roomSize, "%s and %s are the same room", kStagePresets[a].name, kStagePresets[b].name);
    }
    // 4. the placement ids
    for (const char* id : { "micX", "micY", "x0", "x3", "y0", "y3" }) CHECK(isPlacementId(id), "%s is a placement id", id);
    for (const char* id : { "mix", "x4", "y", "xx", "micMode", "width", "wet", "x", "" }) CHECK(!isPlacementId(id), "'%s' is not a placement id", id);
    // 5. the plugin source: applyRoom, applyStyleValues and loadStyleFile never write a placement parameter
    const std::string src = ts::readFile(root + "/Source/PluginProcessor.cpp");
    CHECK(!src.empty(), "cannot read PluginProcessor.cpp");
    const auto body = [&](const char* fn) { auto b = src.find(std::string("void OmniChoirAudioProcessor::") + fn); if (b == std::string::npos) b = src.find(std::string("bool OmniChoirAudioProcessor::") + fn); CHECK(b != std::string::npos, "%s missing", fn); const auto e = src.find("\nOmniChoir", b + 10), e2 = src.find("\nvoid OmniChoirAudioProcessor::", b + 10), e3 = src.find("\nbool OmniChoirAudioProcessor::", b + 10); return src.substr(b, std::min({ e, e2, e3 }) - b); };
    for (const char* fn : { "applyRoom", "applyStyleValues", "loadStyleFile" })
    {
        const std::string t = body(fn);
        for (const char* id : { "\"x0\"", "\"y0\"", "\"micX\"", "\"micY\"", "\"gain", "\"micMode\"", "\"width\"", "\"leadX\"", "\"leadY\"", "applyStagePreset" })
            CHECK(t.find(id) == std::string::npos, "%s touches %s", fn, id);
    }
    CHECK(body("loadStyleFile").find("isPlacementId") != std::string::npos, "loadStyleFile no longer filters placement ids");
    // 6. the editor: rooms are not in the top preset box, the room name opens a room menu, solo/mute are buttons beside the name tags (no strip)
    const std::string ed = ts::readFile(root + "/Source/PluginEditor.cpp");
    CHECK(!ed.empty(), "cannot read PluginEditor.cpp");
    const auto at = ed.find("::rebuildStyleMenu()"); CHECK(at != std::string::npos, "rebuildStyleMenu missing");
    const std::string menuBody = ed.substr(at, ed.find("::handleStyleMenu", at) - at);
    CHECK(menuBody.find("kStagePresets") == std::string::npos && menuBody.find("ROOMS") == std::string::npos, "the top preset box still lists rooms");
    CHECK(ed.find("showRoomMenu();return;") != std::string::npos && ed.find("kStagePresets[i].name,true,i==now") != std::string::npos, "the room name does not open the room menu");
    CHECK(ed.find("stripCell") == std::string::npos, "the old bottom solo/mute strip is still in the editor");
    CHECK(ed.find("drawTagButtons(i)") != std::string::npos && ed.find("drawTagButtons(4)") != std::string::npos, "S / M buttons are not drawn beside every name tag and the lead's");
    std::printf("ROOM APPLY TESTS PASSED: %d rooms, %d styles\n", kNumStagePresets, kNumStylePresets);
}
