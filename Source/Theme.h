#pragma once
// The colour themes, as plain data (JUCE-free, so tests/theme_tests.cpp can check contrast and the default's exactness natively).
// Colours are 0xAARRGGBB. Theme 0 is GOLD, the original look: every value of it is exactly the literal the plugin shipped with before
// themes existed, and tests/theme_tests.cpp compares it against a copy of those literals.
//
// UI/OmniLookAndFeel.h turns a Palette into juce::Colour globals (omni::applyTheme). Nothing else decides a colour.
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

namespace theme {

struct Palette
{
    uint32_t bg, panel, panel2, stroke, track, cream, muted;     // surfaces, strokes and text
    uint32_t gold, goldHi, goldLo, ink;                           // the accent (the "gold" of the gold theme), its glossy gradient, and the text on it
    uint32_t lead;                                                // the lead voice's colour
    std::array<uint32_t, 4> section;                              // soprano, alto, tenor, bass
    uint32_t knobHi, knobLo, slot, thumb, thumbHot, thumbLo,      // knob body, fader slot and fader thumb
             comboHover, listSel, rollBg;                         // hovered button, selected list row, piano-roll background
    uint32_t headerTop, vignette, light, dust, leadFigure;        // header glow, stage corners, window light, dust motes, the lead's figure
    uint32_t muteBadge, record;                                   // MUTE badge, LISTEN recording dot (the same in every theme: they mean something)
    uint32_t stageTint;                                           // laid over the stage picture. Alpha 0 = none (gold leaves the photo untouched)
};

struct Theme { const char* name; Palette p; };

constexpr int kNumThemes = 4;
constexpr int kDefaultTheme = 0;

inline const std::array<Theme, kNumThemes>& all()
{
    static const std::array<Theme, kNumThemes> themes {{
        { "Gold", {
            0xff08060a, 0xff17101a, 0xff271c1e, 0xff5a4036, 0xff3a2a26, 0xfff0e2cc, 0xffa99485,
            0xffe9bb68, 0xfffadc9a, 0xffc98a35, 0xff2a1a0c, 0xfff4eee0,
            { 0xffe58ba0, 0xfff1d27f, 0xff6fc7a8, 0xff7ea4e0 },
            0xff55413a, 0xff130d10, 0xff0c0709, 0xff6b564e, 0xff8a7068, 0xff2a1d1c, 0xff3a2a2a, 0xff6b4a2c, 0xff0e090c,
            0xff2f1b15, 0xff040205, 0xffffe6a8, 0xffffeec0, 0xffeee8d9, 0xff8c2f3a, 0xffe0485a, 0x00000000 } },
        { "Blood Red", {
            0xff0a0606, 0xff1a1010, 0xff271c1c, 0xff5a3637, 0xff3a2627, 0xffebd1d2, 0xffa98586,
            0xffee4b57, 0xffff8a8f, 0xffe5444f, 0xff1c0508, 0xfff4eee0,
            { 0xffe58ba0, 0xfff1d27f, 0xff6fc7a8, 0xff7ea4e0 },
            0xff553a3b, 0xff130d0d, 0xff0c0707, 0xff6b4e4f, 0xff8a6869, 0xff2a1c1c, 0xff3a2a2a, 0xff6b2c2e, 0xff0e0909,
            0xff2f1516, 0xff050202, 0xffffb0a8, 0xffffc8c0, 0xffeee8d9, 0xff8c2f3a, 0xffe0485a, 0x4d8a0a14 } },
        { "Neon Green", {
            0xff060a07, 0xff101a13, 0xff1b281f, 0xff345c40, 0xff253b2c, 0xffd1ebd9, 0xff83ab8f,
            0xff39ff88, 0xff9dffc4, 0xff1fd36a, 0xff03140a, 0xfff4eee0,
            { 0xffe58ba0, 0xfff1d27f, 0xff5fd0e0, 0xff9c8ce8 },
            0xff395642, 0xff0d130f, 0xff070c08, 0xff4d6c56, 0xff668c72, 0xff1b2b20, 0xff293b2f, 0xff296e3e, 0xff090e0a,
            0xff14301d, 0xff020503, 0xffb8ffd0, 0xffd0ffe0, 0xffe3f0e6, 0xff8c2f3a, 0xffe0485a, 0x47087a38 } },
        { "Ice Blue", {
            0xff06080a, 0xff10161a, 0xff1c2227, 0xff364a5a, 0xff26313a, 0xffd1dfeb, 0xff8599a9,
            0xff6ec6ff, 0xffb5e4ff, 0xff3f9ee0, 0xff06131f, 0xfff4eee0,
            { 0xffe58ba0, 0xfff1d27f, 0xff6fc7a8, 0xff9c8ce8 },
            0xff3a4955, 0xff0d1013, 0xff070a0c, 0xff4e5e6b, 0xff687b8a, 0xff1c242a, 0xff2a333a, 0xff2c4f6b, 0xff090c0e,
            0xff15232f, 0xff020405, 0xffb8e0ff, 0xffd8eeff, 0xffe3ecf5, 0xff8c2f3a, 0xffe0485a, 0x47144f8a } },
    }};
    return themes;
}

inline int clampIndex(int i) { return i < 0 || i >= kNumThemes ? kDefaultTheme : i; }
inline const Theme& get(int i) { return all()[(size_t) clampIndex(i)]; }

// ---- contrast (WCAG 2.x relative luminance); alpha is ignored, colours are treated as opaque
inline double channel(uint32_t colour, int shift)
{
    const double c = (double) ((colour >> shift) & 0xff) / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
inline double luminance(uint32_t c) { return 0.2126 * channel(c, 16) + 0.7152 * channel(c, 8) + 0.0722 * channel(c, 0); }
inline double contrast(uint32_t a, uint32_t b)
{
    double la = luminance(a), lb = luminance(b); if (la < lb) std::swap(la, lb);
    return (la + 0.05) / (lb + 0.05);
}
// `over` drawn on `under` at `alpha` (0..1), opaque result
inline uint32_t blend(uint32_t over, uint32_t under, double alpha)
{
    uint32_t out = 0xff000000;
    for (int shift : { 16, 8, 0 })
    {
        const double o = (double) ((over >> shift) & 0xff), u = (double) ((under >> shift) & 0xff);
        out |= (uint32_t) std::lround(o * alpha + u * (1.0 - alpha)) << shift;
    }
    return out;
}

// ---- the remembered choice: theme.txt holds the theme's NAME, so reordering the list never changes anyone's colours
inline std::string toFileText(int index) { return std::string(get(index).name) + "\n"; }
inline int fromFileText(const std::string& text)
{
    std::string t;
    for (char c : text) if (c != '\r' && c != '\n' && c != '\t') t += c;
    while (!t.empty() && t.back() == ' ') t.pop_back();
    while (!t.empty() && t.front() == ' ') t.erase(t.begin());
    const auto lower = [](std::string s) { for (auto& c : s) c = (char) std::tolower((unsigned char) c); return s; };
    for (int i = 0; i < kNumThemes; ++i) if (lower(t) == lower(get(i).name)) return i;
    return kDefaultTheme;   // a missing, empty or unrecognised file is the original look
}

} // namespace theme
