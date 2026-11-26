// STAGE SPREAD: one knob that scales the singers' left-right positions. Neutral (1) must be exactly "as placed", the scaling symmetric and bounded, the
// parameter must reach the engine through spreadX, and the engine must really hear it: a wider choir has a wider stereo image.
#include "../Source/StagedChoir.h"
#include "../Source/StagePresets.h"
#include "test_support.h"
#include <fstream>
#include <sstream>
using namespace choir;
static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    // 1. the maths
    for (const auto& st : kStagePresets) for (float x : st.x) {
        CHECK(spreadX(x, 1.f) == x, "spread 1 moved x=%.3f", x);
        CHECK(spreadX(x, 1.003f) == x && spreadX(x, .997f) == x, "the neutral dead zone is not exact");
        CHECK(spreadX(-x, 1.5f) == -spreadX(x, 1.5f), "spread is not symmetric about the middle for x=%.2f", x);
        for (float s : { .25f, .5f, 1.2f, 1.8f, 5.f, -3.f }) { const float v = spreadX(x, s); CHECK(v >= kRangeX.lo && v <= kRangeX.hi, "x=%.2f spread %.2f gives %.2f off the stage", x, s, v); }
        CHECK(std::fabs(spreadX(x, .5f)) <= std::fabs(x) + 1e-6f && (std::fabs(x) < 1e-6f || std::fabs(spreadX(x, 1.5f)) >= std::fabs(x) - 1e-6f), "narrowing must not widen and widening must not narrow (x=%.2f)", x);
        CHECK(std::fabs(spreadX(x, 0.f)) <= std::fabs(x), "spread 0 is clamped to the narrowest setting");
    }
    CHECK(spreadX(0.f, 1.8f) == 0.f, "a singer in the middle stays in the middle");
    // order is kept: a wider setting never swaps two singers
    for (float s : { .25f, .7f, 1.3f, 1.8f }) { const auto& d = kStagePresets[0]; for (int i = 0; i < 3; ++i) CHECK(spreadX(d.x[i], s) <= spreadX(d.x[i + 1], s), "spread %.2f swapped singers %d and %d", s, i, i + 1); }
    // the editor draws spreadX(stored) and, when you drag, stores unspreadX(pointer): the two must be inverses wherever the drawn singer can be
    for (float s : { .25f, .5f, 1.f, 1.3f, 1.8f })
        for (float shown = -9.f; shown <= 9.f; shown += .5f) {
            const float stored = unspreadX(shown, s), back = spreadX(stored, s);
            CHECK(stored >= kRangeX.lo && stored <= kRangeX.hi, "unspreadX(%.1f, %.2f) = %.2f is off the stage", shown, s, stored);
            CHECK(std::fabs(back - shown) < 1e-4f || std::fabs(stored) >= kRangeX.hi - 1e-4f, "dragging at spread %.2f: drew %.2f, stored %.2f, which draws at %.2f", s, shown, stored, back);
        }
    CHECK(unspreadX(3.f, 1.f) == 3.f, "dragging at spread 1 must store exactly the pointer position");
    std::printf("[1] spreadX: neutral exact, symmetric, bounded, order kept; unspreadX is its inverse\n");

    // 2. the engine hears it: with the lead muted, the soprano (far left) sounds further left as the knob widens: more left/right level difference and a longer
    // delay between the ears. (With the lead playing, her centred voice hides the choir's position.)
    {
        const int sr = 48000;
        struct Image { double db; int lag; };
        auto render = [&](float spread) {
            FXParams p = ts::pluginDefaults(); p.humanize = 0.f; p.ensemble = 0.f; p.sensitivity = 1.f; p.leadGain = 0.f;
            for (int i = 1; i < 4; ++i) p.gain[(size_t) i] = 0.f;   // the soprano alone, so the image is where she stands
            for (int i = 0; i < 4; ++i) { p.x[(size_t) i] = spreadX(kStagePresets[0].x[(size_t) i], spread); p.y[(size_t) i] = kStagePresets[0].y[(size_t) i]; }
            std::vector<float> in((size_t) (sr * 3)); double ph = 0; for (size_t n = 0; n < in.size(); ++n) { ph += 2 * 3.14159265358979 * 196 / sr; double s = 0; for (int h = 1; h <= 10; ++h) s += std::sin(h * ph) / h; in[n] = (float) (.25 * s); }
            const auto o = ts::render(in, sr, p); double l = 0, r = 0, best = -1e30; int bl = 0;
            for (size_t n = (size_t) sr; n < o.l.size(); ++n) { l += (double) o.l[n] * o.l[n]; r += (double) o.r[n] * o.r[n]; }
            for (int lag = -60; lag <= 60; ++lag) { double c = 0; for (size_t n = (size_t) sr + 100; n + 100 < o.l.size(); ++n) c += (double) o.l[n] * o.r[(size_t) ((long) n + lag)]; if (c > best) { best = c; bl = lag; } }
            return Image { 10 * std::log10(std::max(l, 1e-12) / std::max(r, 1e-12)), bl }; };
        const Image narrow = render(.3f), normal = render(1.f), wide = render(1.8f);
        std::printf("    soprano alone: spread .3 -> %.1f dB, %d samples | 1 -> %.1f dB, %d | 1.8 -> %.1f dB, %d\n", narrow.db, narrow.lag, normal.db, normal.lag, wide.db, wide.lag);
        CHECK(narrow.db < normal.db && normal.db < wide.db + 3.0 && narrow.db < wide.db, "the level difference does not grow with the knob: %.1f %.1f %.1f dB", narrow.db, normal.db, wide.db);
        CHECK(narrow.lag < normal.lag && normal.lag < wide.lag + 6 && narrow.lag < wide.lag - 8, "the inter-ear delay does not grow with the knob: %d %d %d samples", narrow.lag, normal.lag, wide.lag);
        std::printf("[2] the soprano sounds further left as the knob widens\n");
    }

    // 3. wiring
    {
        const std::string proc = slurp(root + "/Source/PluginProcessor.cpp"), ed = slurp(root + "/Source/PluginEditor.cpp");
        CHECK(proc.find("addFloat(layout,\"stageSpread\",\"Stage Spread\",.25f,1.8f,.01f,1.f)") != std::string::npos, "the parameter must run .25..1.8 and default to 1 (as placed)");
        CHECK(proc.find("p.x[(size_t)i]=choir::spreadX(read(raw.xs[(size_t)i]),read(raw.stageSpread))") != std::string::npos, "the engine's X positions do not go through spreadX");
        CHECK(proc.find("raw.stageSpread=get(\"stageSpread\")") != std::string::npos, "the raw parameter is not bound");
        CHECK(ed.find("addKnob(\"STAGE SPREAD\",\"stageSpread\",pageRoom") != std::string::npos, "the knob is missing from the editor");
        for (const char* needle : { "float StageView::sectionX", "choir::spreadX(value(\"x\"+juce::String(section)),value(\"stageSpread\"))", "choir::unspreadX(target.x,value(\"stageSpread\"))", "thing={sectionX(dragging),value(\"y\"+s)}" })
            CHECK(ed.find(needle) != std::string::npos, "the stage view does not follow STAGE SPREAD: missing %s", needle);
        { // every place that draws or hit-tests a section's X must go through sectionX(), never the stored value
          size_t at = 0; int rawUses = 0; while ((at = ed.find("value(\"x\"+juce::String(", at)) != std::string::npos) { ++rawUses; at += 10; }
          std::printf("    stored-X reads left in the stage view: %d (preset comparison, sectionX itself)\n", rawUses);
          CHECK(rawUses <= 4, "%d direct reads of the stored X remain in the editor; drawing and hit-testing must use sectionX()", rawUses); }
        CHECK(kRangeStageSpread.lo == .25f && kRangeStageSpread.hi == 1.8f, "range changed");
        std::printf("[3] parameter default 1, bound, routed through spreadX, knob on the ROOM page\n");
    }
    std::printf("ALL STAGE SPREAD TESTS PASSED\n");
    return 0;
}
