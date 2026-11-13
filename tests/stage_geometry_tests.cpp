// Stage geometry: where the mic and the lead are drawn, and what leadX / leadY must be to put them there.
// Header-only (Source/StageGeometry.h), JUCE-free. Usage: stage_geometry_tests <project root>
//
// What this protects (the v0.8 stage bugs):
//   * the lead could not reach the right-hand side of the stage (her parameters were limited to +-9),
//   * moving the mic "kept the lead still" by writing offsets that hit those limits and dragged her along,
//   * the engine is unchanged: the lead at leadX = 0, leadY = 0.4 is exactly dry.
#include "../Source/StageGeometry.h"
#include "test_support.h"

using namespace choir;
namespace G = choir::StageGeometry;

static bool near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }
static bool inRange(float v, float lo, float hi) { return v >= lo - 1e-4f && v <= hi + 1e-4f; }
static bool offsetsInRange(StagePoint o) { return inRange(o.x, G::kOffsetXMin, G::kOffsetXMax) && inRange(o.y, G::kOffsetYMin, G::kOffsetYMax); }
// the unclamped inverse: if THIS is outside the parameter range then offsetFor() had to clamp and the lead would move
static StagePoint rawOffset(StagePoint lead, StagePoint mic) { return { lead.x - mic.x - G::kDisplayShift.x, lead.y - mic.y - G::kDisplayShift.y }; }

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";

    // 1. the constants are what the plugin declares, and what the engine does ---------------------------------------
    {
        const std::string proc = ts::readFile(root + "/Source/PluginProcessor.cpp");
        CHECK(!proc.empty(), "cannot read Source/PluginProcessor.cpp under %s (pass the project root as the first argument)", root.c_str());
        struct Row { const char* id; float lo, hi, def; };
        const Row rows[] = { {"leadX", G::kOffsetXMin, G::kOffsetXMax, 0.f}, {"leadY", G::kOffsetYMin, G::kOffsetYMax, G::kHomeLeadY},
                             {"micX", -G::kMicXMax, G::kMicXMax, 0.f},       {"micY", G::kMicYMin, G::kMicYMax, 0.f} };
        for (const auto& r : rows)
        {
            std::vector<std::string> a; CHECK(ts::layoutArgs(proc, std::string("\"") + r.id + "\"", a), "parameter '%s' not found in PluginProcessor.cpp", r.id);
            CHECK(near((float) ts::num(a[3]), r.lo) && near((float) ts::num(a[4]), r.hi),
                  "range of '%s' in PluginProcessor.cpp is %s..%s, Source/StageGeometry.h says %.2f..%.2f", r.id, a[3].c_str(), a[4].c_str(), r.lo, r.hi);
            CHECK(near((float) ts::num(a[6]), r.def), "default of '%s' is %s, expected %.2f", r.id, a[6].c_str(), r.def);
        }
        // the engine facts the geometry rests on
        const std::string staged = ts::readFile(root + "/Source/StagedChoir.cpp"), stage = ts::readFile(root + "/Source/Stage/Stage.h");
        CHECK(staged.find("lead->setOffset(p.leadX, p.leadY - 0.4)") != std::string::npos, "StagedChoir no longer passes (leadX, leadY - 0.4) to the lead: update StageGeometry::engineOffset");
        CHECK(stage.find("kLeadHomeY = 0.4") != std::string::npos, "the lead's home distance in Stage.h changed: update StageGeometry::kHomeLeadY");
        CHECK(stage.find("m.y + kLeadHomeY + dy") != std::string::npos && stage.find("m.x + dx") != std::string::npos, "the lead's engine position is no longer mic + (dx, 0.4 + dy)");
        CHECK(stage.find("clampv(home.x + dx, -10.0, 10.0), y = clampv(home.y + dy, -10.0, 12.0)") != std::string::npos, "the engine's mic clamp changed: update kEngineMic*");
        std::printf("[1] constants match PluginProcessor.cpp and the engine sources\n");
    }

    // 2. the lead at home is exactly dry, and her icon is never on the mic icon ----------------------------------------
    {
        const auto o = G::engineOffset(0.f, G::kHomeLeadY);
        CHECK(o.x == 0.f && o.y == 0.f, "the home position must give the engine a zero offset (that is the exactly-dry state), got %g,%g", o.x, o.y);
        for (float mx = -G::kMicXMax; mx <= G::kMicXMax; mx += 1.5f)
            for (float my = G::kMicYMin; my <= G::kMicYMax; my += 1.6f)
            {
                const auto h = G::home({ mx, my });
                CHECK(std::hypot(h.x - mx, h.y - my) >= 2.39f, "home lead is drawn %.2f m from the mic icon (mic %.2f,%.2f)", std::hypot(h.x - mx, h.y - my), mx, my);
                // the home spot is exactly what leadX = 0, leadY = 0.4 (the plugin's defaults) draws, and it inverts back to those defaults
                const auto inv = G::offsetFor(h, { mx, my });
                CHECK(near(inv.x, 0.f) && near(inv.y, 0.4f), "home() inverts to leadX,leadY = %.3f,%.3f, not the plugin defaults 0,0.4", inv.x, inv.y);
                CHECK(near(h.x, mx) && near(h.y, my + 2.8f), "home lead should be drawn 2.8 m in front of the mic, got %.2f,%.2f for mic %.2f,%.2f", h.x - mx, h.y - my, mx, my);
            }
        std::printf("[2] home = zero engine offset (dry); the home icon is >= 2.4 m from the mic icon everywhere\n");
    }

    // 3. everything the view allows is reachable with parameters inside their ranges, and the maths round-trips -------------
    long pairs = 0;
    {
        const float step = 0.5f;
        for (float mx = -G::kMicXMax; mx <= G::kMicXMax + 1e-3f; mx += step)
            for (float my = G::kMicYMin; my <= G::kMicYMax + 1e-3f; my += step)
                for (float lx = -G::kLeadXMax; lx <= G::kLeadXMax + 1e-3f; lx += step)
                    for (float ly = G::kLeadYMin; ly <= G::kLeadYMax + 1e-3f; ly += step)
                    {
                        const StagePoint mic { mx, my }, lead { lx, ly };
                        const auto raw = rawOffset(lead, mic);
                        CHECK(offsetsInRange(raw), "mic %.2f,%.2f lead %.2f,%.2f needs leadX/leadY %.2f,%.2f, outside the parameter ranges", mx, my, lx, ly, raw.x, raw.y);
                        const auto o = G::offsetFor(lead, mic);
                        const auto back = G::leadOnStage(mic, o.x, o.y);
                        CHECK(near(back.x, lx) && near(back.y, ly), "round trip: asked %.3f,%.3f got %.3f,%.3f", lx, ly, back.x, back.y);
                        // the engine puts her where the view draws her, less the fixed display shift
                        const auto room = G::leadInRoom(mic, o.x, o.y);
                        CHECK(near(room.x + G::kDisplayShift.x, lx) && near(room.y + G::kDisplayShift.y, ly), "engine and view disagree about where the lead is");
                        ++pairs;
                    }
        std::printf("[3] %ld mic/lead pairs: every one reachable inside the parameter ranges, round trip exact, engine and view agree\n", pairs);
    }

    // 4. moving the mic leaves the lead exactly where she is -------------------------------------------------------------
    {
        long moves = 0;
        const float a[] = { -9.f, -4.f, 0.f, 3.5f, 9.f }, b[] = { -8.f, -2.f, 0.f, 5.f, 8.f };
        const StagePoint leads[] = { {-10.f, -1.f}, {10.f, -1.f}, {10.f, 12.5f}, {-10.f, 12.5f}, {0.f, 2.8f}, {7.3f, 4.1f}, {-6.f, 9.f} };
        for (const auto& lead : leads)
            for (float x0 : a) for (float y0 : b) for (float x1 : a) for (float y1 : b)
            {
                const StagePoint from { x0, y0 }, to { x1, y1 };
                const auto start = G::offsetFor(lead, from);                  // the lead's parameters when the drag starts
                CHECK(near(G::leadOnStage(from, start.x, start.y).x, lead.x), "setup");
                const auto after = G::offsetFor(lead, to);                    // moveMic(): the mic moves, the lead is re-derived from where she WAS
                const auto drawn = G::leadOnStage(to, after.x, after.y);
                CHECK(near(drawn.x, lead.x) && near(drawn.y, lead.y), "mic %.1f,%.1f -> %.1f,%.1f dragged the lead from %.2f,%.2f to %.3f,%.3f", x0, y0, x1, y1, lead.x, lead.y, drawn.x, drawn.y);
                const auto before = G::leadInRoom(from, start.x, start.y), now = G::leadInRoom(to, after.x, after.y);
                CHECK(near(before.x, now.x) && near(before.y, now.y), "the lead moved IN THE ROOM when only the mic was dragged");
                ++moves;
            }
        std::printf("[4] %ld mic moves: the lead never moves, on the stage or in the room\n", moves);
    }

    // 5. the clamps ------------------------------------------------------------------------------------------------------
    {
        const auto m = G::clampMic({ 99.f, -99.f }); CHECK(m.x == G::kMicXMax && m.y == G::kMicYMin, "mic clamp");
        const auto l = G::clampLead({ -99.f, 99.f }); CHECK(l.x == -G::kLeadXMax && l.y == G::kLeadYMax, "lead clamp");
        // the right-hand edge is reachable (this was the original bug: the old +-9 limit stopped the lead short of the right of the stage)
        const auto right = G::offsetFor({ G::kLeadXMax, 6.f }, { -G::kMicXMax, 0.f });
        CHECK(near(right.x, 19.f) && offsetsInRange(right), "the lead cannot reach the far right with the mic at the far left (needs leadX %.2f)", right.x);
        // what the stage view can draw stays inside what the engine accepts
        for (float x = -G::kMicXMax; x <= G::kMicXMax; x += 0.5f)
            for (float y = G::kMicYMin; y <= G::kMicYMax; y += 0.5f)
                CHECK(inRange(x, -G::kEngineMicXMax, G::kEngineMicXMax) && inRange(y, G::kEngineMicYMin, G::kEngineMicYMax), "view mic %.1f,%.1f is outside the engine's mic clamp", x, y);
        // the engine clamps the mic (Stage.h setListener) before placing the lead relative to it; the view never gets there, but the maths must agree
        const auto far = G::leadInRoom({ 15.f, 15.f }, 1.f, 2.f); CHECK(near(far.x, 11.f) && near(far.y, 14.f), "leadInRoom ignores the engine's mic clamp: %.2f,%.2f", far.x, far.y);
        const auto farLow = G::leadInRoom({ -15.f, -15.f }, 0.f, 0.4f); CHECK(near(farLow.x, -10.f) && near(farLow.y, -9.6f), "leadInRoom low clamp: %.2f,%.2f", farLow.x, farLow.y);
        std::printf("[5] clamps; the far right is reachable; the view's mic range sits inside the engine's\n");
    }
    std::printf("stage geometry: all checks passed (%ld mic/lead pairs)\n", pairs);
    return 0;
}
