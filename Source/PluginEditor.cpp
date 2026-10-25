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
        label.setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
        dial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        dial.setTextBoxStyle(juce::Slider::TextBoxBelow,false,65,16);
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
    void resized() override {label.setBounds(0,0,getWidth(),16);dial.setBounds(1,16,getWidth()-2,getHeight()-16);}
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
        label.setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
        combo.addItemList(p.parameters.getParameter(id)->getAllValueStrings(),1);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,id,combo);
        combo.setColour(juce::ComboBox::backgroundColourId,panel);
        combo.setColour(juce::ComboBox::textColourId,cream);
        combo.setColour(juce::ComboBox::outlineColourId,stroke);
        combo.setColour(juce::ComboBox::arrowColourId,brass);
        addAndMakeVisible(label);addAndMakeVisible(combo);
    }
    void resized() override {label.setBounds(0,3,getWidth(),16);combo.setBounds(4,26,getWidth()-8,27);}
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


MeterStrip::MeterStrip(OmniChoirAudioProcessor& p):processor(p) {startTimerHz(20);}
void MeterStrip::paint(juce::Graphics& g) {
    const auto area=getLocalBounds().toFloat();
    g.setColour(panel);g.fillRoundedRectangle(area,5.f);
    g.setColour(stroke);g.drawRoundedRectangle(area.reduced(.5f),5.f,1.f);
    const auto& m=processor.meters();
    const float top=area.getY()+4.f,full=area.getHeight()-8.f;
    // linear level -> a -54 dB .. 0 dB bar
    const auto fraction=[](float level) {
        return juce::jlimit(0.f,1.f,(20.f*std::log10(std::max(level,1.0e-5f))+54.f)/54.f);
    };
    float x=area.getX()+7.f;
    const auto bar=[&](float level,juce::Colour colour) {
        g.setColour(juce::Colours::white.withAlpha(.06f));g.fillRect(x,top,7.f,full);
        const float h=full*fraction(level);
        g.setColour(colour);g.fillRect(x,top+full-h,7.f,h);
        x+=10.f;
    };
    for(int i=0;i<4;++i)bar(m.section[i].load(std::memory_order_relaxed)*4.f,sectionColours[(size_t)i]);
    x+=4.f;
    bar(m.leadIn.load(std::memory_order_relaxed),cream);
    x+=4.f;
    bar(m.outPeakL.load(std::memory_order_relaxed),brass);
    bar(m.outPeakR.load(std::memory_order_relaxed),brass);
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
    const juce::String text=juce::String("WIDTH ")+juce::String((int)std::lround(100.f*m.width.load(std::memory_order_relaxed)))
        +"%   CORR "+juce::String(m.correlation.load(std::memory_order_relaxed),2);
    g.drawText(text,(int)x+6,(int)area.getY(),(int)(area.getRight()-x-8),(int)area.getHeight(),juce::Justification::centredLeft);
}

StageView::StageView(OmniChoirAudioProcessor& p):processor(p),
    cathedral(juce::ImageCache::getFromMemory(BinaryData::cathedral_stage_jpg,
                                              BinaryData::cathedral_stage_jpgSize)) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    startTimerHz(20);
}
juce::Rectangle<float> StageView::plot() const {return getLocalBounds().toFloat().reduced(8.f,8.f);}
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
juce::Point<float> StageView::leadPosition() const {
    // Lead is always distinct from the gold microphone. Her X/Y parameters
    // are offsets, just as in the original web stage JS.
    return worldToPoint(value("micX")-2.35f+value("leadX"),value("micY")+1.9f+value("leadY"));
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
void StageView::paint(juce::Graphics& g) {
    const auto full=getLocalBounds().toFloat();
    g.setColour(panel);g.fillRoundedRectangle(full,12);
    g.setColour(brass.withAlpha(.36f));g.drawRoundedRectangle(full.reduced(.6f),12,1.f);
    const auto r=plot();
    g.saveState();g.reduceClipRegion(r.toNearestInt());
    if(cathedral.isValid())
        g.drawImageWithin(cathedral,(int)r.getX(),(int)r.getY(),(int)r.getWidth(),(int)r.getHeight(),
                          juce::RectanglePlacement::stretchToFit);
    else {g.setColour(bg);g.fillRect(r);}
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack,r.getCentreX(),r.getY(),
                            bg.withAlpha(.85f),r.getCentreX(),r.getBottom(),false));
    g.fillRect(r);
    g.setFont(juce::Font(juce::FontOptions(9.f).withStyle("Bold")));
    g.setColour(brass.withAlpha(.85f));
    g.drawText("DRAG SINGERS  /  LEAD  /  MIC",(int)r.getX()+12,(int)r.getY()+9,280,16,juce::Justification::left);
    g.setColour(cream.withAlpha(.48f));
    g.drawText("CATHEDRAL STAGE",(int)r.getRight()-153,(int)r.getY()+9,140,16,juce::Justification::right);
    const bool on=value("choirOn")>.5f;
    for(int i=0;i<4;++i) {
        const float x=value("x"+juce::String(i)), y=value("y"+juce::String(i));
        const auto c=sectionColours[(size_t)i].withMultipliedSaturation(on?1.f:.35f);
        {   // live glow: how loudly this section is singing right now
            const float level=processor.meters().section[i].load(std::memory_order_relaxed);
            const float glow=juce::jlimit(0.f,1.f,(20.f*std::log10(std::max(level*4.f,1.0e-5f))+54.f)/54.f);
            const auto gc=worldToPoint(x,y);
            g.setColour(c.withAlpha(on?.04f+.30f*glow:0.f));
            g.fillEllipse(gc.x-86.f,gc.y-44.f,172.f,88.f);
        }
        const float singerSize=juce::jlimit(11.f,18.f,(getHeight()*.05f)-.18f*(y-5.f));
        for(int row=2;row>=0;--row)for(int col=0;col<4;++col) {
            const float dx=(col-1.5f)*.47f+(row%2)*.12f,dy=(row-1.f)*.53f;
            drawFigure(g,worldToPoint(x+dx,y+dy),c.withMultipliedAlpha(on?1.f:.5f),singerSize);
        }
        const auto center=worldToPoint(x,y);
        g.setColour(c);
        g.setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
        g.drawText(sectionNames[i],(int)center.x-49,(int)center.y+20,98,14,juce::Justification::centred);
    }
    // From demo.js: white lead at a separate world position near the main mic,
    // phrase glow follows the input envelope, independent of choir power.
    const auto lead=leadPosition();
    const float level=juce::jlimit(0.f,1.f,processor.inputLevel()*5.f);
    g.setColour(cream.withAlpha(.06f+level*.30f));
    g.fillEllipse(lead.x-23,lead.y-24,46,48);
    drawFigure(g,lead,juce::Colour(0xffeee8d9),20.f);
    g.setColour(cream);
    g.setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
    g.drawText("LEAD",(int)lead.x-34,(int)lead.y+22,68,16,juce::Justification::centred);
    const auto mic=worldToPoint(value("micX"),value("micY"));
    g.setColour(brass.withAlpha(.13f));g.fillEllipse(mic.x-21,mic.y-19,42,38);
    g.setColour(brass.withAlpha(.9f));g.drawEllipse(mic.x-17,mic.y-15,34,30,1.1f);
    g.setColour(brass);g.fillRoundedRectangle(mic.x-3,mic.y-13,6,13,2.5f);
    g.drawLine(mic.x,mic.y,mic.x,mic.y+15,1.4f);
    g.drawLine(mic.x-7,mic.y+15,mic.x+7,mic.y+15,1.2f);
    g.drawText("MIC",(int)mic.x-26,(int)mic.y+16,52,15,juce::Justification::centred);
    g.setColour(brass.withAlpha(.22f));g.fillRoundedRectangle(r.getX()+12,r.getBottom()-15,85,4,2);
    g.setColour(brass);g.fillRoundedRectangle(r.getX()+12,r.getBottom()-15,85*level,4,2);
    g.restoreState();
}
void StageView::mouseDown(const juce::MouseEvent& e) {
    dragging=-2;
    const auto pos=e.position;
    float closest=std::max(18.f,getWidth()*.033f);
    auto test=[&](int index,juce::Point<float> target) {
        const float d=pos.getDistanceFrom(target);
        if(d<closest) {closest=d;dragging=index;}
    };
    // Hit any member of the group, not only its centroid, like demo.js.
    for(int i=0;i<4;++i) {
        const float x=value("x"+juce::String(i)),y=value("y"+juce::String(i));
        for(int row=0;row<3;++row)for(int col=0;col<4;++col)
            test(i,worldToPoint(x+(col-1.5f)*.47f+(row%2)*.12f,y+(row-1.f)*.53f));
    }
    test(-1,worldToPoint(value("micX"),value("micY")));
    // Give lead priority when body overlaps mic ring, as original JS.
    if(pos.getDistanceFrom(leadPosition())<27.f)dragging=-3;
    if(dragging!=-2) {
        micStartX=value("micX");micStartY=value("micY");
        leadStartX=value("leadX");leadStartY=value("leadY");
        startGesture(dragging);mouseDrag(e);
    }
}
void StageView::mouseDrag(const juce::MouseEvent& e) {
    if(dragging==-2)return;
    const auto p=pointToWorld(e.position);
    if(dragging==-1) {
        const float x=juce::jlimit(-9.f,9.f,p.x),y=juce::jlimit(-8.f,8.f,p.y);
        change("micX",x);change("micY",y);
        // Keep the white lead stationary in world space when the mic moves.
        change("leadX",juce::jlimit(-9.f,9.f,leadStartX+micStartX-x));
        change("leadY",juce::jlimit(-5.f,12.f,leadStartY+micStartY-y));
    } else if(dragging==-3) {
        change("leadX",juce::jlimit(-9.f,9.f,p.x-value("micX")+2.35f));
        change("leadY",juce::jlimit(-5.f,12.f,p.y-value("micY")-1.9f));
    } else {
        const auto s=juce::String(dragging);
        change("x"+s,juce::jlimit(-9.f,9.f,p.x));
        change("y"+s,juce::jlimit(.7f,11.f,p.y));
    }
}
void StageView::mouseUp(const juce::MouseEvent&) {finishGesture(dragging);dragging=-2;}
void StageView::mouseDoubleClick(const juce::MouseEvent& e) {
    if(e.position.getDistanceFrom(leadPosition())<28) {
        startGesture(-3);change("leadX",0);change("leadY",0);finishGesture(-3);
    } else if(e.position.getDistanceFrom(worldToPoint(value("micX"),value("micY")))<24) {
        startGesture(-1);change("micX",0);change("micY",0);finishGesture(-1);
    }
}

OmniChoirAudioProcessorEditor::OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor& p)
    :AudioProcessorEditor(&p),processor(p),stage(p),meterStrip(p) {
    // 71% of previous 1140x812 pixel dimensions = 29% smaller on both axes.
    setSize(810,577);setResizable(true,true);setResizeLimits(720,510,1400,1000);
    addAndMakeVisible(stage);addAndMakeVisible(scanButton);addAndMakeVisible(choirButton);
    addAndMakeVisible(meterStrip);addAndMakeVisible(presetBox);
    addAndMakeVisible(abButton);addAndMakeVisible(irButton);addAndMakeVisible(irClearButton);
    presetBox.setTextWhenNothingSelected("STAGE PRESET");
    for(int i=0;i<choir::kNumStagePresets;++i)presetBox.addItem(choir::kStagePresets[i].name,i+1);
    presetBox.setTooltip("Sets placement and room only; harmony, mix, lead level and output are never touched.");
    presetBox.onChange=[this] {
        const int index=presetBox.getSelectedId()-1;
        if(index>=0)processor.applyStagePreset(index);
        presetBox.setSelectedId(0,juce::dontSendNotification);
    };
    abButton.setClickingTogglesState(true);
    abButton.setTooltip("A/B: the faithful stage against the classic v0.5 stage. Same latency, so switching never shifts timing.");
    abAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"classicStage",abButton);
    irButton.onClick=[this]{chooseImpulseResponse();};
    irClearButton.onClick=[this]{processor.clearImpulseResponse();};
    irButton.setTooltip("Load a room recording (impulse response) for the HALL. The HALL dial sets how much you hear.");
    irClearButton.setTooltip("Back to the algorithmic hall");
    for(auto* b:{&abButton,&irButton,&irClearButton}) {
        b->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff342522));
        b->setColour(juce::TextButton::textColourOffId,cream);
    }
    abButton.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff6e8f5a));
    abButton.setColour(juce::TextButton::textColourOnId,juce::Colours::black);
    scanButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff54341f));
    scanButton.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffffdca0));
    scanButton.setTooltip("Legacy 4-second analysis of incoming DAW audio. Full take import is on SCAN page.");
    scanButton.onClick=[this] {
        if(auto* a=processor.parameters.getParameter("mode"))
            a->setValueNotifyingHost(a->convertTo0to1(1.f));
        processor.requestScan();
    };
    choirButton.setClickingTogglesState(true);
    choirButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff342522));
    choirButton.setColour(juce::TextButton::buttonOnColourId,brass);
    choirButton.setColour(juce::TextButton::textColourOffId,muted);
    choirButton.setColour(juce::TextButton::textColourOnId,juce::Colours::black);
    choirAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters,"choirOn",choirButton);
    scanStatus.setColour(juce::Label::textColourId,cream);
    scanStatus.setFont(juce::Font(juce::FontOptions(10.f)));
    addAndMakeVisible(scanStatus);
    const char* names[]={"VOICE", "SPACE", "ROOM", "SCAN / EDIT", "STEMS", "SINGERS"};
    for(int i=0;i<6;++i) {
        tabs[(size_t)i].setButtonText(names[i]);
        tabs[(size_t)i].onClick=[this,i]{selectTab(i);};
        addAndMakeVisible(tabs[(size_t)i]);
    }
    addChoice("PROCESS","mode",0);addChoice("HARMONY","harmony",0);
    addDial("SOPRANO","gain0",0);addDial("ALTO","gain1",0);
    addDial("TENOR","gain2",0);addDial("BASS","gain3",0);
    addDial("ENSEMBLE","ensemble",0);addDial("HUMANIZE","humanize",0);
    addDial("CHARACTER","formant",0);addDial("MIX","mix",0);
    addDial("LEAD LEVEL","leadGain",1);addDial("MIC SPACING","width",1);
    addChoice("MIC ARRAY","micMode",1);addDial("SENSITIVITY","sensitivity",1);
    addDial("SOPRANO","gain0",1);addDial("ALTO","gain1",1);
    addDial("TENOR","gain2",1);addDial("BASS","gain3",1);
    addDial("ROOM SIZE","roomSize",2);addDial("REVERB TIME","roomSeconds",2);
    addDial("ABSORPTION","absorption",2);addDial("DIFFUSION","diffusion",2);
    addDial("ROOM RETURN","wet",2);addDial("HALL","hall",2);
    addDial("HALL TIME","hallSeconds",2);addDial("OUTPUT","master",2);
    addDial("VOICE BLEND","voiceBlend",5);addDial("VOWEL","choirVowel",5);addDial("FOLLOW LEAD","vowelFollow",5);
    addDial("BREATH","choirBreath",5);addDial("VIBRATO","vibrato",5);
    addAndMakeVisible(importButton);addAndMakeVisible(exportButton);addAndMakeVisible(midiButton);
    midiButton.onClick=[this]{chooseMidi();};
    addAndMakeVisible(loadStemsButton);addAndMakeVisible(mixStemsButton);
    loadStemsButton.onClick=[this]{chooseStemManifest();};
    mixStemsButton.onClick=[this]{chooseStemOutput();};
    addAndMakeVisible(stemsStatus);addAndMakeVisible(stemsHelp);
    stemsStatus.setColour(juce::Label::textColourId,cream);
    stemsStatus.setFont(juce::Font(juce::FontOptions(11.f)));
    stemsHelp.setColour(juce::Label::textColourId,muted);
    stemsHelp.setFont(juce::Font(juce::FontOptions(12.f)));
    stemsHelp.setJustificationType(juce::Justification::centredLeft);
    stemsHelp.setText("1. EXPORT MIDI ON SCAN TAB  >  2. RECORD OR RENDER VOICE STEMS  >  3. LOAD MANIFEST  >  MIX WAV",juce::dontSendNotification);
    importButton.onClick=[this]{chooseInput();};
    exportButton.onClick=[this]{chooseOutput();};
    addAndMakeVisible(notes);
    notes.setRowHeight(22);notes.setColour(juce::ListBox::backgroundColourId,panel);
    notes.setColour(juce::ListBox::outlineColourId,stroke);
    addAndMakeVisible(takeStatus);
    takeStatus.setColour(juce::Label::textColourId,cream);
    takeStatus.setFont(juce::Font(juce::FontOptions(10.f)));
    for(int i=0;i<8;++i) {
        adjust[(size_t)i].setButtonText(juce::String("SATB"[i/2])+(i%2?"+":"-"));
        adjust[(size_t)i].onClick=[this,i] {
            take.changeNote(notes.getSelectedRow(),i/2,i%2?1:-1);
            visibleScore=take.snapshot(); notes.repaint();
        };
        addAndMakeVisible(adjust[(size_t)i]);
    }
    selectTab(0);
    startTimerHz(10);
}
OmniChoirAudioProcessorEditor::~OmniChoirAudioProcessorEditor() {
    stopTimer();chooser.reset();
}
void OmniChoirAudioProcessorEditor::addDial(const juce::String& name,const juce::String& id,int page) {
    auto c=std::make_unique<Dial>(processor,name,id);addAndMakeVisible(*c);
    controls.push_back({page,std::move(c)});
}
void OmniChoirAudioProcessorEditor::addChoice(const juce::String& name,const juce::String& id,int page) {
    auto c=std::make_unique<Choice>(processor,name,id);addAndMakeVisible(*c);
    controls.push_back({page,std::move(c)});
}
void OmniChoirAudioProcessorEditor::selectTab(int page) {
    currentTab=page;
    for(auto& c:controls)c.item->setVisible(c.page==page);
    const bool show=page==3;
    importButton.setVisible(show);exportButton.setVisible(show);midiButton.setVisible(show);
    loadStemsButton.setVisible(page==4);mixStemsButton.setVisible(page==4);
    stemsHelp.setVisible(page==4);stemsStatus.setVisible(page==4);
    notes.setVisible(show);takeStatus.setVisible(show);
    for(auto& b:adjust)b.setVisible(show);
    for(int i=0;i<6;++i) {
        tabs[(size_t)i].setColour(juce::TextButton::buttonColourId,
            i==page?brass:juce::Colour(0xff33221d));
        tabs[(size_t)i].setColour(juce::TextButton::textColourOffId,
            i==page?juce::Colours::black:cream);
    }
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
void OmniChoirAudioProcessorEditor::paintListBoxItem(int index,juce::Graphics& g,int width,int height,bool selected) {
    if(index<0||index>=(int)visibleScore.notes.size())return;
    const auto& e=visibleScore.notes[(size_t)index];
    g.fillAll(selected?juce::Colour(0xff5d3b28):juce::Colour(0xff1c1418));
    g.setColour(cream);
    g.setFont(juce::Font(juce::FontOptions(11.f)));
    const auto name=[](int midi) {
        return juce::String(pitchClasses[(midi%12+12)%12])+juce::String(midi/12-1);
    };
    juce::String line=juce::String(e.start,1)+"s    LEAD "+name(e.melody)+"   ";
    for(int i=0;i<4;++i)line+=juce::String("SATB"[i])+":"+name(e.satb[(size_t)i])+"   ";
    g.drawText(line,7,0,width-12,height,juce::Justification::centredLeft);
}
void OmniChoirAudioProcessorEditor::timerCallback() {
    juce::String msg;
    switch(processor.scanStatus()) {
        case 1:msg="LIVE INPUT SCAN "+juce::String((int)(processor.scanProgress()*100.f))+"%";break;
        case 2:msg="LIVE INPUT KEY: "+juce::String(pitchClasses[processor.scanRoot()%12]);break;
        case 3:msg="NO CLEAR LIVE PITCH: USE CLEAN SOLO VOCAL";break;
        default:msg="INSERT ON VOCAL TRACK / LIVE MODE = LOW-LATENCY / SCAN TAB = FULL TAKE";break;
    }
    scanStatus.setText(msg,juce::dontSendNotification);
    choirButton.setButtonText(choirButton.getToggleState()?"CHOIR ON":"CHOIR OFF");
    abButton.setButtonText(abButton.getToggleState()?"STAGE: CLASSIC v0.5":"STAGE: FAITHFUL");
    irButton.setButtonText(processor.hasImpulseResponse()?processor.impulseResponseName().toUpperCase().substring(0,10):juce::String("LOAD IR"));
    irClearButton.setEnabled(processor.hasImpulseResponse());
    const int status=take.status();
    if(status!=lastTakeStatus && status!=1 &&status!=3 && status!=5 && status!=7) {
        visibleScore=take.snapshot();notes.updateContent();
        lastTakeStatus=status;
    }
    switch(status) {
        case 1:msg="ANALYZING FULL TAKE  "+juce::String((int)(100*take.progress()))+"%";break;
        case 2:msg=take.fileName()+"  /  "+juce::String(visibleScore.notes.size())+" EDITABLE NOTES";break;
        case 3:msg="RENDERING WAV  "+juce::String((int)(100*take.progress()))+"%";break;
        case 4:msg="WAV RENDERED  /  "+juce::String(visibleScore.notes.size())+" NOTES";break;
        case 5:msg="WRITING MIDI  "+juce::String((int)(100*take.progress()))+"%";break;
        case 6:msg="MIDI EXPORTED (5 PARTS / 120 BPM)";break;
        case 7:msg="MIXING STEMS  "+juce::String((int)(100*take.progress()))+"%";break;
        case 8:msg="STEM WAV RENDERED";break;
        case -1:msg="ERROR: "+take.errorMessage();break;
        default:msg="IMPORT A VOCAL WAV / AIFF / FLAC FOR COMPLETE SCAN";
    }
    takeStatus.setText(msg,juce::dontSendNotification);
    const bool idle=status!=1&&status!=3&&status!=5&&status!=7;
    exportButton.setEnabled(idle && !visibleScore.notes.empty());
    midiButton.setEnabled(idle && !visibleScore.notes.empty());
    mixStemsButton.setEnabled(idle && stemManifest.existsAsFile());
    for(auto& b:adjust)b.setEnabled(idle && !visibleScore.notes.empty() && notes.getSelectedRow()>=0);
    if(status==7 || status==8 || status==-1)stemsStatus.setText(msg,juce::dontSendNotification);
}
void OmniChoirAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(bg);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff301b17),getWidth()*.5f,0,
                                            bg,getWidth()*.5f,100,false));
    g.fillRect(getLocalBounds().removeFromTop(100));
    g.setColour(brass);g.setFont(juce::Font(juce::FontOptions(23.f).withStyle("Bold")));
    g.drawText("OMNICHOIR",14,8,220,33,juce::Justification::left);
    g.setColour(cream);g.setFont(juce::Font(juce::FontOptions(11.f).withStyle("Bold")));
    g.drawText("LIVE CHOIRIFICATION / SCAN & RENDER",247,16,getWidth()-265,23,juce::Justification::left);
    g.setColour(stroke);g.drawLine(14,47,(float)getWidth()-14,47,1.f);
    g.setColour(brass);g.setFont(juce::Font(juce::FontOptions(10.f)));
    g.drawText("YOUR VOICE / A FULL CHOIR  |  16 SINGERS / 4 SECTIONS",15,getHeight()-12,getWidth()-30,11,
               juce::Justification::centredLeft);
}
void OmniChoirAudioProcessorEditor::resized() {
    const int w=getWidth(),h=getHeight(),m=12;
    scanButton.setBounds(m,54,149,29);
    choirButton.setBounds(168,54,112,29);
    scanStatus.setBounds(290,54,w-304,29);
    const int tabsY=h-155;
    presetBox.setBounds(m,88,150,26);
    abButton.setBounds(m+158,88,132,26);
    irButton.setBounds(m+298,88,78,26);irClearButton.setBounds(m+380,88,26,26);
    meterStrip.setBounds(m+414,88,std::max(60,w-2*m-414),26);
    stage.setBounds(m,120,w-2*m,tabsY-126);
    const int tabWidth=(w-2*m)/6;
    for(int i=0;i<6;++i)tabs[(size_t)i].setBounds(m+i*tabWidth,tabsY,tabWidth-2,27);
    const int controlsY=tabsY+32, controlsH=h-controlsY-23;
    for(const int page:{0,1,2,5}) {
        std::vector<juce::Component*> pageControls;
        for(auto& c:controls)if(c.page==page)pageControls.push_back(c.item.get());
        const int n=(int)pageControls.size(),cw=(w-2*m)/std::max(1,n);
        for(int i=0;i<n;++i)
            pageControls[(size_t)i]->setBounds(m+i*cw,controlsY,cw-2,controlsH);
    }
    importButton.setBounds(m,controlsY,138,27);
    exportButton.setBounds(m,controlsY+30,138,27);
    midiButton.setBounds(m,controlsY+60,138,27);
    takeStatus.setBounds(156,controlsY,w-156-136,25);
    notes.setBounds(156,controlsY+27,w-156-136-m,std::max(20,controlsH-27));
    loadStemsButton.setBounds(m,controlsY,154,31);
    mixStemsButton.setBounds(m+162,controlsY,154,31);
    stemsStatus.setBounds(m+324,controlsY,w-(m+324)-m,31);
    stemsHelp.setBounds(m,controlsY+40,w-2*m,std::max(20,controlsH-48));
    const int ax=w-129;
    for(int i=0;i<8;++i)
        adjust[(size_t)i].setBounds(ax+(i%2)*58,controlsY+(i/2)*21,55,20);
}
