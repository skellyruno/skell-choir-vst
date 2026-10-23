#include "TakeWorker.h"
#include <algorithm>
#include <cmath>
#include <limits>

bool TakeWorker::scanFile(const juce::File& file,bool minor) {
    if(isThreadRunning() || !file.existsAsFile())return false;
    {const juce::ScopedLock lock(mutex);
     input=file;score={};score.minor=minor;error.clear();}
    job=Job::Scan;fraction=0;state=1;startThread();return true;
}
bool TakeWorker::exportFile(const juce::File& file,const choir::FXParams& params) {
    if(isThreadRunning() || snapshot().notes.empty())return false;
    {const juce::ScopedLock lock(mutex);output=file;renderParams=params;error.clear();}
    job=Job::Export;fraction=0;state=3;startThread();return true;
}
bool TakeWorker::exportMidiFile(const juce::File& file) {
    if(isThreadRunning() || snapshot().notes.empty())return false;
    {const juce::ScopedLock lock(mutex);output=file;error.clear();}
    job=Job::Midi;fraction=0;state=5;startThread();return true;
}
bool TakeWorker::mixStemsFile(const juce::File& manifest,const juce::File& file,
                              const choir::FXParams& params) {
    if(isThreadRunning() || !manifest.existsAsFile() || file==manifest)return false;
    {const juce::ScopedLock lock(mutex);stemManifest=manifest;output=file;stemParams=params;error.clear();}
    job=Job::Stems;fraction=0;state=7;startThread();return true;
}
bool TakeWorker::changeNote(int row,int part,int step) {
    if(isThreadRunning())return false;
    const juce::ScopedLock lock(mutex);
    return row>=0&&choir::transposeNote(score,(size_t)row,part,step);
}
choir::FullTakeScore TakeWorker::snapshot() const {
    const juce::ScopedLock lock(mutex);return score;
}
juce::String TakeWorker::fileName() const {
    const juce::ScopedLock lock(mutex);return input.getFileName();
}
juce::String TakeWorker::errorMessage() const {
    const juce::ScopedLock lock(mutex);return error;
}
bool TakeWorker::failed(const juce::String& message) {
    {const juce::ScopedLock lock(mutex);error=message;}
    state=-1;return false;
}
void TakeWorker::run() {
    switch(job) {
        case Job::Scan:analyse();break;
        case Job::Export:render();break;
        case Job::Midi:renderMidi();break;
        case Job::Stems:mixStems();break;
    }
}
void TakeWorker::analyse() {
    juce::AudioFormatManager mgr;mgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(input));
    if(!reader) {failed("Unable to open WAV/AIFF/FLAC audio.");return;}
    if(reader->lengthInSamples<=0 || reader->sampleRate<16000) {failed("Empty or unsupported audio sample rate.");return;}
    choir::FullTakeScanner scanner;scanner.prepare(reader->sampleRate);
    juce::AudioBuffer<float> buffer((int)std::max(1u,reader->numChannels),4096);
    const int64_t total=reader->lengthInSamples;
    for(int64_t pos=0;pos<total&&!threadShouldExit();pos+=buffer.getNumSamples()) {
        const int count=(int)std::min<int64_t>(buffer.getNumSamples(),total-pos);
        if(!reader->read(&buffer,0,count,pos,true,true)) {failed("Audio read failed.");return;}
        const auto* l=buffer.getReadPointer(0);
        const auto* r=buffer.getReadPointer(std::min(1,buffer.getNumChannels()-1));
        for(int i=0;i<count;++i)scanner.push(.5f*(l[i]+r[i]));
        fraction.store((float)((pos+count)/(double)total));
    }
    if(threadShouldExit())return;
    bool minor=false;{const juce::ScopedLock lock(mutex);minor=score.minor;}
    auto result=scanner.finish(minor);
    {const juce::ScopedLock lock(mutex);score=std::move(result);}
    state=2;
}
void TakeWorker::render() {
    juce::AudioFormatManager mgr;mgr.registerBasicFormats();
    juce::File source,destination;
    choir::FXParams params;
    choir::FullTakeScore scoreCopy;
    {const juce::ScopedLock lock(mutex);source=input;destination=output;params=renderParams;scoreCopy=score;}
    std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(source));
    if(!reader) {failed("Source audio moved or unavailable.");return;}
    if(source==destination) {failed("Choose a different output file to protect your original.");return;}
    destination.deleteFile();
    auto stream=std::unique_ptr<juce::OutputStream>(destination.createOutputStream());
    if(!stream) {failed("Cannot write output file.");return;}
    juce::WavAudioFormat wav;
    const auto options=juce::AudioFormatWriterOptions()
        .withSampleRate(reader->sampleRate)
        .withChannelLayout(juce::AudioChannelSet::stereo())
        .withBitsPerSample(24);
    auto writer=wav.createWriterFor(stream,options);
    if(!writer) {failed("Cannot create 24-bit WAV writer.");return;}
    choir::ChoirEffect fx; fx.prepare(reader->sampleRate);
    params.choirOn=true; params.mix=1.f;params.mode=0;
    fx.setParameters(params);
    juce::AudioBuffer<float> inputBuffer((int)std::max(1u,reader->numChannels),4096);
    juce::AudioBuffer<float> outputBuffer(2,4096);
    size_t event=0,lastScoreEvent=size_t(-1);
    const int64_t total=reader->lengthInSamples;
    for(int64_t pos=0;pos<total&&!threadShouldExit();pos+=inputBuffer.getNumSamples()) {
        const int count=(int)std::min<int64_t>(inputBuffer.getNumSamples(),total-pos);
        if(!reader->read(&inputBuffer,0,count,pos,true,true)){failed("Input read interrupted.");return;}
        const auto* l=inputBuffer.getReadPointer(0);
        const auto* r=inputBuffer.getReadPointer(std::min(1,inputBuffer.getNumChannels()-1));
        auto* outL=outputBuffer.getWritePointer(0);auto* outR=outputBuffer.getWritePointer(1);
        for(int i=0;i<count;++i) {
            const double now=(pos+i)/reader->sampleRate;
            while(event<scoreCopy.notes.size() && scoreCopy.notes[event].end<now)++event;
            if(event<scoreCopy.notes.size() && scoreCopy.notes[event].start<=now
               && now<=scoreCopy.notes[event].end) {
                const auto& note=scoreCopy.notes[event];
                // only recompute ratios at note boundaries (not every sample)
                if(event!=lastScoreEvent) {fx.setScoreVoicing(note.melody,note.satb);lastScoreEvent=event;}
            } else if(lastScoreEvent!=size_t(-1)) {fx.clearScoreVoicing();lastScoreEvent=size_t(-1);}
            fx.render(l[i],r[i],outL[i],outR[i]);
        }
        if(!writer->writeFromAudioSampleBuffer(outputBuffer,0,count)) {
            failed("Could not write WAV samples.");return;
        }
        fraction.store((float)((pos+count)/(double)total));
    }
    writer.reset(); // flush/close before signaling complete
    if(threadShouldExit()){destination.deleteFile();return;}
    // Export the editable score beside the rendered WAV, so arrangement
    // decisions can be inspected/archived even when the plugin UI is closed.
    const auto csv=destination.getSiblingFile(destination.getFileNameWithoutExtension()+".satb.csv");
    if(auto text=std::unique_ptr<juce::FileOutputStream>(csv.createOutputStream())) {
        text->setPosition(0);text->truncate();
        text->writeText("start,end,melody,soprano,alto,tenor,bass,confidence\n",false,false,nullptr);
        for(const auto& n:scoreCopy.notes) {
            const juce::String row=juce::String(n.start,6)+","+juce::String(n.end,6)+","+
              juce::String(n.melody)+","+juce::String(n.satb[0])+","+juce::String(n.satb[1])+","+
              juce::String(n.satb[2])+","+juce::String(n.satb[3])+","+juce::String(n.confidence,2)+"\n";
            text->writeText(row,false,false,nullptr);
        }
    }
    state=4;
}

// Standard MIDI file Type 1: tempo at 120 BPM and five independent tracks
// (S/A/T/B/lead). Uses seconds -> ticks at a constant tempo to preserve
// original full-take positioning. Guide patch 52 is "Choir Aahs" (GM 0-based).
void TakeWorker::renderMidi() {
    choir::FullTakeScore copy;juce::File path;
    {const juce::ScopedLock lock(mutex);copy=score;path=output;}
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(480);
    juce::MidiMessageSequence timing;
    timing.addEvent(juce::MidiMessage::tempoMetaEvent(500000),0.);
    midi.addTrack(timing);
    constexpr const char* trackNames[]={"Soprano","Alto","Tenor","Bass","Lead"};
    for(int part=0;part<5;++part) {
        juce::MidiMessageSequence seq;
        seq.addEvent(juce::MidiMessage::textMetaEvent(3,trackNames[part]),0.);
        seq.addEvent(juce::MidiMessage::programChange(part+1,52),0.);
        for(const auto& n:copy.notes) {
            const int pitch=part==4?n.melody:n.satb[(size_t)part];
            if(pitch<0||pitch>127||!(n.end>n.start))continue;
            const double start=std::max(0.,n.start)*960.,stop=std::max(n.end*960.,start+1.);
            seq.addEvent(juce::MidiMessage::noteOn(part+1,pitch,(juce::uint8)88),start);
            seq.addEvent(juce::MidiMessage::noteOff(part+1,pitch),stop);
        }
        seq.sort();midi.addTrack(seq);
        fraction.store((part+1)/5.f);
        if(threadShouldExit())return;
    }
    // Never leave an incomplete MIDI after an interruption.
    const auto temp=path.getSiblingFile(path.getFileName()+".tmp");
    {
        std::unique_ptr<juce::FileOutputStream> stream(temp.createOutputStream());
        if(!stream || !stream->openedOk() || !midi.writeTo(*stream)) {
            stream.reset();temp.deleteFile();failed("Cannot export MIDI file.");return;
        }
        stream->flush();
    }
    if(threadShouldExit()){temp.deleteFile();return;}
    if(!temp.moveFileTo(path)) {temp.deleteFile();failed("Cannot save MIDI file.");return;}
    state=6;
}

// Runs the C++17 two-pass, constant-memory stem mixer on a worker thread.
// A standard JSON manifest is the same one accepted by the Python prototype.
void TakeWorker::mixStems() {
    juce::File manifest,path;choir::FXParams stage;
    {const juce::ScopedLock lock(mutex);manifest=stemManifest;path=output;stage=stemParams;}
    auto doc=juce::JSON::parse(manifest.loadFileAsString());
    auto* obj=doc.getDynamicObject();
    if(!obj){failed("Invalid singer JSON manifest.");return;}
    choir::stems::Manifest spec;
    auto number=[&](const juce::DynamicObject* o,const char* key,double defaultValue) {
        return o->hasProperty(key)?(double)o->getProperty(key):defaultValue;
    };
    spec.room=number(obj,"room",.20);spec.outputDb=number(obj,"output_db",-3.);
    auto mic=obj->getProperty("microphone");
    if(auto* arr=mic.getArray()) {
        if(arr->size()!=2){failed("microphone must be [x, y]");return;}
        spec.micX=(double)arr->getReference(0);spec.micY=(double)arr->getReference(1);
    }
    const auto v=obj->getProperty("voices");
    auto* voices=v.getArray();
    if(!voices || voices->isEmpty() || voices->size()>128) {
        failed("Manifest requires 1 to 128 voices.");return;
    }
    // Plugin stage dragging is applied non-destructively on top of manifest
    // coordinates. A manifest may still place individuals around their part.
    spec.micX+=stage.micX;spec.micY+=stage.micY;
    for(const auto& entry:*voices) {
        auto* s=entry.getDynamicObject();
        if(!s){failed("Each voice must be a JSON object.");return;}
        auto part=s->getProperty("section").toString().trim().toLowerCase();
        double x=0,y=0;int sectionIndex=-1;
        if(part=="soprano") {x=-5.1;y=6.;sectionIndex=0;}
        else if(part=="alto") {x=-1.7;y=5.2;sectionIndex=1;}
        else if(part=="tenor") {x=1.7;y=5.2;sectionIndex=2;}
        else if(part=="bass") {x=5.1;y=6.;sectionIndex=3;}
        else if(part=="lead") {x=0.;y=.4;}
        else {failed("Unknown section: "+part);return;}
        const auto filename=s->getProperty("file").toString();
        if(filename.isEmpty()){failed("Missing file in singer manifest.");return;}
        const auto source=manifest.getParentDirectory().getChildFile(filename);
        if(!source.existsAsFile()){failed("Missing stem: "+source.getFileName());return;}
        choir::stems::Voice voice;
        voice.file=source.getFullPathName().toStdString();
        voice.section=part.toStdString();
        voice.x=number(s,"x",x);voice.y=number(s,"y",y);
        voice.offsetMs=number(s,"offset_ms",0.);voice.gainDb=number(s,"gain_db",-6.);
        if(sectionIndex>=0) {
            const auto partIndex=(size_t)sectionIndex;
            voice.x+=stage.x[partIndex]-x;voice.y+=stage.y[partIndex]-y;
            // Per-section gain is honored for external singer renders, too.
            voice.gainDb+=20.*std::log10(std::max(1.e-6f,stage.gain[partIndex]));
        } else {
            // White lead on the stage is independently movable.
            voice.x+=stage.micX-2.35+stage.leadX;
            voice.y+=stage.micY+1.9+stage.leadY-.4;
            voice.gainDb+=20.*std::log10(std::max(1.e-6f,stage.leadGain));
        }
        spec.voices.push_back(std::move(voice));
    }
    choir::stems::Result result;
    std::string err;
    const bool ok=choir::stems::render(spec,path.getFullPathName().toStdString(),result,err,
        [this](float t){fraction.store(t);return !threadShouldExit();});
    if(threadShouldExit())return;
    if(!ok){failed(juce::String(err));return;}
    state=8;
}
