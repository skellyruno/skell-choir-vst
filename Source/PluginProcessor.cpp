#include "PluginProcessor.h"
#include <BinaryData.h>
#include "PluginEditor.h"

namespace {
// Saved-state version. Absent = 1 (v0.4 to v0.6.0). 2 = v0.6.1: CHOIR LEVEL exists and the faithful choir carries a +12 dB make-up.
// 4 = v0.8: KEY/SCALE, CONSONANTS, SWELL, PULSE, LISTEN (mode 1 became Listen). A session older than 4 loads with CONSONANTS off.
// 3 = v0.7: AUTO LEVEL (levelMatch), CHOIR SIZE, solo/mute, MIDI chord mode, built-in rooms. A session older than 3 loads with AUTO LEVEL off.
constexpr int kStateVersion=4;
// A session saved before version 2 was balanced against the quieter choir. To keep it sounding the way it did, it loads with CHOIR LEVEL
// at this value (the dial then shows why). Set it to 0 to give old sessions the new, louder default instead.
constexpr float kLegacyChoirLevelDb=-12.f;
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
    for (auto& note:chordView) note.store(-1);   // no key held
    for (int i=0;i<4;++i) {
        const auto s=juce::String(i);
        raw.gains[(size_t)i]=parameters.getRawParameterValue("gain"+s);
        raw.xs[(size_t)i]=parameters.getRawParameterValue("x"+s);
        raw.ys[(size_t)i]=parameters.getRawParameterValue("y"+s);
    }
    auto get=[this](const juce::String& id){ return parameters.getRawParameterValue(id); };
    raw.micX=get("micX"); raw.micY=get("micY"); raw.width=get("width"); raw.micMode=get("micMode");
    raw.choirOn=get("choirOn");raw.mode=get("mode"); raw.harmony=get("harmony"); raw.ensemble=get("ensemble"); raw.sensitivity=get("sensitivity");
    raw.roomSize=get("roomSize"); raw.roomSeconds=get("roomSeconds"); raw.absorption=get("absorption");
    raw.diffusion=get("diffusion"); raw.wet=get("wet"); raw.mix=get("mix"); raw.master=get("master");
    raw.leadX=get("leadX");raw.leadY=get("leadY");raw.leadGain=get("leadGain");
    raw.humanize=get("humanize");raw.formant=get("formant");
    raw.hall=get("hall");raw.hallSeconds=get("hallSeconds");
    raw.voiceBlend=get("voiceBlend");raw.choirVowel=get("choirVowel");raw.vowelFollow=get("vowelFollow");
    raw.choirBreath=get("choirBreath");raw.vibrato=get("vibrato");raw.classicStage=get("classicStage");
    raw.choirLevel=get("choirLevel");raw.levelMatch=get("levelMatch");raw.choirSize=get("choirSize");
    raw.consonants=get("consonants");raw.swell=get("swell");raw.keyRoot=get("keyRoot");raw.scale=get("scale");raw.pulseRate=get("pulseRate");raw.pulseDepth=get("pulseDepth");
    for(int i=0;i<4;++i) raw.sectionState[(size_t)i]=get("sectionState"+juce::String(i));
    // All Mini/Choir receivers connect automatically. Only the user-facing
    // SEND TO TUNE button in SkellKey starts publication.
    setFollowSkellKey(true);
}
OmniChoirAudioProcessor::~OmniChoirAudioProcessor() { setFollowSkellKey(false); }

bool OmniChoirAudioProcessor::setFollowSkellKey(bool on) {
    if (!on) {
        stopTimer();
        followWorldKey_.store(false,std::memory_order_release);
        linkedKey_.store(0,std::memory_order_release);
        worldLink_.leave();
        return true;
    }
    if (!worldLink_.requested() && !worldLink_.join()) return false;
    followWorldKey_.store(true,std::memory_order_release);
    startTimerHz(5);
    timerCallback();
    return true;
}

void OmniChoirAudioProcessor::timerCallback() {
    if (!followingSkellKey() || !worldLink_.online()) {
        linkedKey_.store(0,std::memory_order_release);
        return;
    }
    skellcore::world::MusicalTuning tuning;
    std::uint32_t tuningVersion=0;
    if (worldLink_.latestMusicalTuning(tuning,tuningVersion)) {
        const int packed=skellcore::world::MusicalKey{tuning.root,tuning.mode}.packed();
        const int previous=linkedKey_.exchange(packed,std::memory_order_acq_rel);
        // Update the existing DAW Key/Scale parameters only when the confirmed
        // sender changes tuning. Do not add IDs or touch the audio callback.
        if (packed != previous) {
            const auto sync=[this](const char* id,float value) {
                auto* p=parameters.getParameter(id);
                const auto* current=parameters.getRawParameterValue(id);
                if (p && current && std::abs(current->load()-value)>0.01f) {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost(p->convertTo0to1(value));
                    p->endChangeGesture();
                }
            };
            sync("keyRoot",float(tuning.root));
            sync("scale",float(tuning.mode==0 ? 1 : 2));
        }
        receivedA4_.store(float(tuning.a4Hz()),std::memory_order_release);
        return;
    }
    skellcore::world::MusicalKey key;
    std::uint64_t version=0;
    linkedKey_.store(worldLink_.latestMusicalKey(key,version) ? key.packed() : 0,
                     std::memory_order_release);
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
        "Processing Mode",juce::StringArray{"Live","Listen","MIDI Chord"},0));
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
    addFloat(layout,"master","Output",0.f,1.5f,.001f,1.f);   // v0.7: default 1.0 (AUTO LEVEL now does the loudness work). New instances only; saved sessions keep their value. The range is unchanged so host automation keeps its meaning.
    addFloat(layout,"choirLevel","Choir Level",-12.f,12.f,.1f,0.f,"dB");
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"levelMatch",1},"Auto Level",true));   // v0.7
    addFloat(layout,"choirSize","Choir Size",0.f,1.f,.001f,.5f);                                                   // v0.7 macro, 0.5 = neutral
    for(int i=0;i<4;++i)                                                                                           // v0.7 solo / mute
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sectionState"+juce::String(i),1},
            names[i]+" Solo/Mute",juce::StringArray{"Normal","Solo","Mute"},0));
    // v0.8: the key and scale are yours to choose (nothing is detected); Chromatic keeps the fixed intervals of earlier versions
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"keyRoot",1},"Key",
        juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},0));
    juce::StringArray scaleNames;
    for(int i=0;i<choir::kNumScales;++i) scaleNames.add(choir::kScales[i].name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scale",1},"Scale",scaleNames,0));
    addFloat(layout,"consonants","Consonants",0.f,1.f,.001f,.5f);       // v0.8: your own s, t and k pass through cleanly
    addFloat(layout,"swell","Swell",0.f,1.f,.001f,0.f);                // v0.8: the choir follows how loud you sing against your own average
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"pulseRate",1},"Pulse Rate",
        juce::StringArray{"Off","2 bars","1 bar","1/2 bar","1 beat","1/2 beat"},0));
    addFloat(layout,"pulseDepth","Pulse Depth",0.f,1.f,.001f,.5f);
    return layout;
}
void OmniChoirAudioProcessor::prepareToPlay(double sampleRate,int samplesPerBlock) {
    hostRate.store(sampleRate);
    monoScratch.assign((size_t)std::max(samplesPerBlock*2,16384),0.f);
    playback.reset();
    staged.prepare(sampleRate);
    staged.voices().setSavedScanRoot(savedRoot.load(), visibleStatus.load()==2);
    staged.setParameters(readParams());
    setLatencySamples(choir::StagedChoir::latencySamples);   // the stage works in 128-sample blocks; the dry lead is delayed to match
}
void OmniChoirAudioProcessor::releaseResources() { staged.reset(); chord.allOff(); playback.reset(); }
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
    p.choirLevelDb=read(raw.choirLevel);p.levelMatch=read(raw.levelMatch)>.5f;
    p.keyRoot=(int)read(raw.keyRoot);p.scale=(int)read(raw.scale);p.consonants=read(raw.consonants);
    if (followWorldKey_.load(std::memory_order_relaxed)) {
        const auto key=skellcore::world::unpackMusicalKey(linkedKey_.load(std::memory_order_acquire));
        if (key.valid()) {
            p.keyRoot=key.root;
            p.scale=key.mode == 0 ? 1 : 2;
        }
    }
    p.swell=read(raw.swell);p.pulseRate=(int)read(raw.pulseRate);p.pulseDepth=read(raw.pulseDepth);
    choir::applySize(p,read(raw.choirSize));
    // solo and mute: a muted section is silent; if any section is soloed, only soloed sections are heard
    bool anySolo=false;
    for(int i=0;i<4;++i) if((int)read(raw.sectionState[(size_t)i])==1) anySolo=true;
    for(int i=0;i<4;++i) {
        const int state=(int)read(raw.sectionState[(size_t)i]);
        if(anySolo?state!=1:state==2) p.gain[(size_t)i]=0.f;
    }
    return p;
}
void OmniChoirAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals denormals;
    for(const auto metadata:midi) {   // v0.7: the chord the choir sings in MIDI Chord mode (tracked in every mode, so switching to it needs no re-press)
        const auto message=metadata.getMessage();
        if(message.isNoteOn()) chord.noteOn(message.getNoteNumber());
        else if(message.isNoteOff()) chord.noteOff(message.getNoteNumber());
        else if(message.isAllNotesOff() || message.isAllSoundOff()) chord.allOff();
        else if(message.isSustainPedalOn()) chord.sustain(true);
        else if(message.isSustainPedalOff()) chord.sustain(false);
    }
    if(restoreRequested.exchange(false,std::memory_order_acq_rel))
        staged.voices().setSavedScanRoot(savedRoot.load(),visibleStatus.load()==2);
    if(scanRequested.exchange(false,std::memory_order_acq_rel)) staged.voices().beginScan();
    // v0.8: the host's position gives LISTEN its timeline and PULSE its tempo
    double hostSeconds=-1.0,hostPpq=-1.0,hostBpm=0.0;bool hostKnown=false;
    if(auto* head=getPlayHead()) {
        const auto position=head->getPosition();
        if(position.hasValue()) {
            const auto samples=position->getTimeInSamples();
            if(samples.hasValue()) {hostSeconds=(double)*samples/getSampleRate();hostKnown=true;}
            const auto ppq=position->getPpqPosition();
            if(ppq.hasValue()) hostPpq=*ppq;
            const auto bpm=position->getBpm();
            if(bpm.hasValue()) hostBpm=*bpm;
        }
    }
    auto params=readParams();
    params.ppq=hostPpq;params.bpm=hostBpm;
    params.midiNotes=chord.sections();
    for(size_t i=0;i<4;++i) chordView[i].store(params.midiNotes[i],std::memory_order_relaxed);
    staged.setParameters(params);
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
    // LISTEN: while listening, the incoming voice is captured (mono) before it is processed in place; in Listen mode the choir then
    // sings the score the take was analysed into, lined up with the host's position
    if(listen.state()==choir::ListenSession::Listening && !monoScratch.empty()) {
        const float* right=(inputs>1 && r!=nullptr)?r:nullptr;
        for(int start=0;start<n;start+=(int)monoScratch.size()) {
            const int count=std::min<int>((int)monoScratch.size(),n-start);
            for(int i=0;i<count;++i) monoScratch[(size_t)i]=right!=nullptr?.5f*(l[start+i]+right[start+i]):l[start+i];
            listen.feed(monoScratch.data(),count,hostSeconds+(double)start/getSampleRate(),hostKnown);
        }
    }
    {
        const choir::FullTakeScore* score=params.mode==1?listen.score():nullptr;
        double takeTime=-1.0;
        if(score!=nullptr && hostKnown && listen.hostPositionKnown()) takeTime=hostSeconds-listen.originSeconds();
        playback.update(staged.voices(),score,takeTime);
        playheadSeconds.store(takeTime,std::memory_order_relaxed);
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
        xml->setAttribute("omniStateVersion",kStateVersion);
        xml->setAttribute("omniProUi",proUi());
        copyXmlToBinary(*xml,dest);
    }
}
void OmniChoirAudioProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) {
        if(xml->hasTagName(parameters.state.getType())) {
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
            const int version=xml->getIntAttribute("omniStateVersion",1);
            if(version<2)   // saved by v0.6.0 or earlier: keep its sound
                if(auto* level=parameters.getParameter("choirLevel")) level->setValueNotifyingHost(level->convertTo0to1(kLegacyChoirLevelDb));
            if(version<3)   // saved before AUTO LEVEL existed: it loads with AUTO LEVEL off, exactly as it sounded
                if(auto* match=parameters.getParameter("levelMatch")) match->setValueNotifyingHost(match->convertTo0to1(0.f));
            if(version<4)   // saved before CONSONANTS existed: it loads with CONSONANTS off, exactly as it sounded
                if(auto* cons=parameters.getParameter("consonants")) cons->setValueNotifyingHost(cons->convertTo0to1(0.f));
            setProUi(xml->getBoolAttribute("omniProUi",true));   // a session from before the Easy mode keeps the full MAIN page
            savedRoot.store(juce::jlimit(0,11,xml->getIntAttribute("scanPitchClass",0)));
            visibleStatus.store(xml->getBoolAttribute("scanWasReady",false)?2:0);
            restoreRequested.store(true,std::memory_order_release);
            const auto path=xml->getStringAttribute("irPath");
            if(path.startsWith("builtin:")) loadBuiltInImpulse(path.fromFirstOccurrenceOf(":",false,false).getIntValue());
            else if(path.isNotEmpty() && juce::File(path).existsAsFile()) loadImpulseResponse(juce::File(path));
            else if(path.isEmpty()) clearImpulseResponse();
        }
    }
}
void OmniChoirAudioProcessor::loadImpulseResponse(const juce::File& file) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(reader==nullptr) return;
    useImpulseResponse(*reader,file.getFullPathName(),file.getFileName());
}
void OmniChoirAudioProcessor::useImpulseResponse(juce::AudioFormatReader& reader,const juce::String& path,const juce::String& name) {
    if(reader.lengthInSamples<=0 || reader.sampleRate<=0) return;
    const int length=(int)std::min<juce::int64>(reader.lengthInSamples,(juce::int64)(oc::Convolver::kMaxSeconds*reader.sampleRate));
    const int channels=(int)std::min<unsigned int>(2u,reader.numChannels);
    juce::AudioBuffer<float> data(channels,length);
    if(!reader.read(&data,0,length,0,true,channels>1)) return;
    staged.setImpulseResponse(data.getReadPointer(0),channels>1?data.getReadPointer(1):nullptr,length,reader.sampleRate);
    irPath=path;irName=name;
}
const char* OmniChoirAudioProcessor::builtInRoomName(int index) {
    switch(index) {case 0:return "Cathedral";case 1:return "Stone Chapel";case 2:return "Small Hall";default:return "";}
}
void OmniChoirAudioProcessor::loadBuiltInImpulse(int index) {
    const char* data=nullptr;int size=0;
    switch(index) {
        case 0:data=BinaryData::room_cathedral_wav;size=BinaryData::room_cathedral_wavSize;break;
        case 1:data=BinaryData::room_chapel_wav;size=BinaryData::room_chapel_wavSize;break;
        case 2:data=BinaryData::room_small_hall_wav;size=BinaryData::room_small_hall_wavSize;break;
        default:return;
    }
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::make_unique<juce::MemoryInputStream>(data,(size_t)size,false)));
    if(reader==nullptr) return;
    useImpulseResponse(*reader,"builtin:"+juce::String(index),builtInRoomName(index));
}
void OmniChoirAudioProcessor::chooseBuiltInRoom(int index) {
    loadBuiltInImpulse(index);
    // a room is only heard through the HALL: if it is off, bring it up so choosing a room does something
    if(auto* hall=parameters.getRawParameterValue("hall"))
        if(hall->load()<.05f) setParameterValue("hall",.25f);
}
void OmniChoirAudioProcessor::clearImpulseResponse() {
    staged.setImpulseResponse(nullptr,nullptr,0,48000.0);
    irPath.clear();irName.clear();
}
void OmniChoirAudioProcessor::setParameterValue(const juce::String& id,float value) {
    if(auto* parameter=parameters.getParameter(id)) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}
void OmniChoirAudioProcessor::applyStagePreset(int index) {
    if(index<0 || index>=choir::kNumStagePresets) return;
    const auto& s=choir::kStagePresets[index];
    const auto set=[this](const juce::String& id,float value) {setParameterValue(id,value);};
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
void OmniChoirAudioProcessor::applyStyleValues(const choir::StylePreset& s) {
    applyStagePreset(s.stage);   // where they stand and the room
    const auto set=[this](const juce::String& id,float value) {setParameterValue(id,value);};
    set("harmony",(float)s.harmony);set("mix",s.mix);set("choirSize",s.size);
    set("ensemble",s.ensemble);set("humanize",s.humanize);set("formant",s.formant);
    set("voiceBlend",s.voiceBlend);set("choirVowel",s.choirVowel);set("vowelFollow",s.vowelFollow);
    set("choirBreath",s.choirBreath);set("vibrato",s.vibrato);
    // never touched: the lead level, OUTPUT, AUTO LEVEL, CHOIR LEVEL, solo/mute and the PROCESS mode
}
void OmniChoirAudioProcessor::applyStyle(int index) {
    if(index<0 || index>=choir::kNumStylePresets) return;
    applyStyleValues(choir::kStylePresets[index]);
}
void OmniChoirAudioProcessor::surpriseMe() {
    if(surpriseState==0) surpriseState=juce::Time::getMillisecondCounter()|1u;
    applyStyleValues(choir::randomStyle(surpriseState));
}
void OmniChoirAudioProcessor::setCurrentProgram(int index) {
    // some hosts set program 0 when they load a plugin: only a real change applies a style, so a restored session is never overwritten
    if(index<0 || index>=choir::kNumStylePresets || index==currentProgram) return;
    currentProgram=index;
    applyStyle(index);
}
juce::File OmniChoirAudioProcessor::userStyleFolder() {
    const auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("OmniChoir FX").getChildFile("Styles");
    folder.createDirectory();
    return folder;
}
juce::Array<juce::File> OmniChoirAudioProcessor::userStyleFiles() const {
    return userStyleFolder().findChildFiles(juce::File::findFiles,false,"*.omnistyle");
}
bool OmniChoirAudioProcessor::saveStyleFile(const juce::File& file) {
    std::vector<std::pair<std::string,float>> values;
    for(int i=0;i<choir::kNumUserStyleIds;++i)
        if(auto* value=parameters.getRawParameterValue(choir::kUserStyleIds[i])) values.emplace_back(choir::kUserStyleIds[i],value->load());
    return file.replaceWithText(juce::String(choir::writeStyleFile(file.getFileNameWithoutExtension().toStdString(),values)));
}
bool OmniChoirAudioProcessor::loadStyleFile(const juce::File& file) {
    if(!file.existsAsFile() || file.getSize()>64*1024) return false;   // a style file is a couple of kilobytes: refuse anything silly
    choir::StyleFileData data;
    if(!choir::parseStyleFile(file.loadFileAsString().toStdString(),data) || data.values.empty()) return false;
    for(const auto& value:data.values) setParameterValue(juce::String(value.first),value.second);
    return true;
}
void OmniChoirAudioProcessor::startListening() {
    const double rate=hostRate.load();
    listen.start(rate,std::min(kMaxListenSeconds,8.0e6/rate));   // at most 120 s, and at most ~32 MB of capture at any sample rate
}
void OmniChoirAudioProcessor::stopListening() {
    const auto p=readParams();
    listen.stop(p.keyRoot,p.scale,p.harmony);
}
void OmniChoirAudioProcessor::clearListening() {listen.clear();}
void OmniChoirAudioProcessor::setMode(int mode) {setParameterValue("mode",(float)mode);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new OmniChoirAudioProcessor();}
