// Headless screenshots of the editor: every tab, Easy and Pro, the first-run tour, a muted section, a captured take in the Listen piano roll,
// and the smallest and a large window. No DAW and no audio device: it builds the processor, feeds it a little synthetic singing so the stage
// glows, and asks JUCE to paint the editor into PNG files.
//   OmniChoirScreenshots [output folder]        (on Linux run it under xvfb-run)
// Built only with -DOMNICHOIR_BUILD_SCREENSHOTS=ON; the CI "screenshots" job builds it and uploads the images.
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
    if (out.openedOk()) { out.setPosition(0); out.truncate(); png.writeImageToStream(image, out); std::printf("saved %s.png (%d x %d)\n", name.toRawUTF8(), image.getWidth(), image.getHeight()); }
    else std::printf("could not write %s.png\n", name.toRawUTF8());
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
    editor->setSize(860, 600);

    const std::vector<int> melody { 60, 64, 67, 64, 69, 65, 62, 60 };
    feed(processor, melody, 2.0);                       // a little singing, so the stage glows and the meters move

    savePng(*editor, folder, "01-first-run-tour");      // a fresh install shows the tour first
    editor->dismissTour();
    editor->showForScreenshot(false, 0);  savePng(*editor, folder, "02-easy-mode");
    editor->showForScreenshot(true, 0);   savePng(*editor, folder, "03-pro-main");
    if (auto* mute = processor.parameters.getParameter("sectionState3")) mute->setValueNotifyingHost(mute->convertTo0to1(2.f));
    if (auto* solo = processor.parameters.getParameter("sectionState0")) solo->setValueNotifyingHost(solo->convertTo0to1(1.f));
    editor->showForScreenshot(true, 0);   savePng(*editor, folder, "04-pro-main-solo-soprano");
    if (auto* mute = processor.parameters.getParameter("sectionState3")) mute->setValueNotifyingHost(0.f);
    if (auto* solo = processor.parameters.getParameter("sectionState0")) solo->setValueNotifyingHost(0.f);
    editor->showForScreenshot(true, 1);   savePng(*editor, folder, "05-room");
    editor->showForScreenshot(true, 2);   savePng(*editor, folder, "06-voice");
    editor->showForScreenshot(true, 3);   savePng(*editor, folder, "07-follow");
    editor->showForScreenshot(true, 4);   savePng(*editor, folder, "08-listen-empty");

    // a take: key of C major, listen to the melody, analyse, then show the piano roll
    if (auto* scale = processor.parameters.getParameter("scale")) scale->setValueNotifyingHost(scale->convertTo0to1(1.f));
    processor.startListening();
    feed(processor, melody, 5.6);
    processor.stopListening();
    for (int i = 0; i < 500 && processor.listenSession().state() != choir::ListenSession::Ready; ++i) juce::Thread::sleep(20);
    processor.setMode(1);
    editor->showForScreenshot(true, 4);   savePng(*editor, folder, "09-listen-take");
    editor->showForScreenshot(true, 5);   savePng(*editor, folder, "10-take-file");
    editor->showForScreenshot(true, 6);   savePng(*editor, folder, "11-stems");
    editor->showForScreenshot(true, 0);
    editor->setSize(800, 540);            savePng(*editor, folder, "12-smallest-window");
    editor->setSize(1300, 900);           savePng(*editor, folder, "13-large-window");
    std::printf("done: %s\n", folder.getFullPathName().toRawUTF8());
    return 0;
}
