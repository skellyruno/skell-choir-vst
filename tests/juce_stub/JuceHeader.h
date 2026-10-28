// A type-checking stand-in for the parts of JUCE 8 that OmniChoir FX's UI uses. Signatures follow the real API. NOT a working JUCE.
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <type_traits>
#define JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(C) C(const C&) = delete; C& operator=(const C&) = delete;
#define JUCE_CALLTYPE
namespace juce {
using int64 = long long; using uint32 = std::uint32_t; using juce_wchar = char32_t; using uint8 = unsigned char;
template <class T> constexpr T jmin(T a, T b) { return a < b ? a : b; }
template <class T> constexpr T jmax(T a, T b) { return a > b ? a : b; }
template <class T> constexpr T jlimit(T lo, T hi, T v) { return v < lo ? lo : (hi < v ? hi : v); }
template <class... A> void ignoreUnused(A&&...) {}
template <class T> int roundToInt(T v) { return (int) std::lround(v); }
template <class T> struct MathConstants { static constexpr T pi = (T) 3.14159265358979323846; };
enum NotificationType { dontSendNotification, sendNotification, sendNotificationSync, sendNotificationAsync };

class String {
public:
    String() = default; String(const char*) {} String(const String&) = default; String(String&&) = default;
    String& operator=(const String&) = default;
    template <class T, class = std::enable_if_t<std::is_arithmetic_v<T>>> explicit String(T) {}
    String(float, int, bool = false) {} String(double, int, bool = false) {}
    static String charToString(juce_wchar) { return {}; }
    String toUpperCase() const { return *this; } String substring(int, int) const { return *this; } String substring(int) const { return *this; }
    bool isNotEmpty() const { return true; } bool isEmpty() const { return false; }
    double getDoubleValue() const { return 0; } int getIntValue() const { return 0; }
    String& operator+=(const String&) { return *this; }
    bool operator==(const String&) const { return true; } bool operator!=(const String&) const { return false; }
    bool operator==(const char*) const { return true; } bool operator!=(const char*) const { return false; }
    void clear() {}
    friend String operator+(const String&, const String&) { return {}; }
    friend String operator+(const char*, const String&) { return {}; }
    friend String operator+(const String&, const char*) { return {}; }
};
class StringArray { public: StringArray() = default; StringArray(std::initializer_list<const char*>) {} String operator[](int) const { return {}; } int size() const { return 0; } };
class File { public: File() = default; bool existsAsFile() const { return true; } String getFileName() const { return {}; } String getFullPathName() const { return {}; }
    File withFileExtension(const String&) const { return *this; } File(const String&) {} bool operator!=(const File&) const { return false; } bool operator==(const File&) const { return true; } };

template <class T> struct Point {
    T x{}, y{}; Point() = default; Point(T a, T b) : x(a), y(b) {}
    T getDistanceFrom(Point o) const { return (T) std::hypot((double) (x - o.x), (double) (y - o.y)); }
};
template <class T> struct Rectangle {
    T x{}, y{}, w{}, h{}; Rectangle() = default; Rectangle(T a, T b, T c, T d) : x(a), y(b), w(c), h(d) {}
    T getX() const { return x; } T getY() const { return y; } T getWidth() const { return w; } T getHeight() const { return h; }
    T getRight() const { return x + w; } T getBottom() const { return y + h; } T getCentreX() const { return x + w / 2; } T getCentreY() const { return y + h / 2; }
    Point<T> getCentre() const { return { getCentreX(), getCentreY() }; }
    Rectangle reduced(T dx, T dy) const { return *this; } Rectangle reduced(T d) const { return *this; }
    Rectangle expanded(T dx, T dy) const { return *this; } Rectangle expanded(T d) const { return *this; }
    Rectangle translated(T dx, T dy) const { return *this; }
    Rectangle<float> toFloat() const { return { (float) x, (float) y, (float) w, (float) h }; }
    Rectangle<int> toNearestInt() const { return { (int) x, (int) y, (int) w, (int) h }; }
    Rectangle removeFromTop(T) { return *this; } Rectangle removeFromLeft(T) { return *this; } Rectangle removeFromRight(T) { return *this; } Rectangle removeFromBottom(T) { return *this; }
    Rectangle withSizeKeepingCentre(T, T) const { return *this; }
    void setHeight(T v) { h = v; }
};
struct Colour {
    Colour() = default; constexpr Colour(uint32) {}
    Colour withAlpha(float) const { return *this; } Colour brighter(float = .4f) const { return *this; } Colour darker(float = .4f) const { return *this; }
    Colour withMultipliedSaturation(float) const { return *this; } Colour withMultipliedAlpha(float) const { return *this; }
};
namespace Colours { inline const Colour black(0xff000000), white(0xffffffff), transparentBlack(0x00000000); }
class FontOptions { public: FontOptions() = default; FontOptions(float) {} FontOptions withStyle(const String&) const { return *this; } FontOptions withName(const String&) const { return *this; } };
class Font { public: Font() = default; Font(const FontOptions&) {} static const String& getDefaultSerifFontName() { static String s; return s; } };
class ColourGradient { public: ColourGradient(Colour, float, float, Colour, float, float, bool) {} };
class PathStrokeType { public: enum JointStyle { mitered, curved, beveled }; enum EndCapStyle { butt, square, rounded };
    PathStrokeType(float, JointStyle = mitered, EndCapStyle = butt) {} };
class AffineTransform {};
class Path { public:
    void startNewSubPath(float, float) {} void lineTo(float, float) {} void closeSubPath() {}
    void cubicTo(float, float, float, float, float, float) {} void quadraticTo(float, float, float, float) {}
    void addRoundedRectangle(Rectangle<float>, float) {} void addTriangle(float, float, float, float, float, float) {}
    void addCentredArc(float, float, float, float, float, float, float, bool) {} };
class Image { public: bool isValid() const { return true; } };
struct ImageCache { static Image getFromMemory(const void*, int) { return {}; } };
class RectanglePlacement { public: enum { xLeft = 1, xRight = 2, xMid = 4, yTop = 8, yBottom = 16, yMid = 32, stretchToFit = 64, fillDestination = 128, onlyReduceInSize = 256, onlyIncreaseInSize = 512, doNotResize = 768 };
    RectanglePlacement(int) {} };
class Justification { public: enum Flags { left = 1, right = 2, horizontallyCentred = 4, centred = 36, centredLeft = 33, centredRight = 34 }; Justification(int) {} };
class Graphics { public:
    void setColour(Colour) {} void setGradientFill(const ColourGradient&) {} void setFont(const Font&) {} void setOpacity(float) {}
    void fillAll() {} void fillAll(Colour) {}
    void fillRect(Rectangle<float>) {} void fillRect(Rectangle<int>) {} void fillRect(int, int, int, int) {} void fillRect(float, float, float, float) {}
    void fillRoundedRectangle(Rectangle<float>, float) {} void fillRoundedRectangle(float, float, float, float, float) {}
    void drawRoundedRectangle(Rectangle<float>, float, float) {} void drawRoundedRectangle(float, float, float, float, float, float) {}
    void fillEllipse(float, float, float, float) {} void fillEllipse(Rectangle<float>) {}
    void drawEllipse(float, float, float, float, float) {}
    void drawLine(float, float, float, float, float = 1.f) {}
    void fillPath(const Path&) {} void strokePath(const Path&, const PathStrokeType&) {}
    void drawText(const String&, int, int, int, int, Justification, bool = true) {} void drawText(const String&, Rectangle<float>, Justification, bool = true) {}
    void drawText(const String&, Rectangle<int>, Justification, bool = true) {}
    void drawFittedText(const String&, Rectangle<int>, Justification, int, float = 0.f) {}
    void drawImageWithin(const Image&, int, int, int, int, RectanglePlacement, bool = false) {}
    void saveState() {} void restoreState() {} bool reduceClipRegion(const Path&, const AffineTransform& = {}) { return true; } bool reduceClipRegion(Rectangle<int>) { return true; }
    struct ScopedSaveState { explicit ScopedSaveState(Graphics&) {} };
};
struct Time { static double getMillisecondCounterHiRes() { return 0; } };
struct MouseCursor { enum StandardCursorType { NormalCursor, DraggingHandCursor, PointingHandCursor }; };
struct MouseEvent { Point<float> position; };
class LookAndFeel { public: virtual ~LookAndFeel() = default; void setColour(int, Colour) {} };
class Timer { public: virtual ~Timer() = default; virtual void timerCallback() = 0; void startTimerHz(int) {} void stopTimer() {} };
class TooltipClient { public: virtual ~TooltipClient() = default; };
class SettableTooltipClient : public TooltipClient { public: void setTooltip(const String&) {} };
class Component {
public:
    virtual ~Component() = default;
    virtual void paint(Graphics&) {} virtual void resized() {}
    virtual void mouseDown(const MouseEvent&) {} virtual void mouseDrag(const MouseEvent&) {} virtual void mouseUp(const MouseEvent&) {}
    virtual void mouseMove(const MouseEvent&) {} virtual void mouseExit(const MouseEvent&) {} virtual void mouseDoubleClick(const MouseEvent&) {}
    void setBounds(int, int, int, int) {} void setBounds(Rectangle<int>) {}
    int getWidth() const { return 0; } int getHeight() const { return 0; } int getX() const { return 0; } int getY() const { return 0; }
    Rectangle<int> getLocalBounds() const { return {}; }
    void repaint() {} void setVisible(bool) {} void setEnabled(bool) {} bool isEnabled() const { return true; }
    void addAndMakeVisible(Component&, int = -1) {} void addAndMakeVisible(Component*, int = -1) {}
    void setMouseCursor(MouseCursor::StandardCursorType) {} void setInterceptsMouseClicks(bool, bool) {}
    void setComponentID(const String&) {} String getComponentID() const { return {}; }
    void setLookAndFeel(LookAndFeel*) {}
    Colour findColour(int) const { return {}; } void setColour(int, Colour) {}
    bool isMouseOver(bool = false) const { return false; } bool isMouseOverOrDragging(bool = false) const { return false; }
    void setSize(int, int) {}
    template <class T> class SafePointer { public: SafePointer(T*) {} bool operator==(std::nullptr_t) const { return false; } bool operator!=(std::nullptr_t) const { return true; } T* operator->() const { return nullptr; } };
};
class TooltipWindow : public Component { public: enum ColourIds { backgroundColourId = 1, textColourId, outlineColourId }; TooltipWindow(Component* = nullptr, int = 700) {} };
class Label : public Component, public SettableTooltipClient { public: enum ColourIds { backgroundColourId = 1, textColourId, outlineColourId };
    void setText(const String&, NotificationType) {} void setJustificationType(Justification) {} void setFont(const Font&) {} };
class Slider : public Component, public SettableTooltipClient {
public:
    enum SliderStyle { LinearHorizontal, LinearVertical, RotaryHorizontalVerticalDrag };
    enum TextEntryBoxPosition { NoTextBox, TextBoxLeft, TextBoxRight, TextBoxAbove, TextBoxBelow };
    enum ColourIds { backgroundColourId = 1, thumbColourId, trackColourId, rotarySliderFillColourId, rotarySliderOutlineColourId, textBoxTextColourId, textBoxBackgroundColourId, textBoxHighlightColourId, textBoxOutlineColourId };
    void setSliderStyle(SliderStyle) {} void setTextBoxStyle(TextEntryBoxPosition, bool, int, int) {}
    double getMinimum() const { return 0; } double getMaximum() const { return 1; } void updateText() {}
    std::function<String(double)> textFromValueFunction; std::function<double(const String&)> valueFromTextFunction;
};
class Button : public Component, public SettableTooltipClient { public:
    std::function<void()> onClick; void setClickingTogglesState(bool) {} void setToggleState(bool, NotificationType) {} bool getToggleState() const { return false; }
    void setButtonText(const String&) {} String getButtonText() const { return {}; } };
class TextButton : public Button { public: enum ColourIds { buttonColourId = 1, buttonOnColourId, textColourOffId, textColourOnId }; TextButton() = default; explicit TextButton(const String&) {} };
class ComboBox : public Component, public SettableTooltipClient { public:
    enum ColourIds { backgroundColourId = 1, textColourId, outlineColourId, buttonColourId, arrowColourId };
    void addItem(const String&, int) {} void addItemList(const StringArray&, int) {} void setSelectedId(int, NotificationType = sendNotificationAsync) {} int getSelectedId() const { return 0; }
    void setTextWhenNothingSelected(const String&) {} std::function<void()> onChange; };
class ScrollBar { public: enum ColourIds { thumbColourId = 1 }; };
class ListBoxModel { public: virtual ~ListBoxModel() = default; virtual int getNumRows() = 0; virtual void paintListBoxItem(int, Graphics&, int, int, bool) = 0; };
class ListBox : public Component { public: enum ColourIds { backgroundColourId = 1, outlineColourId, textColourId }; ListBox(const String& = {}, ListBoxModel* = nullptr) {}
    void setRowHeight(int) {} void updateContent() {} int getSelectedRow(int = 0) const { return 0; } };
class PopupMenu { public: enum ColourIds { backgroundColourId = 1, textColourId, headerTextColourId, highlightedBackgroundColourId, highlightedTextColourId };
    class Options { public: Options withTargetComponent(Component*) const { return *this; } Options withTargetComponent(Component&) const { return *this; } };
    void addItem(int, const String&, bool = true, bool = false) {} void showMenuAsync(const Options&, std::function<void(int)>) {} };
class FileBrowserComponent { public: enum FileChooserFlags { openMode = 1, saveMode = 2, canSelectFiles = 4, canSelectDirectories = 8, warnAboutOverwriting = 64 }; };
class FileChooser { public: FileChooser(const String&, const File& = {}, const String& = {}) {} void launchAsync(int, std::function<void(const FileChooser&)>) {} File getResult() const { return {}; } };

class LookAndFeel_V4 : public LookAndFeel { public:
    virtual void drawRotarySlider(Graphics&, int, int, int, int, float, float, float, Slider&) {}
    virtual int getSliderThumbRadius(Slider&) { return 7; }
    virtual void drawLinearSlider(Graphics&, int, int, int, int, float, float, float, Slider::SliderStyle, Slider&) {}
    virtual Font getTextButtonFont(TextButton&, int) { return {}; }
    virtual void drawButtonBackground(Graphics&, Button&, const Colour&, bool, bool) {}
    virtual void drawButtonText(Graphics&, TextButton&, bool, bool) {}
    virtual Font getComboBoxFont(ComboBox&) { return {}; }
    virtual void positionComboBoxText(ComboBox&, Label&) {}
    virtual void drawComboBox(Graphics&, int, int, bool, int, int, int, int, ComboBox&) {}
};

// ---- audio
class XmlElement { public: bool hasTagName(const String&) const { return true; } void setAttribute(const String&, const String&) {} void setAttribute(const String&, int) {} void setAttribute(const String&, bool) {}
    int getIntAttribute(const String&, int d = 0) const { return d; } bool getBoolAttribute(const String&, bool d = false) const { return d; } String getStringAttribute(const String&) const { return {}; } };
class ValueTree;
template <class T> class AudioBuffer { public: AudioBuffer() = default; AudioBuffer(int, int) {} int getNumSamples() const { return 0; } void clear() {} T* getWritePointer(int) { return nullptr; } const T* getReadPointer(int) const { return nullptr; } int getNumChannels() const { return 0; } };
class MidiBuffer {}; class MemoryBlock {};
class AudioChannelSet { public: static AudioChannelSet stereo() { return {}; } static AudioChannelSet mono() { return {}; } bool isDisabled() const { return false; } bool operator!=(const AudioChannelSet&) const { return false; } bool operator==(const AudioChannelSet&) const { return true; } };
class AudioProcessorEditor;
class AudioProcessorParameter { public: virtual ~AudioProcessorParameter() = default; void beginChangeGesture() {} void endChangeGesture() {} void setValueNotifyingHost(float) {} float getDefaultValue() const { return 0; } StringArray getAllValueStrings() const { return {}; } };
class RangedAudioParameter : public AudioProcessorParameter { public: float convertTo0to1(float v) const { return v; } float convertFrom0to1(float v) const { return v; } };
class AudioProcessor {
public:
    struct BusesLayout { std::vector<AudioChannelSet> outputBuses; std::vector<AudioChannelSet> inputBuses; AudioChannelSet getMainOutputChannelSet() const { return {}; } AudioChannelSet getMainInputChannelSet() const { return {}; } AudioChannelSet getChannelSet(bool, int) const { return {}; } };
    class BusesProperties { public: BusesProperties withInput(const String&, const AudioChannelSet&, bool = true) const { return *this; } BusesProperties withOutput(const String&, const AudioChannelSet&, bool = true) const { return *this; } };
    class Bus { public: bool isEnabled() const { return true; } };
    AudioProcessor() = default; explicit AudioProcessor(const BusesProperties&) {}
    virtual ~AudioProcessor() = default;
    virtual void prepareToPlay(double, int) = 0; virtual void releaseResources() = 0;
    virtual bool isBusesLayoutSupported(const BusesLayout&) const { return true; }
    virtual void processBlock(AudioBuffer<float>&, MidiBuffer&) = 0;
    virtual AudioProcessorEditor* createEditor() = 0; virtual bool hasEditor() const = 0;
    virtual const String getName() const = 0; virtual bool acceptsMidi() const = 0; virtual bool producesMidi() const = 0; virtual bool isMidiEffect() const { return false; }
    virtual double getTailLengthSeconds() const = 0;
    virtual int getNumPrograms() = 0; virtual int getCurrentProgram() = 0; virtual void setCurrentProgram(int) = 0;
    virtual const String getProgramName(int) = 0; virtual void changeProgramName(int, const String&) = 0;
    virtual void getStateInformation(MemoryBlock&) = 0; virtual void setStateInformation(const void*, int) = 0;
    int getMainBusNumInputChannels() const { return 2; } int getMainBusNumOutputChannels() const { return 2; }
    AudioBuffer<float> getBusBuffer(AudioBuffer<float>&, bool, int) const { return {}; } int getBusCount(bool) const { return 0; } Bus* getBus(bool, int) const { return nullptr; }
    void setLatencySamples(int) {}
    static void copyXmlToBinary(const XmlElement&, MemoryBlock&) {}
    static std::unique_ptr<XmlElement> getXmlFromBinary(const void*, int) { return std::make_unique<XmlElement>(); }
};
struct ScopedNoDenormals {};
class AudioFormatReader { public: int64 lengthInSamples = 0; double sampleRate = 0; unsigned int numChannels = 0; bool read(AudioBuffer<float>*, int, int, int64, bool, bool) { return true; } };
class AudioFormatManager { public: void registerBasicFormats() {} AudioFormatReader* createReaderFor(const File&) { return nullptr; } };
class AudioProcessorEditor : public Component { public: explicit AudioProcessorEditor(AudioProcessor*) {} explicit AudioProcessorEditor(AudioProcessor&) {}
    void setResizable(bool, bool) {} void setResizeLimits(int, int, int, int) {} };
struct ParameterID { ParameterID(const char*, int) {} ParameterID(const String&, int) {} };
template <class T> struct NormalisableRange { NormalisableRange(T, T, T) {} };
class AudioParameterFloatAttributes { public: AudioParameterFloatAttributes withLabel(const String&) const { return *this; } };
class AudioParameterFloat : public RangedAudioParameter { public: AudioParameterFloat(const ParameterID&, const String&, NormalisableRange<float>, float, const AudioParameterFloatAttributes& = {}) {} };
class AudioParameterBool : public RangedAudioParameter { public: AudioParameterBool(const ParameterID&, const String&, bool) {} };
class AudioParameterChoice : public RangedAudioParameter { public: AudioParameterChoice(const ParameterID&, const String&, const StringArray&, int) {} };
class ValueTree { public: static ValueTree fromXml(const XmlElement&) { return {}; } String getType() const { return {}; } std::unique_ptr<XmlElement> createXml() const; };
inline std::unique_ptr<XmlElement> ValueTree::createXml() const { return std::make_unique<XmlElement>(); }
class AudioProcessorValueTreeState {
public:
    class ParameterLayout { public: void add(std::unique_ptr<RangedAudioParameter>) {} template <class T> void add(std::unique_ptr<T> p) { add(std::unique_ptr<RangedAudioParameter>(p.release())); } };
    AudioProcessorValueTreeState(AudioProcessor&, void*, const String&, ParameterLayout) {}
    RangedAudioParameter* getParameter(const String&) const { return nullptr; } std::atomic<float>* getRawParameterValue(const String&) const { return nullptr; }
    ValueTree state; ValueTree copyState() { return {}; } void replaceState(const ValueTree&) {}
    class SliderAttachment { public: SliderAttachment(AudioProcessorValueTreeState&, const String&, Slider&) {} };
    class ButtonAttachment { public: ButtonAttachment(AudioProcessorValueTreeState&, const String&, Button&) {} };
    class ComboBoxAttachment { public: ComboBoxAttachment(AudioProcessorValueTreeState&, const String&, ComboBox&) {} };
};
class CriticalSection {};
class Thread { public: explicit Thread(const String&) {} virtual ~Thread() = default; virtual void run() = 0; void signalThreadShouldExit() {} bool stopThread(int) { return true; } void startThread() {} };
}
