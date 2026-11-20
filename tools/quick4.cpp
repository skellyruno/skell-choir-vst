// Local Quick4 renderer: captures the actual SkellyChoir JUCE editor, including
// its native Rooms / Position popup. No mock controls or generated UI are used.
//
// Run under Xvfb on Linux:
//   SkellyChoirQuick4 --output /path/to/quick4
//
// Requires scrot for the one desktop capture that includes JUCE's native popup
// window. The other three scenes are exact JUCE Component snapshots.
#include <JuceHeader.h>
#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace
{
constexpr int kWidth = 765;
constexpr int kHeight = 534;
constexpr int kBlock = 512;

void feedSinging(OmniChoirAudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    double phase = 0.0;
    constexpr double sampleRate = 44100.0;
    for (int blockIndex = 0; blockIndex < 110; ++blockIndex)
    {
        for (int sample = 0; sample < kBlock; ++sample)
        {
            const double t = (blockIndex * kBlock + sample) / sampleRate;
            const double fundamental = 196.0 * std::pow(2.0, 0.15 * std::sin(2.0 * juce::MathConstants<double>::pi * 4.7 * t) / 12.0);
            phase += fundamental / sampleRate;
            double voice = 0.0;
            for (int harmonic = 1; harmonic <= 7; ++harmonic)
                voice += std::sin(2.0 * juce::MathConstants<double>::pi * harmonic * phase)
                         / (harmonic * harmonic);
            const float value = static_cast<float>(0.12 * voice);
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }
        processor.processBlock(buffer, midi);
    }
}

bool savePng(const juce::Image& image, const juce::File& file)
{
    if (!image.isValid() || image.getWidth() < 64 || image.getHeight() < 64)
        return false;

    file.getParentDirectory().createDirectory();
    juce::FileOutputStream stream(file);
    if (!stream.openedOk() || stream.truncate().failed())
        return false;
    const bool encoded = juce::PNGImageFormat().writeImageToStream(image, stream);
    stream.flush();
    return encoded && !stream.getStatus().failed()
        && file.existsAsFile() && file.getSize() > 0
        && file.getSize() == stream.getPosition();
}

bool saveEditor(OmniChoirAudioProcessorEditor& editor, const juce::File& file)
{
    const auto snapshot = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
    return savePng(snapshot, file);
}

bool captureNativePopup(OmniChoirAudioProcessorEditor& editor, const juce::File& output)
{
#if JUCE_LINUX
    const auto rawFile = output.getSiblingFile("quick4-desktop-raw.png");
    rawFile.deleteFile(); // never accept a stale frame if the capture command fails
    juce::ChildProcess process;
    juce::StringArray arguments;
    arguments.add("scrot");
    arguments.add("-o");
    arguments.add(rawFile.getFullPathName());
    if (!process.start(arguments) || !process.waitForProcessToFinish(8000)
        || process.getExitCode() != 0 || !rawFile.existsAsFile() || rawFile.getSize() <= 1024)
    {
        std::fprintf(stderr, "Quick4 could not capture the open native popup. Install scrot and run under Xvfb.\n");
        return false;
    }

    const auto desktop = juce::ImageFileFormat::loadFrom(rawFile);
    rawFile.deleteFile();
    if (!desktop.isValid())
        return false;

    // Include the editor and the adjacent popup area without keeping the full
    // Xvfb desktop's empty margins in the Quick4 scene.
    const auto bounds = editor.getScreenBounds();
    juce::Rectangle<int> crop(bounds.getX() - 14, bounds.getY() - 30,
                              bounds.getWidth() + 250, bounds.getHeight() + 46);
    crop = crop.getIntersection(desktop.getBounds());
    if (crop.getWidth() < bounds.getWidth() || crop.getHeight() < bounds.getHeight())
        return false;
    return savePng(desktop.getClippedImage(crop), output);
#else
    juce::ignoreUnused(editor, output);
    std::fprintf(stderr, "Quick4 native-popup capture currently requires Linux/X11.\n");
    return false;
#endif
}

juce::String sha256(const juce::File& file)
{
    return juce::SHA256(file).toHexString();
}

struct Scene
{
    juce::String file;
    juce::String label;
    juce::File path;
    int width = 0;
    int height = 0;
};

bool writeManifest(const juce::File& output, const std::array<Scene, 4>& scenes)
{
    juce::Array<juce::var> items;
    for (const auto& scene : scenes)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty("file", scene.file);
        item->setProperty("label", scene.label);
        item->setProperty("width", scene.width);
        item->setProperty("height", scene.height);
        item->setProperty("bytes", scene.path.getSize());
        item->setProperty("sha256", sha256(scene.path));
        items.add(juce::var(item));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("passed", true);
    root->setProperty("plugin", "skell-choir-vst");
    root->setProperty("renderer", "SkellyChoirQuick4");
    root->setProperty("count", static_cast<int>(scenes.size()));
    root->setProperty("scenes", juce::var(items));
    root->setProperty("native_popup", "JUCE PopupMenu captured from the live desktop");
    const auto json = juce::JSON::toString(juce::var(root), true);
    return output.getChildFile("manifest.json").replaceWithText(json + "\n");
}

bool writeContactSheet(const juce::File& output, const std::array<Scene, 4>& scenes)
{
    constexpr int tileWidth = 640;
    constexpr int tileHeight = 448;
    constexpr int labelHeight = 36;
    juce::Image sheet(juce::Image::ARGB, tileWidth * 2, (tileHeight + labelHeight) * 2, true);
    juce::Graphics graphics(sheet);
    graphics.fillAll(juce::Colour(0xff080a09));

    for (size_t index = 0; index < scenes.size(); ++index)
    {
        const auto image = juce::ImageFileFormat::loadFrom(scenes[index].path);
        if (!image.isValid())
            return false;
        const int x = static_cast<int>(index % 2) * tileWidth;
        const int y = static_cast<int>(index / 2) * (tileHeight + labelHeight);
        graphics.drawImageWithin(image, x + 8, y + 8, tileWidth - 16, tileHeight - 16,
                                 juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        graphics.drawText(scenes[index].label, x + 10, y + tileHeight + 2,
                          tileWidth - 20, labelHeight - 4, juce::Justification::centredLeft);
    }
    return savePng(sheet, output.getChildFile("contact-sheet.png"));
}

class ChoirWindow final : public juce::DocumentWindow
{
public:
    explicit ChoirWindow(OmniChoirAudioProcessorEditor* editor)
        : juce::DocumentWindow("SkellyChoir - Quick4", juce::Colours::black,
                               juce::DocumentWindow::closeButton, true)
    {
        setUsingNativeTitleBar(false);
        setResizable(false, false);
        editor->setSize(kWidth, kHeight);
        setContentOwned(editor, true);
        setContentComponentSize(kWidth, kHeight);
        setTopLeftPosition(72, 72);
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override { setVisible(false); }
};

class Quick4Runner final : private juce::Timer
{
public:
    explicit Quick4Runner(juce::File output)
        : outputFolder(std::move(output)),
          scenes {{
              { "00-main.png", "Main editor - Pro / Gold", outputFolder.getChildFile("00-main.png") },
              { "01-room-position-popup.png", "Live Rooms / Position popup", outputFolder.getChildFile("01-room-position-popup.png") },
              { "02-active-solo-mute.png", "Active solo / mute state", outputFolder.getChildFile("02-active-solo-mute.png") },
              { "03-neon-green.png", "Neon Green theme", outputFolder.getChildFile("03-neon-green.png") }
          }}
    {
    }

    int run()
    {
        if (!outputFolder.createDirectory() && !outputFolder.isDirectory())
            return 2;

        processor.setProUi(true);
        processor.prepareToPlay(44100.0, kBlock);
        feedSinging(processor);

        auto* newEditor = dynamic_cast<OmniChoirAudioProcessorEditor*>(processor.createEditor());
        if (newEditor == nullptr)
        {
            std::fprintf(stderr, "SkellyChoir did not create its actual JUCE editor.\n");
            return 3;
        }

        newEditor->dismissCurtain();
        newEditor->dismissTour();
        newEditor->showForScreenshot(true, 0);
        newEditor->showThemeForScreenshot(0);
        editor = newEditor;
        window = std::make_unique<ChoirWindow>(editor);

        // A single non-modal message loop keeps working when JUCE_MODAL_LOOPS_PERMITTED=0.
        // Timer callbacks sequence the screenshots without blocking the UI thread.
        step = 0;
        startTimer(250);
        juce::MessageManager::getInstance()->runDispatchLoop();
        return result;
    }

private:
    juce::File outputFolder;
    OmniChoirAudioProcessor processor;
    std::unique_ptr<ChoirWindow> window;
    OmniChoirAudioProcessorEditor* editor = nullptr; // owned by window
    std::array<Scene, 4> scenes;
    int step = 0;
    int result = 1;

    void finish(int code)
    {
        result = code;
        stopTimer();
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }

    void timerCallback() override
    {
        switch (step)
        {
            case 0:
                if (!saveEditor(*editor, scenes[0].path))
                    return finish(4);
                editor->showRoomMenuForScreenshot();
                step = 1;
                startTimer(400);
                break;

            case 1:
                if (!captureNativePopup(*editor, scenes[1].path))
                    return finish(5);
                juce::PopupMenu::dismissAllActiveMenus();

                if (auto* solo = processor.parameters.getParameter("sectionState0"))
                    solo->setValueNotifyingHost(solo->convertTo0to1(1.0f));
                if (auto* mute = processor.parameters.getParameter("sectionState3"))
                    mute->setValueNotifyingHost(mute->convertTo0to1(2.0f));
                editor->showForScreenshot(true, 0);
                step = 2;
                startTimer(120);
                break;

            case 2:
                if (!saveEditor(*editor, scenes[2].path))
                    return finish(6);
                editor->showThemeForScreenshot(1);
                step = 3;
                startTimer(120);
                break;

            case 3:
                if (!saveEditor(*editor, scenes[3].path))
                    return finish(7);

                for (auto& scene : scenes)
                {
                    const auto image = juce::ImageFileFormat::loadFrom(scene.path);
                    if (!image.isValid() || scene.path.getSize() <= 1024)
                        return finish(8);
                    scene.width = image.getWidth();
                    scene.height = image.getHeight();
                }
                if (!writeManifest(outputFolder, scenes) || !writeContactSheet(outputFolder, scenes))
                    return finish(9);

                std::printf("PASS: four current-source JUCE scenes written to %s\n",
                            outputFolder.getFullPathName().toRawUTF8());
                finish(0);
                break;

            default:
                finish(10);
                break;
        }
    }
};

int run(const juce::File& output)
{
    Quick4Runner runner(output);
    return runner.run();
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile("quick4-local");
    for (int index = 1; index + 1 < argc; ++index)
        if (juce::String(argv[index]) == "--output")
            output = juce::File(argv[index + 1]);

    return run(output);
}
