// SkellyChoir compact UI review: four representative renders, with full build
// validation kept in the independent Windows VST3 CI job.
// Screenshots: main, room controls, alternate theme, stage scene.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstdio>

namespace {
void savePng(juce::Component& component, const juce::File& folder, const juce::String& name)
{
    const auto image = component.createComponentSnapshot(component.getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    juce::FileOutputStream out(folder.getChildFile(name + ".png"));
    if (!out.openedOk()) { std::printf("Unable to write %s.png\n", name.toRawUTF8()); return; }
    out.setPosition(0);
    out.truncate();
    png.writeImageToStream(image, out);
    std::printf("saved %s.png (%d x %d)\n", name.toRawUTF8(), image.getWidth(), image.getHeight());
}

void feed(OmniChoirAudioProcessor& processor, double seconds)
{
    const double sr = 44100.0;
    constexpr int block = 512;
    double phase = 0.;
    juce::AudioBuffer<float> audio(2, block);
    juce::MidiBuffer midi;
    for (int done=0; done < (int)(seconds * sr); done+=block) {
        for (int i=0; i<block; ++i) {
            const double frequency = 220.0 + 3. * std::sin(2. * 3.141592653589793 * (done+i) / sr * 5.);
            phase += frequency / sr;
            const float sample = (float)(0.13 * std::sin(2. * 3.141592653589793 * phase));
            audio.getWritePointer(0)[i] = sample;
            audio.getWritePointer(1)[i] = sample;
        }
        processor.processBlock(audio, midi);
    }
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "screenshots");
    folder.createDirectory();

    OmniChoirAudioProcessor processor;
    processor.prepareToPlay(44100., 512);
    std::unique_ptr<juce::AudioProcessorEditor> created(processor.createEditor());
    auto* editor = dynamic_cast<OmniChoirAudioProcessorEditor*>(created.get());
    if (!editor) return 3;
    editor->setSize(860, 600);
    feed(processor, 0.3);
    editor->dismissTour();

    editor->showForScreenshot(true, 0);
    savePng(*editor,folder,"01-main");
    editor->showForScreenshot(true, 1);
    savePng(*editor,folder,"02-room-controls");
    editor->showForScreenshot(true, 0);
    editor->showThemeForScreenshot(2); // Skelly neon green variant
    savePng(*editor,folder,"03-alternate-theme");
    editor->showThemeForScreenshot(0);
    processor.applyStagePreset(8); // Graveyard
    // Start the scene fade in a real paint before waiting for its completion.
    (void) editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    juce::Thread::sleep(450); // allow the new scene's 400 ms visual crossfade
    savePng(*editor,folder,"04-graveyard-scene");
    return 0;
}
