#include "PluginEditor.h"
#include "UI/SkullShape.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include <functional>

using namespace omni;

namespace {
const char* sectionNames[]={"SOPRANOS","ALTOS","TENORS","BASSES"};
const char* pitchClasses[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

// ------------------------------------------------------------------------------------------ value text
struct ValueText {
    std::function<juce::String(double)> text;
    std::function<double(const juce::String&)> parse;
};
juce::String signedOne(double v,const char* unit) {
    return (v>.05?juce::String("+"):juce::String())+juce::String(v,1)+unit;
}
ValueText makeText(ValueFormat f,float unity,double lo,double hi) {
    ValueText t;
    switch(f) {
        case ValueFormat::Percent:
            t.text=[](double v){return juce::String((int)std::lround(100.0*v))+"%";};
            t.parse=[](const juce::String& s){return s.getDoubleValue()/100.0;};
            break;
        case ValueFormat::PercentOfRange:
            t.text=[lo,hi](double v){return juce::String((int)std::lround(100.0*(v-lo)/(hi-lo)))+"%";};
            t.parse=[lo,hi](const juce::String& s){return lo+(hi-lo)*s.getDoubleValue()/100.0;};
            break;
        case ValueFormat::GainDb:   // a linear gain shown in dB against its own "unity"
            t.text=[unity](double v){return v<=1.0e-4?juce::String("-inf dB"):signedOne(20.0*std::log10(v/(double)unity)," dB");};
            t.parse=[unity](const juce::String& s){return (double)unity*std::pow(10.0,s.getDoubleValue()/20.0);};
            break;
        case ValueFormat::Db:
            t.text=[](double v){return signedOne(v," dB");};
            t.parse=[](const juce::String& s){return s.getDoubleValue();};
            break;
        case ValueFormat::Metres:
            t.text=[](double v){return juce::String(v,1)+" m";};
            t.parse=[](const juce::String& s){return s.getDoubleValue();};
            break;
        case ValueFormat::Seconds:
            t.text=[](double v){return juce::String(v,1)+" s";};
            t.parse=[](const juce::String& s){return s.getDoubleValue();};
            break;
        case ValueFormat::Plain: break;
    }
    return t;
}

// ------------------------------------------------------------------------------------------------ the skull
// One shape of UI/SkullShape.h, scaled into `box`. Everything is drawn from the data, so tools/render_skull_preview.py shows the same picture.
void fillSkullShape(juce::Graphics& g,const skull::Shape& s,juce::Rectangle<float> box) {
    const float x0=box.getX(),y0=box.getY(),w=box.getWidth(),h=box.getHeight();
    const auto& v=s.v;
    switch(s.kind) {
        case skull::Kind::Ellipse:
            g.fillEllipse(x0+(v[0]-v[2])*w,y0+(v[1]-v[3])*h,v[2]*2.f*w,v[3]*2.f*h);
            break;
        case skull::Kind::RoundRect:
            g.fillRoundedRectangle(x0+v[0]*w,y0+v[1]*h,v[2]*w,v[3]*h,v[4]*w);
            break;
        case skull::Kind::Poly: {
            juce::Path path;
            path.startNewSubPath(x0+v[0]*w,y0+v[1]*h);
            for(int i=2;i+1<s.count;i+=2) path.lineTo(x0+v[(size_t)i]*w,y0+v[(size_t)i+1]*h);
            path.closeSubPath();
            g.fillPath(path);
            break;
        }
    }
}

// ----------------------------------------------------------------------------------------------- controls
// A control that sets a colour once (a label, a slider fill) re-reads the palette here when the theme changes.
struct Themed { virtual ~Themed()=default; virtual void refreshColours()=0; };

class Dial final : public juce::Component, public juce::SettableTooltipClient, public Themed {
public:
    Dial(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id,const juce::String& tip,ValueFormat format,float unity)
        : attachment(p.parameters,id,dial) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(boldFont(10.f));
        label.setInterceptsMouseClicks(false,false);
        dial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        dial.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,16);
        refreshColours();
        if(format!=ValueFormat::Plain) {
            const auto text=makeText(format,unity,dial.getMinimum(),dial.getMaximum());
            dial.textFromValueFunction=text.text;
            dial.valueFromTextFunction=text.parse;
            dial.updateText();
        }
        setTooltip(tip);dial.setTooltip(tip);
        addAndMakeVisible(label);addAndMakeVisible(dial);
    }
    void resized() override {label.setBounds(0,0,getWidth(),16);dial.setBounds(0,16,getWidth(),getHeight()-16);}
    void refreshColours() override {
        label.setColour(juce::Label::textColourId,kMuted);
        dial.setColour(juce::Slider::rotarySliderFillColourId,kGold);
        repaint();
    }
private:
    juce::Label label;
    juce::Slider dial;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Fader final : public juce::Component, public juce::SettableTooltipClient, public Themed {
public:
    // accent: 0..3 the sections, 4 the lead (omni::faderAccent), so the colour follows the theme
    Fader(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id,int accentIndex,const juce::String& tip,float unity)
        : accentOf(accentIndex),attachment(p.parameters,id,slider) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(boldFont(10.f));
        label.setInterceptsMouseClicks(false,false);
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,50,15);
        refreshColours();
        const auto text=makeText(ValueFormat::GainDb,unity,slider.getMinimum(),slider.getMaximum());
        slider.textFromValueFunction=text.text;
        slider.valueFromTextFunction=text.parse;
        slider.updateText();
        setTooltip(tip);slider.setTooltip(tip);
        addAndMakeVisible(label);addAndMakeVisible(slider);
    }
    void resized() override {label.setBounds(0,0,getWidth(),16);slider.setBounds(0,16,getWidth(),getHeight()-16);}
    void refreshColours() override {
        const auto accent=faderAccent(accentOf);
        label.setColour(juce::Label::textColourId,accent);
        slider.setColour(juce::Slider::trackColourId,accent);
        repaint();
    }
private:
    int accentOf;
    juce::Label label;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Choice final : public juce::Component, public juce::SettableTooltipClient, public Themed {
public:
    Choice(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id,const juce::String& tip) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        refreshColours();
        label.setFont(boldFont(10.f));
        label.setInterceptsMouseClicks(false,false);
        combo.addItemList(p.parameters.getParameter(id)->getAllValueStrings(),1);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,id,combo);
        setTooltip(tip);combo.setTooltip(tip);
        addAndMakeVisible(label);addAndMakeVisible(combo);
    }
    void resized() override {label.setBounds(2,0,getWidth()-2,16);combo.setBounds(0,18,getWidth(),juce::jmin(28,getHeight()-18));}
    void refreshColours() override {label.setColour(juce::Label::textColourId,kMuted);repaint();}
private:
    juce::Label label;
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// A singer: a robed figure with a gradient and a rim light.
void drawFigure(juce::Graphics& g,juce::Point<float> center,juce::Colour colour,float size,float mouth=0.f) {
    const float x=center.x,y=center.y;
    g.setColour(juce::Colours::black.withAlpha(.35f));
    g.fillEllipse(x-size*.72f,y+size*.86f,size*1.44f,size*.21f);
    juce::Path robe;
    robe.startNewSubPath(x-size*.30f,y-size*.23f);
    robe.cubicTo(x-size*.60f,y-size*.06f,x-size*.53f,y+size*.40f,x-size*.62f,y+size*.83f);
    robe.quadraticTo(x,y+size*.97f,x+size*.62f,y+size*.83f);
    robe.cubicTo(x+size*.53f,y+size*.40f,x+size*.60f,y-size*.06f,x+size*.30f,y-size*.23f);
    robe.closeSubPath();
    g.setGradientFill(juce::ColourGradient(colour.brighter(.26f),x-size*.4f,y-size*.4f,
                                         colour.darker(.55f),x+size*.45f,y+size*.95f,false));
    g.fillPath(robe);
    g.setColour(colour.brighter(.45f).withAlpha(.65f));g.strokePath(robe,juce::PathStrokeType(.75f));
    g.setColour(colour.brighter(.25f));g.fillEllipse(x-size*.25f,y-size*.74f,size*.50f,size*.58f);
    g.setColour(juce::Colours::white.withAlpha(.32f));
    g.drawEllipse(x-size*.25f,y-size*.74f,size*.50f,size*.58f,.7f);
    if(mouth>.02f) {   // singing: the mouth opens with the level
        g.setColour(juce::Colours::black.withAlpha(.55f));
        g.fillEllipse(x-size*.065f,y-size*.33f,size*.13f,size*(.025f+.10f*mouth));
    }
}
juce::File settingsFolder() {   // per user, not per project: the tour has been seen, the window size you like
    return OmniChoirAudioProcessor::settingsFolder();
}
void loadWindowSize(int& width,int& height) {
    const auto parts=juce::StringArray::fromTokens(settingsFolder().getChildFile("window-size.txt").loadFileAsString(),juce::String(" \n"),juce::String());
    if(parts.size()>=2 && parts[0].getIntValue()>0 && parts[1].getIntValue()>0) {width=parts[0].getIntValue();height=parts[1].getIntValue();}
}
float levelToGlow(float level) {   // a linear level as 0..1 over -54 dB .. 0 dB
    return juce::jlimit(0.f,1.f,(20.f*std::log10(std::max(level,1.0e-5f))+54.f)/54.f);
}
}

// ============================================================================================ meters
MeterStrip::MeterStrip(OmniChoirAudioProcessor& p):processor(p) {startTimerHz(20);}
void MeterStrip::paint(juce::Graphics& g) {
    const auto area=getLocalBounds().toFloat();
    g.setColour(kPanel);g.fillRoundedRectangle(area,6.f);
    g.setColour(kStroke);g.drawRoundedRectangle(area.reduced(.5f),6.f,1.f);
    const auto& m=processor.meters();
    const float top=area.getY()+4.f,full=area.getHeight()-8.f;
    float x=area.getX()+8.f;
    const auto bar=[&](float level,juce::Colour colour) {
        g.setColour(juce::Colours::white.withAlpha(.06f));g.fillRect(x,top,6.f,full);
        const float h=full*levelToGlow(level);
        g.setColour(colour);g.fillRect(x,top+full-h,6.f,h);
        x+=9.f;
    };
    for(int i=0;i<4;++i)bar(m.section[i].load(std::memory_order_relaxed)*4.f,kSection[(size_t)i]);
    x+=4.f;
    bar(m.leadIn.load(std::memory_order_relaxed),kLead);
    x+=4.f;
    bar(m.outPeakL.load(std::memory_order_relaxed),kGold);
    bar(m.outPeakR.load(std::memory_order_relaxed),kGold);
    g.setColour(kMuted);g.setFont(boldFont(9.f));
    const juce::String text=juce::String("WIDTH ")+juce::String((int)std::lround(100.f*m.width.load(std::memory_order_relaxed)))
        +"%   CORR "+juce::String(m.correlation.load(std::memory_order_relaxed),2);
    g.drawText(text,(int)x+6,(int)area.getY(),juce::jmax(0,(int)(area.getRight()-x-8.f)),(int)area.getHeight(),juce::Justification::centredLeft);
}

// ============================================================================================== stage
juce::Rectangle<float> StageView::pillBounds(int section) const {
    const auto tag=worldToPoint(value("x"+juce::String(section)),value("y"+juce::String(section))-1.05f);
    return juce::Rectangle<float>(tag.x-34.f,tag.y-8.f,68.f,16.f);
}
int StageView::hitPill(juce::Point<float> pos) const {
    for(int i=0;i<4;++i) if(pillBounds(i).expanded(2.f).contains(pos)) return i;
    return -2;
}
void StageView::toggleState(int section,bool mute) {
    const auto id="sectionState"+juce::String(section);
    if(auto* param=processor.parameters.getParameter(id)) {
        const int now=(int)value(id);
        const float next=mute?(now==2?0.f:2.f):(now==1?0.f:1.f);   // click: solo on/off. right-click or shift-click: mute on/off
        param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(next));param->endChangeGesture();
    }
}
void SkullButton::paintButton(juce::Graphics& g,bool shouldDrawButtonAsHighlighted,bool shouldDrawButtonAsDown) {
    const auto area=getLocalBounds().toFloat().reduced(1.f);
    const float size=juce::jmin(area.getWidth(),area.getHeight());
    auto box=juce::Rectangle<float>(area.getCentreX()-size*.5f,area.getCentreY()-size*.5f,size,size);
    if(shouldDrawButtonAsDown) box=box.translated(0.f,1.f);
    if(shouldDrawButtonAsHighlighted||shouldDrawButtonAsDown) {   // a soft halo so it reads as a button
        g.setColour(kGold.withAlpha(shouldDrawButtonAsDown?.30f:.18f));
        g.fillEllipse(box.getX()-3.f,box.getY()-3.f,box.getWidth()+6.f,box.getHeight()+6.f);
    }
    g.setGradientFill(juce::ColourGradient(kGoldHi,0.f,box.getY(),kGoldLo,0.f,box.getBottom(),false));
    for(const auto& s:skull::kShapes) if(s.role==skull::Role::Body) fillSkullShape(g,s,box);
    g.setColour(kBg);
    for(const auto& s:skull::kShapes) if(s.role==skull::Role::Cut) fillSkullShape(g,s,box);
    g.setColour(juce::Colours::white.withAlpha(.34f));
    for(const auto& s:skull::kShapes) if(s.role==skull::Role::Glint) fillSkullShape(g,s,box);
}

const juce::Image& StageView::sceneImage(int index) {
    index=scenes::clampScene(index);
    auto& image=sceneImages[(size_t)index];
    if(!image.isValid()) {
        const char* data=BinaryData::cathedral_stage_jpg;int size=BinaryData::cathedral_stage_jpgSize;   // 0 Default and 1 Cathedral
        switch(index) {
            case 2:data=BinaryData::scene_concert_hall_jpg;size=BinaryData::scene_concert_hall_jpgSize;break;
            case 3:data=BinaryData::scene_studio_booth_jpg;size=BinaryData::scene_studio_booth_jpgSize;break;
            case 4:data=BinaryData::scene_chapel_jpg;size=BinaryData::scene_chapel_jpgSize;break;
            case 5:data=BinaryData::scene_gospel_stage_jpg;size=BinaryData::scene_gospel_stage_jpgSize;break;
            case 6:data=BinaryData::scene_stadium_anthem_jpg;size=BinaryData::scene_stadium_anthem_jpgSize;break;
            case 7:data=BinaryData::scene_headphone_choir_jpg;size=BinaryData::scene_headphone_choir_jpgSize;break;
            default:break;
        }
        image=juce::ImageCache::getFromMemory(data,size);
    }
    return image;
}
void StageView::followScene() {
    const int now=scenes::clampScene(processor.currentScene());
    if(now==shownScene) return;
    fromScene=shownScene;shownScene=now;
    // a preset that keeps the same picture (Default <-> Cathedral) does not fade
    fadeStartMs=scenes::samePicture(fromScene,shownScene)?-1.0e9:juce::Time::getMillisecondCounterHiRes();
}
StageView::StageView(OmniChoirAudioProcessor& p):processor(p) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTooltip("Drag the singers, your lead and the mic. The lead and the mic move independently: moving the mic never moves her. Double-click one to put it back; the faint ring shows where the lead's home spot is. Click a name to solo that section; right-click (or shift-click) to mute it.");
    startTimerHz(30);
}
juce::Rectangle<float> StageView::plot() const {return getLocalBounds().toFloat().reduced(7.f,7.f);}
juce::Point<float> StageView::worldToPoint(float x,float y) const {
    const auto r=plot();
    return {r.getCentreX()+x*r.getWidth()/21.f,
            r.getBottom()-28.f-(y+1.1f)/14.f*(r.getHeight()-52.f)};
}
juce::Point<float> StageView::pointToWorld(juce::Point<float> pt) const {
    const auto r=plot();
    return {(pt.x-r.getCentreX())*21.f/r.getWidth(),
            (r.getBottom()-28.f-pt.y)*14.f/(r.getHeight()-52.f)-1.1f};
}
float StageView::value(const juce::String& id) const {
    if(auto* a=processor.parameters.getRawParameterValue(id))return a->load();
    return 0;
}
void StageView::change(const juce::String& id,float v) {
    if(auto* param=processor.parameters.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(v));
}
void StageView::resetParameter(const juce::String& id) {
    if(auto* param=processor.parameters.getParameter(id))
        param->setValueNotifyingHost(param->getDefaultValue());
}
choir::StagePoint StageView::micWorld() const {return {value("micX"),value("micY")};}
choir::StagePoint StageView::leadWorld() const {return choir::StageGeometry::leadOnStage(micWorld(),value("leadX"),value("leadY"));}
juce::Point<float> StageView::leadPosition() const {
    // The lead is drawn at the mic + a fixed shift + her own offsets, so she is distinct from the gold mic and independent of it.
    const auto lead=leadWorld();
    return worldToPoint(lead.x,lead.y);
}
int StageView::hit(juce::Point<float> pos) const {
    // One score for everything: distance divided by that thing's own reach. The smallest score below 1 wins, so nothing has priority
    // and the thing nearest the pointer (for its size) is the one you pick up.
    int found=-2;
    float best=1.f;
    const auto test=[&](int index,juce::Point<float> target,float reach) {
        const float score=pos.getDistanceFrom(target)/reach;
        if(score<best) {best=score;found=index;}
    };
    // Hit any member of the group, not only its centroid, like demo.js.
    const float groupReach=std::max(18.f,(float)getWidth()*.033f);
    for(int i=0;i<4;++i) {
        const float x=value("x"+juce::String(i)),y=value("y"+juce::String(i));
        for(int row=0;row<3;++row)for(int col=0;col<4;++col)
            test(i,worldToPoint(x+(col-1.5f)*.47f+(row%2)*.12f,y+(row-1.f)*.53f),groupReach);
    }
    test(-1,worldToPoint(value("micX"),value("micY")),24.f);
    test(-3,leadPosition(),26.f);
    return found;
}
void StageView::startGesture(int i) {
    if(i==-2)return;
    const auto s=juce::String(i);
    const juce::String x=i==-1?"micX":i==-3?"leadX":"x"+s;
    const juce::String y=i==-1?"micY":i==-3?"leadY":"y"+s;
    processor.parameters.getParameter(x)->beginChangeGesture();
    processor.parameters.getParameter(y)->beginChangeGesture();
    if(i==-1) {
        processor.parameters.getParameter("leadX")->beginChangeGesture();
        processor.parameters.getParameter("leadY")->beginChangeGesture();
    }
}
void StageView::finishGesture(int i) {
    if(i==-2)return;
    const auto s=juce::String(i);
    const juce::String x=i==-1?"micX":i==-3?"leadX":"x"+s;
    const juce::String y=i==-1?"micY":i==-3?"leadY":"y"+s;
    processor.parameters.getParameter(x)->endChangeGesture();
    processor.parameters.getParameter(y)->endChangeGesture();
    if(i==-1) {
        processor.parameters.getParameter("leadX")->endChangeGesture();
        processor.parameters.getParameter("leadY")->endChangeGesture();
    }
}
void StageView::moveMic(float x,float y) {
    const auto mic=choir::StageGeometry::clampMic({x,y});
    change("micX",mic.x);change("micY",mic.y);
    // The lead stays exactly where she was when the drag began: her offsets are re-derived from that spot, and the parameter ranges
    // are wide enough (StageGeometry.h) that this never has to clamp.
    const auto offset=choir::StageGeometry::offsetFor(leadStart,mic);
    change("leadX",offset.x);change("leadY",offset.y);
}

void StageView::paint(juce::Graphics& g) {
    const auto full=getLocalBounds().toFloat();
    const auto r=plot();
    const float t=(float)(juce::Time::getMillisecondCounterHiRes()*.001);
    const bool on=value("choirOn")>.5f;
    const float input=juce::jlimit(0.f,1.f,processor.inputLevel()*5.f);
    g.setColour(kPanel);g.fillRoundedRectangle(full,14.f);
    {
        juce::Graphics::ScopedSaveState keep(g);
        juce::Path clip;clip.addRoundedRectangle(r,10.f);
        g.reduceClipRegion(clip);
        followScene();
        const auto picture=[&](const juce::Image& image,float opacity) {
            if(!image.isValid()) return;
            g.setOpacity(opacity);
            g.drawImageWithin(image,(int)r.getX(),(int)r.getY(),(int)r.getWidth(),(int)r.getHeight(),
                              juce::RectanglePlacement(juce::RectanglePlacement::xMid|juce::RectanglePlacement::yMid
                                                       |juce::RectanglePlacement::fillDestination));
        };
        g.setColour(kBg);g.fillRect(r);
        const float fade=scenes::fadeProgress(juce::Time::getMillisecondCounterHiRes()-fadeStartMs);
        if(fade<1.f) picture(sceneImage(fromScene),1.f);          // the old room, with the new one fading in over it (400 ms)
        picture(sceneImage(shownScene),fade);
        if(kStageTint.getAlpha()>0) {g.setColour(kStageTint);g.fillRect(r);}   // the theme's wash over the picture (gold has none, so its picture is untouched)
        // the floor fades to black
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack,r.getCentreX(),r.getY()+r.getHeight()*.35f,
                                               kBg.withAlpha(.9f),r.getCentreX(),r.getBottom(),false));
        g.fillRect(r);
        // light falling from the window; it brightens with your voice
        const float shaftTop=r.getY()+r.getHeight()*.12f;
        juce::Path shaft;
        shaft.startNewSubPath(r.getCentreX()-r.getWidth()*.02f,shaftTop);
        shaft.lineTo(r.getCentreX()+r.getWidth()*.02f,shaftTop);
        shaft.lineTo(r.getCentreX()+r.getWidth()*.19f,r.getBottom());
        shaft.lineTo(r.getCentreX()-r.getWidth()*.19f,r.getBottom());
        shaft.closeSubPath();
        const juce::Colour light=kLight;
        g.setGradientFill(juce::ColourGradient(light.withAlpha(.10f+.14f*input),r.getCentreX(),shaftTop,
                                               light.withAlpha(0.f),r.getCentreX(),r.getBottom(),false));
        g.fillPath(shaft);
        // dark corners
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack,r.getCentreX(),r.getCentreY(),
                                               kVignette.withAlpha(.78f),r.getX(),r.getY(),true));
        g.fillRect(r);
        // drifting dust in the light
        const auto hash=[](int i,int k) {const float s=std::sin((float)(i*127+k*311))*43758.547f;return s-std::floor(s);};
        for(int i=0;i<36;++i) {
            const float speed=5.f+9.f*hash(i,3);
            const float px=r.getX()+r.getWidth()*hash(i,1)+3.f*std::sin(t*.6f+(float)i);
            const float py=r.getBottom()-std::fmod(r.getHeight()*hash(i,2)+t*speed,r.getHeight());
            const float edge=1.f-std::abs(px-r.getCentreX())/(r.getWidth()*.5f);
            const float twinkle=.5f+.5f*std::sin(t*1.7f+(float)i*2.1f);
            const float alpha=(.10f+.32f*hash(i,4))*twinkle*(.25f+.75f*edge)*(.6f+.8f*input);
            g.setColour(kDust.withAlpha(juce::jlimit(0.f,1.f,alpha)));
            const float size=1.1f+1.6f*hash(i,5);
            g.fillEllipse(px,py,size,size);
        }
        // the four sections. Click a name tag to solo it, right-click (or shift-click) to mute it.
        bool anySolo=false;
        for(int i=0;i<4;++i) if((int)value("sectionState"+juce::String(i))==1) anySolo=true;
        for(int i=0;i<4;++i) {
            const float x=value("x"+juce::String(i)),y=value("y"+juce::String(i));
            const int state=(int)value("sectionState"+juce::String(i));
            const bool audible=anySolo?state==1:state!=2;      // what you actually hear
            const auto base=kSection[(size_t)i];
            const bool lit=on&&audible;
            const auto c=base.withMultipliedSaturation(lit?1.f:.3f);
            const float glow=lit?levelToGlow(processor.meters().section[i].load(std::memory_order_relaxed)*4.f):0.f;   // how loudly it sings right now
            const auto gc=worldToPoint(x,y);
            g.setGradientFill(juce::ColourGradient(c.withAlpha(.05f+.34f*glow),gc.x,gc.y,c.withAlpha(0.f),gc.x+96.f,gc.y,true));
            g.fillEllipse(gc.x-110.f,gc.y-62.f,220.f,124.f);
            const float singerSize=juce::jlimit(11.f,19.f,(getHeight()*.05f)-.18f*(y-5.f))*(1.f+.07f*glow);
            for(int row=2;row>=0;--row)for(int col=0;col<4;++col) {
                const float dx=(col-1.5f)*.47f+(row%2)*.12f,dy=(row-1.f)*.53f;
                const float phase=(float)(row*4+col+i*5);
                auto where=worldToPoint(x+dx,y+dy);
                const float open=glow*(.55f+.45f*std::sin(t*7.f+phase*1.7f));          // the mouths open with the level
                where.x+=std::sin(t*2.3f+phase)*glow*singerSize*.06f;                  // and they sway a little as they sing
                where.y-=open*singerSize*.04f;
                drawFigure(g,where,c.withMultipliedAlpha(lit?1.f:.45f),singerSize,open);
            }
            if(hover==i||dragging==i) {g.setColour(base.withAlpha(.85f));g.drawEllipse(gc.x-72.f,gc.y-36.f,144.f,72.f,1.4f);}
            const auto pill=pillBounds(i);
            const bool pillHot=hoverPill==i;
            g.setColour(kBg.withAlpha(.62f));g.fillRoundedRectangle(pill,8.f);
            g.setColour((pillHot?base:c).withAlpha(pillHot?1.f:.9f));g.drawRoundedRectangle(pill.reduced(.5f),8.f,pillHot?1.7f:1.f);
            g.setFont(boldFont(9.5f));
            g.setColour(c.withAlpha(audible?1.f:.55f));
            g.drawText(sectionNames[i],state!=0?pill.withTrimmedRight(11.f):pill,juce::Justification::centred);
            if(state!=0) {      // S = soloed, M = muted
                const juce::Rectangle<float> badge(pill.getRight()-15.f,pill.getY()+1.f,14.f,14.f);
                g.setColour(state==1?kGold:kMuteBadge);g.fillEllipse(badge);
                g.setColour(state==1?kInk:kCream);g.setFont(boldFont(9.f));
                g.drawText(state==1?"S":"M",badge,juce::Justification::centred);
            }
        }
        // the lead: glows while you sing, breathes while her pitch is locked
        const auto lead=leadPosition();
        {   // a faint ring where she stands at home (leadX 0, leadY 0.4): double-click her to go back. Hidden while she is there.
            const auto homeWorld=choir::StageGeometry::home(micWorld());
            const auto homePoint=worldToPoint(homeWorld.x,homeWorld.y);
            if(homePoint.getDistanceFrom(lead)>3.f) {
                g.setColour(kLead.withAlpha(.30f));g.drawEllipse(homePoint.x-17.f,homePoint.y-17.f,34.f,34.f,1.f);
                g.setColour(kLead.withAlpha(.30f));g.fillEllipse(homePoint.x-1.5f,homePoint.y-1.5f,3.f,3.f);
            }
        }
        const float confidence=juce::jlimit(0.f,1.f,processor.meters().pitchConfidence.load(std::memory_order_relaxed));
        const float locked=confidence>.5f?(confidence-.5f)*2.f:0.f;               // 0..1 once the tracker is sure of a pitch
        const float breathe=1.f+.12f*locked*std::sin(t*5.f);
        g.setGradientFill(juce::ColourGradient(kLead.withAlpha(.08f+.34f*input),lead.x,lead.y,kLead.withAlpha(0.f),lead.x+44.f*breathe,lead.y,true));
        g.fillEllipse(lead.x-48.f*breathe,lead.y-48.f*breathe,96.f*breathe,96.f*breathe);
        if(locked>0.f) {g.setColour(kGold.withAlpha(.18f+.5f*locked));g.drawEllipse(lead.x-30.f,lead.y+13.f,60.f,16.f,1.3f);}   // a gold ring at her feet while the pitch is locked
        if(hover==-3||dragging==-3) {g.setColour(kLead.withAlpha(.8f));g.drawEllipse(lead.x-27.f,lead.y-29.f,54.f,58.f,1.3f);}
        drawFigure(g,lead,kLeadFigure,20.f,input*(.6f+.4f*std::sin(t*8.f)));
        {
            const juce::Rectangle<float> pill(lead.x-24.f,lead.y+20.f,48.f,15.f);
            g.setColour(kBg.withAlpha(.62f));g.fillRoundedRectangle(pill,7.5f);
            g.setColour(kLead);g.setFont(boldFont(9.5f));g.drawText("LEAD",pill,juce::Justification::centred);
        }
        // the microphone: a gold ring that breathes
        const auto mic=worldToPoint(value("micX"),value("micY"));
        const float pulse=.5f+.5f*std::sin(t*2.2f);
        g.setColour(kGold.withAlpha(.10f+.06f*pulse+((hover==-1||dragging==-1)?.14f:0.f)));g.fillEllipse(mic.x-25.f,mic.y-23.f,50.f,46.f);
        g.setColour(kGold.withAlpha(.9f));g.drawEllipse(mic.x-19.f,mic.y-17.f,38.f,34.f,1.3f);
        g.setColour(kGold);g.fillRoundedRectangle(mic.x-3.5f,mic.y-14.f,7.f,14.f,3.f);
        g.drawLine(mic.x,mic.y,mic.x,mic.y+14.f,1.5f);
        g.drawLine(mic.x-7.f,mic.y+14.f,mic.x+7.f,mic.y+14.f,1.4f);
        {
            const juce::Rectangle<float> pill(mic.x-18.f,mic.y+19.f,36.f,15.f);
            g.setColour(kBg.withAlpha(.62f));g.fillRoundedRectangle(pill,7.5f);
            g.setColour(kGold);g.setFont(boldFont(9.5f));g.drawText("MIC",pill,juce::Justification::centred);
        }
        // labels on the glass
        const auto chip=[&](juce::Rectangle<float> rc,const juce::String& text,juce::Colour colour) {
            g.setColour(kBg.withAlpha(.55f));g.fillRoundedRectangle(rc,9.f);
            g.setColour(colour.withAlpha(.5f));g.drawRoundedRectangle(rc.reduced(.5f),9.f,1.f);
            g.setColour(colour);g.setFont(boldFont(9.f));g.drawText(text,rc,juce::Justification::centred);
        };
        chip({r.getX()+10.f,r.getY()+9.f,184.f,18.f},"DRAG SINGERS / LEAD / MIC",kGold);
        chip({r.getRight()-130.f,r.getY()+9.f,120.f,18.f},"CATHEDRAL STAGE",kCream);
        if(!on)chip({r.getCentreX()-46.f,r.getY()+9.f,92.f,18.f},"CHOIR OFF",kSection[0]);
        if((int)value("mode")==2) {      // MIDI Chord mode: show what is being held, bass to soprano
            juce::String chordText="MIDI CHORD: ";
            int lastShown=-1;bool any=false;
            for(int section=3;section>=0;--section) {
                const int note=processor.chordNote(section);
                if(note>=0&&note!=lastShown) {chordText+=juce::String(pitchClasses[note%12])+" ";lastShown=note;any=true;}
            }
            if(!any)chordText+="HOLD A KEY";
            chip({r.getCentreX()-100.f,r.getY()+31.f,200.f,18.f},chordText,any?kGold:kMuted);
        }
        g.setColour(kMuted);g.setFont(boldFont(8.5f));
        g.drawText("INPUT",juce::Rectangle<float>(r.getX()+12.f,r.getBottom()-31.f,60.f,12.f),juce::Justification::left);
        g.setColour(kGold.withAlpha(.22f));g.fillRoundedRectangle(r.getX()+12.f,r.getBottom()-16.f,96.f,4.f,2.f);
        g.setColour(kGold);g.fillRoundedRectangle(r.getX()+12.f,r.getBottom()-16.f,96.f*input,4.f,2.f);
    }
    g.setColour(kGold.withAlpha(.38f));g.drawRoundedRectangle(full.reduced(.6f),14.f,1.2f);
}
void StageView::mouseDown(const juce::MouseEvent& e) {
    const int tag=hitPill(e.position);
    if(tag>=0) {toggleState(tag,e.mods.isPopupMenu()||e.mods.isShiftDown());return;}
    dragging=hit(e.position);
    if(dragging==-2) return;
    // Remember where the thing is relative to the pointer, so it moves with the pointer instead of jumping its centre to it.
    const auto pointer=pointToWorld(e.position);
    choir::StagePoint thing;
    if(dragging==-1) {thing=micWorld();leadStart=leadWorld();}
    else if(dragging==-3) {thing=leadWorld();leadStart=thing;}
    else {const auto s=juce::String(dragging);thing={value("x"+s),value("y"+s)};}
    grabX=thing.x-pointer.x;grabY=thing.y-pointer.y;
    startGesture(dragging);   // no mouseDrag() here: a click must not move anything
}
void StageView::mouseDrag(const juce::MouseEvent& e) {
    if(dragging==-2)return;
    const auto pointer=pointToWorld(e.position);
    const choir::StagePoint target {pointer.x+grabX,pointer.y+grabY};
    if(dragging==-1) {
        moveMic(target.x,target.y);
    } else if(dragging==-3) {
        const auto lead=choir::StageGeometry::clampLead(target);
        const auto offset=choir::StageGeometry::offsetFor(lead,micWorld());
        change("leadX",offset.x);change("leadY",offset.y);
    } else {
        const auto s=juce::String(dragging);
        change("x"+s,juce::jlimit(-9.f,9.f,target.x));
        change("y"+s,juce::jlimit(.7f,11.f,target.y));
    }
}
void StageView::mouseUp(const juce::MouseEvent&) {finishGesture(dragging);dragging=-2;}
void StageView::mouseMove(const juce::MouseEvent& e) {hoverPill=hitPill(e.position);hover=hoverPill>=0?-2:hit(e.position);}
void StageView::mouseExit(const juce::MouseEvent&) {hover=-2;hoverPill=-2;}
void StageView::mouseDoubleClick(const juce::MouseEvent& e) {
    // Double-click puts the thing back where it started: to its own default, not to zero.
    if(hitPill(e.position)>=0) return;   // the name tags take single clicks only
    const int what=hit(e.position);
    if(what==-3) {
        startGesture(-3);resetParameter("leadX");resetParameter("leadY");finishGesture(-3);
    } else if(what==-1) {
        leadStart=leadWorld();   // she stays put while the mic goes home
        startGesture(-1);moveMic(0.f,0.f);finishGesture(-1);
    } else if(what>=0) {
        const auto s=juce::String(what);
        startGesture(what);resetParameter("x"+s);resetParameter("y"+s);finishGesture(what);
    }
}

// ================================================================================================ editor
OmniChoirAudioProcessorEditor::OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor& p)
    :AudioProcessorEditor(&p),processor(p),stage(p),meterStrip(p),roll(p) {
    setLookAndFeel(&lnf);
    omni::applyTheme(loadSavedTheme());lnf.refreshColours();appliedTheme=omni::themeIndex();   // before any component takes a colour
    {
        int width=860,height=600;
        loadWindowSize(width,height);
        setResizable(true,true);setResizeLimits(800,540,1500,1050);
        setSize(juce::jlimit(800,1500,width),juce::jlimit(540,1050,height));
    }
    addAndMakeVisible(stage);addAndMakeVisible(meterStrip);

    // ---- header: key, scale, style / room, choir on/off
    {
        static const char* scaleLabels[]={"NO SCALE","MAJOR","MINOR","HARM. MINOR","DORIAN","MIXOLYDIAN","PENTA MAJOR","PENTA MINOR"};
        for(int i=0;i<12;++i)keyBox.addItem("KEY "+juce::String(choir::kKeyNames[i]),i+1);
        for(int i=0;i<choir::kNumScales;++i)scaleBox.addItem(scaleLabels[i],i+1);
        keyAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"keyRoot",keyBox);
        scaleAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"scale",scaleBox);
        keyBox.setTooltip("The key of your song. With a scale chosen, the choir sings harmony that stays in this key.");
        scaleBox.setTooltip("The scale of your song. NO SCALE leaves the harmony at fixed intervals (a fifth, a third, a fourth below, an octave below). Nothing is detected: you choose.");
        addAndMakeVisible(keyBox);addAndMakeVisible(scaleBox);
    }
    choirButton.setComponentID("power");
    choirButton.setClickingTogglesState(true);
    choirButton.setTooltip("Choir on or off. Your own voice is always heard.");
    choirAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters,"choirOn",choirButton);
    addAndMakeVisible(choirButton);
    addAndMakeVisible(presetBox);
    presetBox.setTextWhenNothingSelected("STYLE / ROOM");
    presetBox.setTooltip("A style sets the room, where the singers stand and how they sing, in one click. A room sets only where they stand and the space. MY STYLES are sounds you saved. Your lead level, output and solo/mute are never touched.");
    rebuildStyleMenu();
    presetBox.onChange=[this] {
        const int id=presetBox.getSelectedId();
        presetBox.setSelectedId(0,juce::dontSendNotification);
        handleStyleMenu(id);
    };
    surpriseButton.setComponentID("action");
    surpriseButton.setTooltip("A random but musical style: a room, a harmony and a voice character. Your lead level, output and solo/mute are never touched. Press it again for another.");
    surpriseButton.onClick=[this]{processor.surpriseMe();};
    modeButton.setTooltip("Switch between Easy (just the big choir controls) and Pro (everything).");
    modeButton.onClick=[this]{processor.setProUi(!processor.proUi());applyMode();};
    addAndMakeVisible(surpriseButton);addAndMakeVisible(modeButton);
    addAndMakeVisible(skullButton);
    skullButton.setTooltip("Change the colours: gold, blood red, neon green or ice blue. Your choice is remembered.");
    skullButton.onClick=[this] {showThemeMenu();};
    statusLabel.setColour(juce::Label::textColourId,kMuted);
    statusLabel.setFont(plainFont(11.f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    // ---- tabs
    const char* tabNames[]={"MAIN","ROOM","VOICE","FOLLOW","LISTEN","TAKE","STEMS"};
    const char* tabTips[]={"The controls you reach for","The room, the microphones and the hall","How the singers sing",
                           "How the choir follows you: loudness, consonants, tempo","Let the plugin listen to your take, then have the choir sing it",
                           "Scan a whole recording from a file and edit its harmony","Mix separately recorded singers"};
    for(int i=0;i<numPages;++i) {
        auto& tab=tabs[(size_t)i];
        tab.setButtonText(tabNames[i]);tab.setTooltip(tabTips[i]);tab.setComponentID("tab");
        tab.onClick=[this,i]{selectTab(i);};
        addAndMakeVisible(tab);
    }

    // ---- MAIN: five knobs and five faders are all most people need
    addFader("SOPRANO","gain0",0,"Soprano level. 0 dB is the default.",.83f);
    addFader("ALTO","gain1",1,"Alto level. 0 dB is the default.",.83f);
    addFader("TENOR","gain2",2,"Tenor level. 0 dB is the default.",.83f);
    addFader("BASS","gain3",3,"Bass level. 0 dB is the default.",.83f);
    addFader("LEAD","leadGain",4,"Your own voice, placed on the stage. 0 dB is unity.",1.f);
    addKnob("CHOIR SIZE","choirSize",pageMain,"One knob for how big the choir sounds: tighter, drier and closer below the middle, looser, wetter and wider above it. The detailed knobs keep their own meaning.",ValueFormat::Percent,1.f,true);
    addKnob("CHOIR MIX","mix",pageMain,"Choir against your own voice. 0% is only your voice, 100% is only the choir. With AUTO LEVEL on, the loudness stays steady as you move it.",ValueFormat::Percent,1.f,true);
    addKnob("ENSEMBLE","ensemble",pageMain,"How far the singers in a section drift apart in tuning: more sounds like a bigger choir.",ValueFormat::Percent);
    addKnob("HUMANIZE","humanize",pageMain,"Timing and pitch differences between singers. More is looser and more human.",ValueFormat::Percent);
    addKnob("SPACE","wet",pageMain,"How much of the room you hear around the choir.",ValueFormat::PercentOfRange);
    addKnob("OUTPUT","master",pageMain,"Overall output level.",ValueFormat::GainDb,1.f,true);
    addChoice("PROCESS","mode",pageMain,"Live follows your singing at once. Scan / Harmony uses the key found by SCAN LIVE INPUT. MIDI Chord sings the keys you hold (route a MIDI track to this plugin).");
    addChoice("HARMONY","harmony",pageMain,"The chord colour the singers follow. With a SCALE chosen the scale decides; unison always keeps everyone on your note.",true);
    listenEasyButton.setComponentID("action");
    listenEasyButton.setTooltip("Press it, play your vocal through the plugin from the start, press it again. The choir then sings what you sang, in your key.");
    listenEasyButton.onClick=[this]{listenClicked();};
    addSide(pageMain,listenEasyButton,false,true);

    // ---- ROOM: for when you want to shape the space
    addKnob("ROOM SIZE","roomSize",pageRoom,"Size of the room the choir stands in.",ValueFormat::Percent);
    addKnob("REVERB TIME","roomSeconds",pageRoom,"How long the room rings.",ValueFormat::Seconds);
    addKnob("DAMPING","absorption",pageRoom,"Soft walls: more damping gives a duller, shorter tail.",ValueFormat::Percent);
    addKnob("DIFFUSION","diffusion",pageRoom,"How smooth and even the reflections are.",ValueFormat::Percent);
    addKnob("HALL","hall",pageRoom,"An extra hall under the choir. Off at 0%.",ValueFormat::PercentOfRange);
    addKnob("HALL TIME","hallSeconds",pageRoom,"Tail length of the extra hall.",ValueFormat::Seconds);
    addKnob("MIC SPACING","width",pageRoom,"Distance between the two microphones.",ValueFormat::Metres);
    addChoice("MIC ARRAY","micMode",pageRoom,"How the microphones are arranged to listen to the stage.");
    abButton.setClickingTogglesState(true);
    abButton.setTooltip("Faithful: the full stage model. Classic: the simpler v0.5 stage. Same latency, so switching never shifts timing.");
    abAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"classicStage",abButton);
    irButton.setTooltip("Pick one of three built-in rooms (synthetic: cathedral, stone chapel, small hall) or load your own room recording (an impulse response). It is heard through the HALL knob; choosing a room brings the HALL up if it was off.");
    irButton.onClick=[this] {
        juce::PopupMenu menu;
        for(int room=0;room<OmniChoirAudioProcessor::kNumBuiltInRooms;++room)
            menu.addItem(1+room,juce::String(OmniChoirAudioProcessor::builtInRoomName(room))+" (built in)",true,
                         processor.hasImpulseResponse() && processor.impulseResponseName()==OmniChoirAudioProcessor::builtInRoomName(room));
        menu.addItem(10,"Load your own room recording...");
        if(processor.hasImpulseResponse())menu.addItem(11,"Remove it (back to the built-in hall)");
        juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&irButton),[safe](int result) {
            if(safe==nullptr)return;
            if(result>=1 && result<=OmniChoirAudioProcessor::kNumBuiltInRooms)safe->processor.chooseBuiltInRoom(result-1);
            else if(result==10)safe->chooseImpulseResponse();
            else if(result==11)safe->processor.clearImpulseResponse();
        });
    };
    addSide(pageRoom,abButton);addSide(pageRoom,irButton);

    // ---- VOICE: how the singers sound
    addKnob("CHARACTER","formant",pageVoice,"Keeps each singer's vowel colour natural when the pitch moves.",ValueFormat::Percent);
    addKnob("VOICE BLEND","voiceBlend",pageVoice,"Mixes in synthesised singers that follow your voice.",ValueFormat::Percent);
    addKnob("VOWEL","choirVowel",pageVoice,"The vowel the synthesised singers sing.",ValueFormat::Percent);
    addKnob("FOLLOW LEAD","vowelFollow",pageVoice,"How closely the synthesised singers copy your vowel.",ValueFormat::Percent);
    addKnob("BREATH","choirBreath",pageVoice,"Breathiness of the synthesised singers.",ValueFormat::Percent);
    addKnob("VIBRATO","vibrato",pageVoice,"Vibrato of the synthesised singers.",ValueFormat::Percent);

    autoLevelButton.setClickingTogglesState(true);
    autoLevelButton.setTooltip("Keeps the loudness steady as CHOIR MIX moves, so turning the choir up never makes the plugin quieter. Off gives the raw mix, as in versions before 0.7. (Faithful engine only.)");
    autoLevelAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"levelMatch",autoLevelButton);
    addKnob("CHOIR LEVEL","choirLevel",pageFollow,"The choir against your voice, in dB. Works with CHOIR MIX.",ValueFormat::Db);
    addKnob("SENSITIVITY","sensitivity",pageFollow,"How quiet a sound can be and still start the choir. Raise it for soft singing, lower it for a noisy input.",ValueFormat::Percent);
    addKnob("CONSONANTS","consonants",pageFollow,"Your own s, t and k pass through cleanly instead of being smeared by sixteen singers. The choir sings the vowels. 0% is off.",ValueFormat::Percent);
    addKnob("SWELL","swell",pageFollow,"The choir blooms when you sing louder than your own average and recedes when you sing softer. 0% follows your level one to one.",ValueFormat::Percent);
    addKnob("PULSE DEPTH","pulseDepth",pageFollow,"How far the choir breathes down on each pulse. Needs PULSE RATE set to something other than Off.",ValueFormat::Percent);
    addChoice("PULSE RATE","pulseRate",pageFollow,"The choir breathes in time with the song: once per 2 bars, 1 bar, half a bar, a beat or half a beat. Follows your DAW's tempo and position.");
    addSide(pageFollow,autoLevelButton);

    // ---- LISTEN: capture a take, see its notes, edit them, and let the choir sing them
    for(auto* b:{&listenButton,&clearListenButton}) {b->setComponentID("action");addAndMakeVisible(*b);}
    addAndMakeVisible(roll);
    listenButton.setTooltip("Press it, then play your vocal through the plugin from the start. Press STOP when the take is over. The choir will then sing what you sang, lined up with your DAW's play position.");
    listenButton.onClick=[this]{listenClicked();};
    clearListenButton.setTooltip("Forget this take.");
    clearListenButton.onClick=[this]{processor.clearListening();roll.clearSelection();};
    for(int i=0;i<8;++i) {
        auto& b=listenAdjust[(size_t)i];
        b.setButtonText(juce::String::charToString((juce::juce_wchar)"SATB"[i/2])+(i%2?"+":"-"));
        b.setTooltip("Move the selected note's part up or down one semitone. Click a note in the piano roll first.");
        b.onClick=[this,i] {
            if(roll.selected()>=0) processor.listenSession().transpose((size_t)roll.selected(),i/2,i%2?1:-1);
            roll.repaint();
        };
        addAndMakeVisible(b);
    }

    // ---- TAKE and STEMS
    for(auto* b:{&importButton,&exportButton,&midiButton,&loadStemsButton,&mixStemsButton}) {
        b->setComponentID("action");addAndMakeVisible(*b);
    }
    importButton.setTooltip("Choose a recorded solo vocal (WAV, AIFF or FLAC). It is scanned into notes you can edit.");
    exportButton.setTooltip("Render the scanned take with your edited harmony to a new stereo WAV.");
    midiButton.setTooltip("Save the scanned take as a MIDI file: soprano, alto, tenor, bass and lead.");
    midiButton.onClick=[this]{chooseMidi();};
    loadStemsButton.onClick=[this]{chooseStemManifest();};
    mixStemsButton.onClick=[this]{chooseStemOutput();};
    importButton.onClick=[this]{chooseInput();};
    exportButton.onClick=[this]{chooseOutput();};
    addAndMakeVisible(stemsStatus);addAndMakeVisible(stemsHelp);
    stemsStatus.setColour(juce::Label::textColourId,kCream);
    stemsStatus.setFont(plainFont(11.f));
    stemsHelp.setColour(juce::Label::textColourId,kMuted);
    stemsHelp.setFont(plainFont(12.f));
    stemsHelp.setJustificationType(juce::Justification::centredLeft);
    stemsHelp.setText("1. EXPORT MIDI ON THE TAKE TAB  >  2. RECORD OR RENDER VOICE STEMS  >  3. LOAD MANIFEST  >  MIX WAV",juce::dontSendNotification);
    addAndMakeVisible(notes);
    notes.setRowHeight(22);
    addAndMakeVisible(takeStatus);
    takeStatus.setColour(juce::Label::textColourId,kCream);
    takeStatus.setFont(plainFont(11.f));
    for(int i=0;i<8;++i) {
        adjust[(size_t)i].setButtonText(juce::String::charToString((juce::juce_wchar)"SATB"[i/2])+(i%2?"+":"-"));
        adjust[(size_t)i].setTooltip("Move the selected note's part up or down one semitone.");
        adjust[(size_t)i].onClick=[this,i] {
            take.changeNote(notes.getSelectedRow(),i/2,i%2?1:-1);
            visibleScore=take.snapshot(); notes.repaint();
        };
        addAndMakeVisible(adjust[(size_t)i]);
    }
    applyMode();
    if(!settingsFolder().getChildFile("tour-done").existsAsFile()) startTour();   // the first time this user opens the plugin
    startTimerHz(10);
}
OmniChoirAudioProcessorEditor::~OmniChoirAudioProcessorEditor() {
    stopTimer();chooser.reset();tour.reset();
    setLookAndFeel(nullptr);
}
void OmniChoirAudioProcessorEditor::addKnob(const juce::String& title,const juce::String& id,int page,const juce::String& tip,ValueFormat format,float unity,bool easy) {
    auto c=std::make_unique<Dial>(processor,title,id,tip,format,unity);
    addAndMakeVisible(*c);
    slots.push_back({page,Kind::Knob,c.get(),easy});
    owned.push_back(std::move(c));
}
void OmniChoirAudioProcessorEditor::addFader(const juce::String& title,const juce::String& id,int accent,const juce::String& tip,float unity) {
    auto c=std::make_unique<Fader>(processor,title,id,accent,tip,unity);
    addAndMakeVisible(*c);
    slots.push_back({pageMain,Kind::Fader,c.get(),false});
    owned.push_back(std::move(c));
}
void OmniChoirAudioProcessorEditor::addChoice(const juce::String& title,const juce::String& id,int page,const juce::String& tip,bool easy) {
    auto c=std::make_unique<Choice>(processor,title,id,tip);
    addAndMakeVisible(*c);
    slots.push_back({page,Kind::Side,c.get(),easy});
    owned.push_back(std::move(c));
}
void OmniChoirAudioProcessorEditor::addSide(int page,juce::Component& item,bool easy,bool easyOnly) {
    addAndMakeVisible(item);
    slots.push_back({page,Kind::Side,&item,easy,easyOnly});
}
void OmniChoirAudioProcessorEditor::applyMode() {
    pro=processor.proUi();
    for(auto& tab:tabs)tab.setVisible(pro);
    modeButton.setButtonText(pro?"EASY":"PRO");
    selectTab(pro?currentTab:(int)pageMain);
}
void OmniChoirAudioProcessorEditor::selectTab(int page) {
    currentTab=page;
    for(auto& s:slots)s.item->setVisible(s.page==page && (pro?!s.easyOnly:(s.easy||s.easyOnly)));
    const bool listenPage=page==pageListen;
    roll.setVisible(listenPage);listenButton.setVisible(listenPage);clearListenButton.setVisible(listenPage);
    for(auto& b:listenAdjust)b.setVisible(listenPage);
    const bool takePage=page==pageTake,stemsPage=page==pageStems;
    importButton.setVisible(takePage);exportButton.setVisible(takePage);midiButton.setVisible(takePage);
    notes.setVisible(takePage);takeStatus.setVisible(takePage);
    for(auto& b:adjust)b.setVisible(takePage);
    loadStemsButton.setVisible(stemsPage);mixStemsButton.setVisible(stemsPage);
    stemsHelp.setVisible(stemsPage);stemsStatus.setVisible(stemsPage);
    for(int i=0;i<numPages;++i)tabs[(size_t)i].setToggleState(i==page,juce::dontSendNotification);
    resized();
}

void OmniChoirAudioProcessorEditor::chooseInput() {
    chooser=std::make_unique<juce::FileChooser>("Select full vocal take",juce::File{},"*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& f){
            if(safe!=nullptr) {
                const auto input=f.getResult();
                if(input.existsAsFile()) {
                    const bool minor=safe->processor.parameters.getRawParameterValue("harmony")->load()>=.5f
                                      &&safe->processor.parameters.getRawParameterValue("harmony")->load()<1.5f;
                    safe->take.scanFile(input,minor);
                }
            }
        });
}
void OmniChoirAudioProcessorEditor::chooseImpulseResponse() {
    chooser=std::make_unique<juce::FileChooser>("Select a room impulse response",juce::File{},"*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& f){
            if(safe!=nullptr) {
                const auto file=f.getResult();
                if(file.existsAsFile())safe->processor.loadImpulseResponse(file);
            }
        });
}
void OmniChoirAudioProcessorEditor::chooseOutput() {
    if(visibleScore.notes.empty())return;
    chooser=std::make_unique<juce::FileChooser>("Export choir take as WAV",juce::File{},"*.wav");
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& f){
            if(safe!=nullptr &&f.getResult()!=juce::File{})
                safe->take.exportFile(f.getResult().withFileExtension("wav"),safe->processor.snapshotParams());
        });
}
void OmniChoirAudioProcessorEditor::chooseMidi() {
    if(visibleScore.notes.empty())return;
    chooser=std::make_unique<juce::FileChooser>("Export SATB + lead as MIDI",juce::File{},"*.mid");
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& f){
            if(safe!=nullptr && f.getResult()!=juce::File{})
                safe->take.exportMidiFile(f.getResult().withFileExtension("mid"));
        });
}
void OmniChoirAudioProcessorEditor::chooseStemManifest() {
    chooser=std::make_unique<juce::FileChooser>("Select the singer voice manifest",juce::File{},"*.json");
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& f){
            if(safe!=nullptr && f.getResult().existsAsFile()) {
                safe->stemManifest=f.getResult();
                safe->stemsStatus.setText("LOADED: "+safe->stemManifest.getFileName(),juce::dontSendNotification);
            }
        });
}
void OmniChoirAudioProcessorEditor::chooseStemOutput() {
    if(!stemManifest.existsAsFile())return;
    chooser=std::make_unique<juce::FileChooser>("Mix external singers to stereo WAV",juce::File{},"*.wav");
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& f){
            if(safe!=nullptr && f.getResult()!=juce::File{})
                safe->take.mixStemsFile(safe->stemManifest,f.getResult().withFileExtension("wav"),
                                        safe->processor.snapshotParams());
        });
}
// ============================================================================================ piano roll
PianoRoll::PianoRoll(OmniChoirAudioProcessor& p):processor(p) {
    setTooltip("The notes you sang (white) and the four parts the choir sings (coloured). Click a note to select it, then use the buttons on the right to move a part by a semitone. The wheel scrolls.");
}
double PianoRoll::timeAt(float x) const {return viewStart+(double)x/std::max(1,getWidth())*viewSpan;}
float PianoRoll::xAt(double t) const {return (float)((t-viewStart)/viewSpan*getWidth());}
void PianoRoll::refresh() {
    const double playhead=processor.listenPlayhead();
    if(playhead>=0 && (playhead<viewStart || playhead>viewStart+viewSpan*.92)) viewStart=std::max(0.0,playhead-viewSpan*.1);
    repaint();
}
void PianoRoll::mouseDown(const juce::MouseEvent& e) {
    const auto* score=processor.listenSession().score();
    if(score==nullptr) {selectedNote=-1;return;}
    selectedNote=choir::findNoteIndex(*score,timeAt(e.position.x),.05);
    repaint();
}
void PianoRoll::mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel) {
    const auto* score=processor.listenSession().score();
    const double duration=score!=nullptr&&!score->notes.empty()?score->notes.back().end+1.0:viewSpan;
    viewStart=juce::jlimit(0.0,std::max(0.0,duration-viewSpan),viewStart-(double)wheel.deltaY*viewSpan*.5);
    repaint();
}
void PianoRoll::paint(juce::Graphics& g) {
    const auto area=getLocalBounds().toFloat();
    g.setColour(kRollBg);g.fillRoundedRectangle(area,8.f);
    g.setColour(kStroke);g.drawRoundedRectangle(area.reduced(.5f),8.f,1.f);
    const auto& session=processor.listenSession();
    const auto state=session.state();
    const auto message=[&](const juce::String& text,juce::Colour colour) {
        g.setColour(colour);g.setFont(boldFont(11.f));
        g.drawFittedText(text,getLocalBounds().reduced(14,6),juce::Justification::centred,3);
    };
    if(state==choir::ListenSession::Listening) {
        const float pulse=.5f+.5f*std::sin((float)(juce::Time::getMillisecondCounterHiRes()*.006));
        g.setColour(kRecord.withAlpha(.5f+.5f*pulse));g.fillEllipse(area.getCentreX()-60.f,area.getCentreY()-26.f,10.f,10.f);
        message("LISTENING  "+juce::String(session.capturedSeconds(),1)+" s\nPlay your take through the plugin from the start, then press STOP.",kCream);
        return;
    }
    if(state==choir::ListenSession::Analysing) {message("ANALYSING YOUR TAKE...",kGold);return;}
    const auto* score=session.score();
    if(score==nullptr || score->notes.empty()) {
        message(state==choir::ListenSession::Ready?"No notes were heard. Try again with a clean, solo vocal.":
                "Press LISTEN, then play your vocal through the plugin from the start. The notes appear here and the choir will sing them.",kMuted);
        return;
    }
    int low=200,high=0;
    for(const auto& n:score->notes) {
        low=std::min(low,std::min(n.melody,*std::min_element(n.satb.begin(),n.satb.end())));
        high=std::max(high,std::max(n.melody,*std::max_element(n.satb.begin(),n.satb.end())));
    }
    low-=1;high+=1;
    if(high-low<20) {const int grow=(20-(high-low))/2+1;low-=grow;high+=grow;}
    const float rowHeight=area.getHeight()/(float)(high-low+1);
    const auto yOf=[&](int midi) {return area.getBottom()-(float)(midi-low+1)*rowHeight;};
    const int scale=(int)processor.parameters.getRawParameterValue("scale")->load(),root=(int)processor.parameters.getRawParameterValue("keyRoot")->load();
    for(int midi=low;midi<=high;++midi) {      // rows: out-of-scale rows are darker; every C is marked
        const bool inKey=scale==0||choir::inScale(midi,root,scale);
        g.setColour(inKey?juce::Colour(0x14ffffff):juce::Colour(0x00000000));
        if(inKey)g.fillRect(0.f,yOf(midi),area.getWidth(),rowHeight);
        if(midi%12==0) {
            g.setColour(kMuted.withAlpha(.5f));g.drawLine(0.f,yOf(midi)+rowHeight,area.getWidth(),yOf(midi)+rowHeight,.6f);
            g.setFont(plainFont(8.5f));g.drawText("C"+juce::String(midi/12-1),3,(int)yOf(midi),22,(int)std::max(10.f,rowHeight),juce::Justification::centredLeft);
        }
    }
    const int first=std::max(0,choir::findNoteIndex(*score,viewStart));
    for(int i=first;i<(int)score->notes.size();++i) {
        const auto& n=score->notes[(size_t)i];
        if(n.start>viewStart+viewSpan) break;
        const float x0=xAt(n.start),x1=std::max(x0+2.f,xAt(n.end));
        if(x1<0.f) continue;
        for(int part=0;part<4;++part) {      // the four parts the choir sings
            g.setColour(kSection[(size_t)part].withAlpha(.85f));
            g.fillRoundedRectangle(x0,yOf(n.satb[(size_t)part])+rowHeight*.2f,x1-x0,rowHeight*.55f,2.f);
        }
        g.setColour(kLead.withAlpha(.95f));      // what you sang, on top
        g.fillRoundedRectangle(x0,yOf(n.melody)+rowHeight*.08f,x1-x0,rowHeight*.84f,2.5f);
        if(i==selectedNote) {g.setColour(kGold);g.drawRoundedRectangle(x0-1.f,yOf(n.melody)-1.f,x1-x0+2.f,rowHeight+2.f,3.f,1.6f);}
    }
    const double playhead=processor.listenPlayhead();
    if(playhead>=0) {g.setColour(kGold);const float x=xAt(playhead);g.drawLine(x,0.f,x,area.getBottom(),1.6f);}
}

// ================================================================================================ tour
TourOverlay::TourOverlay(std::vector<Step> s,std::function<void()> onFinished):steps(std::move(s)),finished(std::move(onFinished)) {
    addAndMakeVisible(next);addAndMakeVisible(skip);
    next.setComponentID("action");skip.setComponentID("action");
    next.onClick=[this] {
        if(++index>=steps.size()) {if(finished)finished();return;}
        next.setButtonText(index+1==steps.size()?"DONE":"NEXT");
        resized();repaint();
    };
    skip.onClick=[this]{if(finished)finished();};
    if(steps.size()==1) next.setButtonText("DONE");
}
juce::Rectangle<int> TourOverlay::targetBounds() const {
    if(index>=steps.size()) return {};
    auto r=steps[index].target->getBounds();
    if(steps[index].alsoTarget!=nullptr) r=r.getUnion(steps[index].alsoTarget->getBounds());
    return r.expanded(6);
}
juce::Rectangle<int> TourOverlay::bubbleBounds() const {
    const auto target=targetBounds();
    const int width=juce::jmin(330,getWidth()-24),height=122;
    const bool below=target.getBottom()+height+14<getHeight();
    const int y=below?target.getBottom()+10:juce::jmax(8,target.getY()-height-10);
    const int x=juce::jlimit(12,juce::jmax(12,getWidth()-width-12),target.getCentreX()-width/2);
    return {x,y,width,height};
}
void TourOverlay::resized() {
    const auto bubble=bubbleBounds();
    next.setBounds(bubble.getRight()-82,bubble.getBottom()-36,72,26);
    skip.setBounds(bubble.getRight()-160,bubble.getBottom()-36,72,26);
}
void TourOverlay::paint(juce::Graphics& g) {
    if(index>=steps.size()) return;
    const auto target=targetBounds().toFloat();
    juce::Path dim;
    dim.addRectangle(getLocalBounds().toFloat());
    dim.addRoundedRectangle(target,10.f);
    dim.setUsingNonZeroWinding(false);          // the target is a hole in the dimming
    g.setColour(juce::Colours::black.withAlpha(.74f));g.fillPath(dim);
    g.setColour(kGold);g.drawRoundedRectangle(target,10.f,2.f);
    const auto bubble=bubbleBounds().toFloat();
    g.setColour(kPanel);g.fillRoundedRectangle(bubble,12.f);
    g.setColour(kGold);g.drawRoundedRectangle(bubble.reduced(.5f),12.f,1.4f);
    g.setFont(boldFont(13.f));g.drawText(steps[index].title,bubble.reduced(14.f,8.f).withHeight(20.f),juce::Justification::centredLeft);
    g.setColour(kCream);g.setFont(plainFont(12.f));
    g.drawFittedText(steps[index].text,bubble.toNearestInt().reduced(14,0).withTrimmedTop(32).withTrimmedBottom(42),juce::Justification::topLeft,4);
    g.setColour(kMuted);g.setFont(boldFont(9.5f));
    g.drawText(juce::String((int)index+1)+" / "+juce::String((int)steps.size()),bubble.reduced(14.f,0.f).withTrimmedTop(bubble.getHeight()-34.f).withHeight(26.f),juce::Justification::centredLeft);
}

// ============================================================================================== listen, styles, tour
void OmniChoirAudioProcessorEditor::listenClicked() {
    auto& session=processor.listenSession();
    if(session.state()==choir::ListenSession::Listening) processor.stopListening();
    else if(session.state()!=choir::ListenSession::Analysing) processor.startListening();
}
void OmniChoirAudioProcessorEditor::rebuildStyleMenu() {
    presetBox.clear(juce::dontSendNotification);
    presetBox.addSectionHeading("STYLES: a whole sound");
    for(int i=0;i<choir::kNumStylePresets;++i)presetBox.addItem(choir::kStylePresets[i].name,1+i);
    styleFiles.clear();
    for(const auto& file:processor.userStyleFiles()) if(styleFiles.size()<80) styleFiles.push_back(file);
    if(!styleFiles.empty()) {
        presetBox.addSeparator();
        presetBox.addSectionHeading("MY STYLES");
        for(size_t i=0;i<styleFiles.size();++i)presetBox.addItem(styleFiles[i].getFileNameWithoutExtension(),300+(int)i);
    }
    presetBox.addSeparator();
    presetBox.addSectionHeading("ROOMS: where they stand");
    for(int i=0;i<choir::kNumStagePresets;++i)presetBox.addItem(choir::kStagePresets[i].name,100+i);
    presetBox.addSeparator();
    presetBox.addItem("Save this sound as a style...",900);
    presetBox.addItem("Load a style file...",901);
    presetBox.addItem("Open my styles folder",902);
    presetBox.addItem("Show the tour",903);
}
void OmniChoirAudioProcessorEditor::handleStyleMenu(int id) {
    if(id>=300 && id<900) {
        const size_t index=(size_t)(id-300);
        if(index<styleFiles.size()) processor.loadStyleFile(styleFiles[index]);
        return;
    }
    if(id>=100 && id<300) {processor.applyStagePreset(id-100);return;}
    if(id>=1 && id<100) {processor.applyStyle(id-1);return;}
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    if(id==900) {
        chooser=std::make_unique<juce::FileChooser>("Save this sound as a style",OmniChoirAudioProcessor::userStyleFolder().getChildFile("My style.omnistyle"),"*.omnistyle");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
            [safe](const juce::FileChooser& fc) {
                if(safe==nullptr) return;
                const auto file=fc.getResult();
                if(file==juce::File()) return;
                safe->processor.saveStyleFile(file.withFileExtension("omnistyle"));
                safe->rebuildStyleMenu();
            });
    } else if(id==901) {
        chooser=std::make_unique<juce::FileChooser>("Load a style file",OmniChoirAudioProcessor::userStyleFolder(),"*.omnistyle");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& fc) {
                if(safe==nullptr) return;
                const auto file=fc.getResult();
                if(file==juce::File()) return;
                if(safe->processor.loadStyleFile(file)) {
                    const auto folder=OmniChoirAudioProcessor::userStyleFolder();
                    if(!file.isAChildOf(folder)) file.copyFileTo(folder.getChildFile(file.getFileName()));   // keep a copy among MY STYLES
                    safe->rebuildStyleMenu();
                }
            });
    } else if(id==902) {
        OmniChoirAudioProcessor::userStyleFolder().revealToUser();
    } else if(id==903) {
        startTour();
    }
}
void OmniChoirAudioProcessorEditor::showThemeMenu() {
    juce::PopupMenu menu;
    for(int i=0;i<theme::kNumThemes;++i) menu.addItem(100+i,juce::String(theme::get(i).name),true,i==omni::themeIndex());
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&skullButton),[safe](int result) {
        if(safe==nullptr || result<100 || result>=100+theme::kNumThemes) return;
        safe->setTheme(result-100);
    });
}
void OmniChoirAudioProcessorEditor::startTour() {
    if(tour!=nullptr) return;
    std::vector<TourOverlay::Step> steps;
    const auto add=[&](juce::Component* target,juce::Component* also,const char* title,const char* text) {
        if(target!=nullptr && target->isVisible()) steps.push_back({target,also,title,text});
    };
    add(&stage,nullptr,"YOUR CHOIR","Drag the four sections, your lead voice and the microphone around the stage. Click a section's name to solo it, right-click to mute it.");
    add(&presetBox,nullptr,"STYLES AND ROOMS","Pick a style for a whole sound in one click, or a room for just the space. You can save your own sounds here too.");
    add(&scaleBox,&keyBox,"YOUR KEY","Choose your song's key and scale and the choir's harmony stays in key. Leave it on NO SCALE for fixed intervals. Nothing is detected: you choose.");
    for(auto& slot:slots) if(slot.kind==Kind::Knob && slot.page==pageMain && slot.item->isVisible()) {
        add(slot.item,nullptr,"HOW BIG","One knob for how big the choir sounds. The knobs next to it set how much choir you hear.");
        break;
    }
    add(&listenEasyButton,nullptr,"LISTEN, THEN CHOIR","Press LISTEN, play your vocal through the plugin from the start, then press it again. The plugin works out your notes and the choir sings them.");
    add(&modeButton,&surpriseButton,"EASY OR PRO","Easy shows just the big controls. Pro shows everything. SURPRISE ME picks a random musical style.");
    add(&skullButton,nullptr,"MAKE IT YOURS","Click the skull to change the colours: gold, blood red, neon green or ice blue. SkellyChoir remembers your choice.");
    if(steps.empty()) return;
    tour=std::make_unique<TourOverlay>(std::move(steps),[this]{finishTour();});
    addAndMakeVisible(*tour);
    tour->setBounds(getLocalBounds());
    tour->toFront(false);
}
void OmniChoirAudioProcessorEditor::finishTour() {
    const auto flag=settingsFolder().getChildFile("tour-done");
    flag.getParentDirectory().createDirectory();
    flag.create();                       // seen once: it does not come back by itself
    juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
    juce::MessageManager::callAsync([safe]{ if(safe!=nullptr) safe->tour.reset(); });   // not from inside the overlay's own button
}
int OmniChoirAudioProcessorEditor::loadSavedTheme() {
    const auto file=settingsFolder().getChildFile("theme.txt");
    if(!file.existsAsFile()) return theme::kDefaultTheme;
    return theme::fromFileText(file.loadFileAsString().toStdString());   // unknown or damaged: the original gold
}
void OmniChoirAudioProcessorEditor::saveTheme(int index) {
    const auto folder=settingsFolder();
    folder.createDirectory();
    folder.getChildFile("theme.txt").replaceWithText(juce::String(theme::toFileText(index)));
}
void OmniChoirAudioProcessorEditor::setTheme(int index) {
    omni::applyTheme(index);
    refreshThemedColours();
    saveTheme(omni::themeIndex());
}
void OmniChoirAudioProcessorEditor::refreshThemedColours() {
    appliedTheme=omni::themeIndex();
    lnf.refreshColours();
    for(auto& item:owned) if(auto* themed=dynamic_cast<Themed*>(item.get())) themed->refreshColours();
    statusLabel.setColour(juce::Label::textColourId,kMuted);
    stemsStatus.setColour(juce::Label::textColourId,kCream);
    stemsHelp.setColour(juce::Label::textColourId,kMuted);
    takeStatus.setColour(juce::Label::textColourId,kCream);
    notes.updateContent();
    repaint();
}
void OmniChoirAudioProcessorEditor::saveWindowSize() {
    const auto folder=settingsFolder();
    folder.createDirectory();
    folder.getChildFile("window-size.txt").replaceWithText(juce::String(getWidth())+" "+juce::String(getHeight()));
}
void OmniChoirAudioProcessorEditor::paintListBoxItem(int index,juce::Graphics& g,int width,int height,bool selected) {
    if(index<0||index>=(int)visibleScore.notes.size())return;
    const auto& e=visibleScore.notes[(size_t)index];
    g.fillAll(selected?kListSel:(index%2?kPanel:kPanel2.darker(.25f)));
    const auto name=[](int midi) {
        return juce::String(pitchClasses[(midi%12+12)%12])+juce::String(midi/12-1);
    };
    g.setFont(boldFont(11.f));
    g.setColour(kMuted);g.drawText(juce::String(e.start,1)+"s",8,0,46,height,juce::Justification::centredLeft);
    g.setColour(kLead);g.drawText("LEAD "+name(e.melody),56,0,78,height,juce::Justification::centredLeft);
    for(int i=0;i<4;++i) {
        g.setColour(kSection[(size_t)i]);
        g.drawText(juce::String::charToString((juce::juce_wchar)"SATB"[i])+" "+name(e.satb[(size_t)i]),
                   138+i*62,0,juce::jmax(0,juce::jmin(60,width-138-i*62)),height,juce::Justification::centredLeft);
    }
}
void OmniChoirAudioProcessorEditor::timerCallback() {
    if(omni::themeIndex()!=appliedTheme) refreshThemedColours();   // another open instance of the plugin changed the theme
    juce::String msg="Drag the singers, your lead and the mic on the stage. Double-click one to put it back.";
    // ---- LISTEN: the take is captured, analysed and then sung
    auto& session=processor.listenSession();
    const int listenState=(int)session.state();
    if(listenState==(int)choir::ListenSession::Listening && session.isFull()) processor.stopListening();   // the buffer is full: stop and analyse
    if(lastListenState==(int)choir::ListenSession::Analysing && listenState==(int)choir::ListenSession::Ready) {
        processor.setMode(1);                 // the take is ready: the choir now sings it
        roll.clearSelection();
    }
    lastListenState=listenState;
    {   // changing the key, scale or harmony re-voices the take, unless you have edited notes by hand
        const auto read=[this](const char* id) {return (int)processor.parameters.getRawParameterValue(id)->load();};
        const int key=read("keyRoot"),scale=read("scale"),harmony=read("harmony");
        if((key!=lastKey||scale!=lastScale||harmony!=lastHarmony) && lastKey>=0) session.revoice(key,scale,harmony);
        lastKey=key;lastScale=scale;lastHarmony=harmony;
        keyBox.setEnabled(scale>0);
    }
    switch(session.state()) {
        case choir::ListenSession::Listening:msg="LISTENING: play your take through the plugin from the start, then press STOP.   "+juce::String(session.capturedSeconds(),1)+" s";break;
        case choir::ListenSession::Analysing:msg="Analysing your take...";break;
        case choir::ListenSession::Ready:
            if(session.score()!=nullptr)
                msg=juce::String((int)session.score()->notes.size())+" notes heard. PROCESS is set to Listen: play the take back from where you started."+
                    (session.hostPositionKnown()?juce::String():juce::String("  Your DAW does not report its play position, so the choir cannot line up with the take."));
            break;
        default:break;
    }
    {
        const bool busy=session.state()==choir::ListenSession::Analysing;
        const juce::String text=session.state()==choir::ListenSession::Listening?"STOP":(session.state()==choir::ListenSession::Analysing?"ANALYSING...":(session.state()==choir::ListenSession::Ready?"LISTEN AGAIN":"LISTEN"));
        listenButton.setButtonText(text);listenEasyButton.setButtonText(text);
        listenButton.setEnabled(!busy);listenEasyButton.setEnabled(!busy);
        clearListenButton.setEnabled(session.state()==choir::ListenSession::Ready);
        const bool ready=session.state()==choir::ListenSession::Ready&&roll.selected()>=0;
        for(auto& b:listenAdjust)b.setEnabled(ready);
        roll.refresh();
    }
    // ---- the window size you like is remembered (written once it has stopped changing)
    if(sizeDirty && ++sizeTicks>=10) {saveWindowSize();sizeDirty=false;}
    choirButton.setButtonText(choirButton.getToggleState()?"CHOIR ON":"CHOIR OFF");
    abButton.setButtonText(abButton.getToggleState()?"ENGINE: CLASSIC":"ENGINE: FAITHFUL");
    autoLevelButton.setButtonText(autoLevelButton.getToggleState()?"AUTO LEVEL: ON":"AUTO LEVEL: OFF");
    irButton.setButtonText(processor.hasImpulseResponse()?"ROOM: "+processor.impulseResponseName().toUpperCase().substring(0,12):juce::String("ROOMS / LOAD IR"));
    const int status=take.status();
    if(status!=lastTakeStatus && status!=1 &&status!=3 && status!=5 && status!=7) {
        visibleScore=take.snapshot();notes.updateContent();
        lastTakeStatus=status;
    }
    juce::String takeMsg;
    switch(status) {
        case 1:takeMsg="ANALYZING FULL TAKE  "+juce::String((int)(100*take.progress()))+"%";break;
        case 2:takeMsg=take.fileName()+"  /  "+juce::String(visibleScore.notes.size())+" EDITABLE NOTES";break;
        case 3:takeMsg="RENDERING WAV  "+juce::String((int)(100*take.progress()))+"%";break;
        case 4:takeMsg="WAV RENDERED  /  "+juce::String(visibleScore.notes.size())+" NOTES";break;
        case 5:takeMsg="WRITING MIDI  "+juce::String((int)(100*take.progress()))+"%";break;
        case 6:takeMsg="MIDI EXPORTED (5 PARTS / 120 BPM)";break;
        case 7:takeMsg="MIXING STEMS  "+juce::String((int)(100*take.progress()))+"%";break;
        case 8:takeMsg="STEM WAV RENDERED";break;
        case -1:takeMsg="ERROR: "+take.errorMessage();break;
        default:takeMsg="IMPORT A VOCAL WAV / AIFF / FLAC FOR COMPLETE SCAN";
    }
    takeStatus.setText(takeMsg,juce::dontSendNotification);
    const bool busy=status==1||status==3||status==5||status==7;
    statusLabel.setText(busy||status==-1?takeMsg:msg,juce::dontSendNotification);
    const bool idle=!busy;
    exportButton.setEnabled(idle && !visibleScore.notes.empty());
    midiButton.setEnabled(idle && !visibleScore.notes.empty());
    mixStemsButton.setEnabled(idle && stemManifest.existsAsFile());
    for(auto& b:adjust)b.setEnabled(idle && !visibleScore.notes.empty() && notes.getSelectedRow()>=0);
    if(status==7 || status==8 || status==-1)stemsStatus.setText(takeMsg,juce::dontSendNotification);
}
void OmniChoirAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setGradientFill(juce::ColourGradient(kHeaderTop,(float)getWidth()*.5f,0.f,kBg,(float)getWidth()*.5f,130.f,false));
    g.fillRect(0,0,getWidth(),130);
    g.setGradientFill(juce::ColourGradient(kGoldHi,0.f,6.f,kGoldLo,0.f,40.f,false));
    g.setFont(serifFont(titleLayout.fontHeight));
    g.drawText(branding::kTitle,14,5,(int)std::ceil(titleLayout.titleWidth)+8,36,juce::Justification::centredLeft);
    g.setColour(kCream);g.setFont(boldFont(9.f));
    g.drawText(branding::kSubtitle,16,41,juce::jmax(120,keyBox.getX()-16-6),14,juce::Justification::centredLeft);
    g.setColour(kGold.withAlpha(.30f));g.drawLine(14.f,62.f,(float)getWidth()-14.f,62.f,1.f);
    g.setColour(kPanel.withAlpha(.85f));g.fillRoundedRectangle(deckBounds.toFloat(),12.f);
    g.setColour(kStroke);g.drawRoundedRectangle(deckBounds.toFloat().reduced(.5f),12.f,1.f);
}
void OmniChoirAudioProcessorEditor::layoutPage(int page,juce::Rectangle<int> area) {
    std::vector<juce::Component*> knobs,faders,sides;
    for(auto& s:slots) {
        if(s.page!=page || !(pro?!s.easyOnly:(s.easy||s.easyOnly)))continue;
        auto& list=s.kind==Kind::Knob?knobs:(s.kind==Kind::Fader?faders:sides);
        list.push_back(s.item);
    }
    if(!faders.empty()) {
        const int faderWidth=46;
        auto strip=area.removeFromLeft((int)faders.size()*faderWidth);
        for(auto* f:faders)f->setBounds(strip.removeFromLeft(faderWidth));
        area.removeFromLeft(14);
    }
    if(!sides.empty()) {
        auto column=area.removeFromRight(156);
        area.removeFromRight(12);
        // a labelled dropdown needs 46 px, a button 28 px; stack them, centred in the column
        const auto heightOf=[](juce::Component* c) {return dynamic_cast<juce::Button*>(c)!=nullptr?28:46;};
        int total=-2;
        for(auto* s:sides)total+=heightOf(s)+2;
        int y=column.getY()+juce::jmax(0,(column.getHeight()-total)/2);
        for(auto* s:sides) {
            const int rowHeight=heightOf(s);
            s->setBounds(column.getX(),y,column.getWidth(),rowHeight);
            y+=rowHeight+2;
        }
    }
    if(!knobs.empty()) {
        const int cellWidth=juce::jmin(pro?112:170,area.getWidth()/(int)knobs.size());
        if(!pro)area.removeFromLeft((area.getWidth()-cellWidth*(int)knobs.size())/2);   // Easy: a few big knobs, centred
        for(auto* k:knobs)k->setBounds(area.removeFromLeft(cellWidth));
    }
}
void OmniChoirAudioProcessorEditor::resized() {
    sizeDirty=true;sizeTicks=0;
    if(tour!=nullptr) tour->setBounds(getLocalBounds());
    const int w=getWidth(),h=getHeight(),m=14,headerHeight=62,footerHeight=26,tabsHeight=30;
    const int controlsHeight=juce::jlimit(122,170,(int)((float)h*.215f));

    auto header=getLocalBounds().removeFromTop(headerHeight).reduced(m,0);
    choirButton.setBounds(header.removeFromRight(126).withSizeKeepingCentre(126,36));
    header.removeFromRight(8);
    presetBox.setBounds(header.removeFromRight(160).withSizeKeepingCentre(160,30));
    header.removeFromRight(8);
    scaleBox.setBounds(header.removeFromRight(124).withSizeKeepingCentre(124,30));
    header.removeFromRight(6);
    keyBox.setBounds(header.removeFromRight(78).withSizeKeepingCentre(78,30));
    {   // the title shrinks if it must so that title + skull always end before the first control (UI/TitleLayout.h)
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(serifFont(titlelayout::kBaseFont),branding::kTitle,0.f,0.f);
        titleLayout=titlelayout::compute(glyphs.getBoundingBox(0,-1,true).getWidth(),(float)keyBox.getX());
        const int size=juce::roundToInt(titleLayout.skullSize);
        skullButton.setBounds(juce::roundToInt(titleLayout.skullX),5+(36-size)/2,size,size);
    }

    const int footerTop=h-footerHeight-6;
    const int controlsTop=footerTop-6-controlsHeight;
    const int tabsTop=controlsTop-tabsHeight-6;
    const int stageTop=headerHeight+4;
    stage.setBounds(m,stageTop,w-2*m,juce::jmax(60,tabsTop-6-stageTop));
    const int tabWidth=juce::jmin(92,(w-2*m-172)/numPages-3);
    for(int i=0;i<numPages;++i)tabs[(size_t)i].setBounds(m+i*(tabWidth+3),tabsTop,tabWidth,tabsHeight);
    auto tabRow=juce::Rectangle<int>(m,tabsTop,w-2*m,tabsHeight);
    modeButton.setBounds(tabRow.removeFromRight(64).reduced(0,1));
    tabRow.removeFromRight(4);
    surpriseButton.setBounds(tabRow.removeFromRight(112).reduced(0,1));
    deckBounds=juce::Rectangle<int>(m,controlsTop,w-2*m,controlsHeight);
    auto footer=juce::Rectangle<int>(m,footerTop,w-2*m,footerHeight);
    meterStrip.setBounds(footer.removeFromRight(juce::jmin(330,footer.getWidth()/2)));
    footer.removeFromRight(10);
    statusLabel.setBounds(footer);

    const auto inner=deckBounds.reduced(14,8);
    for(const int page:{(int)pageMain,(int)pageRoom,(int)pageVoice,(int)pageFollow})layoutPage(page,inner);
    // LISTEN
    listenButton.setBounds(inner.getX(),inner.getY(),136,30);
    clearListenButton.setBounds(inner.getX(),inner.getY()+36,136,26);
    roll.setBounds(inner.getX()+148,inner.getY(),juce::jmax(40,inner.getWidth()-148-126),inner.getHeight());
    for(int i=0;i<8;++i)
        listenAdjust[(size_t)i].setBounds(inner.getRight()-116+(i%2)*58,inner.getY()+(i/2)*25,56,23);
    // TAKE
    importButton.setBounds(inner.getX(),inner.getY(),140,28);
    exportButton.setBounds(inner.getX(),inner.getY()+32,140,28);
    midiButton.setBounds(inner.getX(),inner.getY()+64,140,28);
    const int listLeft=inner.getX()+152,listRight=inner.getRight()-126;
    takeStatus.setBounds(listLeft,inner.getY(),juce::jmax(0,listRight-listLeft),22);
    notes.setBounds(listLeft,inner.getY()+24,juce::jmax(0,listRight-listLeft),juce::jmax(20,inner.getHeight()-24));
    for(int i=0;i<8;++i)
        adjust[(size_t)i].setBounds(inner.getRight()-116+(i%2)*58,inner.getY()+(i/2)*25,56,23);
    // STEMS
    loadStemsButton.setBounds(inner.getX(),inner.getY(),156,32);
    mixStemsButton.setBounds(inner.getX()+166,inner.getY(),156,32);
    stemsStatus.setBounds(inner.getX()+332,inner.getY(),juce::jmax(0,inner.getWidth()-332),32);
    stemsHelp.setBounds(inner.getX(),inner.getY()+42,inner.getWidth(),juce::jmax(20,inner.getHeight()-42));
}
