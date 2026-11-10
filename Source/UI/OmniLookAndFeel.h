#pragma once
// The OmniChoir look: warm black and brass, gold glossy buttons, gold-arc knobs, colour-coded faders.
// Header-only; every string in the UI must stay plain ASCII (juce::String asserts on non-ASCII char literals).
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace omni {

inline const juce::Colour kBg(0xff08060a), kPanel(0xff17101a), kPanel2(0xff271c1e), kStroke(0xff5a4036), kTrack(0xff3a2a26);
inline const juce::Colour kCream(0xfff0e2cc), kMuted(0xffa99485), kGold(0xffe9bb68), kGoldHi(0xfffadc9a), kGoldLo(0xffc98a35), kInk(0xff2a1a0c);
inline const std::array<juce::Colour, 4> kSection { juce::Colour(0xffe58ba0), juce::Colour(0xfff1d27f), juce::Colour(0xff6fc7a8), juce::Colour(0xff7ea4e0) };
inline const juce::Colour kLead(0xfff4eee0);

inline juce::Font boldFont(float height) { return juce::Font(juce::FontOptions(height).withStyle("Bold")); }
inline juce::Font plainFont(float height) { return juce::Font(juce::FontOptions(height)); }
inline juce::Font serifFont(float height) { return juce::Font(juce::FontOptions(height).withName(juce::Font::getDefaultSerifFontName()).withStyle("Bold")); }

// the glossy gold fill used by the primary buttons, the selected tab and the knob accents
inline void fillGold(juce::Graphics& g, juce::Rectangle<float> r, float radius, bool pressed)
{
    const auto hi = pressed ? kGold : kGoldHi, lo = pressed ? kGoldLo.darker(.2f) : kGoldLo;
    g.setGradientFill(juce::ColourGradient(hi, r.getX(), r.getY(), lo, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    juce::Rectangle<float> gloss = r.reduced(2.f, 1.5f); gloss.setHeight(r.getHeight() * .46f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(.34f), gloss.getX(), gloss.getY(),
                                           juce::Colours::white.withAlpha(0.f), gloss.getX(), gloss.getBottom(), false));
    g.fillRoundedRectangle(gloss, radius * .8f);
}

class OmniLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OmniLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, kPanel);
        setColour(juce::PopupMenu::textColourId, kCream);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kGold);
        setColour(juce::PopupMenu::highlightedTextColourId, kInk);
        setColour(juce::ComboBox::textColourId, kCream);
        setColour(juce::ComboBox::backgroundColourId, kPanel2);
        setColour(juce::ComboBox::outlineColourId, kStroke);
        setColour(juce::Label::textColourId, kCream);
        setColour(juce::Slider::textBoxTextColourId, kCream);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TooltipWindow::backgroundColourId, kPanel);
        setColour(juce::TooltipWindow::textColourId, kCream);
        setColour(juce::TooltipWindow::outlineColourId, kGold.withAlpha(.6f));
        setColour(juce::ListBox::backgroundColourId, kPanel);
        setColour(juce::ListBox::outlineColourId, kStroke);
        setColour(juce::ScrollBar::thumbColourId, kStroke);
    }

    // ------------------------------------------------------------------------------------------------ knobs
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const juce::Rectangle<float> area((float) x, (float) y, (float) w, (float) h);
        const float d = juce::jmin(area.getWidth(), area.getHeight()) - 4.f;
        if (d < 14.f) return;
        const auto c = area.getCentre();
        const float r = d * .5f, tr = r - 3.f;
        const auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);
        const bool hot = s.isMouseOverOrDragging();
        const float angle = a0 + pos * (a1 - a0);
        const auto stroke = [](float t) { return juce::PathStrokeType(t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };

        juce::Path track; track.addCentredArc(c.x, c.y, tr, tr, 0.f, a0, a1, true);
        g.setColour(kTrack); g.strokePath(track, stroke(4.f));
        if (pos > .002f)
        {
            juce::Path lit; lit.addCentredArc(c.x, c.y, tr, tr, 0.f, a0, angle, true);
            g.setColour(accent.withAlpha(hot ? .30f : .17f)); g.strokePath(lit, stroke(9.f));      // glow
            g.setColour(accent); g.strokePath(lit, stroke(4.f));
        }
        const float br = r - 9.f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff55413a), c.x - br * .5f, c.y - br * .8f, juce::Colour(0xff130d10), c.x + br * .4f, c.y + br, false));
        g.fillEllipse(c.x - br, c.y - br, br * 2.f, br * 2.f);
        g.setColour(juce::Colours::black.withAlpha(.55f)); g.drawEllipse(c.x - br, c.y - br, br * 2.f, br * 2.f, 1.4f);
        g.setColour(accent.withAlpha(hot ? .55f : .30f)); g.drawEllipse(c.x - br + 1.5f, c.y - br + 1.5f, br * 2.f - 3.f, br * 2.f - 3.f, 1.f);
        const float p0 = br * .30f, p1 = br * .84f;
        g.setColour(kCream);
        g.drawLine(c.x + p0 * std::sin(angle), c.y - p0 * std::cos(angle), c.x + p1 * std::sin(angle), c.y - p1 * std::cos(angle), 2.6f);
    }

    // ---------------------------------------------------------------------------------------------- faders
    int getSliderThumbRadius(juce::Slider&) override { return 8; }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                          juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, sliderPos, minPos, maxPos, style, s); return; }
        const auto accent = s.findColour(juce::Slider::trackColourId);
        const float cx = (float) x + (float) w * .5f, top = (float) y + 8.f, bottom = (float) (y + h) - 8.f;
        const juce::Rectangle<float> slot(cx - 3.f, top, 6.f, juce::jmax(1.f, bottom - top));
        g.setColour(juce::Colour(0xff0c0709)); g.fillRoundedRectangle(slot.expanded(1.f, 0.f), 4.f);
        g.setColour(kStroke.withAlpha(.6f)); g.drawRoundedRectangle(slot.expanded(1.f, 0.f), 4.f, 1.f);
        for (int i = 0; i <= 4; ++i)                                                      // scale ticks
        {
            const float ty = top + (bottom - top) * (float) i / 4.f;
            g.setColour(kMuted.withAlpha(.35f)); g.drawLine(cx + 7.f, ty, cx + 11.f, ty, 1.f);
        }
        if (sliderPos < bottom)
        {
            const juce::Rectangle<float> lit(cx - 3.f, sliderPos, 6.f, bottom - sliderPos);
            g.setColour(accent.withAlpha(.25f)); g.fillRoundedRectangle(lit.expanded(3.f, 0.f), 5.f);   // glow
            g.setGradientFill(juce::ColourGradient(accent.brighter(.35f), cx, sliderPos, accent.darker(.45f), cx, bottom, false));
            g.fillRoundedRectangle(lit, 3.f);
        }
        const juce::Rectangle<float> thumb(cx - 15.f, sliderPos - 7.f, 30.f, 14.f);
        const bool hot = s.isMouseOverOrDragging();
        g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillRoundedRectangle(thumb.translated(0.f, 1.5f), 5.f);
        g.setGradientFill(juce::ColourGradient(hot ? juce::Colour(0xff8a7068) : juce::Colour(0xff6b564e), thumb.getX(), thumb.getY(),
                                               juce::Colour(0xff2a1d1c), thumb.getX(), thumb.getBottom(), false));
        g.fillRoundedRectangle(thumb, 5.f);
        g.setColour(accent); g.drawRoundedRectangle(thumb.reduced(.5f), 5.f, 1.1f);
        g.setColour(kCream); g.drawLine(thumb.getX() + 7.f, thumb.getCentreY(), thumb.getRight() - 7.f, thumb.getCentreY(), 1.6f);
    }

    // --------------------------------------------------------------------------------------------- buttons
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override { return boldFont((float) juce::jlimit(9, 13, (int) ((float) buttonHeight * .40f))); }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto id = b.getComponentID();
        const bool on = b.getToggleState(), enabled = b.isEnabled();
        auto r = b.getLocalBounds().toFloat().reduced(3.f, 2.5f);
        if (down) r = r.translated(0.f, 1.f);
        const float radius = id == "tab" ? 9.f : r.getHeight() * .5f;
        const bool gold = (id == "tab" || id == "power") && on;
        if (gold)
        {
            g.setColour(kGold.withAlpha(.20f)); g.fillRoundedRectangle(r.expanded(2.5f, 2.f), radius + 2.f);   // glow
            fillGold(g, r, radius, down);
        }
        else
        {
            g.setColour(juce::Colours::black.withAlpha(.35f)); g.fillRoundedRectangle(r.translated(0.f, 1.5f), radius);
            g.setGradientFill(juce::ColourGradient(over ? juce::Colour(0xff3a2a2a) : kPanel2, r.getX(), r.getY(), kPanel, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r, radius);
            g.setColour((id == "tab" ? kStroke : kGold).withAlpha(enabled ? (over ? .85f : .5f) : .2f));
            g.drawRoundedRectangle(r.reduced(.5f), radius, 1.1f);
        }
        if (id == "power")                                                                // the power symbol
        {
            const float cx = r.getX() + 19.f, cy = r.getCentreY(), rad = 6.5f;
            juce::Path arc; arc.addCentredArc(cx, cy, rad, rad, 0.f, .6f, 6.28318f - .6f, true);
            g.setColour(on ? kInk : kMuted);
            g.strokePath(arc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine(cx, cy - rad - 1.f, cx, cy - 1.f, 2.2f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto id = b.getComponentID();
        const bool on = b.getToggleState(), enabled = b.isEnabled();
        const bool gold = (id == "tab" || id == "power") && on;
        g.setColour(gold ? kInk : (!enabled ? kMuted.withAlpha(.45f) : (id == "action" ? kGoldHi : kCream)));
        g.setFont(getTextButtonFont(b, b.getHeight()));
        auto r = b.getLocalBounds().reduced(8, 0);
        if (id == "power") r.removeFromLeft(26);
        g.drawFittedText(b.getButtonText(), r, juce::Justification::centred, 1);
    }

    // -------------------------------------------------------------------------------------------- dropdowns
    juce::Font getComboBoxFont(juce::ComboBox&) override { return boldFont(11.f); }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(10, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<float>(0.f, 0.f, (float) width, (float) height).reduced(1.f);
        const bool hot = box.isMouseOver(true);
        g.setGradientFill(juce::ColourGradient(kPanel2, 0.f, r.getY(), kPanel, 0.f, r.getBottom(), false));
        g.fillRoundedRectangle(r, 8.f);
        g.setColour(kGold.withAlpha(hot ? .85f : .5f)); g.drawRoundedRectangle(r.reduced(.5f), 8.f, 1.1f);
        const float cx = r.getRight() - 13.f, cy = r.getCentreY();
        juce::Path arrow; arrow.addTriangle(cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
        g.setColour(kGold); g.fillPath(arrow);
    }
};

} // namespace omni
