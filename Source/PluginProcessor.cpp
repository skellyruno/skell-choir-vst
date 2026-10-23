#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

namespace {
void addFloat(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
              const juce::String& id, const juce::String& name,
              float min, float max, float step, float value, const juce::String& unit = {}) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float>(min, max, step), value,
        juce::AudioParameterFloatAttributes().withLabel(unit)));
}
}

OmniChoirAudioProcessor::OmniChoirAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Stereo Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "OMNICHOIR_PARAMETERS", createLayout()) {
    for (int i=0; i<4; ++i) {
        const auto suffix = juce::String(i);
        raw.gains[(size_t)i] = parameters.getRawParameterValue("gain"+suffix);
        raw.xs[(size_t)i] = parameters.getRawParameterValue("x"+suffix);
        raw.ys[(size_t)i] = parameters.getRawParameterValue("y"+suffix);
    }
    raw.micX = parameters.getRawParameterValue("micX"); raw.micY = parameters.getRawParameterValue("micY");
    raw.width = parameters.getRawParameterValue("width"); raw.micMode = parameters.getRawParameterValue("micMode");
    raw.vowel = parameters.getRawParameterValue("vowel");
    raw.attack = parameters.getRawParameterValue("attack"); raw.release = parameters.getRawParameterValue("release");
    raw.ensemble = parameters.getRawParameterValue("ensemble"); raw.breath = parameters.getRawParameterValue("breath");
    raw.roomSize = parameters.getRawParameterValue("roomSize"); raw.roomSeconds = parameters.getRawParameterValue("roomSeconds");
    raw.absorption = parameters.getRawParameterValue("absorption"); raw.diffusion = parameters.getRawParameterValue("diffusion");
    raw.wet = parameters.getRawParameterValue("wet"); raw.master = parameters.getRawParameterValue("master");
}

juce::AudioProcessorValueTreeState::ParameterLayout OmniChoirAudioProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::StringArray names { "Soprano", "Alto", "Tenor", "Bass" };
    const float x[4] { -5.1f, -1.7f, 1.7f, 5.1f }, y[4] { 6.f, 5.2f, 5.2f, 6.f };
    for (int i = 0; i < 4; ++i) {
        const auto id = juce::String(i);
        addFloat(layout, "gain"+id, names[i]+" Level", 0, 1.5f, .001f, i==0 ? .82f : .78f);
        addFloat(layout, "x"+id, names[i]+" X", -9, 9, .01f, x[i], "m");
        addFloat(layout, "y"+id, names[i]+" Y", .7f, 11, .01f, y[i], "m");
    }
    addFloat(layout, "micX", "Listener X", -9, 9, .01f, 0, "m");
    addFloat(layout, "micY", "Listener Y", -8, 8, .01f, 0, "m");
    addFloat(layout, "width", "Microphone Spacing", .1f, 3, .01f, .6f, "m");
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "micMode", 1 },
        "Microphone Array", juce::StringArray { "AB", "ORTF", "XY", "Mid/Side", "Decca", "Binaural" }, 1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "vowel", 1 },
        "Vowel", juce::StringArray { "Ah", "Eh", "Ee", "Oh", "Oo" }, 0));
    addFloat(layout, "attack", "Attack", .008f, 2.5f, .001f, .13f, "s");
    addFloat(layout, "release", "Release", .05f, 5.0f, .001f, .6f, "s");
    addFloat(layout, "ensemble", "Ensemble", 0, 1, .001f, .48f);
    addFloat(layout, "breath", "Breath", 0, 1, .001f, .12f);
    addFloat(layout, "roomSize", "Room Size", 0, 1, .001f, .52f);
    addFloat(layout, "roomSeconds", "Reverb Time", .2f, 10, .01f, 2.4f, "s");
    addFloat(layout, "absorption", "Absorption", 0, 1, .001f, .4f);
    addFloat(layout, "diffusion", "Diffusion", 0, 1, .001f, .7f);
    addFloat(layout, "wet", "Room Level", 0, 1.5f, .001f, .32f);
    addFloat(layout, "master", "Output", 0, 1.5f, .001f, .7f);
    return layout;
}

void OmniChoirAudioProcessor::prepareToPlay(double sampleRate, int) {
    instrument.prepare(sampleRate);
    instrument.setParameters(readParams());
    activeVoiceCount.store(0, std::memory_order_relaxed);
}
void OmniChoirAudioProcessor::releaseResources() { instrument.allNotesOff(); }
bool OmniChoirAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    return layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && layout.getMainInputChannelSet().isDisabled();
}
choir::Params OmniChoirAudioProcessor::readParams() const {
    choir::Params p;
    auto read = [](const std::atomic<float>* ptr) { return ptr->load(std::memory_order_relaxed); };
    for (int i = 0; i < 4; ++i) {
        p.gain[(size_t)i] = read(raw.gains[(size_t)i]);
        p.x[(size_t)i] = read(raw.xs[(size_t)i]);
        p.y[(size_t)i] = read(raw.ys[(size_t)i]);
    }
    p.micX = read(raw.micX); p.micY = read(raw.micY);
    p.width = read(raw.width); p.micMode = (int)read(raw.micMode); p.vowel = (int)read(raw.vowel);
    p.attack = read(raw.attack); p.release = read(raw.release);
    p.ensemble = read(raw.ensemble); p.breath = read(raw.breath);
    p.roomSize = read(raw.roomSize); p.roomSeconds = read(raw.roomSeconds);
    p.absorption = read(raw.absorption); p.diffusion = read(raw.diffusion);
    p.wet = read(raw.wet); p.master = read(raw.master);
    return p;
}

void OmniChoirAudioProcessor::handleMidi(const juce::MidiMessage& m) {
    if (m.isNoteOn()) instrument.noteOn(m.getChannel(), m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) instrument.noteOff(m.getChannel(), m.getNoteNumber());
    else if (m.isPitchWheel()) instrument.pitchWheel(m.getChannel(), m.getPitchWheelValue());
    else if (m.isController() && m.getControllerNumber() == 64) instrument.sustain(m.getChannel(), m.getControllerValue() >= 64);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) instrument.allNotesOff();
}
void OmniChoirAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals denormals;
    buffer.clear();
    instrument.setParameters(readParams());
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();
    int cursor = 0;
    auto renderThrough = [&](int end) {
        end = juce::jlimit(cursor, n, end);
        while (cursor < end) {
            instrument.render(left[cursor], right[cursor]);
            ++cursor;
        }
    };
    // Honor each MIDI event's sample offset, including multiple events in one DAW block.
    for (const auto event : midi) {
        renderThrough(event.samplePosition);
        handleMidi(event.getMessage());
    }
    renderThrough(n);
    activeVoiceCount.store(instrument.activeVoices(), std::memory_order_relaxed);
    midi.clear();
}
juce::AudioProcessorEditor* OmniChoirAudioProcessor::createEditor() { return new OmniChoirAudioProcessorEditor(*this); }
void OmniChoirAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void OmniChoirAudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OmniChoirAudioProcessor(); }
