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
                     .withOutput("Audio Output",juce::AudioChannelSet::stereo(),true)
                     // optional stem outputs, off until the host enables them (one stereo bus per section, plus the placed lead)
                     .withOutput("Sopranos",juce::AudioChannelSet::stereo(),false)
                     .withOutput("Altos",juce::AudioChannelSet::stereo(),false)
                     .withOutput("Tenors",juce::AudioChannelSet::stereo(),false)
                     .withOutput("Basses",juce::AudioChannelSet::stereo(),false)
                     .withOutput("Lead",juce::AudioChannelSet::stereo(),false)),
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
    raw.voiceBlend=get("voiceBlend");raw.choirVowel=get("choirVowel");raw.vowelFollow=get("vowelFollow");
    raw.choirBreath=get("choirBreath");raw.vibrato=get("vibrato");raw.classicStage=get("classicStage");
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
    addFloat(layout,"voiceBlend","Choir Voice",0.f,1.f,.001f,0.f);
    addFloat(layout,"choirVowel","Vowel",0.f,1.f,.001f,0.f);
    addFloat(layout,"vowelFollow","Vowel Follows Lead",0.f,1.f,.001f,0.f);
    addFloat(layout,"choirBreath","Breath",0.f,1.f,.001f,.5f);
    addFloat(layout,"vibrato","Vibrato",0.f,1.f,.001f,.6f);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"classicStage",1},"Classic v0.5 Stage (A/B)",false));
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
    for(int i=1;i<layout.outputBuses.size();++i) {   // stem buses: off, or stereo
        const auto stem=layout.outputBuses[i];
        if(!stem.isDisabled() && stem!=juce::AudioChannelSet::stereo()) return false;
    }
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
    p.voiceBlend=read(raw.voiceBlend);p.choirVowel=read(raw.choirVowel);p.vowelFollow=read(raw.vowelFollow);
    p.choirBreath=read(raw.choirBreath);p.vibrato=read(raw.vibrato);p.classicStage=read(raw.classicStage)>.5f;
    return p;
}
void OmniChoirAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals denormals;
    juce::ignoreUnused(midi);
    if(restoreRequested.exchange(false,std::memory_order_acq_rel))
        staged.voices().setSavedScanRoot(savedRoot.load(),visibleStatus.load()==2);
    if(scanRequested.exchange(false,std::memory_order_acq_rel)) staged.voices().beginScan();
    staged.setParameters(readParams());
    const int n=buffer.getNumSamples(),inputs=getMainBusNumInputChannels(),outputs=getMainBusNumOutputChannels();
    if(outputs==0 || inputs==0) {buffer.clear();return;}
    auto mainOut=getBusBuffer(buffer,false,0);
    auto* l=mainOut.getWritePointer(0);
    auto* r=outputs>1?mainOut.getWritePointer(1):nullptr;
    // Optional stem buses: only those the host has switched on are rendered.
    choir::StagedChoir::StemOut stems[choir::StagedChoir::numStems];
    bool anyStem=false;
    for(int s=0;s<choir::StagedChoir::numStems;++s) {
        const int busIndex=s+1;
        if(busIndex>=getBusCount(false)) break;
        auto* bus=getBus(false,busIndex);
        if(bus==nullptr || !bus->isEnabled()) continue;
        auto stemBuffer=getBusBuffer(buffer,false,busIndex);
        if(stemBuffer.getNumChannels()<2) {stemBuffer.clear();continue;}
        stems[s].left=stemBuffer.getWritePointer(0);
        stems[s].right=stemBuffer.getWritePointer(1);
        anyStem=true;
    }
    // In place: the stage reads each input sample before it writes that output sample. Mono in -> the left channel
    // feeds both sides of the lead path.
    staged.process(l,(inputs>1 && r!=nullptr)?r:nullptr,l,r,n,anyStem?stems:nullptr);
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
        xml->setAttribute("irPath",irPath);
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
            const auto path=xml->getStringAttribute("irPath");
            if(path.isNotEmpty() && juce::File(path).existsAsFile()) loadImpulseResponse(juce::File(path));
            else if(path.isEmpty()) clearImpulseResponse();
        }
    }
}
void OmniChoirAudioProcessor::loadImpulseResponse(const juce::File& file) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(reader==nullptr || reader->lengthInSamples<=0 || reader->sampleRate<=0) return;
    const int length=(int)std::min<juce::int64>(reader->lengthInSamples,(juce::int64)(oc::Convolver::kMaxSeconds*reader->sampleRate));
    const int channels=(int)std::min<unsigned int>(2u,reader->numChannels);
    juce::AudioBuffer<float> data(channels,length);
    if(!reader->read(&data,0,length,0,true,channels>1)) return;
    staged.setImpulseResponse(data.getReadPointer(0),channels>1?data.getReadPointer(1):nullptr,length,reader->sampleRate);
    irPath=file.getFullPathName();irName=file.getFileName();
}
void OmniChoirAudioProcessor::clearImpulseResponse() {
    staged.setImpulseResponse(nullptr,nullptr,0,48000.0);
    irPath.clear();irName.clear();
}
void OmniChoirAudioProcessor::applyStagePreset(int index) {
    if(index<0 || index>=choir::kNumStagePresets) return;
    const auto& s=choir::kStagePresets[index];
    const auto set=[this](const juce::String& id,float value) {
        if(auto* parameter=parameters.getParameter(id)) {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    for(int i=0;i<4;++i) {
        const auto k=juce::String(i);
        set("x"+k,s.x[i]);set("y"+k,s.y[i]);set("gain"+k,s.gain[i]);
    }
    set("micX",s.micX);set("micY",s.micY);set("width",s.width);set("micMode",(float)s.micMode);
    set("roomSize",s.roomSize);set("roomSeconds",s.roomSeconds);set("absorption",s.absorption);
    set("diffusion",s.diffusion);set("wet",s.wet);set("hall",s.hall);set("hallSeconds",s.hallSeconds);
    set("ensemble",s.ensemble);set("humanize",s.humanize);
    // the lead keeps standing where the user put it relative to the new microphone position
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new OmniChoirAudioProcessor();}
