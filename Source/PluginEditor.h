#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Offline/TakeWorker.h"
#include <memory>
#include <vector>
#include <array>

class StageView final : public juce::Component,private juce::Timer {
public:
    explicit StageView(OmniChoirAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    OmniChoirAudioProcessor& processor;
    juce::Image cathedral;
    int dragging=-2; // -2 none, -1 mic, -3 white lead, 0..3 SATB
    float micStartX=0,micStartY=0,leadStartX=0,leadStartY=0;
    juce::Point<float> worldToPoint(float,float) const;
    juce::Point<float> pointToWorld(juce::Point<float>) const;
    juce::Point<float> leadPosition() const;
    juce::Rectangle<float> plot() const;
    float value(const juce::String&) const;
    void change(const juce::String&,float);
    void startGesture(int);
    void finishGesture(int);
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
    OmniChoirAudioProcessor& processor;
    StageView stage;
    TakeWorker take;
    juce::TextButton scanButton {"SCAN LIVE INPUT"}, choirButton {"CHOIR ON"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> choirAttachment;
    juce::Label scanStatus;
    std::array<juce::TextButton,5> tabs;
    struct PageControl {int page;std::unique_ptr<juce::Component> item;};
    std::vector<PageControl> controls;
    int currentTab=0;
    void addDial(const juce::String&,const juce::String&,int page);
    void addChoice(const juce::String&,const juce::String&,int page);
    void selectTab(int);

    juce::TextButton importButton {"IMPORT FULL TAKE"},exportButton {"RENDER WAV"},
                     midiButton {"EXPORT MIDI"},loadStemsButton {"LOAD STEM JSON"},
                     mixStemsButton {"MIX STEMS WAV"};
    juce::ListBox notes {"Editable SATB score",this};
    std::array<juce::TextButton,8> adjust;
    juce::Label takeStatus,stemsStatus,stemsHelp;
    juce::File stemManifest;
    std::unique_ptr<juce::FileChooser> chooser;
    choir::FullTakeScore visibleScore;
    int lastTakeStatus=-100;
    void chooseInput();
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
