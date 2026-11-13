// The title and the skull always fit in the header, from the minimum window width up. JUCE-free.
#include "../Source/UI/TitleLayout.h"
#include "test_support.h"

int main()
{
    // header controls: 126 + 8 + 160 + 8 + 124 + 6 + 78 wide, 14 margin each side (PluginEditor.cpp resized()); the minimum window is 800 wide
    const float controls = 126 + 8 + 160 + 8 + 124 + 6 + 78;
    long cases = 0; float smallestFont = 99;
    for (int window = 800; window <= 2000; window += 10)
    {
        const float controlsLeft = (float) window - 14.f - controls;
        for (float titleWidth = 120.f; titleWidth <= 360.f; titleWidth += 5.f)   // SKELLYCHOIR in a bold serif at 31 pt is roughly 230-290 px: the range is far wider than that
        {
            const auto r = titlelayout::compute(titleWidth, controlsLeft);
            const float skullRight = r.skullX + r.skullSize;
            CHECK(skullRight <= controlsLeft - titlelayout::kClear + 0.01f, "window %d, title %.0f px: the skull ends at %.1f but the controls start at %.1f", window, titleWidth, skullRight, controlsLeft);
            CHECK(r.skullX >= titlelayout::kLeft + r.titleWidth + titlelayout::kGap - .01f, "the skull overlaps the title");
            CHECK(r.skullSize >= titlelayout::kMinSkull - .01f && r.skullSize <= titlelayout::kSkull + .01f, "skull size %.1f out of range", r.skullSize);
            CHECK(r.fontHeight >= titlelayout::kMinFont - .01f && r.fontHeight <= titlelayout::kBaseFont + .01f, "window %d, title %.0f px: font %.1f is outside %.0f..%.0f", window, titleWidth, r.fontHeight, titlelayout::kMinFont, titlelayout::kBaseFont);
            if (window >= 860 && titleWidth <= 270.f) CHECK(r.fontHeight >= titlelayout::kBaseFont - .01f || r.skullSize <= titlelayout::kSkull, "needless shrinking");
            smallestFont = std::min(smallestFont, r.fontHeight); ++cases;
        }
    }
    // with room to spare nothing shrinks
    const auto roomy = titlelayout::compute(250.f, 900.f);
    CHECK(roomy.fontHeight == titlelayout::kBaseFont && roomy.skullSize == titlelayout::kSkull, "a roomy header must keep the full-size title and skull");
    // an absurdly wide title (a different font, a much longer name) bottoms out at a readable size instead of vanishing
    const auto extreme = titlelayout::compute(2000.f, 276.f);
    CHECK(extreme.fontHeight >= 14.f && extreme.skullSize == titlelayout::kMinSkull, "the last-resort floor: font %.1f, skull %.1f", extreme.fontHeight, extreme.skullSize);
    std::printf("title layout: %ld width combinations fit; the smallest title font used is %.1f pt\n", cases, smallestFont);
    return 0;
}
