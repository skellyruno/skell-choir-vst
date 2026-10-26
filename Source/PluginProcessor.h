#pragma once
#include <JuceHeader.h>
#include "StagedChoir.h"
#include "StagePresets.h"
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
    // v0.6 additions (message thread unless noted)
    const choir::StagedChoir::Meters& meters() const { return staged.meters; }   // any thread
    void loadImpulseResponse(const juce::File&);    // a WAV/AIFF/FLAC room recording for the HALL (up to 8 s)
    void clearImpulseResponse();
    bool hasImpulseResponse() const { return staged.hasImpulseResponse(); }
    juce::String impulseResponseName() const { return irName; }
    void applyStagePreset(int index);               // placement + room only; never harmony, mix, lead level or output
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
private:
    choir::StagedChoir staged;   // v0.6: 16 singer layers -> faithful stage (mic arrays, reflections, FDN) -> hall -> lead
    std::atomic<bool> scanRequested { false }, restoreRequested { false };
    std::atomic<int> visibleStatus { 0 }, savedRoot { 0 };
    std::atomic<float> visibleProgress { 0 }, visiblePitch { -1 }, visibleRms { 0 };
    struct RawParameters {
        std::array<std::atomic<float>*,4> gains {}, xs {}, ys {};
        std::atomic<float>* micX=nullptr,*micY=nullptr,*width=nullptr,*micMode=nullptr;
        std::atomic<float>* choirOn=nullptr,*mode=nullptr,*harmony=nullptr,*ensemble=nullptr,*sensitivity=nullptr;
        std::atomic<float>* roomSize=nullptr,*roomSeconds=nullptr,*absorption=nullptr,*diffusion=nullptr;
        std::atomic<float>* wet=nullptr,*mix=nullptr,*master=nullptr;
        std::atomic<float>* leadX=nullptr,*leadY=nullptr,*leadGain=nullptr,*humanize=nullptr,*formant=nullptr,*hall=nullptr,*hallSeconds=nullptr;
        std::atomic<float>* voiceBlend=nullptr,*choirVowel=nullptr,*vowelFollow=nullptr,*choirBreath=nullptr,*vibrato=nullptr,*classicStage=nullptr,*choirLevel=nullptr;
    } raw;
    choir::FXParams readParams() const;
    juce::String irPath, irName;   // message thread only
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmniChoirAudioProcessor)
};
