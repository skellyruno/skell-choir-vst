#include "PluginEditor.h"
#include <BinaryData.h>
#include <array>
#include <cmath>

namespace {
const juce::Colour bg(0xff09070e),panel(0xff18111a),stroke(0xff553c34);
const juce::Colour cream(0xffefe1cb),muted(0xffa48f86),brass(0xffe8ba67);
const std::array<juce::Colour,4> sectionColours {
    juce::Colour(0xffdd8897),juce::Colour(0xffedcd80),
    juce::Colour(0xff72bca2),juce::Colour(0xff749ad4)
};
const char* sectionNames[]={"SOPRANOS","ALTOS","TENORS","BASSES"};
const char* pitchClasses[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

class Dial final : public juce::Component {
public:
    Dial(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id)
        : attachment(p.parameters,id,dial) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,muted);
        label.setFont(juce::Font(juce::FontOptions(11.f).withStyle("Bold")));
        dial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        dial.setTextBoxStyle(juce::Slider::TextBoxBelow,false,85,18);
        dial.setColour(juce::Slider::rotarySliderFillColourId,brass);
        dial.setColour(juce::Slider::rotarySliderOutlineColourId,stroke);
        dial.setColour(juce::Slider::thumbColourId,cream);
        dial.setColour(juce::Slider::textBoxTextColourId,cream);
        dial.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        dial.setColour(juce::Slider::textBoxBackgroundColourId,panel);
        if(auto* param=p.parameters.getParameter(id))
            dial.setDoubleClickReturnValue(true,param->convertFrom0to1(param->getDefaultValue()));
        dial.setTooltip(title);
        addAndMakeVisible(label);addAndMakeVisible(dial);
    }
    void resized() override {label.setBounds(0,0,getWidth(),18);dial.setBounds(2,18,getWidth()-4,getHeight()-18);}
private:
    juce::Label label;
    juce::Slider dial;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};
class Choice final : public juce::Component {
public:
    Choice(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,muted);
        label.setFont(juce::Font(juce::FontOptions(11.f).withStyle("Bold")));
        combo.addItemList(p.parameters.getParameter(id)->getAllValueStrings(),1);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,id,combo);
        combo.setColour(juce::ComboBox::backgroundColourId,panel);
        combo.setColour(juce::ComboBox::textColourId,cream);
        combo.setColour(juce::ComboBox::outlineColourId,stroke);
        combo.setColour(juce::ComboBox::arrowColourId,brass);
        addAndMakeVisible(label);addAndMakeVisible(combo);
    }
    void resized() override {label.setBounds(0,7,getWidth(),24);combo.setBounds(5,42,getWidth()-10,28);}
private:
    juce::Label label;
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
void drawFigure(juce::Graphics& g,juce::Point<float> center,juce::Colour colour,float size) {
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
}
}

StageView::StageView(OmniChoirAudioProcessor& p) : processor(p),
    cathedral(juce::ImageCache::getFromMemory(BinaryData::cathedral_stage_jpg,
                                             BinaryData::cathedral_stage_jpgSize)) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    startTimerHz(20);
}
juce::Rectangle<float> StageView::plot() const {return getLocalBounds().toFloat().reduced(17.f,14.f);}
juce::Point<float> StageView::worldToPoint(float x,float y) const {
    const auto r=plot();
    // Same draggable 2D stage concept as the web demo; performer X/Y in metres.
    return {r.getCentreX()+x*r.getWidth()/21.f,
            r.getBottom()-38.f-(y+1.1f)/14.f*(r.getHeight()-62.f)};
}
juce::Point<float> StageView::pointToWorld(juce::Point<float> pt) const {
    const auto r=plot();
    return {(pt.x-r.getCentreX())*21.f/r.getWidth(),
            (r.getBottom()-38.f-pt.y)*14.f/(r.getHeight()-62.f)-1.1f};
}
float StageView::value(const juce::String& id) const {
    if(const auto* x=processor.parameters.getRawParameterValue(id))return x->load(std::memory_order_relaxed);
    return 0.f;
}
void StageView::change(const juce::String& id,float x) {
    if(auto* param=processor.parameters.getParameter(id))param->setValueNotifyingHost(param->convertTo0to1(x));
}
void StageView::startGesture(int index) {
    if(index==-2)return;
    const auto suffix=juce::String(index);
    processor.parameters.getParameter(index==-1?"micX":"x"+suffix)->beginChangeGesture();
    processor.parameters.getParameter(index==-1?"micY":"y"+suffix)->beginChangeGesture();
}
void StageView::finishGesture(int index) {
    if(index==-2)return;
    const auto suffix=juce::String(index);
    processor.parameters.getParameter(index==-1?"micX":"x"+suffix)->endChangeGesture();
    processor.parameters.getParameter(index==-1?"micY":"y"+suffix)->endChangeGesture();
}
void StageView::paint(juce::Graphics& g) {
    const auto full=getLocalBounds().toFloat();
    g.setColour(panel);g.fillRoundedRectangle(full,18);
    g.setColour(brass.withAlpha(.30f));g.drawRoundedRectangle(full.reduced(.9f),18,1.2f);
    const auto r=plot();
    g.saveState();g.reduceClipRegion(r.toNearestInt());
    if(cathedral.isValid())g.drawImageWithin(cathedral,(int)r.getX(),(int)r.getY(),
          (int)r.getWidth(),(int)r.getHeight(),juce::RectanglePlacement::stretchToFit);
    else {g.setColour(bg);g.fillRect(r);}
    // Simulated warm sanctuary wash over the perspective stage.
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack,r.getCentreX(),r.getY()+r.getHeight()*.24f,
                                            bg.withAlpha(.84f),r.getCentreX(),r.getBottom(),false));
    g.fillRect(r);
    g.setColour(brass.withAlpha(.65f));
    g.setFont(juce::Font(juce::FontOptions(11.f).withStyle("Bold")));
    g.drawText("MOVE THE SINGERS · HEAR THEIR POSITION",(int)r.getX()+20,(int)r.getY()+13,360,22,juce::Justification::left);
    g.setColour(cream.withAlpha(.48f));
    g.drawText("LISTENING STAGE / METRES",(int)r.getRight()-254,(int)r.getY()+13,230,22,juce::Justification::right);
    for(int i=0;i<4;++i) {
        const float x=value("x"+juce::String(i)), y=value("y"+juce::String(i));
        const juce::Colour colour=sectionColours[(size_t)i];
        const float height=juce::jlimit(16.f,25.f,21.f-(y-5.f)*.5f);
        for(int row=2;row>=0;--row)for(int col=0;col<4;++col) {
            const float dx=(col-1.5f)*.46f+(row%2)*.12f;
            const float dy=(row-1.f)*.45f;
            drawFigure(g,worldToPoint(x+dx,y+dy),colour,height*(1.f+.05f*(2-row)));
        }
        const auto center=worldToPoint(x,y);
        g.setColour(colour);
        g.setFont(juce::Font(juce::FontOptions(14.f).withStyle("Bold")));
        g.drawText(sectionNames[i],(int)center.x-82,(int)center.y+34,164,23,juce::Justification::centred);
    }
    const auto mic=worldToPoint(value("micX"),value("micY"));
    g.setColour(brass.withAlpha(.17f));g.fillEllipse(mic.x-31,mic.y-27,62,54);
    g.setColour(brass.withAlpha(.9f));g.drawEllipse(mic.x-22,mic.y-19,44,38,1.1f);
    g.setColour(brass);g.fillRoundedRectangle(mic.x-4,mic.y-15,8,17,3);
    g.drawLine(mic.x,mic.y+2,mic.x,mic.y+18,2);
    g.drawLine(mic.x-9,mic.y+18,mic.x+9,mic.y+18,1.5f);
    g.setFont(juce::Font(juce::FontOptions(12.f).withStyle("Bold")));
    g.drawText("MIC",(int)mic.x-43,(int)mic.y+23,86,20,juce::Justification::centred);
    const float rms=processor.inputLevel();
    g.setColour(brass.withAlpha(.30f));g.fillRoundedRectangle(r.getX()+24,r.getBottom()-23,116,5,2.5f);
    g.setColour(brass);g.fillRoundedRectangle(r.getX()+24,r.getBottom()-23,116*juce::jlimit(0.f,1.f,rms*7.f),5,2.5f);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.f)));
    g.drawText("INPUT",(int)r.getX()+26,(int)r.getBottom()-42,70,18,juce::Justification::left);
    g.restoreState();
}
void StageView::mouseDown(const juce::MouseEvent& e) {
    dragging=-2;
    float closest=51.f;
    const auto point=e.position;
    const auto mic=worldToPoint(value("micX"),value("micY"));
    if(point.getDistanceFrom(mic)<closest){closest=point.getDistanceFrom(mic);dragging=-1;}
    for(int i=0;i<4;++i) {
        const auto center=worldToPoint(value("x"+juce::String(i)),value("y"+juce::String(i)));
        const float d=point.getDistanceFrom(center);
        if(d<closest){closest=d;dragging=i;}
    }
    if(dragging!=-2){startGesture(dragging);mouseDrag(e);}
}
void StageView::mouseDrag(const juce::MouseEvent& e) {
    if(dragging==-2)return;
    const auto pt=pointToWorld(e.position);
    const bool mic=dragging==-1;
    const auto suffix=juce::String(dragging);
    change(mic?"micX":"x"+suffix,juce::jlimit(-9.f,9.f,pt.x));
    change(mic?"micY":"y"+suffix,mic?juce::jlimit(-8.f,8.f,pt.y):juce::jlimit(.7f,11.f,pt.y));
}
void StageView::mouseUp(const juce::MouseEvent&) {finishGesture(dragging);dragging=-2;}
void StageView::mouseDoubleClick(const juce::MouseEvent& e) {
    if(e.position.getDistanceFrom(worldToPoint(value("micX"),value("micY")))<42.f) {
        startGesture(-1);change("micX",0);change("micY",0);finishGesture(-1);
    }
}

OmniChoirAudioProcessorEditor::OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor& p)
    :AudioProcessorEditor(&p),processor(p),stage(p) {
    setSize(1140,812);setResizable(true,true);setResizeLimits(900,720,1750,1200);
    addAndMakeVisible(stage);
    addAndMakeVisible(scanButton);
    scanButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff503014));
    scanButton.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffffdfa0));
    scanButton.setTooltip("Play your vocal track in the DAW, then scan four seconds of incoming audio.");
    scanButton.onClick=[this] {
        if(auto* mode=processor.parameters.getParameter("mode"))mode->setValueNotifyingHost(mode->convertTo0to1(1.f));
        processor.requestScan();
    };
    addAndMakeVisible(choirButton);
    choirButton.setClickingTogglesState(true);
    choirButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff241817));
    choirButton.setColour(juce::TextButton::buttonOnColourId,brass);
    choirButton.setColour(juce::TextButton::textColourOffId,muted);
    choirButton.setColour(juce::TextButton::textColourOnId,juce::Colours::black);
    choirAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"choirOn",choirButton);
    addAndMakeVisible(scanStatus);
    scanStatus.setColour(juce::Label::textColourId,cream);
    scanStatus.setFont(juce::Font(juce::FontOptions(12.f)));
    scanStatus.setJustificationType(juce::Justification::centredLeft);
    // Row 1: LIVE/SCAN engine and SATB balance.
    addChoice("PROCESSING","mode");addChoice("HARMONY","harmony");addChoice("MIC ARRAY","micMode");
    addDial("SOPRANO","gain0");addDial("ALTO","gain1");addDial("TENOR","gain2");addDial("BASS","gain3");
    addDial("ENSEMBLE","ensemble");addDial("SENSITIVITY","sensitivity");addDial("CHOIR MIX","mix");
    // Row 2: spatial acoustics and master.
    addDial("MIC SPACING","width");addDial("ROOM SIZE","roomSize");addDial("REVERB TIME","roomSeconds");
    addDial("ABSORPTION","absorption");addDial("DIFFUSION","diffusion");
    addDial("ROOM RETURN","wet");addDial("OUTPUT","master");
    startTimerHz(15);
    timerCallback();
}
void OmniChoirAudioProcessorEditor::addDial(const juce::String& text,const juce::String& id) {
    auto component=std::make_unique<Dial>(processor,text,id);
    addAndMakeVisible(*component);controls.push_back(std::move(component));
}
void OmniChoirAudioProcessorEditor::addChoice(const juce::String& text,const juce::String& id) {
    auto component=std::make_unique<Choice>(processor,text,id);
    addAndMakeVisible(*component);controls.push_back(std::move(component));
}
void OmniChoirAudioProcessorEditor::timerCallback() {
    juce::String msg;
    switch(processor.scanStatus()) {
        case 1:msg="ANALYZING INPUT  "+juce::String((int)(processor.scanProgress()*100.f))+"%";break;
        case 2:msg="SCAN READY  ·  DOMINANT PITCH CLASS: "+juce::String(pitchClasses[processor.scanRoot()%12]);break;
        case 3:msg="NO CLEAR PITCH DETECTED  ·  PLAY A SOLO VOCAL AND RETRY";break;
        default:msg="LIVE: INSTANT CHOIR  ·  SCAN: PLAY AUDIO, THEN PRESS SCAN";break;
    }
    const float midi=processor.trackedMidi();
    if(midi>=0 && std::isfinite(midi)) {
        const int note=(int)std::round(midi);
        msg+="     INPUT NOTE: "+juce::String(pitchClasses[((note%12)+12)%12])+juce::String((int)std::floor(note/12.f)-1);
    }
    choirButton.setButtonText(choirButton.getToggleState()?"◉  CHOIR ON":"○  CHOIR OFF");
    scanStatus.setText(msg,juce::dontSendNotification);
}
void OmniChoirAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(bg);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff301b17),getWidth()*.5f,0,bg,getWidth()*.5f,170,false));
    g.fillRect(getLocalBounds().removeFromTop(170));
    g.setColour(brass);
    g.setFont(juce::Font(juce::FontOptions(30.f).withStyle("Bold")));
    g.drawText("OMNICHOIR",23,12,280,45,juce::Justification::centredLeft);
    g.setColour(cream);
    g.setFont(juce::Font(juce::FontOptions(14.f).withStyle("Bold")));
    g.drawText("LIVE CHOIRIFICATION  /  VOCAL EFFECT",301,24,430,24,juce::Justification::left);
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(11.f)));
    g.drawText("INSERT ON A VOCAL TRACK  ·  PLAY INPUT AUDIO  ·  DRAG EACH SECTION AND THE MIC",
               304,48,getWidth()-340,20,juce::Justification::left);
    g.setColour(stroke);g.drawLine(20,76,(float)getWidth()-20,76,1.2f);
    g.setColour(brass);
    g.setFont(juce::Font(juce::FontOptions(12.f).withStyle("Bold")));
    g.drawText("CHOIR  /  HARMONIZER",24,getHeight()-239,270,20,juce::Justification::left);
    g.drawText("MICROPHONES  /  CATHEDRAL",24,getHeight()-116,330,20,juce::Justification::left);
}
void OmniChoirAudioProcessorEditor::resized() {
    constexpr int margin=20,footer=245;
    scanButton.setBounds(margin,86,195,36);
    choirButton.setBounds(227,86,150,36);
    scanStatus.setBounds(390,86,getWidth()-412,36);
    stage.setBounds(margin,131,getWidth()-2*margin,getHeight()-131-footer);
    const int topY=getHeight()-214;
    const int bottomY=getHeight()-95;
    const int cellWidth=(getWidth()-2*margin)/10;
    for(int i=0;i<(int)controls.size();++i) {
        const bool second=i>=10;
        const int col=second?i-10:i;
        const int x=margin+col*cellWidth+(second?cellWidth/2:0);
        controls[(size_t)i]->setBounds(x,second?bottomY:topY,cellWidth-2,95);
    }
}
