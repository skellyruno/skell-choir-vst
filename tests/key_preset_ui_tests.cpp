#include "test_support.h"
#include "../Source/StylePresets.h"
#include "../Source/Scales.h"
#include <set>
using namespace choir;
int main(int argc, char** argv) {
    const std::string root=argc>1?argv[1]:".";
    const auto ed=ts::readFile(root+"/Source/PluginEditor.cpp");
    const auto proc=ts::readFile(root+"/Source/PluginProcessor.cpp");
    const auto proch=ts::readFile(root+"/Source/PluginProcessor.h");
    CHECK(!ed.empty()&&!proc.empty()&&!proch.empty(),"UI/processor sources missing");
    const auto look=ts::readFile(root+"/Source/UI/OmniLookAndFeel.h");
    CHECK(look.find("juce::Slider::textBoxOutlineColourId, kStroke.withAlpha(")!=std::string::npos,
          "slider value readout borders should remain visible after preset changes");
    CHECK(kNumStylePresets==11,"expected the six original and five new factory presets");
    std::set<std::string> names;
    for(const auto& p:kStylePresets) {
        CHECK(names.insert(p.name).second,"duplicate preset name %s",p.name);
        CHECK(p.stage>=0&&p.stage<kNumStagePresets,"invalid room in %s",p.name);
    }
    CHECK(ed.find("keyBox.setEnabled(scale>0)")==std::string::npos,
          "key picker must remain enabled with default SCALE/off");
    CHECK(ed.find("keyBox.addItem(choir::kKeyNames[i],i+1)")!=std::string::npos,
          "chosen key should show only the note without KEY prefix");
    CHECK(ed.find("scaleBox.addItem(\"SCALE\",1)")!=std::string::npos,
          "chromatic/off should display SCALE, not MAJOR");
    CHECK(ed.find("parameter->beginChangeGesture()")!=std::string::npos &&
          ed.find("parameter->setValueNotifyingHost(parameter->convertTo0to1((float)(id-1)))")!=std::string::npos,
          "key changes must send host-visible keyRoot automation gestures");
    CHECK(ed.find("syncKeySelection()")!=std::string::npos &&
          ed.find("observedKey!=lastSyncedKey")!=std::string::npos,
          "host key automation should refresh the picker");
    CHECK(ed.find("refreshPresetSelection()")!=std::string::npos &&
          ed.find("presetBox.setText(chosen,juce::dontSendNotification)")!=std::string::npos,
          "picked preset name should stay in the preset box");
    CHECK(proc.find("setActiveStyleName(choir::kStylePresets[index].name)")!=std::string::npos &&
          proc.find("setActiveStyleName(data.name.empty()")!=std::string::npos,
          "factory and user presets must save their names");
    CHECK(proch.find("parameters.state.setProperty(\"skellyActiveStyle\"")!=std::string::npos,
          "preset name must persist inside existing plugin state");
    CHECK(kNumScales==8&&kFixedIntervals[0]==7,"existing scale DSP/automation mapping must not change");
    std::printf("UI key, scale, preset labels: 11 presets and stable DSP mappings PASS\n");
    return 0;
}
