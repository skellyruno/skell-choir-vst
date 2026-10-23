#pragma once
#include "FullTakeScan.h"
#include "ExternalStemMixer.h"
#include <JuceHeader.h>
#include <atomic>

// All disk I/O, full-take analysis and WAV rendering happen on this worker,
// never on the host's audio callback. File path is deliberately not written
// to DAW project state (external media must remain available at its location).
class TakeWorker final : private juce::Thread {
public:
    TakeWorker():Thread("OmniChoir full-take scanner / renderer") {}
    ~TakeWorker() override { signalThreadShouldExit();stopThread(10000); }
    bool scanFile(const juce::File& file,bool minor);
    bool exportFile(const juce::File& file,const choir::FXParams& params);
    bool exportMidiFile(const juce::File& file);
    bool mixStemsFile(const juce::File& manifest,const juce::File& outputWav,
                      const choir::FXParams& stageParams);
    bool changeNote(int row,int part,int step);
    choir::FullTakeScore snapshot() const;
    juce::String fileName() const;
    juce::String errorMessage() const;
    // 0 empty, 1 scan, 2 ready, 3 render, 4 WAV ready, 5 MIDI, 6 MIDI ready,
    // 7 stem mix, 8 stem mix ready, -1 error.
    int status() const {return state.load();}
    float progress() const {return fraction.load();}
private:
    enum class Job { Scan, Export, Midi, Stems } job=Job::Scan;
    mutable juce::CriticalSection mutex;
    juce::File input,output,stemManifest;
    choir::FXParams renderParams,stemParams;
    choir::FullTakeScore score;
    juce::String error;
    std::atomic<int> state {0};
    std::atomic<float> fraction {0};
    void run() override;
    void analyse();
    void render();
    void renderMidi();
    void mixStems();
    bool failed(const juce::String&);
};
