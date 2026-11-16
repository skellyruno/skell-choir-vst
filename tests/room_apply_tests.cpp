// Rooms and styles never move anyone: applyRoomOnly / applyStyle change only the seven room fields, the processor's applyRoom, applyStyleValues
// and loadStyleFile never write a placement parameter, room names are unique and every room differs from the others.
#include "test_support.h"
#include "../Source/StagePresets.h"
#include "../Source/UI/StageControlLayout.h"
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
    // 2. full styles recall choir, mic and lead positions, without changing gain or unrelated settings.
    for (int i = 0; i < kNumStylePresets; ++i) {
        FXParams p=ts::pluginDefaults();p.x={-3.f,-1.f,1.f,3.f};
        p.micX=1.5f;p.micY=-2.f;p.leadX=.4f;const FXParams before=p;float sz=0.f;
        applyStyle(kStylePresets[i],p,sz);
        const auto& expected=kStagePresets[kStylePresets[i].stage];
        for(size_t k=0;k<4;++k)
            CHECK(p.x[k]==expected.x[k]&&p.y[k]==expected.y[k]&&p.gain[k]==before.gain[k],
                  "style %s did not restore its choir arrangement",kStylePresets[i].name);
        CHECK(p.micX==kStylePresets[i].micX && p.micY==kStylePresets[i].micY
              && p.leadX==kStylePresets[i].leadX && p.leadY==kStylePresets[i].leadY,
              "style %s did not restore mic/lead position", kStylePresets[i].name);
        CHECK(p.micMode==before.micMode && p.width==before.width && p.leadGain==before.leadGain,
              "style %s changed mic array or lead level",kStylePresets[i].name);
    }
    // 3. names are unique and every room is a different room
    std::set<std::string> names; for (int i = 0; i < kNumStagePresets; ++i) CHECK(names.insert(kStagePresets[i].name).second, "duplicate room name %s", kStagePresets[i].name);
    for (int a = 0; a < kNumStagePresets; ++a) for (int b = a + 1; b < kNumStagePresets; ++b)
    {
        FXParams pa, pb; applyRoomOnly(kStagePresets[a], pa); applyRoomOnly(kStagePresets[b], pb);
        CHECK(pa.roomSeconds != pb.roomSeconds || pa.wet != pb.wet || pa.hall != pb.hall || pa.roomSize != pb.roomSize, "%s and %s are the same room", kStagePresets[a].name, kStagePresets[b].name);
    }
    // 4. the placement ids
    for (const char* id : { "micX", "micY", "leadX", "leadY", "x0", "x3", "y0", "y3" }) CHECK(isPlacementId(id), "%s is a placement id", id);
    for (const char* id : { "mix", "x4", "y", "xx", "micMode", "width", "wet", "x", "" }) CHECK(!isPlacementId(id), "'%s' is not a placement id", id);
    // 5. only rooms leave everyone in place; full and saved styles recall stage placements.
    const std::string src = ts::readFile(root + "/Source/PluginProcessor.cpp");
    CHECK(!src.empty(), "cannot read PluginProcessor.cpp");
    const auto body = [&](const char* fn) { auto b = src.find(std::string("void OmniChoirAudioProcessor::") + fn); if (b == std::string::npos) b = src.find(std::string("bool OmniChoirAudioProcessor::") + fn); CHECK(b != std::string::npos, "%s missing", fn); const auto e = src.find("\nOmniChoir", b + 10), e2 = src.find("\nvoid OmniChoirAudioProcessor::", b + 10), e3 = src.find("\nbool OmniChoirAudioProcessor::", b + 10); return src.substr(b, std::min({ e, e2, e3 }) - b); };
    for(const char* id : { "\"x0\"", "\"y0\"", "\"micX\"", "\"micY\"", "\"leadX\"", "\"leadY\"", "applyRoomLayout" })
        CHECK(body("applyRoom").find(id)==std::string::npos,"room-only change touches %s",id);
    CHECK(body("applyStyleValues").find("applyRoomLayout()")!=std::string::npos,"full preset must move choir");
    for(const char* id : { "\"micX\"", "\"micY\"", "\"leadX\"", "\"leadY\"" })
        CHECK(body("applyStyleValues").find(id)!=std::string::npos,"full preset must move %s",id);
    CHECK(body("loadStyleFile").find("setParameterValue(juce::String(value.first),value.second)")!=std::string::npos,
          "saved style should restore allow-listed stage positions");
    // 6. the editor: rooms are not in the top preset box, the room name opens a room menu, solo/mute are buttons beside the name tags (no strip)
    const std::string ed = ts::readFile(root + "/Source/PluginEditor.cpp");
    CHECK(!ed.empty(), "cannot read PluginEditor.cpp");
    const auto at = ed.find("::rebuildStyleMenu()"); CHECK(at != std::string::npos, "rebuildStyleMenu missing");
    const std::string menuBody = ed.substr(at, ed.find("::handleStyleMenu", at) - at);
    CHECK(menuBody.find("kStagePresets") == std::string::npos && menuBody.find("ROOMS") == std::string::npos, "the top preset box still lists rooms");
    CHECK(ed.find("showRoomMenu();return;")!=std::string::npos && ed.find("menu.addSubMenu(\"ROOMS\"")!=std::string::npos
          && ed.find("menu.addSubMenu(\"POSITION\"")!=std::string::npos, "room or position category missing");
    CHECK(ed.find("stripCell") == std::string::npos, "the old bottom solo/mute strip is still in the editor");
    CHECK(ed.find("drawTagButtons(i)") != std::string::npos && ed.find("drawTagButtons(4)") != std::string::npos, "S / M buttons are not drawn beside every name tag and the lead's");
    CHECK(ed.find("stagecontrols::beside(")!=std::string::npos && ed.find("stagecontrols::besideLead(")!=std::string::npos,"solo/mute buttons should track choir and lead at every size");
    CHECK(ed.find("showThemeMenu()")==std::string::npos && ed.find("cycleTheme();")!=std::string::npos,"skull must cycle themes");
    CHECK(ed.find("return curtain::Mode::OncePerSession;")!=std::string::npos,"curtain must play once per session");
    // Check responsive stage badges at default/compact/expanded sizes and after extreme dragging.
    for (const float width:{730.f,765.f,860.f,1300.f,1500.f}) {
        const float left=7.f,right=width-35.f,ppm=(right-left)/21.7f,centre=width*.48f;
        const float topLeft=centre-.705f*ppm,topRight=centre+.705f*ppm,figure=width<900.f?12.f:19.f;
        const auto s=stagecontrols::beside(topLeft,topRight,120.f,figure,left,10.f,right,450.f,false);
        const auto m=stagecontrols::beside(topLeft,topRight,120.f,figure,left,10.f,right,450.f,true);
        CHECK(s.x>=topRight+.62f*figure-.001f && s.x<topRight+.62f*figure+2.f,"S overlaps or detaches from figures at %.0f",width);
        CHECK(std::abs(m.x-s.x)<.01f && std::abs(m.y-s.y-stagecontrols::kRowStep)<.01f,"S/M pair must stack");
    }
    for (const float x:{-65.f,0.f,300.f,600.f,740.f}) for (const float y:{5.f,45.f,380.f,460.f}) {
        const auto s=stagecontrols::beside(x-25.f,x+25.f,y,15.f,7.f,7.f,730.f,500.f,false);
        const auto m=stagecontrols::beside(x-25.f,x+25.f,y,15.f,7.f,7.f,730.f,500.f,true);
        CHECK(s.x>=10.f && s.x+s.width<=727.f && m.y>=37.f && m.y+m.height<=497.f,"S/M escapes stage at %.1f,%.1f",x,y);
    }
    std::printf("ROOM APPLY TESTS PASSED: %d rooms, %d styles, responsive S/M placements\n", kNumStagePresets, kNumStylePresets);
}
