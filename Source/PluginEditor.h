#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Offline/TakeWorker.h"
#include "UI/OmniLookAndFeel.h"
#include <SkellCore/GuidedTour.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

// How a control shows its value. Plain = the parameter's own text.
enum class ValueFormat { Plain, Percent, PercentOfRange, GainDb, Db, Metres, Seconds };

// The stage: the cathedral, the four sections, the lead and the microphone. Drag any of them; double-click to put one back.
class StageView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
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
    int dragging=-2, hover=-2, hoverPill=-2;   // -2 none, -1 mic, -3 lead, 0..3 soprano..bass (hoverPill: 0..3, the name tags)
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
    juce::Rectangle<float> pillBounds(int section) const;   // the name tag under a section: click to solo, right-click to mute
    int hitPill(juce::Point<float>) const;
    void toggleState(int section,bool mute);
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

// The Melodyne-like view of a LISTEN take: the notes you sang (white) and the four parts the choir sings (coloured), over the scale you chose.
// Click a note to select it; the wheel scrolls through the take; a gold line follows the host's play position.
class PianoRoll final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit PianoRoll(OmniChoirAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    int selected() const {return selectedNote;}
    void clearSelection() {selectedNote=-1;}
    void refresh();   // follows the play position and repaints (called from the editor's timer)
private:
    OmniChoirAudioProcessor& processor;
    int selectedNote=-1;
    double viewStart=0,viewSpan=16;
    double timeAt(float x) const;
    float xAt(double t) const;
};

class OmniChoirAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer,
                                           private juce::ListBoxModel {
public:
    explicit OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor&);
    ~OmniChoirAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    // for tools/screenshots.cpp: show a given mode and tab, and drop the first-run tour
    void showForScreenshot(bool proMode,int tab) {processor.setProUi(proMode);currentTab=tab;applyMode();}
    void dismissTour() {if(tour)tour->dismiss();}
private:
    enum class Kind { Knob, Fader, Side };
    enum Page { pageMain=0, pageRoom, pageVoice, pageFollow, pageListen, pageTake, pageStems, numPages };
    struct Slot { int page; Kind kind; juce::Component* item; bool easy; bool easyOnly=false; };   // easy: still shown in Easy mode; easyOnly: shown only there

    OmniChoirAudioProcessor& processor;
    omni::OmniLookAndFeel lnf;                       // declared first, so it outlives every child that draws with it
    juce::TooltipWindow tooltipWindow { this, 600 };
    StageView stage;
    MeterStrip meterStrip;
    TakeWorker take;

    // header
    juce::TextButton choirButton {"CHOIR ON"};
    juce::ComboBox keyBox, scaleBox;   // v0.8: you choose the key and scale; nothing is detected
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> choirAttachment;
    juce::ComboBox presetBox;
    juce::Label statusLabel;
    juce::TextButton surpriseButton {"SURPRISE ME"}, modeButton {"EASY"};
    bool pro=false;

    // the pages
    std::array<juce::TextButton,numPages> tabs;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Slot> slots;
    int currentTab=pageMain;
    juce::Rectangle<int> deckBounds;
    void addKnob(const juce::String& title,const juce::String& id,int page,const juce::String& tip,ValueFormat format,float unity=1.f,bool easy=false);
    void addFader(const juce::String& title,const juce::String& id,juce::Colour accent,const juce::String& tip,float unity);
    void addChoice(const juce::String& title,const juce::String& id,int page,const juce::String& tip,bool easy=false);
    void addSide(int page,juce::Component&,bool easy=false,bool easyOnly=false);
    void applyMode();   // Easy or Pro
    void layoutPage(int page,juce::Rectangle<int> area);
    void selectTab(int);

    // ROOM page extras
    juce::TextButton abButton {"ENGINE: FAITHFUL"}, irButton {"ROOMS / LOAD IR"}, autoLevelButton {"AUTO LEVEL: ON"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment, autoLevelAttachment;

    // LISTEN page (and an Easy-mode button on MAIN)
    PianoRoll roll;
    juce::TextButton listenButton {"LISTEN"}, listenEasyButton {"LISTEN"}, clearListenButton {"CLEAR"};
    std::array<juce::TextButton,8> listenAdjust;
    int lastListenState=0, lastKey=-1, lastScale=-1, lastHarmony=-1;
    void listenClicked();
    // styles, the first-run tour and the remembered window size
    std::vector<juce::File> styleFiles;
    void rebuildStyleMenu();
    void handleStyleMenu(int id);
    std::unique_ptr<skellcore::ui::GuidedTourController> tour;
    void startTour(bool firstRun=false);
    int sizeTicks=0;
    bool sizeDirty=false;
    void saveWindowSize();

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
