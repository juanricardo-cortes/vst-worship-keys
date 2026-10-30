#pragma once
#include "StationProcessor.h"
#include "../../worship-keys/Source/HardwareLook.h"

class StationLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StationLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (15.0f, h * 0.42f), juce::Font::bold)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
};

// Controller setup: learn the pack selector wheel / buttons, choose switch or split routing.
class SetupPanel : public juce::Component
{
public:
    explicit SetupPanel (StationProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (int blink);
    void paintContent (juce::Graphics&);
    std::function<void()> onClose;

private:
    struct Row
    {
        StationProcessor::Control control;
        juce::String label;
        juce::Label value;
        juce::TextButton learn { "LEARN" }, clear { "CLEAR" };
    };

    StationProcessor& proc;
    std::vector<std::unique_ptr<Row>> rows;
    juce::ComboBox wheelType;
    juce::TextButton switchButton { "ONE KEYBOARD - SWITCH PACKS" }, splitButton { "EACH PACK ON ITS OWN MIDI" }, doneButton { "DONE" };
    std::array<juce::ComboBox, StationProcessor::numPacks> channelBoxes, deviceBoxes;
    juce::Label monitor, deviceNote;
    juce::StringArray shownDevices;
    juce::Rectangle<int> controlsArea, routingArea, plateArea;
    hw::Page content;          // everything below the title; scrolls on a phone
    juce::Viewport viewport;

    void rebuildDeviceBoxes();
};

class StationEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StationEditor (StationProcessor&);
    ~StationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPack (int index);
    void layoutPackEditor();

    StationProcessor& proc;
    StationLookAndFeel lnf;
    std::array<juce::TextButton, StationProcessor::numPacks> packButtons;
    std::array<juce::Slider, StationProcessor::numPacks> volumeKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, StationProcessor::numPacks> volumeAttachments;
    std::array<float, StationProcessor::numPacks> meters {};
    juce::TextButton setupButton { "CONTROLLER SETUP" };
    juce::Label hint;
    SetupPanel setup;
    std::unique_ptr<juce::AudioProcessorEditor> packEditor;
    int shownPack = -1, blink = 0;
    juce::Rectangle<int> header, headerControls, body;
    bool compact = false;
    hw::CachedBackground background;
    static constexpr int cheekW = 14;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StationEditor)
};
