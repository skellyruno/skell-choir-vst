#include "PluginEditor.h"
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

// ----------------------------------------------------------------------------------------------- controls
class Dial final : public juce::Component, public juce::SettableTooltipClient {
public:
    Dial(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id,const juce::String& tip,ValueFormat format,float unity)
        : attachment(p.parameters,id,dial) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,kMuted);
        label.setFont(boldFont(10.f));
        label.setInterceptsMouseClicks(false,false);
        dial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        dial.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,16);
        dial.setColour(juce::Slider::rotarySliderFillColourId,kGold);
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
private:
    juce::Label label;
    juce::Slider dial;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Fader final : public juce::Component, public juce::SettableTooltipClient {
public:
    Fader(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id,juce::Colour accent,const juce::String& tip,float unity)
        : attachment(p.parameters,id,slider) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,accent);
        label.setFont(boldFont(10.f));
        label.setInterceptsMouseClicks(false,false);
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,50,15);
        slider.setColour(juce::Slider::trackColourId,accent);
        const auto text=makeText(ValueFormat::GainDb,unity,slider.getMinimum(),slider.getMaximum());
        slider.textFromValueFunction=text.text;
        slider.valueFromTextFunction=text.parse;
        slider.updateText();
        setTooltip(tip);slider.setTooltip(tip);
        addAndMakeVisible(label);addAndMakeVisible(slider);
    }
    void resized() override {label.setBounds(0,0,getWidth(),16);slider.setBounds(0,16,getWidth(),getHeight()-16);}
private:
    juce::Label label;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Choice final : public juce::Component, public juce::SettableTooltipClient {
public:
    Choice(OmniChoirAudioProcessor& p,const juce::String& title,const juce::String& id,const juce::String& tip) {
        label.setText(title,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId,kMuted);
        label.setFont(boldFont(10.f));
        label.setInterceptsMouseClicks(false,false);
        combo.addItemList(p.parameters.getParameter(id)->getAllValueStrings(),1);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,id,combo);
        setTooltip(tip);combo.setTooltip(tip);
        addAndMakeVisible(label);addAndMakeVisible(combo);
    }
    void resized() override {label.setBounds(2,0,getWidth()-2,16);combo.setBounds(0,18,getWidth(),juce::jmin(28,getHeight()-18));}
private:
    juce::Label label;
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// A singer: a robed figure with a gradient and a rim light.
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
StageView::StageView(OmniChoirAudioProcessor& p):processor(p),
    cathedral(juce::ImageCache::getFromMemory(BinaryData::cathedral_stage_jpg,
                                              BinaryData::cathedral_stage_jpgSize)) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
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
juce::Point<float> StageView::leadPosition() const {
    // Lead is always distinct from the gold microphone. Her X/Y parameters
    // are offsets, just as in the original web stage JS.
    return worldToPoint(value("micX")-2.35f+value("leadX"),value("micY")+1.9f+value("leadY"));
}
int StageView::hit(juce::Point<float> pos) const {
    int found=-2;
    float closest=std::max(18.f,(float)getWidth()*.033f);
    const auto test=[&](int index,juce::Point<float> target) {
        const float d=pos.getDistanceFrom(target);
        if(d<closest) {closest=d;found=index;}
    };
    // Hit any member of the group, not only its centroid, like demo.js.
    for(int i=0;i<4;++i) {
        const float x=value("x"+juce::String(i)),y=value("y"+juce::String(i));
        for(int row=0;row<3;++row)for(int col=0;col<4;++col)
            test(i,worldToPoint(x+(col-1.5f)*.47f+(row%2)*.12f,y+(row-1.f)*.53f));
    }
    test(-1,worldToPoint(value("micX"),value("micY")));
    // Give the lead priority when her body overlaps the mic ring, as in the original JS.
    if(pos.getDistanceFrom(leadPosition())<27.f)found=-3;
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
    change("micX",x);change("micY",y);
    // Keep the white lead stationary in world space when the mic moves.
    change("leadX",juce::jlimit(-9.f,9.f,leadStartX+micStartX-x));
    change("leadY",juce::jlimit(-5.f,12.f,leadStartY+micStartY-y));
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
        if(cathedral.isValid())
            g.drawImageWithin(cathedral,(int)r.getX(),(int)r.getY(),(int)r.getWidth(),(int)r.getHeight(),
                              juce::RectanglePlacement(juce::RectanglePlacement::xMid|juce::RectanglePlacement::yMid
                                                       |juce::RectanglePlacement::fillDestination));
        else {g.setColour(kBg);g.fillRect(r);}
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
        const juce::Colour light(0xffffe6a8);
        g.setGradientFill(juce::ColourGradient(light.withAlpha(.10f+.14f*input),r.getCentreX(),shaftTop,
                                               light.withAlpha(0.f),r.getCentreX(),r.getBottom(),false));
        g.fillPath(shaft);
        // dark corners
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack,r.getCentreX(),r.getCentreY(),
                                               juce::Colour(0xff040205).withAlpha(.78f),r.getX(),r.getY(),true));
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
            g.setColour(juce::Colour(0xffffeec0).withAlpha(juce::jlimit(0.f,1.f,alpha)));
            const float size=1.1f+1.6f*hash(i,5);
            g.fillEllipse(px,py,size,size);
        }
        // the four sections
        for(int i=0;i<4;++i) {
            const float x=value("x"+juce::String(i)),y=value("y"+juce::String(i));
            const auto base=kSection[(size_t)i];
            const auto c=base.withMultipliedSaturation(on?1.f:.3f);
            const float glow=on?levelToGlow(processor.meters().section[i].load(std::memory_order_relaxed)*4.f):0.f;   // how loudly it sings right now
            const auto gc=worldToPoint(x,y);
            g.setGradientFill(juce::ColourGradient(c.withAlpha(.05f+.34f*glow),gc.x,gc.y,c.withAlpha(0.f),gc.x+96.f,gc.y,true));
            g.fillEllipse(gc.x-110.f,gc.y-62.f,220.f,124.f);
            const float singerSize=juce::jlimit(11.f,19.f,(getHeight()*.05f)-.18f*(y-5.f))*(1.f+.07f*glow);
            for(int row=2;row>=0;--row)for(int col=0;col<4;++col) {
                const float dx=(col-1.5f)*.47f+(row%2)*.12f,dy=(row-1.f)*.53f;
                drawFigure(g,worldToPoint(x+dx,y+dy),c.withMultipliedAlpha(on?1.f:.5f),singerSize);
            }
            if(hover==i||dragging==i) {g.setColour(base.withAlpha(.85f));g.drawEllipse(gc.x-72.f,gc.y-36.f,144.f,72.f,1.4f);}
            const auto tag=worldToPoint(x,y-1.05f);
            const juce::Rectangle<float> pill(tag.x-34.f,tag.y-8.f,68.f,16.f);
            g.setColour(kBg.withAlpha(.62f));g.fillRoundedRectangle(pill,8.f);
            g.setColour(c.withAlpha(.9f));g.drawRoundedRectangle(pill.reduced(.5f),8.f,1.f);
            g.setFont(boldFont(9.5f));
            g.drawText(sectionNames[i],pill,juce::Justification::centred);
        }
        // the lead: glows while you sing
        const auto lead=leadPosition();
        g.setGradientFill(juce::ColourGradient(kLead.withAlpha(.08f+.34f*input),lead.x,lead.y,kLead.withAlpha(0.f),lead.x+44.f,lead.y,true));
        g.fillEllipse(lead.x-48.f,lead.y-48.f,96.f,96.f);
        if(hover==-3||dragging==-3) {g.setColour(kLead.withAlpha(.8f));g.drawEllipse(lead.x-27.f,lead.y-29.f,54.f,58.f,1.3f);}
        drawFigure(g,lead,juce::Colour(0xffeee8d9),20.f);
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
        g.setColour(kMuted);g.setFont(boldFont(8.5f));
        g.drawText("INPUT",juce::Rectangle<float>(r.getX()+12.f,r.getBottom()-31.f,60.f,12.f),juce::Justification::left);
        g.setColour(kGold.withAlpha(.22f));g.fillRoundedRectangle(r.getX()+12.f,r.getBottom()-16.f,96.f,4.f,2.f);
        g.setColour(kGold);g.fillRoundedRectangle(r.getX()+12.f,r.getBottom()-16.f,96.f*input,4.f,2.f);
    }
    g.setColour(kGold.withAlpha(.38f));g.drawRoundedRectangle(full.reduced(.6f),14.f,1.2f);
}
void StageView::mouseDown(const juce::MouseEvent& e) {
    dragging=hit(e.position);
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
        moveMic(juce::jlimit(-9.f,9.f,p.x),juce::jlimit(-8.f,8.f,p.y));
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
void StageView::mouseMove(const juce::MouseEvent& e) {hover=hit(e.position);}
void StageView::mouseExit(const juce::MouseEvent&) {hover=-2;}
void StageView::mouseDoubleClick(const juce::MouseEvent& e) {
    // Double-click puts the thing back where it started: to its own default, not to zero.
    const int what=hit(e.position);
    if(what==-3) {
        startGesture(-3);resetParameter("leadX");resetParameter("leadY");finishGesture(-3);
    } else if(what==-1) {
        micStartX=value("micX");micStartY=value("micY");leadStartX=value("leadX");leadStartY=value("leadY");
        startGesture(-1);moveMic(0.f,0.f);finishGesture(-1);
    } else if(what>=0) {
        const auto s=juce::String(what);
        startGesture(what);resetParameter("x"+s);resetParameter("y"+s);finishGesture(what);
    }
}

// ================================================================================================ editor
OmniChoirAudioProcessorEditor::OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor& p)
    :AudioProcessorEditor(&p),processor(p),stage(p),meterStrip(p) {
    setLookAndFeel(&lnf);
    setSize(860,600);setResizable(true,true);setResizeLimits(740,540,1500,1050);
    addAndMakeVisible(stage);addAndMakeVisible(meterStrip);

    // ---- header: scan, stage preset, choir on/off
    scanButton.setComponentID("action");
    scanButton.setTooltip("Listens to 4 seconds of your singing and finds the key. Then set PROCESS to Scan / Harmony.");
    scanButton.onClick=[this] {
        if(auto* a=processor.parameters.getParameter("mode"))
            a->setValueNotifyingHost(a->convertTo0to1(1.f));
        processor.requestScan();
    };
    choirButton.setComponentID("power");
    choirButton.setClickingTogglesState(true);
    choirButton.setTooltip("Choir on or off. Your own voice is always heard.");
    choirAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters,"choirOn",choirButton);
    addAndMakeVisible(scanButton);addAndMakeVisible(choirButton);
    addAndMakeVisible(presetBox);
    presetBox.setTextWhenNothingSelected("STAGE PRESET");
    for(int i=0;i<choir::kNumStagePresets;++i)presetBox.addItem(choir::kStagePresets[i].name,i+1);
    presetBox.setTooltip("Try a room. It moves the singers and sets the space only: harmony, mix, lead level and output are never touched.");
    presetBox.onChange=[this] {
        const int index=presetBox.getSelectedId()-1;
        if(index>=0)processor.applyStagePreset(index);
        presetBox.setSelectedId(0,juce::dontSendNotification);
    };
    statusLabel.setColour(juce::Label::textColourId,kMuted);
    statusLabel.setFont(plainFont(11.f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    // ---- tabs
    const char* tabNames[]={"MAIN","ROOM","VOICE","TAKE","STEMS"};
    const char* tabTips[]={"The controls you reach for","The room, the microphones and the hall","How the singers sound",
                           "Scan a whole recording and edit its harmony","Mix separately recorded singers"};
    for(int i=0;i<numPages;++i) {
        auto& tab=tabs[(size_t)i];
        tab.setButtonText(tabNames[i]);tab.setTooltip(tabTips[i]);tab.setComponentID("tab");
        tab.onClick=[this,i]{selectTab(i);};
        addAndMakeVisible(tab);
    }

    // ---- MAIN: five knobs and five faders are all most people need
    addFader("SOPRANO","gain0",kSection[0],"Soprano level. 0 dB is the default.",.83f);
    addFader("ALTO","gain1",kSection[1],"Alto level. 0 dB is the default.",.83f);
    addFader("TENOR","gain2",kSection[2],"Tenor level. 0 dB is the default.",.83f);
    addFader("BASS","gain3",kSection[3],"Bass level. 0 dB is the default.",.83f);
    addFader("LEAD","leadGain",kLead,"Your own voice, placed on the stage. 0 dB is unity.",1.f);
    addKnob("CHOIR MIX","mix",pageMain,"Choir against your own voice. 0% is only your voice, 100% is only the choir.",ValueFormat::Percent);
    addKnob("ENSEMBLE","ensemble",pageMain,"How far the singers in a section drift apart in tuning: more sounds like a bigger choir.",ValueFormat::Percent);
    addKnob("HUMANIZE","humanize",pageMain,"Timing and pitch differences between singers. More is looser and more human.",ValueFormat::Percent);
    addKnob("SPACE","wet",pageMain,"How much of the room you hear around the choir.",ValueFormat::PercentOfRange);
    addKnob("OUTPUT","master",pageMain,"Overall output level.",ValueFormat::GainDb,1.f);
    addChoice("PROCESS","mode",pageMain,"Live follows your singing at once. Scan / Harmony uses the key found by SCAN LIVE INPUT.");
    addChoice("HARMONY","harmony",pageMain,"The chord colour the singers follow.");

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
    irButton.setTooltip("Use a recording of a real room (an impulse response) for the HALL. The HALL knob sets how much you hear.");
    irButton.onClick=[this] {
        if(!processor.hasImpulseResponse()) {chooseImpulseResponse();return;}
        juce::PopupMenu menu;
        menu.addItem(1,"Load a different room recording...");
        menu.addItem(2,"Remove it (back to the built-in hall)");
        juce::Component::SafePointer<OmniChoirAudioProcessorEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&irButton),[safe](int result) {
            if(safe==nullptr)return;
            if(result==1)safe->chooseImpulseResponse();
            else if(result==2)safe->processor.clearImpulseResponse();
        });
    };
    addSide(pageRoom,abButton);addSide(pageRoom,irButton);

    // ---- VOICE: how the singers sound
    addKnob("CHOIR LEVEL","choirLevel",pageVoice,"The choir against your voice, in dB. Works with CHOIR MIX.",ValueFormat::Db);
    addKnob("CHARACTER","formant",pageVoice,"Keeps each singer's vowel colour natural when the pitch moves.",ValueFormat::Percent);
    addKnob("SENSITIVITY","sensitivity",pageVoice,"How quiet a sound can be and still start the choir. Raise it for soft singing, lower it for a noisy input.",ValueFormat::Percent);
    addKnob("VOICE BLEND","voiceBlend",pageVoice,"Mixes in synthesised singers that follow your voice.",ValueFormat::Percent);
    addKnob("VOWEL","choirVowel",pageVoice,"The vowel the synthesised singers sing.",ValueFormat::Percent);
    addKnob("FOLLOW LEAD","vowelFollow",pageVoice,"How closely the synthesised singers copy your vowel.",ValueFormat::Percent);
    addKnob("BREATH","choirBreath",pageVoice,"Breathiness of the synthesised singers.",ValueFormat::Percent);
    addKnob("VIBRATO","vibrato",pageVoice,"Vibrato of the synthesised singers.",ValueFormat::Percent);

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
    selectTab(pageMain);
    startTimerHz(10);
}
OmniChoirAudioProcessorEditor::~OmniChoirAudioProcessorEditor() {
    stopTimer();chooser.reset();
    setLookAndFeel(nullptr);
}
void OmniChoirAudioProcessorEditor::addKnob(const juce::String& title,const juce::String& id,int page,const juce::String& tip,ValueFormat format,float unity) {
    auto c=std::make_unique<Dial>(processor,title,id,tip,format,unity);
    addAndMakeVisible(*c);
    slots.push_back({page,Kind::Knob,c.get()});
    owned.push_back(std::move(c));
}
void OmniChoirAudioProcessorEditor::addFader(const juce::String& title,const juce::String& id,juce::Colour accent,const juce::String& tip,float unity) {
    auto c=std::make_unique<Fader>(processor,title,id,accent,tip,unity);
    addAndMakeVisible(*c);
    slots.push_back({pageMain,Kind::Fader,c.get()});
    owned.push_back(std::move(c));
}
void OmniChoirAudioProcessorEditor::addChoice(const juce::String& title,const juce::String& id,int page,const juce::String& tip) {
    auto c=std::make_unique<Choice>(processor,title,id,tip);
    addAndMakeVisible(*c);
    slots.push_back({page,Kind::Side,c.get()});
    owned.push_back(std::move(c));
}
void OmniChoirAudioProcessorEditor::addSide(int page,juce::Component& item) {
    addAndMakeVisible(item);
    slots.push_back({page,Kind::Side,&item});
}
void OmniChoirAudioProcessorEditor::selectTab(int page) {
    currentTab=page;
    for(auto& s:slots)s.item->setVisible(s.page==page);
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
void OmniChoirAudioProcessorEditor::paintListBoxItem(int index,juce::Graphics& g,int width,int height,bool selected) {
    if(index<0||index>=(int)visibleScore.notes.size())return;
    const auto& e=visibleScore.notes[(size_t)index];
    g.fillAll(selected?juce::Colour(0xff6b4a2c):(index%2?kPanel:kPanel2.darker(.25f)));
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
    juce::String msg;
    switch(processor.scanStatus()) {
        case 1:msg="Listening for your key... "+juce::String((int)(processor.scanProgress()*100.f))+"%";break;
        case 2:msg="Key found: "+juce::String(pitchClasses[processor.scanRoot()%12])+". Set PROCESS to Scan / Harmony to use it.";break;
        case 3:msg="No clear pitch heard. Try again with a clean, solo vocal.";break;
        default:msg="Drag the singers, your lead and the mic on the stage. Double-click one to put it back.";break;
    }
    choirButton.setButtonText(choirButton.getToggleState()?"CHOIR ON":"CHOIR OFF");
    abButton.setButtonText(abButton.getToggleState()?"ENGINE: CLASSIC":"ENGINE: FAITHFUL");
    irButton.setButtonText(processor.hasImpulseResponse()?"ROOM: "+processor.impulseResponseName().toUpperCase().substring(0,12):juce::String("LOAD ROOM IR"));
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
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2f1b15),(float)getWidth()*.5f,0.f,kBg,(float)getWidth()*.5f,130.f,false));
    g.fillRect(0,0,getWidth(),130);
    g.setGradientFill(juce::ColourGradient(kGoldHi,0.f,6.f,kGoldLo,0.f,40.f,false));
    g.setFont(serifFont(31.f));
    g.drawText("OMNICHOIR",14,5,250,36,juce::Justification::centredLeft);
    g.setColour(kCream);g.setFont(boldFont(9.f));
    g.drawText("LIVE CHOIRIFICATION / SCAN & RENDER",16,41,260,14,juce::Justification::centredLeft);
    g.setColour(kGold.withAlpha(.30f));g.drawLine(14.f,62.f,(float)getWidth()-14.f,62.f,1.f);
    g.setColour(kPanel.withAlpha(.85f));g.fillRoundedRectangle(deckBounds.toFloat(),12.f);
    g.setColour(kStroke);g.drawRoundedRectangle(deckBounds.toFloat().reduced(.5f),12.f,1.f);
}
void OmniChoirAudioProcessorEditor::layoutPage(int page,juce::Rectangle<int> area) {
    std::vector<juce::Component*> knobs,faders,sides;
    for(auto& s:slots) {
        if(s.page!=page)continue;
        auto& list=s.kind==Kind::Knob?knobs:(s.kind==Kind::Fader?faders:sides);
        list.push_back(s.item);
    }
    if(!faders.empty()) {
        const int faderWidth=50;
        auto strip=area.removeFromLeft((int)faders.size()*faderWidth);
        for(auto* f:faders)f->setBounds(strip.removeFromLeft(faderWidth));
        area.removeFromLeft(14);
    }
    if(!sides.empty()) {
        auto column=area.removeFromRight(172);
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
        const int cellWidth=juce::jmin(112,area.getWidth()/(int)knobs.size());
        for(auto* k:knobs)k->setBounds(area.removeFromLeft(cellWidth));
    }
}
void OmniChoirAudioProcessorEditor::resized() {
    const int w=getWidth(),h=getHeight(),m=14,headerHeight=62,footerHeight=26,tabsHeight=30;
    const int controlsHeight=juce::jlimit(122,170,(int)((float)h*.215f));

    auto header=getLocalBounds().removeFromTop(headerHeight).reduced(m,0);
    choirButton.setBounds(header.removeFromRight(126).withSizeKeepingCentre(126,36));
    header.removeFromRight(8);
    presetBox.setBounds(header.removeFromRight(160).withSizeKeepingCentre(160,30));
    header.removeFromRight(8);
    scanButton.setBounds(header.removeFromRight(156).withSizeKeepingCentre(156,32));

    const int footerTop=h-footerHeight-6;
    const int controlsTop=footerTop-6-controlsHeight;
    const int tabsTop=controlsTop-tabsHeight-6;
    const int stageTop=headerHeight+4;
    stage.setBounds(m,stageTop,w-2*m,juce::jmax(60,tabsTop-6-stageTop));
    const int tabWidth=juce::jmin(118,(w-2*m)/numPages-4);
    for(int i=0;i<numPages;++i)tabs[(size_t)i].setBounds(m+i*(tabWidth+4),tabsTop,tabWidth,tabsHeight);
    deckBounds=juce::Rectangle<int>(m,controlsTop,w-2*m,controlsHeight);
    auto footer=juce::Rectangle<int>(m,footerTop,w-2*m,footerHeight);
    meterStrip.setBounds(footer.removeFromRight(juce::jmin(330,footer.getWidth()/2)));
    footer.removeFromRight(10);
    statusLabel.setBounds(footer);

    const auto inner=deckBounds.reduced(14,8);
    for(const int page:{(int)pageMain,(int)pageRoom,(int)pageVoice})layoutPage(page,inner);
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
