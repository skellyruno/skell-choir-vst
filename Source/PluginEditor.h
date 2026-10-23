#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class StageView final : public juce::Component, private juce::Timer {
public:
    explicit StageView(OmniChoirAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    OmniChoirAudioProcessor& processor;
    int dragging = -2;
    juce::Point<float> worldToPoint(float x, float y) const;
    juce::Point<float> pointToWorld(juce::Point<float> point) const;
    juce::Rectangle<float> plot() const;
    float value(const juce::String&) const;
    void change(const juce::String&, float);
    void startGesture(int);
    void finishGesture(int);
    void timerCallback() override { repaint(); }
};

class OmniChoirAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit OmniChoirAudioProcessorEditor(OmniChoirAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    OmniChoirAudioProcessor& processor;
    StageView stage;
    std::vector<std::unique_ptr<juce::Component>> controls;
    void addDial(const juce::String& text, const juce::String& id);
    void addChoice(const juce::String& text, const juce::String& id);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmniChoirAudioProcessorEditor)
};
