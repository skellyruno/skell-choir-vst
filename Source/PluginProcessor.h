#pragma once
#include <JuceHeader.h>
#include "ChoirDSP.h"
#include <atomic>

class OmniChoirAudioProcessor final : public juce::AudioProcessor {
public:
    OmniChoirAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "OmniChoir"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<int> activeVoiceCount { 0 };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    choir::Instrument instrument;
    // Cached raw parameter atomics; the audio thread never constructs strings or queries the tree.
    struct RawParameters {
        std::array<std::atomic<float>*,4> gains {}, xs {}, ys {};
        std::atomic<float>* micX=nullptr, *micY=nullptr, *width=nullptr, *micMode=nullptr, *vowel=nullptr;
        std::atomic<float>* attack=nullptr, *release=nullptr, *ensemble=nullptr, *breath=nullptr;
        std::atomic<float>* roomSize=nullptr, *roomSeconds=nullptr, *absorption=nullptr, *diffusion=nullptr;
        std::atomic<float>* wet=nullptr, *master=nullptr;
    } raw;
    choir::Params readParams() const;
    void handleMidi(const juce::MidiMessage&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmniChoirAudioProcessor)
};
