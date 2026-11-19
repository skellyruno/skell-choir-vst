// Headless screenshots of the editor: every tab, Easy and Pro, the first-run tour, a muted section, a captured take in the Listen piano roll,
// and the smallest and a large window. No DAW and no audio device: it builds the processor, feeds it a little synthetic singing so the stage
// glows, and asks JUCE to paint the editor into PNG files.
//   OmniChoirScreenshots [output folder]        (on Linux run it under xvfb-run)
// Built only with -DOMNICHOIR_BUILD_SCREENSHOTS=ON; the CI "screenshots" job builds it and uploads the images.
#include "../Source/UI/CurtainModel.h"
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstdio>

namespace {
bool savePng(juce::Component& component, const juce::File& folder, const juce::String& name)
{
    const auto image = component.createComponentSnapshot(component.getLocalBounds(), true, 1.0f);
    if (!image.isValid() || image.getWidth() <= 0 || image.getHeight() <= 0)
    {
        std::fprintf(stderr, "ERROR: empty editor snapshot for %s.png\n", name.toRawUTF8());
        return false;
    }

    juce::PNGImageFormat png;
    const auto file = folder.getChildFile(name + ".png");
    juce::FileOutputStream out(file);
    if (!out.openedOk())
    {
        std::fprintf(stderr, "ERROR: could not open %s.png for writing\n", name.toRawUTF8());
        return false;
    }

    out.setPosition(0);
    out.truncate();
    const bool encoded = png.writeImageToStream(image, out);
    out.flush();
    if (!encoded || out.getStatus().failed())
    {
        std::fprintf(stderr, "ERROR: PNG encoding or flush failed for %s.png: %s\n",
                     name.toRawUTF8(), out.getStatus().getErrorMessage().toRawUTF8());
        return false;
    }
    if (!file.existsAsFile() || file.getSize() <= 0)
    {
        std::fprintf(stderr, "ERROR: %s.png was not persisted\n", name.toRawUTF8());
        return false;
    }

    std::printf("saved %s.png (%d x %d)\n", name.toRawUTF8(), image.getWidth(), image.getHeight());
    return true;
}

// a sung melody with vibrato: C E G E A F D C, 0.55 s each
void feed(OmniChoirAudioProcessor& processor, const std::vector<int>& notes, double seconds)
{
    const double sr = 44100.0; const int block = 512; double phase = 0; size_t sample = 0;
    const double noteLength = 0.64 * sr;
    juce::AudioBuffer<float> buffer(2, block); juce::MidiBuffer midi;
    for (double done = 0; done < seconds * sr; done += block)
    {
        for (int i = 0; i < block; ++i, ++sample)
        {
            const size_t index = (size_t) ((double) sample / noteLength) % notes.size();
            const double inNote = std::fmod((double) sample, noteLength) / noteLength;
            const double gate = inNote < .86 ? 1.0 : 0.0;
            const double frequency = 440.0 * std::pow(2.0, (notes[index] - 69) / 12.0) * std::pow(2.0, .3 / 12.0 * std::sin(2 * 3.14159265 * 5.5 * (double) sample / sr));
            phase += frequency / sr;
            double v = 0; for (int h = 1; h <= 8; ++h) v += std::sin(2 * 3.14159265 * h * phase) / (h * h);
            const float s = (float) (.16 * gate * v);
            buffer.getWritePointer(0)[i] = s; buffer.getWritePointer(1)[i] = s;
        }
        processor.processBlock(buffer, midi);
    }
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "screenshots");
    folder.createDirectory();

    OmniChoirAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> base(processor.createEditor());
    auto* editor = dynamic_cast<OmniChoirAudioProcessorEditor*>(base.get());
    if (editor == nullptr) { std::printf("the processor did not create its editor\n"); return 1; }
    editor->setSize(765, 534);
    const auto save = [&](const juce::String& name) { return savePng(*editor, folder, name); };

    const std::vector<int> melody { 60, 64, 67, 64, 69, 65, 62, 60 };
    feed(processor, melody, 2.0);                       // a little singing, so the stage glows and the meters move

    editor->showCurtainForScreenshot(1200.f);               if (!save("00a-curtain-closed")) return 2;      // every open starts on the curtain
    editor->showCurtainForScreenshot(curtain::kHoldMs + curtain::kOpenMs * .5f);  if (!save("00b-curtain-parting")) return 2;
    editor->showTourForScreenshot();  if (!save("01-first-run-tour")) return 2;      // a fresh install shows the tour once the curtain has parted
    editor->dismissTour();
    editor->showForScreenshot(false, 0);  if (!save("02-easy-mode")) return 2;
    editor->showForScreenshot(true, 0);   if (!save("03-pro-main")) return 2;
    if (auto* mute = processor.parameters.getParameter("sectionState3")) mute->setValueNotifyingHost(mute->convertTo0to1(2.f));
    if (auto* solo = processor.parameters.getParameter("sectionState0")) solo->setValueNotifyingHost(solo->convertTo0to1(1.f));
    editor->showForScreenshot(true, 0);   if (!save("04-pro-main-solo-soprano")) return 2;
    if (auto* mute = processor.parameters.getParameter("sectionState3")) mute->setValueNotifyingHost(0.f);
    if (auto* solo = processor.parameters.getParameter("sectionState0")) solo->setValueNotifyingHost(0.f);
    editor->showForScreenshot(true, 1);   if (!save("05-room")) return 2;
    editor->showForScreenshot(true, 2);   if (!save("06-voice")) return 2;
    editor->showForScreenshot(true, 3);   if (!save("07-follow")) return 2;
    editor->showForScreenshot(true, 4);   if (!save("08-listen-empty")) return 2;

    // a take: key of C major, listen to the melody, analyse, then show the piano roll
    if (auto* scale = processor.parameters.getParameter("scale")) scale->setValueNotifyingHost(scale->convertTo0to1(1.f));
    processor.startListening();
    feed(processor, melody, 5.6);
    processor.stopListening();
    for (int i = 0; i < 500 && processor.listenSession().state() != choir::ListenSession::Ready; ++i) juce::Thread::sleep(20);
    processor.setMode(1);
    editor->showForScreenshot(true, 4);   if (!save("09-listen-take")) return 2;
    editor->showForScreenshot(true, 5);   if (!save("10-take-file")) return 2;
    editor->showForScreenshot(true, 6);   if (!save("11-stems")) return 2;
    editor->showForScreenshot(true, 0);
    editor->setSize(730, 500);            if (!save("12-smallest-window")) return 2;
    editor->setSize(1300, 900);           if (!save("13-large-window")) return 2;

    // every theme on the main page, then every stage scene (the picture follows the stage preset; wait out the 400 ms crossfade)
    editor->setSize(765, 534);
    editor->showForScreenshot(true, 0);
    for (int i = 0; i < theme::kNumThemes; ++i)
    {
        editor->showThemeForScreenshot(i);
        juce::String name = theme::get(i).name; name = name.replaceCharacter(' ', '-').toLowerCase();
        if (!save("14-theme-" + name)) return 2;
    }
    editor->showThemeForScreenshot(0);
    for (int i = 0; i < scenes::kNumScenes; ++i)
    {
        processor.applyRoom(i);
        editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);   // the first paint starts the fade...
        juce::Thread::sleep(scenes::kFadeMs + 80);                               // ...so let it finish before the picture is taken
        juce::String name = scenes::kScenes[i].presetName; name = name.replaceCharacter(' ', '-').toLowerCase();
        if (!save("15-scene-" + juce::String(i) + "-" + name)) return 2;
    }
    // Exercise the actual JUCE editor, not only string checks: preset display
    // and key changes must work with SCALE still in its off state.
    editor->setSize(765, 534);
    editor->showForScreenshot(true, 0);
    editor->showPresetForScreenshot(6); // Midnight Cathedral (long name)
    if (processor.activeStyleName() != juce::String(choir::kStylePresets[6].name)) {
        std::fprintf(stderr,"preset selection failed to retain its name\n"); return 4;
    }
    if (!save("16-selected-preset")) return 2;
    editor->showKeyScaleForScreenshot(0,0);
    editor->showKeyScaleForScreenshot(6,0);
    if ((int)processor.parameters.getRawParameterValue("keyRoot")->load()!=6
        || (int)processor.parameters.getRawParameterValue("scale")->load()!=0) {
        std::fprintf(stderr,"independent key selection failed\n"); return 5;
    }
    editor->showKeyScaleForScreenshot(6,1);
    if ((int)processor.parameters.getRawParameterValue("scale")->load()!=1) {
        std::fprintf(stderr,"scale selection failed\n"); return 6;
    }
    if (!save("17-key-scale-selected")) return 2;
    std::printf("done: %s\n", folder.getFullPathName().toRawUTF8());
    return 0;
}
