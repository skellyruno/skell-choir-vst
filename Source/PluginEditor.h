#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Offline/TakeWorker.h"
#include "UI/OmniLookAndFeel.h"
#include <array>
#include <memory>
#include <vector>

// How a control shows its value. Plain = the parameter's own text.
enum class ValueFormat { Plain, Percent, PercentOfRange, GainDb, Db, Metres, Seconds };

// The stage: the cathedral, the four sections, the lead and the microphone. Drag any of them; double-click to put one back.
class StageView final : public juce::Component, private juce::Timer {
public:
    explicit StageView(OmniChoirAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    OmniChoirAudioProcessor& processor;
    juce::Image cathedral;
    int dragging=-2, hover=-2;   // -2 none, -1 mic, -3 lead, 0..3 soprano..bass
    float micStartX=0,micStartY=0,leadStartX=0,leadStartY=0;
    juce::Point<float> worldToPoint(float,float) const;
    juce::Point<float> pointToWorld(juce::Point<float>) const;
    juce::Point<float> leadPosition() const;
    juce::Rectangle<float> plot() const;
    float value(const juce::String&) const;
    void change(const juce::String&,float);
    void resetParameter(const juce::String&);
    void moveMic(float x,float y);
    int hit(juce::Point<float>) const;
    void startGesture(int);
    void finishGesture(int);
    void timerCallback() override {repaint();}
};

// Live level strip: the four sections, the lead, the output peaks, and the stereo width/correlation.
class MeterStrip final : public juce::Component, private juce::Timer {
public:
    explicit MeterStrip(OmniChoirAudioProcessor&);
    void paint(juce::Graphics&) override;
private:
    OmniChoirAudioProcessor& processor;
    void timerCallback() override {repaint();}
};

class OmniChoirAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer,
                                           private juce::ListBoxModel {
public:
    explicit OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor&);
    ~OmniChoirAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    enum class Kind { Knob, Fader, Side };
    enum Page { pageMain=0, pageRoom, pageVoice, pageTake, pageStems, numPages };
    struct Slot { int page; Kind kind; juce::Component* item; };

    OmniChoirAudioProcessor& processor;
    omni::OmniLookAndFeel lnf;                       // declared first, so it outlives every child that draws with it
    juce::TooltipWindow tooltipWindow { this, 600 };
    StageView stage;
    MeterStrip meterStrip;
    TakeWorker take;

    // header
    juce::TextButton scanButton {"SCAN LIVE INPUT"}, choirButton {"CHOIR ON"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> choirAttachment;
    juce::ComboBox presetBox;
    juce::Label statusLabel;

    // the pages
    std::array<juce::TextButton,numPages> tabs;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Slot> slots;
    int currentTab=pageMain;
    juce::Rectangle<int> deckBounds;
    void addKnob(const juce::String& title,const juce::String& id,int page,const juce::String& tip,ValueFormat format,float unity=1.f);
    void addFader(const juce::String& title,const juce::String& id,juce::Colour accent,const juce::String& tip,float unity);
    void addChoice(const juce::String& title,const juce::String& id,int page,const juce::String& tip);
    void addSide(int page,juce::Component&);
    void layoutPage(int page,juce::Rectangle<int> area);
    void selectTab(int);

    // ROOM page extras
    juce::TextButton abButton {"ENGINE: FAITHFUL"}, irButton {"LOAD ROOM IR"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;

    // TAKE and STEMS pages
    juce::TextButton importButton {"IMPORT FULL TAKE"}, exportButton {"RENDER WAV"}, midiButton {"EXPORT MIDI"},
                     loadStemsButton {"LOAD STEM JSON"}, mixStemsButton {"MIX STEMS WAV"};
    juce::ListBox notes {"Editable SATB score",this};
    std::array<juce::TextButton,8> adjust;
    juce::Label takeStatus, stemsStatus, stemsHelp;
    juce::File stemManifest;
    std::unique_ptr<juce::FileChooser> chooser;
    choir::FullTakeScore visibleScore;
    int lastTakeStatus=-100;
    void chooseInput();
    void chooseImpulseResponse();
    void chooseOutput();
    void chooseMidi();
    void chooseStemManifest();
    void chooseStemOutput();

    // juce::ListBoxModel
    int getNumRows() override {return (int)visibleScore.notes.size();}
    void paintListBoxItem(int row,juce::Graphics&,int width,int height,bool selected) override;
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmniChoirAudioProcessorEditor)
};
