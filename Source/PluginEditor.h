#pragma once
#include "PluginProcessor.h"

class NordLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NordLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (14.0f, juce::Font::bold)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class WorshipKeysEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WorshipKeysEditor (WorshipKeysProcessor&);
    ~WorshipKeysEditor() override;

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
        std::vector<Control*> controls;
        juce::Rectangle<int> bounds;
    };

    // Hosts the knob sections so they can scroll when the screen is small (phones).
    struct Panel : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    void timerCallback() override;
    void paintSections (juce::Graphics&);
    Control* addKnob (const juce::String& id, const juce::String& label);
    Control* addChoice (const juce::String& id, const juce::String& label, const juce::StringArray& items, int firstItemId);
    void layoutRow (std::vector<Section*> row, juce::Rectangle<int> area);

    WorshipKeysProcessor& proc;
    NordLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<std::vector<Section*>> wideRows, compactRows;
    Panel panel;
    juce::Viewport viewport;
    juce::Rectangle<int> titleArea;
    bool compact = false;

    juce::ComboBox presetBox;
    juce::Label statusLabel;
    juce::MidiKeyboardComponent keyboard;
    int shownProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorshipKeysEditor)
};
