#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StageGeometry.h"
#include "Theme.h"
#include "Scenes.h"
#include <array>
#include "UI/TitleLayout.h"
#include "Offline/TakeWorker.h"
#include "UI/OmniLookAndFeel.h"
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
    std::array<juce::Image,scenes::kNumScenes> sceneImages;   // loaded on first use; Default and Cathedral share one picture
    int shownScene=0,fromScene=0;                              // the picture on show, and the one fading out
    double fadeStartMs=-1.0e9;
    const juce::Image& sceneImage(int index);
    void followScene();                                        // starts a crossfade when the processor's scene changed
    int dragging=-2, hover=-2, hoverPill=-2;   // -2 none, -1 mic, -3 lead, 0..3 soprano..bass (hoverPill: 0..3, the name tags)
    float grabX=0,grabY=0;            // where the thing was relative to the pointer when the drag began (world metres): nothing snaps to the cursor
    choir::StagePoint leadStart;      // where the lead stood (on the stage) when a mic drag began: she stays there while the mic moves
    juce::Point<float> worldToPoint(float,float) const;
    juce::Point<float> pointToWorld(juce::Point<float>) const;
    juce::Point<float> leadPosition() const;
    juce::Rectangle<float> plot() const;
    float value(const juce::String&) const;
    void change(const juce::String&,float);
    void resetParameter(const juce::String&);
    choir::StagePoint micWorld() const;
    choir::StagePoint leadWorld() const;   // where the lead is drawn: mic + display shift + (leadX, leadY)
    void moveMic(float x,float y);          // clamps the mic, then re-derives leadX/leadY from leadStart so she does not move
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

// A short first-run tour: dims the editor and points at one control at a time.
// The skull in the header: click it to choose a theme. Painted from the shape list in UI/SkullShape.h.
class SkullButton final : public juce::Button {
public:
    SkullButton() : juce::Button("theme") {setMouseCursor(juce::MouseCursor::PointingHandCursor);}
    void paintButton(juce::Graphics& g,bool shouldDrawButtonAsHighlighted,bool shouldDrawButtonAsDown) override;
};
class TourOverlay final : public juce::Component {
public:
    struct Step { juce::Component* target; juce::Component* alsoTarget; juce::String title,text; };
    TourOverlay(std::vector<Step> steps,std::function<void()> onFinished);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override {}   // swallow clicks: the editor behind is not touched during the tour
private:
    std::vector<Step> steps;
    size_t index=0;
    std::function<void()> finished;
    juce::TextButton next {"NEXT"}, skip {"SKIP"};
    juce::Rectangle<int> targetBounds() const;
    juce::Rectangle<int> bubbleBounds() const;
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
    void showThemeForScreenshot(int index) {omni::applyTheme(index);refreshThemedColours();}   // like the skull menu, but nothing is saved to theme.txt
    void dismissTour() {tour.reset();}
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
    SkullButton skullButton;                       // top of the header, right of the title: the theme changer
    titlelayout::Result titleLayout {titlelayout::kBaseFont,250.f,titlelayout::kSkull,272.f};
    void showThemeMenu();
    bool pro=false;

    // the pages
    std::array<juce::TextButton,numPages> tabs;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Slot> slots;
    int currentTab=pageMain;
    juce::Rectangle<int> deckBounds;
    void addKnob(const juce::String& title,const juce::String& id,int page,const juce::String& tip,ValueFormat format,float unity=1.f,bool easy=false);
    void addFader(const juce::String& title,const juce::String& id,int accent,const juce::String& tip,float unity);   // accent: 0..3 sections, 4 lead
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
    // themes: the colours follow omni::applyTheme; the choice is remembered in theme.txt
    int appliedTheme=-1;
    void setTheme(int index);              // apply, refresh every themed colour, remember
    void refreshThemedColours();           // re-reads the palette into every component that took a colour at construction
    static int loadSavedTheme();
    static void saveTheme(int index);
    // styles, the first-run tour and the remembered window size
    std::vector<juce::File> styleFiles;
    void rebuildStyleMenu();
    void handleStyleMenu(int id);
    std::unique_ptr<TourOverlay> tour;
    void startTour();
    void finishTour();
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
