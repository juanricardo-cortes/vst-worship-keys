#pragma once
#include "PluginProcessor.h"
#include "../../worship-keys/Source/HardwareLook.h"

class AnthemLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnthemLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return juce::Font (juce::FontOptions (b.getHeight() > 32 ? 15.0f : 12.0f, juce::Font::bold)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (14.0f, h * 0.5f), juce::Font::bold)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class AnthemSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AnthemSynthEditor (AnthemSynthProcessor&);
    ~AnthemSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::String paramId, label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        juce::Rectangle<int> bounds;
    };

    struct Section
    {
        juce::String title;
        juce::Colour accent;
        std::vector<Control*> controls;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    std::unique_ptr<Control> makeKnob (const juce::String& id, const juce::String& label, juce::Component& parent, bool big);
    Control* addKnob (const juce::String& id, const juce::String& label);
    Control* addChoice (const juce::String& id, const juce::String& label);
    void addEasyKnob (const juce::String& id, const juce::String& label, juce::Colour accent);
    Section* addSection (const juce::String& title, juce::Colour accent, std::vector<Control*> items);
    void layoutRow (std::vector<Section*> row, juce::Rectangle<int> area);
    void layoutEasy();
    void paintFullPage (juce::Graphics&);
    void paintEasyPage (juce::Graphics&);
    void setEasyView (bool easy);
    void stepPreset (int delta);

    AnthemSynthProcessor& proc;
    AnthemLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls, easyKnobs;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<std::vector<Section*>> wideRows, compactRows;
    hw::Page fullPage, easyPage;
    juce::Viewport viewport;   // scrolls the full page on small screens
    bool compact = false;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, easyButton { "EASY" }, fullButton { "FULL" };
    juce::Label tempoLabel;
    hw::Keyboard keyboard;
    hw::CachedBackground background, fullBackground, easyBackground;
    juce::Rectangle<int> titleArea, scopeArea;
    std::array<float, AnthemSynthProcessor::scopeSize> scopeData {};
    float meterLevel = -60;
    int shownProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnthemSynthEditor)
};
