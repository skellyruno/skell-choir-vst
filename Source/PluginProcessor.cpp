#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    : AudioProcessor(BusesProperties()
                     .withInput("Audio Input",juce::AudioChannelSet::stereo(),true)
                     .withOutput("Audio Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"OMNICHOIR_FX_PARAMETERS",createLayout()) {
    for (int i=0;i<4;++i) {
        const auto s=juce::String(i);
        raw.gains[(size_t)i]=parameters.getRawParameterValue("gain"+s);
        raw.xs[(size_t)i]=parameters.getRawParameterValue("x"+s);
        raw.ys[(size_t)i]=parameters.getRawParameterValue("y"+s);
    }
    auto get=[this](const char* id){ return parameters.getRawParameterValue(id); };
    raw.micX=get("micX"); raw.micY=get("micY"); raw.width=get("width"); raw.micMode=get("micMode");
    raw.choirOn=get("choirOn");raw.mode=get("mode"); raw.harmony=get("harmony"); raw.ensemble=get("ensemble"); raw.sensitivity=get("sensitivity");
    raw.roomSize=get("roomSize"); raw.roomSeconds=get("roomSeconds"); raw.absorption=get("absorption");
    raw.diffusion=get("diffusion"); raw.wet=get("wet"); raw.mix=get("mix"); raw.master=get("master");
    raw.leadX=get("leadX");raw.leadY=get("leadY");raw.leadGain=get("leadGain");
    raw.humanize=get("humanize");raw.formant=get("formant");
    raw.hall=get("hall");raw.hallSeconds=get("hallSeconds");
}
juce::AudioProcessorValueTreeState::ParameterLayout OmniChoirAudioProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::StringArray names { "Soprano", "Alto", "Tenor", "Bass" };
    const float x[4] {-5.1f,-1.7f,1.7f,5.1f}, y[4] {6.f,5.2f,5.2f,6.f};
    for(int i=0;i<4;++i) {
        const auto s=juce::String(i);
        addFloat(layout,"gain"+s,names[i]+" Level",0.f,1.5f,.001f,.83f);
        addFloat(layout,"x"+s,names[i]+" X",-9.f,9.f,.01f,x[i],"m");
        addFloat(layout,"y"+s,names[i]+" Y",.7f,11.f,.01f,y[i],"m");
    }
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"choirOn",1},"Choir Enabled",true));
    addFloat(layout,"micX","Listener X",-9.f,9.f,.01f,0.f,"m");
    addFloat(layout,"micY","Listener Y",-8.f,8.f,.01f,0.f,"m");
    addFloat(layout,"width","Mic Spacing",.1f,3.f,.01f,.6f,"m");
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"micMode",1},
        "Microphone Array",juce::StringArray{"AB","ORTF","XY","Mid/Side","Decca","Binaural"},1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},
        "Processing Mode",juce::StringArray{"Live","Scan / Harmony"},0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"harmony",1},
        "Harmony",juce::StringArray{"Major","Minor","Unison"},0));
    addFloat(layout,"ensemble","Ensemble Detune",0.f,1.f,.001f,.58f);
    addFloat(layout,"humanize","Human Variation",0.f,1.f,.001f,.75f);
    addFloat(layout,"formant","Singer Character",0.f,1.f,.001f,.5f);
    addFloat(layout,"leadX","Lead X",-9.f,9.f,.01f,0.f,"m");
    addFloat(layout,"leadY","Lead Y",-5.f,12.f,.01f,.4f,"m");
    addFloat(layout,"leadGain","Lead Level",0.f,1.5f,.001f,1.f);
    addFloat(layout,"sensitivity","Input Sensitivity",0.f,1.f,.001f,.65f);
    addFloat(layout,"roomSize","Room Size",0.f,1.f,.001f,.52f);
    addFloat(layout,"roomSeconds","Reverb Time",.2f,10.f,.01f,2.4f,"s");
    addFloat(layout,"absorption","Absorption",0.f,1.f,.001f,.4f);
    addFloat(layout,"diffusion","Diffusion",0.f,1.f,.001f,.7f);
    addFloat(layout,"wet","Room Return",0.f,1.5f,.001f,.37f);
    addFloat(layout,"hall","Hall",0.f,.5f,.001f,0.f);
    addFloat(layout,"hallSeconds","Hall Time",.5f,8.f,.01f,2.4f,"s");
    addFloat(layout,"mix","Choir Mix",0.f,1.f,.001f,.78f);
    addFloat(layout,"master","Output",0.f,1.5f,.001f,.9f);
    return layout;
}
void OmniChoirAudioProcessor::prepareToPlay(double sampleRate,int) {
    staged.prepare(sampleRate);
    staged.voices().setSavedScanRoot(savedRoot.load(), visibleStatus.load()==2);
    staged.setParameters(readParams());
    setLatencySamples(choir::StagedChoir::latencySamples);   // the stage works in 128-sample blocks; the dry lead is delayed to match
}
void OmniChoirAudioProcessor::releaseResources() { staged.reset(); }
bool OmniChoirAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto in=layout.getMainInputChannelSet(),out=layout.getMainOutputChannelSet();
    return (in==juce::AudioChannelSet::mono() || in==juce::AudioChannelSet::stereo())
        && (out==juce::AudioChannelSet::stereo() || (out==juce::AudioChannelSet::mono() && in==juce::AudioChannelSet::mono()));
}
choir::FXParams OmniChoirAudioProcessor::readParams() const {
    choir::FXParams p;
    const auto read=[](const std::atomic<float>* x){return x->load(std::memory_order_relaxed);};
    for(int i=0;i<4;++i) {p.gain[(size_t)i]=read(raw.gains[(size_t)i]);p.x[(size_t)i]=read(raw.xs[(size_t)i]);p.y[(size_t)i]=read(raw.ys[(size_t)i]);}
    p.micX=read(raw.micX);p.micY=read(raw.micY);p.width=read(raw.width);p.micMode=(int)read(raw.micMode);
    p.choirOn=read(raw.choirOn)>.5f;p.mode=(int)read(raw.mode);p.harmony=(int)read(raw.harmony);p.ensemble=read(raw.ensemble);p.sensitivity=read(raw.sensitivity);
    p.roomSize=read(raw.roomSize);p.roomSeconds=read(raw.roomSeconds);p.absorption=read(raw.absorption);
    p.diffusion=read(raw.diffusion);p.wet=read(raw.wet);p.mix=read(raw.mix);p.master=read(raw.master);
    // These GUI/APVTS controls existed in v0.4 but were not passed to DSP.
    p.leadX=read(raw.leadX);p.leadY=read(raw.leadY);p.leadGain=read(raw.leadGain);
    p.humanize=read(raw.humanize);p.formant=read(raw.formant);
    p.hall=read(raw.hall);p.hallSeconds=read(raw.hallSeconds);
    return p;
}
void OmniChoirAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals denormals;
    juce::ignoreUnused(midi);
    if(restoreRequested.exchange(false,std::memory_order_acq_rel))
        staged.voices().setSavedScanRoot(savedRoot.load(),visibleStatus.load()==2);
    if(scanRequested.exchange(false,std::memory_order_acq_rel)) staged.voices().beginScan();
    staged.setParameters(readParams());
    const int n=buffer.getNumSamples(),inputs=getTotalNumInputChannels(),outputs=getTotalNumOutputChannels();
    if(outputs==0 || inputs==0) {buffer.clear();return;}
    auto* l=buffer.getWritePointer(0);
    auto* r=outputs>1?buffer.getWritePointer(1):nullptr;
    // In place: the stage reads each input sample before it writes that output sample. Mono in -> the left channel
    // feeds both sides of the lead path.
    staged.process(l,(inputs>1 && r!=nullptr)?r:nullptr,l,r,n);
    for(int c=outputs;c<buffer.getNumChannels();++c)buffer.clear(c,0,n);
    auto& effect=staged.voices();
    visibleStatus.store(effect.scanStatus(),std::memory_order_relaxed);
    visibleProgress.store(effect.scanProgress(),std::memory_order_relaxed);
    visiblePitch.store(effect.detectedMidi(),std::memory_order_relaxed);
    visibleRms.store(effect.inputRms(),std::memory_order_relaxed);
    savedRoot.store(effect.scanRoot(),std::memory_order_relaxed);
}
juce::AudioProcessorEditor* OmniChoirAudioProcessor::createEditor() {return new OmniChoirAudioProcessorEditor(*this);}
void OmniChoirAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if(auto xml=parameters.copyState().createXml()) {
        xml->setAttribute("scanPitchClass",savedRoot.load());
        xml->setAttribute("scanWasReady",visibleStatus.load()==2);
        copyXmlToBinary(*xml,dest);
    }
}
void OmniChoirAudioProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) {
        if(xml->hasTagName(parameters.state.getType())) {
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
            savedRoot.store(juce::jlimit(0,11,xml->getIntAttribute("scanPitchClass",0)));
            visibleStatus.store(xml->getBoolAttribute("scanWasReady",false)?2:0);
            restoreRequested.store(true,std::memory_order_release);
        }
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new OmniChoirAudioProcessor();}
