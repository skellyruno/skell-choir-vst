#pragma once
#include <JuceHeader.h>
#include "StagedChoir.h"
#include "StagePresets.h"
#include "StylePresets.h"
#include "MidiChord.h"
#include "Scales.h"
#include "StyleFile.h"
#include "Offline/ListenSession.h"
#include <SkellCore/WorldLink.h>
#include <atomic>
#include <cstdint>

// Stereo/mono insert AUDIO EFFECT. Optional MIDI input (v0.7): hold keys and the choir sings that chord from your voice. No pre-rendered choir samples.
class OmniChoirAudioProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    OmniChoirAudioProcessor();
    ~OmniChoirAudioProcessor() override;
    bool setFollowSkellKey(bool on); // message thread: explicit opt-in
    bool followingSkellKey() const noexcept { return followWorldKey_.load(std::memory_order_acquire); }
    bool skellWorldOnline() const noexcept { return worldLink_.online(); }
    skellcore::world::MusicalKey skellWorldKey() const noexcept {
        return skellcore::world::unpackMusicalKey(linkedKey_.load(std::memory_order_acquire));
    }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "OmniChoir FX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.; }
    // v0.8: the factory styles are the host's preset list
    int getNumPrograms() override { return choir::kNumStylePresets; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override { return index>=0 && index<choir::kNumStylePresets ? juce::String(choir::kStylePresets[index].name) : juce::String(); }
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
    // v0.7 (message thread)
    static constexpr int kNumBuiltInRooms = 3;
    static const char* builtInRoomName(int index);   // Cathedral, Stone Chapel, Small Hall: synthetic impulse responses shipped inside the plugin
    void chooseBuiltInRoom(int index);               // loads it and, if the HALL is off, brings the HALL up so the room can be heard
    void applyStyle(int index);                      // a whole sound: stage preset + how the singers sing; never lead level, output or solo/mute
    void surpriseMe();                               // a random but musical style
    int chordNote(int section) const { return chordView[(size_t) section].load(std::memory_order_relaxed); }   // the stage shows the chord being held (-1 = none)
    // v0.8: your own styles (plain-text .omnistyle files in the user's application-data folder)
    static juce::File userStyleFolder();
    juce::Array<juce::File> userStyleFiles() const;
    bool saveStyleFile(const juce::File& file);   // the current sound: never your lead level, output, key, mode or solo/mute
    bool loadStyleFile(const juce::File& file);
    // v0.8: LISTEN. Play a take through the plugin once while it listens; it becomes a score the choir sings in time with the host.
    static constexpr double kMaxListenSeconds = 120.0;
    choir::ListenSession& listenSession() { return listen; }
    const choir::ListenSession& listenSession() const { return listen; }
    void startListening();
    void stopListening();     // analyses on a worker thread; the session goes Analysing, then Ready
    void clearListening();
    void setMode(int mode);   // 0 Live, 1 Listen, 2 MIDI Chord
    double listenPlayhead() const { return playheadSeconds.load(std::memory_order_relaxed); }   // seconds into the take, or -1
    bool proUi() const { return proUiMode.load(std::memory_order_relaxed); }
    void setProUi(bool on) { proUiMode.store(on,std::memory_order_relaxed); }
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
private:
    void timerCallback() override;
    skellcore::world::WorldLink worldLink_ {"skell.choir", "OmniChoir"};
    std::atomic<bool> followWorldKey_ {false};
    std::atomic<int> linkedKey_ {0};
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
        std::atomic<float>* voiceBlend=nullptr,*choirVowel=nullptr,*vowelFollow=nullptr,*choirBreath=nullptr,*vibrato=nullptr,*classicStage=nullptr,*choirLevel=nullptr,*levelMatch=nullptr,*choirSize=nullptr,*consonants=nullptr,*swell=nullptr,*keyRoot=nullptr,*scale=nullptr,*pulseRate=nullptr,*pulseDepth=nullptr;
        std::array<std::atomic<float>*,4> sectionState {};   // 0 normal, 1 solo, 2 mute
    } raw;
    choir::FXParams readParams() const;
    juce::String irPath, irName;   // message thread only. irPath is a file path, or "builtin:N" for a shipped room
    choir::ChordTracker chord;     // audio thread only
    choir::ListenSession listen;
    choir::ListenPlayback playback;   // audio thread only
    std::vector<float> monoScratch;   // the capture's mono mix, sized in prepareToPlay
    std::atomic<double> hostRate { 44100.0 };
    std::atomic<double> playheadSeconds { -1.0 };
    int currentProgram = 0;
    std::array<std::atomic<int>,4> chordView {};   // chord.sections(), for the UI
    std::atomic<bool> proUiMode { false };
    std::uint32_t surpriseState = 0;
    void loadBuiltInImpulse(int index);
    void useImpulseResponse(juce::AudioFormatReader&,const juce::String& path,const juce::String& name);
    void applyStyleValues(const choir::StylePreset&);
    void setParameterValue(const juce::String& id,float value);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmniChoirAudioProcessor)
};
