#include "PluginEditor.h"
#include <array>
#include <cmath>

namespace {
const juce::Colour bg(0xff0b1018), panel(0xff17222b), stroke(0xff35505b);
const juce::Colour cream(0xffe9e5d8), muted(0xff98aab6), brass(0xffe0b877);
const std::array<juce::Colour, 4> sectionColours {
    juce::Colour(0xffe88d9d), juce::Colour(0xffe7b76f),
    juce::Colour(0xff74cfb0), juce::Colour(0xff86b7eb)
};
const char* sectionNames[] = { "SOPRANOS", "ALTOS", "TENORS", "BASSES" };

class Dial final : public juce::Component {
public:
    Dial(OmniChoirAudioProcessor& p, const juce::String& title, const juce::String& id)
        : attachment(p.parameters, id, dial) {
        name.setText(title, juce::dontSendNotification);
        name.setJustificationType(juce::Justification::centred);
        name.setColour(juce::Label::textColourId, muted);
        name.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        dial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        dial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 92, 18);
        dial.setColour(juce::Slider::rotarySliderFillColourId, brass);
        dial.setColour(juce::Slider::rotarySliderOutlineColourId, stroke);
        dial.setColour(juce::Slider::thumbColourId, cream);
        dial.setColour(juce::Slider::textBoxTextColourId, cream);
        dial.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        dial.setColour(juce::Slider::textBoxBackgroundColourId, panel);
        dial.setDoubleClickReturnValue(true, p.parameters.getParameter(id)->convertFrom0to1(p.parameters.getParameter(id)->getDefaultValue()));
        dial.setTooltip(title);
        addAndMakeVisible(name); addAndMakeVisible(dial);
    }
    void resized() override { name.setBounds(0, 1, getWidth(), 20); dial.setBounds(4, 20, getWidth()-8, getHeight()-20); }
private:
    juce::Label name;
    juce::Slider dial;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};
class Choice final : public juce::Component {
public:
    Choice(OmniChoirAudioProcessor& p, const juce::String& title, const juce::String& id) {
        name.setText(title, juce::dontSendNotification);
        name.setJustificationType(juce::Justification::centred);
        name.setColour(juce::Label::textColourId, muted);
        name.setFont(juce::Font(juce::FontOptions(12.f).withStyle("Bold")));
        combo.addItemList(p.parameters.getParameter(id)->getAllValueStrings(), 1);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, id, combo);
        combo.setColour(juce::ComboBox::backgroundColourId, panel);
        combo.setColour(juce::ComboBox::textColourId, cream);
        combo.setColour(juce::ComboBox::outlineColourId, stroke);
        combo.setColour(juce::ComboBox::arrowColourId, brass);
        addAndMakeVisible(name); addAndMakeVisible(combo);
    }
    void resized() override { name.setBounds(0, 6, getWidth(), 24); combo.setBounds(6, 44, getWidth()-12, 30); }
private:
    juce::Label name;
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
}

StageView::StageView(OmniChoirAudioProcessor& p) : processor(p) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    startTimerHz(25);
}
juce::Rectangle<float> StageView::plot() const { return getLocalBounds().toFloat().reduced(24.f, 26.f); }
juce::Point<float> StageView::worldToPoint(float x, float y) const {
    const auto r = plot();
    // Coordinates are metres: X horizontal and Y away from the microphones.
    return { r.getCentreX() + x*r.getWidth()/21.f,
             r.getBottom()-20.f - (y+2.f)/14.f*(r.getHeight()-48.f) };
}
juce::Point<float> StageView::pointToWorld(juce::Point<float> point) const {
    const auto r = plot();
    return { (point.x-r.getCentreX())*21.f/r.getWidth(),
            (r.getBottom()-20.f-point.y)*14.f/(r.getHeight()-48.f)-2.f };
}
float StageView::value(const juce::String& id) const {
    return processor.parameters.getRawParameterValue(id)->load(std::memory_order_relaxed);
}
void StageView::change(const juce::String& id, float v) {
    if (auto* param = processor.parameters.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(v));
}
void StageView::startGesture(int index) {
    if (index == -2) return;
    const auto suffix = index == -1 ? juce::String() : juce::String(index);
    const auto a = index == -1 ? "micX" : "x"+suffix;
    const auto b = index == -1 ? "micY" : "y"+suffix;
    processor.parameters.getParameter(a)->beginChangeGesture();
    processor.parameters.getParameter(b)->beginChangeGesture();
}
void StageView::finishGesture(int index) {
    if (index == -2) return;
    const auto suffix = index == -1 ? juce::String() : juce::String(index);
    const auto a = index == -1 ? "micX" : "x"+suffix;
    const auto b = index == -1 ? "micY" : "y"+suffix;
    processor.parameters.getParameter(a)->endChangeGesture();
    processor.parameters.getParameter(b)->endChangeGesture();
}
void StageView::paint(juce::Graphics& g) {
    const auto full = getLocalBounds().toFloat();
    g.setColour(panel); g.fillRoundedRectangle(full, 17);
    g.setColour(stroke.withAlpha(.8f)); g.drawRoundedRectangle(full.reduced(1), 17, 1);
    auto r = plot();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff23323c), 0, r.getY(),
                                          juce::Colour(0xff0e181d), 0, r.getBottom(), false));
    g.fillRoundedRectangle(r, 10);
    g.setColour(cream.withAlpha(.07f));
    for (int i = -8; i <= 8; i += 2) {
        const auto a = worldToPoint((float)i,-2), b = worldToPoint((float)i,12);
        g.drawLine(a.x, a.y, b.x,b.y, .8f);
    }
    for (int j = 0; j <= 12; j += 2) {
        auto a = worldToPoint(-10,(float)j), b = worldToPoint(10,(float)j);
        g.drawLine(a.x,a.y,b.x,b.y,.8f);
    }
    g.setColour(muted.withAlpha(.8f));
    g.setFont(juce::Font(juce::FontOptions(12).withStyle("Bold")));
    g.drawText("UPSTAGE  /  DISTANCE IN METRES", (int)r.getX()+16, (int)r.getY()+8, 290, 20, juce::Justification::left);
    g.drawText("DRAG A SECTION OR THE LISTENER", (int)r.getRight()-320, (int)r.getY()+8, 300, 20, juce::Justification::right);
    for (int s = 0; s < 4; ++s) {
        const float x = value("x"+juce::String(s)), y = value("y"+juce::String(s));
        const auto center = worldToPoint(x,y);
        const auto color = sectionColours[(size_t)s];
        g.setColour(color.withAlpha(.07f));
        g.fillEllipse(center.x-58,center.y-39,116,84);
        g.setColour(color.withAlpha(.3f));
        g.drawEllipse(center.x-58,center.y-39,116,84,1.0f);
        // Stylized 16-person section; positions are listening geometry, not individual synth keys.
        for (int row=3; row>=0; --row) for (int col=0; col<4; ++col) {
            const auto pt = worldToPoint(x + (col-1.5f)*.49f + (row%2)*.16f,
                                         y + (row-1.5f)*.49f);
            const float rad = 3.5f + (3-row)*.45f;
            g.setColour(color.withAlpha(.83f)); g.fillEllipse(pt.x-rad,pt.y-rad,rad*2,rad*2);
            g.setColour(juce::Colours::white.withAlpha(.24f)); g.fillEllipse(pt.x-rad*.45f,pt.y-rad*.5f,rad*.7f,rad*.7f);
        }
        g.setFont(juce::Font(juce::FontOptions(13.f).withStyle("Bold")));
        g.setColour(color);
        g.drawText(sectionNames[s], (int)(center.x-70), (int)(center.y+29),140,21,juce::Justification::centred);
    }
    const auto mic = worldToPoint(value("micX"), value("micY"));
    g.setColour(brass.withAlpha(.18f)); g.fillEllipse(mic.x-29,mic.y-29,58,58);
    g.setColour(brass); g.drawEllipse(mic.x-14,mic.y-14,28,28,2);
    g.drawLine(mic.x-22,mic.y,mic.x+22,mic.y,1.3f);
    g.drawLine(mic.x,mic.y-22,mic.x,mic.y+22,1.3f);
    g.setFont(juce::Font(juce::FontOptions(12).withStyle("Bold")));
    g.drawText("YOU / MIC",(int)mic.x-50,(int)mic.y+21,100,20,juce::Justification::centred);
    g.setColour(muted);
    g.drawText(juce::String(processor.activeVoiceCount.load(std::memory_order_relaxed)) + " ACTIVE VOICES",
               (int)r.getRight()-170, (int)r.getBottom()-25,154,18,juce::Justification::right);
}
void StageView::mouseDown(const juce::MouseEvent& e) {
    dragging = -2;
    float closest = 43.f;
    const auto pointer = e.position;
    const auto mic = worldToPoint(value("micX"), value("micY"));
    if (pointer.getDistanceFrom(mic) < closest) { closest = pointer.getDistanceFrom(mic); dragging = -1; }
    for (int i=0;i<4;++i) {
        const auto center = worldToPoint(value("x"+juce::String(i)),value("y"+juce::String(i)));
        const float d = pointer.getDistanceFrom(center);
        if (d < closest) { closest = d; dragging = i; }
    }
    if (dragging != -2) { startGesture(dragging); mouseDrag(e); }
}
void StageView::mouseDrag(const juce::MouseEvent& e) {
    if (dragging == -2) return;
    const auto pt = pointToWorld(e.position);
    const bool mic = dragging == -1;
    const auto suffix = juce::String(dragging);
    change(mic ? "micX" : "x"+suffix, juce::jlimit(-9.f,9.f,pt.x));
    change(mic ? "micY" : "y"+suffix, mic ? juce::jlimit(-8.f,8.f,pt.y) : juce::jlimit(.7f,11.f,pt.y));
}
void StageView::mouseUp(const juce::MouseEvent&) { finishGesture(dragging); dragging = -2; }
void StageView::mouseDoubleClick(const juce::MouseEvent& e) {
    // Double-click the microphone to reset its position.
    if (e.position.getDistanceFrom(worldToPoint(value("micX"),value("micY"))) < 40.f) {
        startGesture(-1); change("micX",0); change("micY",0); finishGesture(-1);
    }
}

OmniChoirAudioProcessorEditor::OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), stage(p) {
    setSize(1120, 755);
    setResizable(true, true);
    setResizeLimits(880,660,1650,1100);
    addAndMakeVisible(stage);
    // First control row: performance and section balance.
    addChoice("VOWEL", "vowel");
    addChoice("MIC ARRAY", "micMode");
    addDial("SOPRANO", "gain0"); addDial("ALTO", "gain1");
    addDial("TENOR", "gain2"); addDial("BASS", "gain3");
    addDial("ATTACK", "attack"); addDial("RELEASE", "release");
    addDial("ENSEMBLE", "ensemble");
    // Second row: spatial and room controls.
    addDial("BREATH", "breath"); addDial("MIC WIDTH", "width");
    addDial("ROOM SIZE", "roomSize"); addDial("RT60", "roomSeconds");
    addDial("ABSORPTION", "absorption"); addDial("DIFFUSION", "diffusion");
    addDial("ROOM LEVEL", "wet"); addDial("OUTPUT", "master");
}
void OmniChoirAudioProcessorEditor::addDial(const juce::String& text, const juce::String& id) {
    auto comp = std::make_unique<Dial>(processor,text,id);
    addAndMakeVisible(*comp); controls.push_back(std::move(comp));
}
void OmniChoirAudioProcessorEditor::addChoice(const juce::String& text, const juce::String& id) {
    auto comp = std::make_unique<Choice>(processor,text,id);
    addAndMakeVisible(*comp); controls.push_back(std::move(comp));
}
void OmniChoirAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(bg);
    auto header = getLocalBounds().removeFromTop(78);
    g.setColour(brass);
    g.setFont(juce::Font(juce::FontOptions(31.f).withStyle("Bold")));
    g.drawText("OMNICHOIR", header.removeFromLeft(270).translated(22,5), juce::Justification::centredLeft);
    g.setColour(cream);
    g.setFont(juce::Font(juce::FontOptions(14.f).withStyle("Bold")));
    g.drawText("SPATIAL CHOIR INSTRUMENT", 298,25,300,22,juce::Justification::left);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(12.f)));
    g.drawText("CH 1: SMART SATB  •  CH 2–5: S / A / T / B  •  DRAG SINGERS TO RESHAPE THE SOUND",
               300,47,getWidth()-320,18,juce::Justification::left);
    g.setColour(stroke);
    g.drawLine(22,77,(float)getWidth()-22,77,1.f);
    g.setColour(brass);
    g.setFont(juce::Font(juce::FontOptions(12.f).withStyle("Bold")));
    g.drawText("VOICE / SECTION",24,getHeight()-252,280,19,juce::Justification::left);
    g.drawText("MICROPHONES / ROOM",24,getHeight()-126,280,19,juce::Justification::left);
}
void OmniChoirAudioProcessorEditor::resized() {
    const int margin = 20;
    const int footer = 255;
    stage.setBounds(margin, 90, getWidth()-margin*2, getHeight()-90-footer);
    const int cellWidth = (getWidth()-margin*2)/9;
    const int topY = getHeight()-229;
    const int bottomY = getHeight()-105;
    for (int i=0;i<(int)controls.size();++i) {
        const bool second = i >= 9;
        const int col = second ? i-9 : i;
        const int x = margin + col*cellWidth + (second ? cellWidth/2 : 0);
        controls[(size_t)i]->setBounds(x, second ? bottomY : topY, cellWidth-2, 99);
    }
}
