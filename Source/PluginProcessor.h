#pragma once
#include <JuceHeader.h>
#include "ChoirFX.h"
#include <atomic>

// Stereo/mono insert AUDIO EFFECT. No MIDI notes or pre-rendered choir samples.
class OmniChoirAudioProcessor final : public juce::AudioProcessor {
public:
    OmniChoirAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "OmniChoir FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void requestScan() { scanRequested.store(true,std::memory_order_release); }
    int scanStatus() const { return visibleStatus.load(std::memory_order_relaxed); }
    float scanProgress() const { return visibleProgress.load(std::memory_order_relaxed); }
    int scanRoot() const { return savedRoot.load(std::memory_order_relaxed); }
    float trackedMidi() const { return visiblePitch.load(std::memory_order_relaxed); }
    float inputLevel() const { return visibleRms.load(std::memory_order_relaxed); }
    choir::FXParams snapshotParams() const {return readParams();}
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
private:
    choir::ChoirEffect effect;
    std::atomic<bool> scanRequested { false }, restoreRequested { false };
    std::atomic<int> visibleStatus { 0 }, savedRoot { 0 };
    std::atomic<float> visibleProgress { 0 }, visiblePitch { -1 }, visibleRms { 0 };
    struct RawParameters {
        std::array<std::atomic<float>*,4> gains {}, xs {}, ys {};
        std::atomic<float>* micX=nullptr,*micY=nullptr,*width=nullptr,*micMode=nullptr;
        std::atomic<float>* choirOn=nullptr,*mode=nullptr,*harmony=nullptr,*ensemble=nullptr,*sensitivity=nullptr;
        std::atomic<float>* roomSize=nullptr,*roomSeconds=nullptr,*absorption=nullptr,*diffusion=nullptr;
        std::atomic<float>* wet=nullptr,*mix=nullptr,*master=nullptr;
        std::atomic<float>* leadX=nullptr,*leadY=nullptr,*leadGain=nullptr,*humanize=nullptr,*formant=nullptr;
    } raw;
    choir::FXParams readParams() const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmniChoirAudioProcessor)
};
