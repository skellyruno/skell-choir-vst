#pragma once
// Where the title text and the skull theme button go in the header (JUCE-free, so the fit can be tested natively).
// The editor measures the title's width at the base font size and tells us where the header controls start; we shrink the title
// (never the other way round) so that  title + gap + skull  always ends before the controls, at any window width.
#include <algorithm>
#include <cmath>

namespace titlelayout {

constexpr float kLeft = 14.f;            // the title's left edge
constexpr float kBaseFont = 31.f;        // the title's size when there is room
constexpr float kMinFont = 18.f;
constexpr float kGap = 8.f;              // title text to skull
constexpr float kSkull = 30.f, kMinSkull = 22.f;
constexpr float kClear = 10.f;           // always left free before the first header control

struct Result { float fontHeight, titleWidth, skullSize, skullX; };

inline Result compute(float titleWidthAtBase, float controlsLeft)
{
    const float avail = std::max(0.f, controlsLeft - kLeft - kClear);
    const float w = std::max(1.f, titleWidthAtBase);
    float skull = kSkull;
    float k = std::min(1.f, (avail - kGap - skull) / w);
    if (k * kBaseFont < 24.f) { skull = kMinSkull; k = std::min(1.f, (avail - kGap - skull) / w); }   // a small title before a small skull
    k = std::max(k, kMinFont / kBaseFont * .8f);   // never below ~14 pt, whatever happens
    const float width = w * k;
    return { kBaseFont * k, width, skull, kLeft + width + kGap };
}

} // namespace titlelayout
