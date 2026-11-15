// Easy / Pro: the Pro page does not repeat what Easy shows. JUCE-free. Usage: page_rules_tests <project root>
#include "../Source/UI/PageRules.h"
#include "test_support.h"

int main(int argc, char** argv)
{
    using pagerules::visibleIn;
    // Easy shows the easy and easy-only controls; Pro shows exactly the rest (every control is in one mode, never both, never neither)
    for (bool easy : { false, true }) for (bool easyOnly : { false, true })
    {
        const bool inEasy = visibleIn(false, easy, easyOnly), inPro = visibleIn(true, easy, easyOnly);
        CHECK(inEasy == (easy || easyOnly), "Easy shows easy=%d easyOnly=%d wrongly", easy, easyOnly);
        CHECK(inEasy != inPro, "easy=%d easyOnly=%d is in %s", easy, easyOnly, inEasy && inPro ? "both modes" : "neither mode");
    }
    CHECK(!visibleIn(true, true, false), "a knob that Easy shows must not be repeated in Pro");
    CHECK(visibleIn(true, false, false) && !visibleIn(false, false, false), "a plain control is Pro only");
    // the editor uses the rule in both places, and no copy of the old expression is left
    const std::string root = argc > 1 ? argv[1] : ".", ed = ts::readFile(root + "/Source/PluginEditor.cpp");
    CHECK(!ed.empty(), "cannot read PluginEditor.cpp");
    size_t n = 0, pos = 0; while ((pos = ed.find("pagerules::visibleIn(pro,s.easy,s.easyOnly)", pos)) != std::string::npos) { ++n; pos += 10; }
    CHECK(n == 2, "selectTab and layoutPage must both use pagerules::visibleIn (found %zu uses)", n);
    CHECK(ed.find("pro?!s.easyOnly") == std::string::npos, "an old copy of the visibility rule is still in the editor");
    std::printf("page rules: Easy and Pro never overlap, and the editor uses the rule in both places\n");
    return 0;
}
