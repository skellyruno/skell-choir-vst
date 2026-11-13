// Themes: the default is bit-for-bit the original gold, every theme is readable, and the saved choice round-trips. JUCE-free.
// Usage: theme_tests <project root>
#include "../Source/Theme.h"
#include "test_support.h"
#include <regex>
#include <set>

using theme::Palette;

static double redmean(uint32_t a, uint32_t b)   // a cheap perceptual distance between two colours
{
    const double r1 = (a >> 16) & 255, g1 = (a >> 8) & 255, b1 = a & 255, r2 = (b >> 16) & 255, g2 = (b >> 8) & 255, b2 = b & 255, rm = (r1 + r2) / 2;
    return std::sqrt((2 + rm / 256) * (r1 - r2) * (r1 - r2) + 4 * (g1 - g2) * (g1 - g2) + (2 + (255 - rm) / 256) * (b1 - b2) * (b1 - b2));
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";

    // 1. GOLDEN: theme 0 is exactly what the plugin shipped with. These literals are a frozen copy of the old OmniLookAndFeel.h / PluginEditor.cpp / look-and-feel values.
    {
        const Palette& g = theme::get(0).p;
        CHECK(std::string(theme::get(0).name) == "Gold" && theme::kDefaultTheme == 0, "theme 0 must be Gold");
        struct Pair { const char* what; uint32_t now, shipped; };
        const Pair golden[] = {
            {"kBg", g.bg, 0xff08060a}, {"kPanel", g.panel, 0xff17101a}, {"kPanel2", g.panel2, 0xff271c1e}, {"kStroke", g.stroke, 0xff5a4036}, {"kTrack", g.track, 0xff3a2a26},
            {"kCream", g.cream, 0xfff0e2cc}, {"kMuted", g.muted, 0xffa99485}, {"kGold", g.gold, 0xffe9bb68}, {"kGoldHi", g.goldHi, 0xfffadc9a}, {"kGoldLo", g.goldLo, 0xffc98a35},
            {"kInk", g.ink, 0xff2a1a0c}, {"kLead", g.lead, 0xfff4eee0},
            {"kSection[0]", g.section[0], 0xffe58ba0}, {"kSection[1]", g.section[1], 0xfff1d27f}, {"kSection[2]", g.section[2], 0xff6fc7a8}, {"kSection[3]", g.section[3], 0xff7ea4e0},
            {"knob top", g.knobHi, 0xff55413a}, {"knob bottom", g.knobLo, 0xff130d10}, {"fader slot", g.slot, 0xff0c0709}, {"fader thumb", g.thumb, 0xff6b564e},
            {"fader thumb hot", g.thumbHot, 0xff8a7068}, {"fader thumb bottom", g.thumbLo, 0xff2a1d1c}, {"button hover", g.comboHover, 0xff3a2a2a},
            {"list selection", g.listSel, 0xff6b4a2c}, {"piano roll", g.rollBg, 0xff0e090c}, {"header glow", g.headerTop, 0xff2f1b15}, {"stage corners", g.vignette, 0xff040205},
            {"window light", g.light, 0xffffe6a8}, {"dust", g.dust, 0xffffeec0}, {"lead figure", g.leadFigure, 0xffeee8d9}, {"mute badge", g.muteBadge, 0xff8c2f3a},
            {"record dot", g.record, 0xffe0485a}, {"stage tint (none)", g.stageTint, 0x00000000} };
        for (const auto& p : golden) CHECK(p.now == p.shipped, "Gold '%s' is 0x%08x, the shipped value was 0x%08x: the default look must not change", p.what, (unsigned) p.now, (unsigned) p.shipped);
        std::printf("[1] Gold is bit-identical to the shipped look (%zu colours compared)\n", sizeof(golden) / sizeof(golden[0]));
    }

    // 2. every theme is complete and readable ------------------------------------------------------------------------------
    std::set<std::string> names;
    for (int i = 0; i < theme::kNumThemes; ++i)
    {
        const auto& t = theme::get(i); const Palette& p = t.p; const std::string n = t.name;
        CHECK(names.insert(n).second, "duplicate theme name %s", n.c_str());
        const uint32_t opaque[] = { p.bg, p.panel, p.panel2, p.stroke, p.track, p.cream, p.muted, p.gold, p.goldHi, p.goldLo, p.ink, p.lead, p.section[0], p.section[1], p.section[2], p.section[3],
                                    p.knobHi, p.knobLo, p.slot, p.thumb, p.thumbHot, p.thumbLo, p.comboHover, p.listSel, p.rollBg, p.headerTop, p.vignette, p.light, p.dust, p.leadFigure, p.muteBadge, p.record };
        for (uint32_t c : opaque) CHECK((c >> 24) == 0xff, "%s: a colour is not opaque (0x%08x)", n.c_str(), (unsigned) c);

        // text on the surfaces it is drawn on: at least 4.5:1 (WCAG AA)
        for (uint32_t surface : { p.bg, p.panel, p.panel2 })
        {
            CHECK(theme::contrast(p.cream, surface) >= 4.5, "%s: cream text on 0x%06x is %.2f:1", n.c_str(), (unsigned) (surface & 0xffffff), theme::contrast(p.cream, surface));
            CHECK(theme::contrast(p.muted, surface) >= 4.5, "%s: muted labels on 0x%06x are %.2f:1", n.c_str(), (unsigned) (surface & 0xffffff), theme::contrast(p.muted, surface));
        }
        // the ink on the accent gradient (buttons, the selected tab, the highlighted menu row), including the white gloss the fill paints on top
        for (uint32_t accent : { p.gold, p.goldHi, p.goldLo, theme::blend(0xffffffff, p.goldHi, .34), theme::blend(0xffffffff, p.gold, .34) })
            CHECK(theme::contrast(p.ink, accent) >= 4.5, "%s: ink on accent 0x%06x is %.2f:1", n.c_str(), (unsigned) (accent & 0xffffff), theme::contrast(p.ink, accent));
        // the accent as a line, a ring or a knob arc on the dark surfaces: at least 3:1 (WCAG non-text)
        for (uint32_t accent : { p.gold, p.goldHi, p.goldLo })
            for (uint32_t surface : { p.bg, p.panel, p.panel2 })
                CHECK(theme::contrast(accent, surface) >= 3.0, "%s: accent 0x%06x on 0x%06x is %.2f:1", n.c_str(), (unsigned) (accent & 0xffffff), (unsigned) (surface & 0xffffff), theme::contrast(accent, surface));
        // the four sections and the lead can be told apart, and each shows on the dark surfaces
        const uint32_t voices[] = { p.section[0], p.section[1], p.section[2], p.section[3], p.lead };
        for (size_t a = 0; a < 5; ++a)
        {
            CHECK(theme::contrast(voices[a], p.bg) >= 3.0 && theme::contrast(voices[a], p.panel) >= 3.0, "%s: voice colour %zu is hard to see (%.2f:1 on the background)", n.c_str(), a, theme::contrast(voices[a], p.bg));
            for (size_t b = a + 1; b < 5; ++b) CHECK(redmean(voices[a], voices[b]) >= 100.0, "%s: voice colours %zu and %zu are too alike (%.0f)", n.c_str(), a, b, redmean(voices[a], voices[b]));
        }
        CHECK(theme::contrast(p.stroke, p.panel) >= 1.5, "%s: outlines vanish against the panel (%.2f:1)", n.c_str(), theme::contrast(p.stroke, p.panel));
        CHECK(theme::contrast(p.track, p.panel) >= 1.15, "%s: knob tracks vanish against the panel", n.c_str());
        // the stage tint: none for gold, otherwise visible but gentle
        const double a = (double) (p.stageTint >> 24) / 255.0;
        if (i == 0) CHECK(p.stageTint >> 24 == 0, "Gold must not tint the stage picture");
        else CHECK(a >= 0.15 && a <= 0.45, "%s: stage tint alpha %.2f should be 0.15..0.45", n.c_str(), a);
    }
    for (int a = 0; a < theme::kNumThemes; ++a) for (int b = a + 1; b < theme::kNumThemes; ++b)
        CHECK(redmean(theme::get(a).p.gold, theme::get(b).p.gold) >= 100.0, "themes %d and %d have accents that are too alike", a, b);
    std::printf("[2] %d themes: text >= 4.5:1, ink on the accent gradient >= 4.5:1, accents >= 3:1, voices distinct, tints gentle\n", theme::kNumThemes);

    // 3. the saved choice ---------------------------------------------------------------------------------------------------
    {
        for (int i = 0; i < theme::kNumThemes; ++i)
        {
            CHECK(theme::fromFileText(theme::toFileText(i)) == i, "theme %d does not survive a save and load", i);
            std::string shouted = theme::toFileText(i); for (auto& c : shouted) c = (char) std::toupper((unsigned char) c);
            CHECK(theme::fromFileText("  " + shouted + "\r\n") == i, "theme %d is not read back from upper case with CRLF and spaces", i);
        }
        CHECK(theme::fromFileText("") == 0 && theme::fromFileText("Purple Haze") == 0 && theme::fromFileText("2") == 0 && theme::fromFileText("\xff\xfe\x01") == 0, "a missing or unknown choice must be Gold");
        CHECK(theme::clampIndex(-1) == 0 && theme::clampIndex(99) == 0 && theme::clampIndex(2) == 2, "clampIndex");
        std::printf("[3] theme.txt: names round-trip; empty, unknown or corrupt files give Gold\n");
    }

    // 4. no colour is hard-coded in the UI any more: all of it comes from Theme.h -----------------------------------------
    {
        const std::regex literal("0x[fF][fF][0-9a-fA-F]{6}");
        int checkedFiles = 0;
        for (const char* f : { "Source/PluginEditor.cpp", "Source/UI/OmniLookAndFeel.h", "tools/screenshots.cpp" })
        {
            const std::string text = ts::readFile(root + "/" + f); CHECK(!text.empty(), "cannot read %s", f);
            std::istringstream lines(text); std::string line; int n = 0;
            while (std::getline(lines, line)) { ++n; std::smatch m; CHECK(!std::regex_search(line, m, literal), "%s:%d hard-codes a colour (%s): put it in Source/Theme.h so themes can change it", f, n, m.str().c_str()); }
            ++checkedFiles;
        }
        const std::string lnf = ts::readFile(root + "/Source/UI/OmniLookAndFeel.h");
        CHECK(lnf.find("applyTheme") != std::string::npos && lnf.find("theme::get") != std::string::npos, "OmniLookAndFeel.h must build its colours from Theme.h");
        std::printf("[4] no hard-coded colours left in %d UI files\n", checkedFiles);
    }
    std::printf("themes: all checks passed\n");
    return 0;
}
